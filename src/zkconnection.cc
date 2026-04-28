#include "zkconnection.h"
#include "XrpcLogger.h"
#include <mutex>
#include <condition_variable>

// 连接上下文，用于在 watcher 回调中传递信息
// 生命周期由 ZkConnection 对象管理，在 connect() 调用期间有效
struct ConnectionContext
{
    std::mutex mutex;
    std::condition_variable cv;
    bool connected = false;
};

// ZooKeeper watcher 回调函数
void connection_watcher(zhandle_t *zh, int type, int state,
              const char *path, void *context)
{
    (void)zh;
    (void)path;
    if (type == ZOO_SESSION_EVENT)
    {
        if (state == ZOO_CONNECTED_STATE)
        {
            auto *ctx = static_cast<ConnectionContext *>(context);
            if (ctx)
            {
                std::lock_guard<std::mutex> lock(ctx->mutex);
                ctx->connected = true;
                ctx->cv.notify_all();
            }
        }
    }
}

ZkConnection::ZkConnection()
    : m_zhandle(nullptr), m_connected(false), m_createTime(std::chrono::steady_clock::now()), m_lastActiveTime(m_createTime)
{
}

ZkConnection::~ZkConnection()
{
    close();
}

bool ZkConnection::connect(const std::string &host, int timeout_ms)
{
    if (m_zhandle != nullptr)
    {
        close();
    }

    ConnectionContext ctx;

    // 使用 zookeeper_init 异步建立连接
    // connection_watcher 会在连接成功时被调用
    m_zhandle = zookeeper_init(
        host.c_str(),
        connection_watcher,
        timeout_ms,
        nullptr, // recv timeout
        &ctx,    // context 传递给 watcher
        0        // flags
    );

    if (m_zhandle == nullptr)
    {
        LOG(ERROR) << "zookeeper_init failed, host: " << host;
        return false;
    }

    // 等待连接成功
    {
        std::unique_lock<std::mutex> lock(ctx.mutex);
        auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(timeout_ms);

        while (!ctx.connected)
        {
            auto remaining = deadline - std::chrono::steady_clock::now();
            if (remaining <= std::chrono::milliseconds(0))
            {
                break;
            }
            // 使用 wait_for 等待，带超时
            ctx.cv.wait_for(lock, remaining);
        }

        if (!ctx.connected)
        {
            LOG(ERROR) << "zookeeper connection timeout, host: " << host;
            close();
            return false;
        }
    }

    m_createTime = std::chrono::steady_clock::now();
    m_lastActiveTime = m_createTime;
    m_connected = true;
    LOG(INFO) << "zookeeper connected, host: " << host;
    return true;
}

bool ZkConnection::createZnode(const char *path, const char *data, int datalen, int state)
{

    char path_buffer[128]; // 用于存储创建的节点路径
    int bufferlen = sizeof(path_buffer);

    // 检查节点是否已经存在
    int flag = zoo_exists(m_zhandle, path, 0, nullptr);
    if (flag == ZNONODE)
    { // 如果节点不存在
        // 创建指定的ZooKeeper节点
        flag = zoo_create(m_zhandle, path, data, datalen, &ZOO_OPEN_ACL_UNSAFE, state, path_buffer, bufferlen);
        if (flag == ZOK)
        { // 创建成功
            LOG(INFO) << "znode create success... path:" << path;
        }
        else
        { // 创建失败
            LOG(ERROR) << "znode create failed... path:" << path;
            return false; // 退出程序
        }
    }
    return true;
}

void ZkConnection::close()
{
    if (m_zhandle != nullptr)
    {
        zookeeper_close(m_zhandle);
        m_zhandle = nullptr;
    }
    m_connected = false;
}

bool ZkConnection::isExpired() const
{
    constexpr int kConnectionMaxAgeMs = 300000; // 5分钟
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now - m_createTime)
                       .count();
    return elapsed > kConnectionMaxAgeMs;
}

std::string ZkConnection::getData(const char *path)
{
    if (m_zhandle == nullptr || !m_connected.load())
    {
        LOG(ERROR) << "getData: not connected";
        return "";
    }

    char buf[256];
    int bufferlen = sizeof(buf);

    int flag = zoo_get(m_zhandle, path, 0, buf, &bufferlen, nullptr);
    if (flag != ZOK)
    {
        LOG(ERROR) << "zoo_get failed, path: " << path;
        return "";
    }

    m_lastActiveTime = std::chrono::steady_clock::now();
    return std::string(buf, bufferlen > 0 ? bufferlen : 0);
}
