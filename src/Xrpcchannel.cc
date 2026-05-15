#include "Xrpcchannel.h"

#include <arpa/inet.h>

#include <array>
#include <cstring>
#include <exception>
#include <future>
#include <mutex>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

#include <asio/buffer.hpp>
#include <asio/co_spawn.hpp>
#include <asio/connect.hpp>
#include <asio/read.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/use_future.hpp>
#include <asio/write.hpp>

#include "Xrpcheader.pb.h"
#include "XrpcLogger.h"

XrpcChannel::XrpcChannel(bool /*connectNow*/)
    : ioc_(1),
      guard_(std::make_unique<WorkGuard>(ioc_.get_executor())),
      sock_(),
      service_name_(),
      method_name_(),
      m_ip(),
      m_port(0),
      m_idx(0)
{
    // connectNow 参数保留以保持构造签名兼容；
    // 真实连接延迟到首次 CallMethod 时建立，
    // 因为构造期还没有 method 信息可用于 zookeeper 查找。
}

XrpcChannel::~XrpcChannel()
{
    if (guard_)
        guard_->reset();
    if (!ioc_.stopped())
        ioc_.stop();
    if (io_thread_.joinable())
        io_thread_.join();
}

void XrpcChannel::EnsureIoThread()
{
    if (!io_thread_.joinable())
    {
        io_thread_ = std::thread([this] { ioc_.run(); });
    }
}

void XrpcChannel::CallMethod(const google::protobuf::MethodDescriptor *method,
                             google::protobuf::RpcController *controller,
                             const google::protobuf::Message *request,
                             google::protobuf::Message *response,
                             google::protobuf::Closure *done)
{
    EnsureIoThread();
    auto fut = asio::co_spawn(
        ioc_,
        CallImpl(method, request, response),
        asio::use_future);
    try
    {
        fut.get();
    }
    catch (const std::exception &e)
    {
        if (controller)
            controller->SetFailed(e.what());
    }
    if (done)
        done->Run();
}

asio::awaitable<void> XrpcChannel::CallImpl(
    const google::protobuf::MethodDescriptor *method,
    const google::protobuf::Message *request,
    google::protobuf::Message *response)
{
    if (!sock_)
    {
        service_name_ = method->service()->name();
        method_name_ = method->name();

        auto zk_conn = ZkClientPool::getInstance().getConnection();
        if (!zk_conn || !zk_conn->isConnected())
        {
            throw std::runtime_error("zookeeper connection error");
        }
        std::string host_data =
            QueryServiceHost(zk_conn.get(), service_name_, method_name_, m_idx);
        ZkClientPool::getInstance().returnConnection(std::move(zk_conn));

        if (host_data.empty() || host_data == " ")
        {
            throw std::runtime_error("zookeeper query error");
        }
        m_ip = host_data.substr(0, m_idx);
        std::string port_str = host_data.substr(m_idx + 1);
        try
        {
            m_port = static_cast<uint16_t>(std::stoul(port_str));
        }
        catch (const std::exception &)
        {
            throw std::runtime_error("invalid port: " + port_str);
        }

        sock_.emplace(co_await asio::this_coro::executor);
        asio::ip::tcp::endpoint ep(asio::ip::make_address(m_ip), m_port);
        co_await sock_->async_connect(ep, asio::use_awaitable);
    }

    std::string args_str;
    if (!request->SerializeToString(&args_str))
    {
        throw std::runtime_error("serialize request fail");
    }

    Xrpc::RpcHeader xrpc_header;
    xrpc_header.set_service_name(service_name_);
    xrpc_header.set_method_name(method_name_);
    xrpc_header.set_args_size(args_str.size());

    std::string rpc_header_str;
    if (!xrpc_header.SerializeToString(&rpc_header_str))
    {
        throw std::runtime_error("serialize rpc header error");
    }

    // 帧格式: [4B total_len][4B header_len][header][args]
    // total_len = 4 + header_size + args_size
    std::uint32_t header_size = static_cast<std::uint32_t>(rpc_header_str.size());
    std::uint32_t total_len =
        4u + header_size + static_cast<std::uint32_t>(args_str.size());
    std::uint32_t net_total_len = htonl(total_len);
    std::uint32_t net_header_len = htonl(header_size);

    std::array<asio::const_buffer, 4> send_bufs = {
        asio::buffer(&net_total_len, 4),
        asio::buffer(&net_header_len, 4),
        asio::buffer(rpc_header_str),
        asio::buffer(args_str),
    };
    try
    {
        co_await asio::async_write(*sock_, send_bufs, asio::use_awaitable);

        // 响应: [4B resp_len][resp]
        std::uint32_t resp_len_net = 0;
        co_await asio::async_read(
            *sock_, asio::buffer(&resp_len_net, 4), asio::use_awaitable);
        std::uint32_t resp_len = ntohl(resp_len_net);
        if (resp_len > MAX_RESPONSE_LEN)
        {
            throw std::runtime_error("response length too long");
        }
        std::vector<char> body(resp_len);
        co_await asio::async_read(
            *sock_, asio::buffer(body), asio::use_awaitable);

        if (!response->ParseFromArray(body.data(), static_cast<int>(resp_len)))
        {
            throw std::runtime_error("parse response error");
        }
    }
    catch (...)
    {
        // 网络层异常后强制重连
        std::error_code ec;
        if (sock_)
        {
            sock_->shutdown(asio::ip::tcp::socket::shutdown_both, ec);
            sock_->close(ec);
            sock_.reset();
        }
        throw;
    }
    co_return;
}

// ZkConnection::getData 不是线程安全的，保留一把互斥锁
std::string XrpcChannel::QueryServiceHost(ZkConnection *zkconn,
                                          const std::string &service_name,
                                          const std::string &method_name,
                                          int &idx)
{
    static std::mutex zk_mtx;
    std::string method_path = "/" + service_name + "/" + method_name;

    std::string host_data;
    {
        std::lock_guard<std::mutex> lk(zk_mtx);
        host_data = zkconn->getData(method_path.c_str());
    }
    if (host_data.empty())
    {
        LOG(ERROR) << method_path << " is not exist!";
        return " ";
    }
    auto pos = host_data.find(':');
    if (pos == std::string::npos)
    {
        LOG(ERROR) << method_path << " address is invalid!";
        return " ";
    }
    idx = static_cast<int>(pos);
    return host_data;
}
