#ifndef _zkclientpool_h_
#define _zkclientpool_h_

#include "zkconnection.h"
#include <memory>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <string>
#include <thread>

/**
 * @brief ZooKeeper 连接池单例类
 *
 * 管理 ZooKeeper 连接的生命周期，提供连接的获取和归还
 * 支持连接复用和健康检查
 */
class ZkClientPool
{
public:
    // 获取单例实例
    static ZkClientPool &getInstance();

    // 禁用拷贝
    ZkClientPool(const ZkClientPool &) = delete;
    ZkClientPool &operator=(const ZkClientPool &) = delete;

    // 启动连接池（预创建连接）
    void start(int initialSize = 4);

    // 销毁连接池
    void destroy();

    // 获取连接（带超时）
    std::shared_ptr<ZkConnection> getConnection(int timeout_ms = 3000);

    // 归还连接
    void returnConnection(std::shared_ptr<ZkConnection> connection);

    // 获取连接池状态（调试用）
    size_t availableCount() const;
    size_t totalCount() const { return m_allConnections.size(); }

    // 检查连接是否有效
    bool isConnectionValid(ZkConnection *conn) const;

private:
    ZkClientPool();
    ~ZkClientPool();

    // 创建新连接
    std::shared_ptr<ZkConnection> createConnection();

    // 健康检查线程函数
    void healthCheckLoop();

private:
    // 连接池配置常量
    static constexpr size_t kMaxPoolSize = 16;
    static constexpr size_t kMinPoolSize = 2;
    static constexpr int kHealthCheckIntervalMs = 30000; // 30秒检查一次
    static constexpr int kConnectionMaxAgeMs = 300000;   // 连接最大存活5分钟

    // 成员变量
    std::queue<std::shared_ptr<ZkConnection>> m_availableQueue;  // 可用连接队列
    std::vector<std::shared_ptr<ZkConnection>> m_allConnections; // 所有创建的连接

    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_running;

    // std::thread *m_healthCheckThread; // 健康检查线程
    std::unique_ptr<std::thread> m_healthCheckThread;

    std::string m_zkHost; // ZooKeeper 主机地址
};

#endif
