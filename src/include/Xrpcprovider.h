#ifndef _Xrpcprovider_H__
#define _Xrpcprovider_H__
#include "google/protobuf/service.h"
#include "zookeeperutil.h"
#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include <google/protobuf/descriptor.h>
#include <functional>
#include <string>
#include <unordered_map>
/**
 * @brief 该类用于注册服务，处理客户端的请求
 * 1.服务注册：根据当前服务类提供的名称和方法，将其注册到ZooKeeper中，具体来说 节点 ZNode = /${service}/${method}，节点的值为 ${ip}:${port}
 * 2.接收请求：接收客户端的请求包，将其解包 = 包大小 + 头长度 + 头数据 + 参数数据，其中头数据包含服务名称和方法名称，通过服务名称和方法名称，到具体的服务类上执行方法
 * 3.
 */
class XrpcProvider
{

private:
    // 服务信息
    struct ServiceInfo
    {
        google::protobuf::Service *service; // 服务基类指针，会指向派生类，用于调用其指定服务
        std::unordered_map<std::string, const google::protobuf::MethodDescriptor *> method_map; // 方法和方向描述的映射
    };

public:
    // 这里是提供给外部使用的，可以发布rpc方法的函数接口。
    void NotifyService(google::protobuf::Service *service);
    // 析构清理资源
    ~XrpcProvider();
    // 启动rpc服务节点，开始提供rpc远程网络调用服务
    void Run();

private:
    // 连接回调， 客户端的连接
    void OnConnection(const muduo::net::TcpConnectionPtr &conn);
    // 消息回调
    void OnMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buffer, muduo::Timestamp receive_time);
    // 返回Rpc响应
    void SendRpcResponse(const muduo::net::TcpConnectionPtr &conn, google::protobuf::Message *response);

private:
    muduo::net::EventLoop event_loop;

    std::unordered_map<std::string, ServiceInfo> service_map; // 保存服务对象和rpc方法
};
#endif
