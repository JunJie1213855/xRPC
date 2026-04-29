# Xrpc

一个基于 C++ 和 Muduo 网络库的高性能 RPC 远程过程调用框架。

## 特性

- **高性能**: 基于 Muduo Reactor 网络模型，支持高并发 TCP 连接
- **服务发现**: 集成 ZooKeeper 实现服务的自动注册与发现
- **连接池**: ZooKeeper 连接池复用，显著降低连接开销
- **序列化**: 使用 Protobuf 进行高效的数据序列化/反序列化
- **多线程**: 支持多线程事件循环，处理高并发请求
- **粘包处理**: 自定义协议解决 TCP 粘包问题

## 项目结构

```
xRPC/
├── src/                    # 核心源代码
│   ├── include/            # 头文件
│   │   ├── Xrpcprovider.h  # RPC 服务提供者
│   │   ├── Xrpcchannel.h   # RPC 客户端通道
│   │   ├── Xrpcapplication.h # 应用单例
│   │   ├── Xrpcconfig.h    # 配置加载器
│   │   ├── Xrpccontroller.h # RPC 控制器
│   │   ├── XrpcLogger.h   # 日志封装
│   │   ├── zkconnection.h  # ZooKeeper 连接封装
│   │   └── zkclientpool.h  # ZooKeeper 连接池
│   ├── Xrpcprovider.cc     # 服务端实现
│   ├── Xrpcchannel.cc      # 客户端通道实现
│   ├── Xrpcapplication.cc  # 应用初始化
│   ├── Xrpcconfig.cc       # 配置解析
│   ├── Xrpccontroller.cc  # 控制器实现
│   ├── zkconnection.cc     # ZooKeeper 连接实现
│   ├── zkclientpool.cc     # 连接池实现
│   └── Xrpcheader.proto    # RPC 协议头定义
├── example/                # 示例代码
│   ├── callee/             # 服务端示例
│   │   └── Xserver.cc      # 用户服务提供者
│   └── caller/             # 客户端示例
│       └── Xclient.cc      # RPC 调用客户端
├── tests/                  # 单元测试
│   ├── CMakeLists.txt
│   ├── zkconnection_test.cc
│   ├── zkclientpool_test.cc
│   ├── Xrpcconfig_test.cc
│   ├── Xrpccontroller_test.cc
│   ├── Xrpcchannel_test.cc
│   └── Xrpcapplication_test.cc
├── bin/                    # 编译输出目录
│   ├── server              # 服务端可执行文件
│   ├── client              # 客户端可执行文件
│   └── test.conf           # 配置文件
├── CMakeLists.txt          # CMake 构建配置
└── LICENSE                 # 许可证
```

## 架构设计

### 核心组件

| 组件 | 文件 | 职责 |
|------|------|------|
| `XrpcProvider` | `Xrpcprovider.cc` | RPC 服务端，负责发布服务到 ZooKeeper，处理 incoming 请求 |
| `XrpcChannel` | `Xrpcchannel.cc` | RPC 客户端通道，负责向服务端发送请求并接收响应 |
| `XrpcApplication` | `Xrpcapplication.cc` | 应用单例类，负责框架初始化和配置管理 |
| `XrpcConfig` | `Xrpcconfig.cc` | 配置文件加载器，解析 `.conf` 格式的配置文件 |
| `ZkConnection` | `zkconnection.cc` | ZooKeeper 连接封装，管理 zhandle_t 生命周期 |
| `ZkClientPool` | `zkclientpool.cc` | ZooKeeper 连接池单例，实现连接复用和健康检查 |

### 通信协议

自定义 RPC 协议基于 TCP，使用 Protobuf 序列化：

```
[4字节 Total Length][4字节 Header Length][Header Data][Args Data]
```

- **Total Length**: 整个数据包长度（不含自身4字节）
- **Header Length**: Header 数据长度
- **Header Data**: `XrpcHeader` 序列化后的数据（service_name + method_name + args_size）
- **Args Data**: 方法参数的序列化数据

### 服务发布流程（服务端）

```
NotifyService(UserService)
    → 从 protobuf Service 提取 descriptor
    → 从连接池获取 ZooKeeper 连接
    → 将所有方法注册到 ZooKeeper (/UserService/Login → host:port)
    → 归还连接到连接池
    → 启动 Muduo TcpServer
    → OnMessage: 解析 XrpcHeader → 反序列化参数 → CallMethod → SendRpcResponse
```

### 服务发现流程（客户端）

