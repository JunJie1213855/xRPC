#include "zkclientpool.h"
#include "XrpcLogger.h"
#include "Xrpcapplication.h"

// 定义 constexpr 成员变量
constexpr size_t ZkClientPool::kMaxPoolSize;
constexpr size_t ZkClientPool::kMinPoolSize;
constexpr int ZkClientPool::kHealthCheckIntervalMs;
constexpr int ZkClientPool::kConnectionMaxAgeMs;

ZkClientPool::ZkClientPool()
    : m_running(false), m_healthCheckThread(nullptr)
{
    // 从配置读取 ZooKeeper 地址
    auto &config = XrpcApplication::GetInstance().GetConfig();
    m_zkHost = config.Load("zookeeperip") + ":" + config.Load("zookeeperport");
}

ZkClientPool::~ZkClientPool()
{
    destroy();
}

ZkClientPool &ZkClientPool::getInstance()
{
    static ZkClientPool instance;
    return instance;
}

void ZkClientPool::start(int initialSize)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_running)
    {
        LOG(WARNING) << "ZkClientPool already started";
        return;
    }

    m_running = true;

    // 预创建初始连接
    int count = std::min(static_cast<size_t>(initialSize), kMaxPoolSize);
    for (int i = 0; i < count; ++i)
    {
        auto conn = createConnection();
        if (conn && conn->isConnected())
        {
            m_availableQueue.push(conn);
            m_allConnections.push_back(conn);
        }
    }

    LOG(INFO) << "ZkClientPool started with " << m_allConnections.size()
              << " connections";

    // 启动健康检查线程
    m_healthCheckThread = new std::thread(&ZkClientPool::healthCheckLoop, this);
}

void ZkClientPool::destroy()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_running)
        {
            return;
        }

        m_running = false;
        m_cv.notify_all();
    }

    // 等待健康检查线程结束
    if (m_healthCheckThread && m_healthCheckThread->joinable())
    {
        m_healthCheckThread->join();
        delete m_healthCheckThread;
        m_healthCheckThread = nullptr;
    }

    // 清理所有连接
    std::lock_guard<std::mutex> lock(m_mutex);
    while (!m_availableQueue.empty())
    {
        m_availableQueue.pop();
    }
    m_allConnections.clear();

    LOG(INFO) << "ZkClientPool destroyed";
}

std::shared_ptr<ZkConnection> ZkClientPool::createConnection()
{
    auto conn = std::make_shared<ZkConnection>();
    if (conn->connect(m_zkHost))
    {
        return conn;
    }
    return nullptr;
}

std::shared_ptr<ZkConnection> ZkClientPool::getConnection(int timeout_ms)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(timeout_ms);

    // 等待可用连接或超时
    while (m_availableQueue.empty() && m_running)
    {
        auto remaining = deadline - std::chrono::steady_clock::now();
        if (remaining <= std::chrono::milliseconds(0))
        {
            // 超时，创建临时连接（不放入池中）
            LOG(WARNING) << "ZkClientPool: timeout getting connection, creating temporary";
            return createConnection();
        }

        if (m_cv.wait_for(lock, remaining) == std::cv_status::timeout)
        {
            break;
        }
    }

    if (!m_availableQueue.empty())
    {
        auto conn = m_availableQueue.front();
        m_availableQueue.pop();

        // 检查连接是否有效
        if (isConnectionValid(conn.get()))
        {
            return conn;
        }

        // 连接无效，创建新的
        LOG(INFO) << "ZkClientPool: reusing expired connection, creating new one";
        conn = createConnection();
        return conn;
    }

    // 池已停止或超时，创建临时连接
    return createConnection();
}

void ZkClientPool::returnConnection(std::shared_ptr<ZkConnection> connection)
{
    if (!connection)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_running)
    {
        return; // 池已销毁，不归还
    }

    // 检查连接是否仍然有效
    if (isConnectionValid(connection.get()))
    {
        m_availableQueue.push(connection);
        m_cv.notify_one(); // 通知等待的消费者
    }
    else
    {
        LOG(INFO) << "ZkClientPool: returned invalid connection, discarding";
    }
}

bool ZkClientPool::isConnectionValid(ZkConnection *conn) const
{
    if (!conn)
    {
        return false;
    }

    if (!conn->isConnected())
    {
        return false;
    }

    // 检查连接是否过期
    if (conn->isExpired())
    {
        return false;
    }

    // 使用 zoo_state() 检查 ZooKeeper 连接状态
    int state = zoo_state(conn->getHandle());
    if (state != ZOO_CONNECTED_STATE)
    {
        return false;
    }

    return true;
}

size_t ZkClientPool::availableCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_availableQueue.size();
}

void ZkClientPool::healthCheckLoop()
{
    while (m_running)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(kHealthCheckIntervalMs));

        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_running)
        {
            break;
        }

        // 清理可用队列中的失效连接
        std::queue<std::shared_ptr<ZkConnection>> newQueue;
        while (!m_availableQueue.empty())
        {
            auto conn = m_availableQueue.front();
            m_availableQueue.pop();

            if (isConnectionValid(conn.get()))
            {
                newQueue.push(conn);
            }
            else
            {
                // 从所有连接中移除
                auto it = std::find(m_allConnections.begin(), m_allConnections.end(), conn);
                if (it != m_allConnections.end())
                {
                    m_allConnections.erase(it);
                }
                LOG(INFO) << "ZkClientPool: removed expired connection";
            }
        }
        m_availableQueue = std::move(newQueue);

        // 补充连接到最小数量
        while (m_allConnections.size() < kMinPoolSize && m_running)
        {
            auto conn = createConnection();
            if (conn && conn->isConnected())
            {
                m_availableQueue.push(conn);
                m_allConnections.push_back(conn);
            }
            else
            {
                break;
            }
        }

        LOG(INFO) << "ZkClientPool: health check done, available="
                  << m_availableQueue.size()
                  << ", total=" << m_allConnections.size();
    }
}
