#ifndef _zkconnection_h_
#define _zkconnection_h_

#include <zookeeper/zookeeper.h>
#include <atomic>
#include <chrono>
#include <string>

/**
 * @brief ZooKeeper 连接包装类
 *
 * 封装 zhandle_t 提供连接管理、数据获取等功能
 * 支持连接状态检查和过期检测
 */
class ZkConnection
{
public:
    ZkConnection();
    ~ZkConnection();

    // 禁用拷贝
    ZkConnection(const ZkConnection &) = delete;
    ZkConnection &operator=(const ZkConnection &) = delete;

    // 连接 ZooKeeper 服务器
    bool connect(const std::string &host, int timeout_ms = 6000);

    // 关闭连接
    void close();

    bool createZnode(const char *path, const char *data, int datalen, int state = 0);

    // 获取节点数据
    std::string getData(const char *path);

    // 状态查询
    bool isConnected() const { return m_connected.load(); }
    bool isExpired() const;

    // 获取底层句柄（供健康检查用）
    zhandle_t *getHandle() const { return m_zhandle; }

private:
    zhandle_t *m_zhandle;                                   // ZooKeeper 句柄
    std::atomic<bool> m_connected;                          // 连接状态
    std::chrono::steady_clock::time_point m_createTime;     // 创建时间
    std::chrono::steady_clock::time_point m_lastActiveTime; // 最后活跃时间
};

#endif
