# xRPC

基于 **C++20 协程 + standalone asio + Protobuf + ZooKeeper** 的轻量级 RPC 框架。

服务端用 1 个 acceptor `io_context` + N 个 worker `io_context`（muduo 子 reactor 风格）承载连接；客户端封装为同步 `CallMethod`（兼容 protobuf stub）但底层走 asio 协程，对外提供熟悉的 `stub.Method(...)` 调用方式。

实测单机回环（100 线程 × 5000 请求 = 500K RPC）成功率 100%，QPS ≈ 50K–60K。

## 特性

- **C++20 协程**：服务端会话与客户端调用均为 `asio::awaitable<void>`，"读包头 → 读包体 → 派发 → 回包"线性表达，无回调地狱。
- **standalone asio**：header-only，不依赖 Boost。
- **多 io_context 子 reactor**：1 个 acceptor + 4 个 worker，新连接 round-robin 分发，会话内部天然串行。
- **ZooKeeper 服务发现**：服务端启动时将 `/<service>/<method> → ip:port` 注册到 zk；客户端首次调用时查询。
- **ZooKeeper 连接池**：单例 `ZkClientPool`，4 个长连接复用，避免 zk 反复建连。
- **Protobuf 帧协议**：自定义两段长度前缀帧，明确处理粘包/拆包。
- **客户端同步语义保留**：`stub.Login(...)` 调用方式不变，stub 代码零改动。

## 项目结构

```
xRPC/
├── CMakeLists.txt              # C++20 / -DASIO_STANDALONE / libs: protobuf, pthread, zookeeper_mt, glog
├── src/
│   ├── include/
│   │   ├── Xrpcprovider.h      # 服务端：1 acceptor + N worker io_context
│   │   ├── Xrpcchannel.h       # 客户端：per-channel io_context + 协程 CallImpl
│   │   ├── Xrpcapplication.h   # 框架入口、配置加载、zk 连接池启动
│   │   ├── Xrpcconfig.h        # .conf 文件解析
│   │   ├── Xrpccontroller.h    # protobuf RpcController 实现
│   │   ├── XrpcLogger.h        # glog 包装
│   │   ├── zkconnection.h      # 单条 zk 连接（zhandle_t RAII）
│   │   └── zkclientpool.h      # zk 连接池（单例）
│   ├── Xrpcprovider.cc         # 服务端：AcceptLoop / Session 协程
│   ├── Xrpcchannel.cc          # 客户端：CallImpl 协程 + CallMethod 同步壳
│   ├── Xrpcapplication.cc
│   ├── Xrpcconfig.cc
│   ├── Xrpccontroller.cc
│   ├── zkconnection.cc
│   ├── zkclientpool.cc
│   └── Xrpcheader.proto        # RpcHeader = service_name + method_name + args_size
├── example/
│   ├── test.conf               # 示例配置
│   ├── user.proto              # UserServiceRpc { Login, Register }
│   ├── callee/Xserver.cc       # 服务端示例
│   └── caller/Xclient.cc       # 客户端示例（100 线程 × 5000 请求压测）
├── tests/                      # GoogleTest 单元测试
└── bin/                        # 编译产物 server / client
```

## 架构

### 服务端线程模型

```
                                   ┌──── worker_ioc_[0] ──── thread 0
                                   │     (session 协程在此)
                                   │
   ┌──────────────────┐  accept    ├──── worker_ioc_[1] ──── thread 1
   │  acceptor_ioc_   │ ─────────→ │
   │  (主线程)         │ round-robin├──── worker_ioc_[2] ──── thread 2
   └──────────────────┘            │
                                   └──── worker_ioc_[3] ──── thread 3
```

- `acceptor_ioc_` 跑在 `Run()` 调用线程，只做 `async_accept`。
- 每个 worker `io_context` 独占一根线程，`executor_work_guard` 保证空闲不退出。
- 新连接被 move 到下一个 worker 的执行器上，`co_spawn` 启动 `Session` 协程。
- 会话只在分配到的那个 worker 上执行，**无锁、无 strand**。

对应 `src/include/Xrpcprovider.h`：

```cpp
static constexpr std::size_t kWorkerCount = 4;

std::unique_ptr<asio::io_context> acceptor_ioc_;
std::array<std::unique_ptr<asio::io_context>, kWorkerCount> workers_;
std::array<std::unique_ptr<WorkGuard>, kWorkerCount> guards_;
std::array<std::thread, kWorkerCount> worker_threads_;
std::atomic<std::size_t> next_worker_{0};
```

### 客户端模型

每个 `XrpcChannel` 实例自带一个 `asio::io_context` + 后台线程；`CallMethod` 用 `co_spawn(ioc_, CallImpl(...), use_future).get()` 把协程结果同步返回，保持 protobuf stub 接口语义不变。

