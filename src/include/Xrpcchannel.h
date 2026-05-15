#ifndef _Xrpcchannel_h_
#define _Xrpcchannel_h_

#include <google/protobuf/service.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <thread>

#include <asio/awaitable.hpp>
#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

#include "zkclientpool.h"

// 最大响应回包大小
constexpr size_t MAX_RESPONSE_LEN = 64 * 1024 * 1024; // 64 MB

/**
 * @brief RpcChannel 客户端实现（standalone asio + C++20 协程）。
 *
 * 对外保持 google::protobuf::RpcChannel::CallMethod 同步语义不变，
 * 内部通过 co_spawn + use_future 包装一层把协程结果同步返回，
 * 使得现有 protobuf stub 调用方（Xclient.cc）无需任何改动即可工作。
 *
 * 调用流程：
 *  1. 首次调用从 zookeeper 查询服务地址、async_connect 建链。
 *  2. async_write 三段缓冲：[total_len][header_len][header][args]。
 *  3. async_read 4B 响应长度 → 按长度读 body → 反序列化到 response。
 */
class XrpcChannel : public google::protobuf::RpcChannel
{
public:
    XrpcChannel(const XrpcChannel &) = delete;
    XrpcChannel &operator=(const XrpcChannel &) = delete;

public:
    explicit XrpcChannel(bool connectNow);
    ~XrpcChannel() override;

    void CallMethod(const google::protobuf::MethodDescriptor *method,
                    google::protobuf::RpcController *controller,
                    const google::protobuf::Message *request,
                    google::protobuf::Message *response,
                    google::protobuf::Closure *done) override;

private:
    // 协程版核心实现：返回 awaitable，可被同步 CallMethod 包装或被外层协程直接 co_await
    asio::awaitable<void> CallImpl(const google::protobuf::MethodDescriptor *method,
                                   const google::protobuf::Message *request,
                                   google::protobuf::Message *response);

    // 懒启动后台 io_thread；同步 CallMethod 在该线程上跑协程
    void EnsureIoThread();

    // 从 ZooKeeper 查询服务地址，沿用原同步实现（启动期一次性开销）
    std::string QueryServiceHost(ZkConnection *zkconn,
                                 const std::string &service_name,
                                 const std::string &method_name,
                                 int &idx);

    using WorkGuard = asio::executor_work_guard<asio::io_context::executor_type>;

    asio::io_context ioc_;
    std::unique_ptr<WorkGuard> guard_;
    std::thread io_thread_;
    std::optional<asio::ip::tcp::socket> sock_;

    std::string service_name_;
    std::string method_name_;
    std::string m_ip;
    uint16_t m_port;
    int m_idx;
};

#endif