```
XrpcChannel::QueryServiceHost(zkconn, service_name, method_name)
    → 从连接池获取 ZooKeeper 连接
    → ZooKeeper 查询 /UserService/Login
    → 返回 host:port 字符串，解析为 IP 和端口
    → 归还连接到连接池
    → newConnect(ip, port) 建立 TCP 连接
    → CallMethod 序列化请求 → 发送 → 等待响应
```

### ZooKeeper 连接池

`ZkClientPool` 实现单例模式，管理 ZooKeeper 连接的生命周期：

- **连接复用**: 从池中获取/归还连接，避免频繁创建/销毁
- **健康检查**: 后台线程定期清理过期连接
- **线程安全**: 使用 mutex 和 condition_variable 保护共享状态

## 依赖

- **C++11** 或更高版本
- **Muduo**: 高性能网络库
- **Protobuf 3.x**: 协议缓冲区
- **ZooKeeper**: 服务注册与发现
- **glog**: Google 日志库
- **CMake**: 构建工具

## 快速开始

### 编译项目

```bash
# 创建构建目录
mkdir -p build && cd build

# CMake 配置
cmake ..

# 编译
make -j$(nproc)
```

### 运行测试

```bash
# 运行单元测试
cd build/tests && ./zk_test

# 查看测试覆盖
ctest --output-on-failure
```

### 配置

编辑 `bin/test.conf` 配置文件：

```ini
rpcserverip=127.0.0.1
rpcserverport=8888
zookeeperip=127.0.0.1
zookeeperport=2181
```

### 运行

**1. 启动 ZooKeeper 服务**

确保 ZooKeeper 服务正常运行在配置的端口（默认 2181）。

**2. 启动服务端**

```bash
cd bin
./server -i test.conf
```

**3. 运行客户端**

```bash
cd bin
./client -i test.conf
```

## 添加新的 RPC 服务

### 步骤 1: 定义服务

在 `.proto` 文件中定义服务，使用 `option cc_generic_services = true`：

```protobuf
syntax = "proto3";
package user;

message LoginRequest {
    string name = 1;
    string pwd = 2;
}

message LoginResponse {
    int32 errcode = 1;
    string errmsg = 2;
    bool success = 3;
}

service UserServiceRpc {
    rpc Login(LoginRequest) returns (LoginResponse);
}
```

### 步骤 2: 生成代码

```bash
protoc --cpp_out=./ user.proto
```

### 步骤 3: 实现服务

继承生成的 `XxxServiceRpc` 类并重写 RPC 方法：

```cpp
class UserService : public user::UserServiceRpc {
public:
    bool Login(std::string name, std::string pwd) {
        // 本地业务逻辑
        return true;
    }

    void Login(::google::protobuf::RpcController* controller,
               const ::user::LoginRequest* request,
               ::user::LoginResponse* response,
               ::google::protobuf::Closure* done) override {
        // 调用本地方法
        bool result = Login(request->name(), request->pwd());

        // 填充响应
        response->set_success(result);
        done->Run();
    }
};
```

### 步骤 4: 发布服务

```cpp
int main(int argc, char** argv) {
    XrpcApplication::Init(argc, argv);
    XrpcProvider provider;
    provider.NotifyService(new UserService());
    provider.Run();
    return 0;
}
```

## 单元测试

项目使用 GoogleTest 进行单元测试：

| 测试文件 | 测试内容 |
|---------|---------|
| `zkconnection_test.cc` | ZkConnection 构造函数、连接状态、getData、createZnode |
| `zkclientpool_test.cc` | ZkClientPool 单例、初始状态、销毁 |
| `Xrpcconfig_test.cc` | 配置加载、键查找、空白字符修剪 |
| `Xrpccontroller_test.cc` | 失败状态、错误文本、Reset |
| `Xrpcchannel_test.cc` | 构造函数 |
| `Xrpcapplication_test.cc` | 单例获取 |

运行测试：

```bash
cd build/tests
./zk_test
```

## 协议细节

### 粘包处理

使用 `recv_exact()` 函数确保读取完整的数据包：

```cpp
ssize_t recv_exact(int fd, char* buf, size_t size) {
    size_t total_read = 0;
    while (total_read < size) {
        ssize_t ret = recv(fd, buf + total_read, size - total_read, 0);
        if (ret == 0) return 0;  // 对端关闭
        if (ret == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        total_read += ret;
    }
    return total_read;
}
```

### 字节序

所有多字节整数使用 **网络字节序（大端序）** 传输：
- 发送：`htonl()`
- 接收：`ntohl()`

## 许可证

本项目采用 MIT 许可证。详见 [LICENSE](LICENSE) 文件。