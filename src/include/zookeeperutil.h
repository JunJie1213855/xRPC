#ifndef _zookeeperutil_h_
#define _zookeeperutil_h_

#include <semaphore.h>
#include <zookeeper/zookeeper.h>
#include <string>
#include <queue>
#include <atomic>
#include <optional>


/**
 * @brief 封装的zk客户端，服务端用于创建消息节点
 */
class ZkClient
{
public:
    ZkClient();
    ~ZkClient();

    // zkclient启动连接zkserver
    void Start();

    // 在zkserver中创建一个节点，根据指定的path
    void Create(const char *path, const char *data, int datalen, int state = 0);

    // 根据参数指定的znode节点路径，或者znode节点值
    std::string GetData(const char *path);

private:
    // Zk的客户端句柄
    zhandle_t *m_zhandle;
};

// 可以在这里写一个连接池


#endif