```cpp
void CallMethod(...) override {
    EnsureIoThread();
    auto fut = asio::co_spawn(ioc_, CallImpl(method, req, resp), asio::use_future);
    try        { fut.get(); }
    catch (...) { controller->SetFailed(...); }
    if (done)  done->Run();
}
```

### 通信协议

请求帧（client → server）：

```
[4B total_len (net)] [4B header_len (net)] [Xrpc::RpcHeader bytes] [args bytes]
                     └─────────────── total_len bytes ──────────────┘
```

- `Xrpc::RpcHeader` = `{ service_name, method_name, args_size }`。
- `total_len = 4 + header_len + args_size`（包含 header_len 字段本身，不含开头 4B 的 total_len）。

响应帧（server → client）：

```
[4B resp_len (net)] [serialized response bytes]
```

所有 4 字节长度字段走 **网络字节序**（`htonl` / `ntohl`）。

### 服务发布流程（服务端）

```
provider.NotifyService(new UserService)
    ├─ 从 protobuf ServiceDescriptor 提取所有方法
    └─ service_map[service_name] = { service*, { method_name → MethodDescriptor* } }

provider.Run()
    ├─ 启动 4 个 worker io_context + 线程 + work_guard
    ├─ ZooKeeper 注册：循环创建 /<service> 永久节点和 /<service>/<method> = "ip:port" 临时节点
    ├─ 在 acceptor_ioc_ 上 co_spawn(AcceptLoop)
    └─ acceptor_ioc_->run()  // 阻塞

AcceptLoop()  [协程]
    while true:
        idx = next_worker_++ % 4
        sock = co_await acc.async_accept(workers_[idx]->get_executor())
        co_spawn(workers_[idx], Session(std::move(sock)), detached)

Session(sock)  [协程]
    while true:
        co_await async_read(sock, &total_len, 4)
        co_await async_read(sock, frame, total_len)
        解析 [header_len][header][args] → 找到 service+method
        service->CallMethod(method, nullptr, req, resp, NewCallback(&DoNothing))
        co_await async_write(sock, [resp_len][resp])
```

### 服务发现流程（客户端）

```
stub.Login(controller, &req, &resp, nullptr)
    └─ XrpcChannel::CallMethod
         ├─ EnsureIoThread()  // 懒启动 io_thread
         └─ co_spawn(ioc_, CallImpl(...), use_future).get()

CallImpl()  [协程]
    if (!sock_):                                # 首次调用
        zk = ZkClientPool::getInstance().getConnection()
        host_data = QueryServiceHost(zk, "UserServiceRpc", "Login", idx)  # 从 zk 读 "ip:port"
        ZkClientPool::returnConnection(zk)
        sock_.emplace(executor)
        co_await sock_->async_connect({ip, port})

    co_await async_write(sock, [total_len][header_len][header][args])
    co_await async_read(sock, &resp_len, 4)
    co_await async_read(sock, body, resp_len)
    response->ParseFromArray(body, resp_len)
```

异常路径：协程中抛出的 `std::system_error` 被 `CallMethod` 捕获并转为 `controller->SetFailed(e.what())`，socket 被 reset 让下一次调用重连。

### ZooKeeper 连接池

`src/include/zkclientpool.h` 的单例 `ZkClientPool`：

- `start(N)` 创建 N 条长连接（默认 4 条）。
- `getConnection()` / `returnConnection()` 复用避免反复 `zookeeper_init`。
- 后台心跳线程剔除掉过期 session（`isExpired()`）。
- 析构时统一关闭，与 `XrpcApplication` 生命周期绑定。

## 依赖

| 依赖 | 用途 | apt 包 |
|------|------|--------|
| GCC 13+ / Clang 16+ | C++20 协程 | `g++-13` |
| CMake ≥ 3.20 | 构建 | `cmake` |
| standalone asio ≥ 1.28 | 网络 + 协程 | `libasio-dev` |
| Protobuf 3.x | 序列化 + RpcChannel 抽象 | `libprotobuf-dev`, `protobuf-compiler` |
| Apache ZooKeeper C 客户端 | 服务注册/发现 | `libzookeeper-mt-dev` + 运行中的 zk server |
| glog | 日志 | `libgoogle-glog-dev` |
| GoogleTest | 单元测试 | `libgtest-dev`（或 `/usr/local` 自编译） |

> 历史版本曾使用 muduo 作为网络层，已在 2026-05 切换到 standalone asio + C++20 协程。`CMakeLists.txt` 中通过 `-DASIO_STANDALONE` 强制只用 asio，不引入 Boost。

## 快速开始

### 1. 安装依赖

```bash
sudo apt install -y g++-13 cmake libasio-dev libprotobuf-dev protobuf-compiler \
                    libzookeeper-mt-dev libgoogle-glog-dev libgtest-dev
```

### 2. 启动 ZooKeeper

```bash
# 假设你已安装 ZooKeeper
sudo systemctl start zookeeper
# 或本地直接起一个 standalone
zkServer.sh start
```

