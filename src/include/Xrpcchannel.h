#ifndef _Xrpcchannel_h_
#define _Xrpcchannel_h_

#include <google/protobuf/service.h>
#include <unistd.h>
#include <array>
#include <mutex>
#include "zkclientpool.h"


// 最大响应回包大小
constexpr size_t MAX_RESPONSE_LEN = 64 * 1024 * 1024; // 64 MB

/**
 * @brief 此类是继承自google::protobuf::RpcChannel
 * 目的是为了给客户端调用Rpc时，架构内部使用
 * 1.发送请求：客户端调用方法 -> 从 zookeeper 获取服务端地址 = ip : port -> 向服务端发送 方法 + 参数 数据
 * 2.接收响应：接收来自服务端的响应 -> 将tcp回包解包 = 包头 + 包体 -> 包体反序列化为响应response -> 响应返回给客户端
 */
class XrpcChannel : public google::protobuf::RpcChannel
{
public:
    XrpcChannel(const XrpcChannel &) = delete;
    XrpcChannel &operator=(const XrpcChannel &) = delete;

public:
    XrpcChannel(bool connectNow);
    virtual ~XrpcChannel()
    {
        if (m_clientfd >= 0)
        {
            close(m_clientfd);
        }
    }
    void CallMethod(const ::google::protobuf::MethodDescriptor *method,
                    ::google::protobuf::RpcController *controller,
                    const ::google::protobuf::Message *request,
                    ::google::protobuf::Message *response,
                    ::google::protobuf::Closure *done) override; // override可以验证是否是虚函数
private:
    // 根据ip和port保存客户端的连接信息，也就是说 channel
    bool newConnect(const char *ip, uint16_t port);

    // 获取服务的地址 ip:port
    std::string QueryServiceHost(ZkConnection *zkconn, const std::string &service_name, const std::string &method_name, int &idx);

    // 新增：确保读取指定长度的数据，解决TCP拆包
    ssize_t recv_exact(int fd, char *buf, size_t size);

private:
    std::mutex mtx;           // 数据互斥锁
    int m_clientfd;           // 存放服务端套接字
    std::string service_name; // 服务名称
    std::string m_ip;         // 服务端 ip 地址
    uint16_t m_port;          // 服务端服务的端口
    std::string method_name;  // 调用的方法名称
    int m_idx;                // 用来划分服务器ip和port的下标
};
#endif
