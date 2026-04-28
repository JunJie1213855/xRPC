# ZooKeeper 连接池实现

## 修改时间

2026-04-28

## 背景

原 `XrpcChannel` 每次 RPC 调用都创建新的 `ZkClient`：

```cpp
// 之前 - 每次调用都新建连接
ZkClient zkCli;
zkCli.Start();
std::string host_data = QueryServiceHost(&zkCli, service_name, method_name, m_idx);
// zkCli 析构，关闭连接
```

这导致每次 RPC 调用都经历：创建客户端 → TCP 三次握手 → ZooKeeper 连接认证 → 查询 → 关闭连接，代价极高。

## 新增文件

| 文件 | 说明 |
|------|------|
| `src/include/zkconnection.h` | ZkConnection 连接包装类 |
| `src/zkconnection.cc` | ZkConnection 实现 |
| `src/include/zkclientpool.h` | ZkClientPool 连接池单例类 |
| `src/zkclientpool.cc` | 连接池实现 |

## ZkConnection 类

封装 `zhandle_t` 提供连接管理：

```cpp
class ZkConnection {
public:
    ZkConnection();
    ~ZkConnection();

    bool connect(const std::string& host, int timeout_ms = 6000);
    void close();
    bool createZnode(const char* path, const char* data, int datalen, int state = 0);
    std::string getData(const char* path);

    bool isConnected() const { return m_connected.load(); }
    bool isExpired() const;
    zhandle_t* getHandle() const { return m_zhandle; }

private:
    zhandle_t* m_zhandle;
    std::atomic<bool> m_connected;
    std::chrono::steady_clock::time_point m_createTime;
    std::chrono::steady_clock::time_point m_lastActiveTime;
};
```

## ZkClientPool 单例类

管理连接池生命周期：

```cpp
class ZkClientPool {
public:
    static ZkClientPool& getInstance();

    std::shared_ptr<ZkConnection> getConnection(int timeout_ms = 3000);
    void returnConnection(std::shared_ptr<ZkConnection> connection);

    void start(int initialSize = 4);
    void destroy();

    size_t availableCount() const;
    size_t totalCount() const;

private:
    // ... 内部实现
};
```

## 修改的文件

### XrpcChannel.cc

```cpp
// 之前 (每次调用都新建连接)
ZkClient zkCli;
zkCli.Start();
std::string host_data = QueryServiceHost(&zkCli, service_name, method_name, m_idx);

// 之后 (从连接池获取)
auto zkConn = ZkClientPool::getInstance().getConnection();
std::string host_data = QueryServiceHost(zkConn.get(), service_name, method_name, m_idx);
ZkClientPool::getInstance().returnConnection(std::move(zkConn));
```

### XrpcProvider.cc

```cpp
// 服务端注册服务到 ZooKeeper
auto zkConn = ZkClientPool::getInstance().getConnection();
// 创建服务节点和方法节点...
ZkClientPool::getInstance().returnConnection(zkConn);
```

### XrpcApplication.cc

```cpp
// Init() 中启动连接池
ZkClientPool::getInstance().start();

// 析构时销毁
ZkClientPool::getInstance().destroy();
```

## BUG 修复

### CRITICAL: g_connected 全局状态污染

**问题**: 所有 ZkConnection 实例共享同一个 `g_connected` 原子变量

**修复**: 移除全局变量，改用 ConnectionContext 仅通过 context 指针传递

### CRITICAL: ConnectionContext 栈分配悬空指针

**问题**: ConnectionContext 是栈上局部变量，但传递给 zookeeper_init

**修复**: ctx 在 connect() 调用期间有效，watcher 回调通过 context 指针访问

## 性能提升

- **之前**: 每次 RPC 调用都创建新连接 → TCP 三次握手 → ZooKeeper 认证 → 查询 → 关闭
- **之后**: 从池中获取可用连接 → 查询 → 归还连接

显著减少 ZooKeeper 连接建立开销。

## 提交记录

`8a5ccd3` - feat: 实现 ZooKeeper 连接池复用