确认能连：

```bash
zkCli.sh -server 127.0.0.1:2181
```

### 3. 生成 protobuf 代码

```bash
cd src && protoc --cpp_out=. Xrpcheader.proto && cd ..
cd example && protoc --cpp_out=. user.proto && cd ..
```

### 4. 编译

```bash
mkdir -p build && cd build
cmake ..
cmake --build . -j$(nproc)
```

产物：

- `bin/server` —— 示例服务端
- `bin/client` —— 示例客户端（100 线程 × 5000 请求压测）
- `build/tests/zk_test` —— 单元测试

### 5. 运行

终端 A：

```bash
./bin/server -i ./example/test.conf
```

终端 B：

```bash
./bin/client -i ./example/test.conf
```

`bin/client` 跑完会打印：

```
Total requests: 500000
Success count: 500000
Fail count: 0
Elapsed time: ~9s
QPS: ~50000
```

可以另开终端在 `zkCli.sh` 里验证服务注册：

```
[zk] ls /UserServiceRpc
[Login, Register]
[zk] get /UserServiceRpc/Login
127.0.0.1:8000
```

## 配置文件

`example/test.conf`：

```ini
rpcserverip=127.0.0.1
rpcserverport=8000
zookeeperip=127.0.0.1
zookeeperport=2181
```

`XrpcApplication::Init(argc, argv)` 会从 `-i <file>` 加载。

## 写一个新的 RPC 服务

### Step 1 · 定义 proto

`my_service.proto`：

```protobuf
syntax = "proto3";
package myapp;
option cc_generic_services = true;

message EchoRequest  { string msg = 1; }
message EchoResponse { string echoed = 1; }

service EchoService {
    rpc Echo(EchoRequest) returns (EchoResponse);
}
```

```bash
protoc --cpp_out=. my_service.proto
```

### Step 2 · 实现服务端

```cpp
#include "Xrpcapplication.h"
#include "Xrpcprovider.h"
#include "my_service.pb.h"

class EchoImpl : public myapp::EchoService {
public:
    void Echo(google::protobuf::RpcController* ctrl,
              const myapp::EchoRequest*  req,
              myapp::EchoResponse*       resp,
              google::protobuf::Closure* done) override {
        resp->set_echoed(req->msg());
        done->Run();
    }
};

int main(int argc, char** argv) {
    XrpcApplication::Init(argc, argv);
    XrpcProvider provider;
    provider.NotifyService(new EchoImpl());
    provider.Run();  // 阻塞
    return 0;
}
```

### Step 3 · 实现客户端

```cpp
#include "Xrpcapplication.h"
#include "Xrpcchannel.h"
#include "Xrpccontroller.h"
#include "my_service.pb.h"

int main(int argc, char** argv) {
    XrpcApplication::Init(argc, argv);

    myapp::EchoService_Stub stub(new XrpcChannel(false));
    myapp::EchoRequest  req;  req.set_msg("hello");
    myapp::EchoResponse resp;
    XrpcController ctrl;

    stub.Echo(&ctrl, &req, &resp, nullptr);
    if (ctrl.Failed())
        std::cerr << "rpc error: " << ctrl.ErrorText() << std::endl;
    else
        std::cout << "echoed: " << resp.echoed() << std::endl;
    return 0;
}
```

## 单元测试

```bash
cd build && ctest --output-on-failure
# 或直接：
./build/tests/zk_test
```

| 测试文件 | 覆盖范围 |
|---------|---------|
| `zkconnection_test.cc` | ZkConnection 构造、连接状态、getData、createZnode |
| `zkclientpool_test.cc` | ZkClientPool 单例、初始状态、销毁 |
| `Xrpcconfig_test.cc` | 配置加载、键查找、空白字符修剪 |
| `Xrpccontroller_test.cc` | 失败状态、错误文本、Reset |
| `Xrpcchannel_test.cc` | 通道构造 |
| `Xrpcapplication_test.cc` | 单例获取 |

## 设计取舍

| 决策 | 选择 | 理由 |
|------|------|------|
| asio 发行版 | standalone | 不引入 Boost；header-only ~3MB |
| 服务端线程模型 | N 个 io_context 子 reactor | 对齐原 muduo `setThreadNum(4)`；会话天然串行，无需 strand |
| 客户端协程模型 | per-channel io_context + 后台线程 | `CallMethod` 同步包装保持 protobuf stub 兼容 |
| 服务派发 | 同步调用 `CallMethod` 后立即 `async_write` | 项目 service 实现是同步的；如未来需异步可改为 `co_await promise` |
| zk 查询 | 同步调用 `QueryServiceHost` | 首次调用一次性开销，async 化收益低 |
| 帧最大长度 | 64 MB | 防止恶意 `total_len` 引发巨大分配 |

## 许可证

MIT License — 详见 [LICENSE](LICENSE)。
