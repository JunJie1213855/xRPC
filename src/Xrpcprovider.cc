#include "Xrpcprovider.h"

#include <arpa/inet.h>

#include <array>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

#include <asio/buffer.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/error.hpp>
#include <asio/read.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/write.hpp>

#include <google/protobuf/stubs/callback.h>

#include "XrpcLogger.h"
#include "Xrpcapplication.h"
#include "Xrpcheader.pb.h"
#include "zkclientpool.h"

namespace
{
// 单个 RPC 帧体最大长度，避免恶意/异常帧导致大内存分配（含 [4B header_len][header][args]）
constexpr std::uint32_t kMaxFrameBytes = 64u * 1024u * 1024u; // 64MB
} // namespace

void XrpcProvider::NotifyService(google::protobuf::Service *service)
{
    ServiceInfo service_info;

    const google::protobuf::ServiceDescriptor *psd = service->GetDescriptor();
    std::string service_name = psd->name();
    int method_count = psd->method_count();

    LOG(INFO) << "service_name=" << service_name;
    for (int i = 0; i < method_count; ++i)
    {
        const google::protobuf::MethodDescriptor *pmd = psd->method(i);
        std::string method_name = pmd->name();
        LOG(INFO) << "method_name=" << method_name;
        service_info.method_map.emplace(method_name, pmd);
    }
    service_info.service = service;
    service_map.emplace(std::move(service_name), std::move(service_info));
}

bool XrpcProvider::RegisterToZookeeper(const std::string &ip, uint16_t port)
{
    auto zkclient = ZkClientPool::getInstance().getConnection();

    for (auto &sp : service_map)
    {
        std::string service_path = "/" + sp.first;
        if (!zkclient->createZnode(service_path.c_str(), nullptr, 0))
        {
            ZkClientPool::getInstance().returnConnection(zkclient);
            return false;
        }
        for (auto &mp : sp.second.method_map)
        {
            std::string method_path = service_path + "/" + mp.first;
            char method_path_data[128] = {0};
            std::snprintf(method_path_data, sizeof(method_path_data), "%s:%u",
                          ip.c_str(), static_cast<unsigned>(port));
            if (!zkclient->createZnode(method_path.c_str(), method_path_data,
                                       sizeof(method_path_data)))
            {
                ZkClientPool::getInstance().returnConnection(zkclient);
                return false;
            }
        }
    }
    ZkClientPool::getInstance().returnConnection(zkclient);
    return true;
}

void XrpcProvider::Run()
{
    std::string ip = XrpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    uint16_t port = 0;
    try
    {
        port = static_cast<uint16_t>(std::stoul(
            XrpcApplication::GetInstance().GetConfig().Load("rpcserverport")));
    }
    catch (const std::exception &e)
    {
        LOG(ERROR) << "Invalid port: "
                   << XrpcApplication::GetInstance().GetConfig().Load("rpcserverport");
        return;
    }

    // 1. 启动 worker io_contexts + 线程
    for (std::size_t i = 0; i < kWorkerCount; ++i)
    {
        workers_[i] = std::make_unique<asio::io_context>(1);
        guards_[i] = std::make_unique<WorkGuard>(workers_[i]->get_executor());
        worker_threads_[i] = std::thread([this, i] { workers_[i]->run(); });
    }

    // 2. zookeeper 注册
    if (!RegisterToZookeeper(ip, port))
    {
        LOG(ERROR) << "register services to zookeeper failed";
        for (auto &g : guards_)
        {
            if (g)
                g->reset();
        }
        for (auto &t : worker_threads_)
        {
            if (t.joinable())
                t.join();
        }
        return;
    }

    // 3. acceptor io_context 在当前线程跑
    acceptor_ioc_ = std::make_unique<asio::io_context>(1);
    asio::co_spawn(*acceptor_ioc_, AcceptLoop(ip, port), asio::detached);

    LOG(INFO) << "RpcProvider start service at ip:" << ip << " port:" << port;
    acceptor_ioc_->run();

    // 4. 清理 worker
    for (auto &g : guards_)
    {
        if (g)
            g->reset();
    }
    for (auto &t : worker_threads_)
    {
        if (t.joinable())
            t.join();
    }
}

asio::awaitable<void> XrpcProvider::AcceptLoop(std::string ip, uint16_t port)
{
    asio::ip::tcp::acceptor acc(*acceptor_ioc_,
                                asio::ip::tcp::endpoint(asio::ip::make_address(ip), port));

    for (;;)
    {
        std::size_t idx = next_worker_.fetch_add(1, std::memory_order_relaxed) % kWorkerCount;
        try
        {
            asio::ip::tcp::socket sock(workers_[idx]->get_executor());
            co_await acc.async_accept(sock, asio::use_awaitable);
            asio::co_spawn(workers_[idx]->get_executor(),
                           Session(std::move(sock)), asio::detached);
        }
        catch (const std::system_error &e)
        {
            LOG(WARNING) << "accept error: " << e.code().message();
        }
    }
}

asio::awaitable<void> XrpcProvider::Session(asio::ip::tcp::socket sock)
{
    try
    {
        for (;;)
        {
            // 读 [4B total_len]
            std::uint32_t total_len_net = 0;
            co_await asio::async_read(sock, asio::buffer(&total_len_net, 4),
                                      asio::use_awaitable);
            std::uint32_t total_len = ntohl(total_len_net);
            if (total_len < 4 || total_len > kMaxFrameBytes)
            {
                LOG(ERROR) << "invalid frame total_len=" << total_len;
                break;
            }

            // 读完整帧体: [4B header_len][header][args]
            std::vector<char> frame(total_len);
            co_await asio::async_read(sock, asio::buffer(frame), asio::use_awaitable);

            std::uint32_t header_len_net = 0;
            std::memcpy(&header_len_net, frame.data(), 4);
            std::uint32_t header_len = ntohl(header_len_net);
            if (header_len + 4u > total_len)
            {
                LOG(ERROR) << "invalid header_len=" << header_len
                           << " total_len=" << total_len;
                break;
            }

            Xrpc::RpcHeader xrpc_header;
            if (!xrpc_header.ParseFromArray(frame.data() + 4, static_cast<int>(header_len)))
            {
                LOG(ERROR) << "header parse error";
                break;
            }
            std::string args_str(frame.data() + 4 + header_len,
                                 total_len - 4 - header_len);

            const std::string &service_name = xrpc_header.service_name();
            const std::string &method_name = xrpc_header.method_name();

            auto it = service_map.find(service_name);
            if (it == service_map.end())
            {
                LOG(ERROR) << service_name << " is not exist!";
                break;
            }
            auto mit = it->second.method_map.find(method_name);
            if (mit == it->second.method_map.end())
            {
                LOG(ERROR) << service_name << "." << method_name << " is not exist!";
                break;
            }

            google::protobuf::Service *service = it->second.service;
            const google::protobuf::MethodDescriptor *method = mit->second;

            std::unique_ptr<google::protobuf::Message> request(
                service->GetRequestPrototype(method).New());
            if (!request->ParseFromString(args_str))
            {
                LOG(ERROR) << "request parse error";
                break;
            }
            std::unique_ptr<google::protobuf::Message> response(
                service->GetResponsePrototype(method).New());

            // 同步派发：项目示例的 Service 实现是同步的，CallMethod 返回后 response 已就绪
            google::protobuf::Closure *done =
                google::protobuf::NewCallback(&google::protobuf::DoNothing);
            service->CallMethod(method, nullptr, request.get(), response.get(), done);

            // 回包 [4B resp_len][resp]
            std::string response_str;
            if (!response->SerializeToString(&response_str))
            {
                LOG(ERROR) << "serialize response error";
                break;
            }
            std::uint32_t resp_len_net = htonl(static_cast<std::uint32_t>(response_str.size()));
            std::array<asio::const_buffer, 2> bufs = {
                asio::buffer(&resp_len_net, 4),
                asio::buffer(response_str),
            };
            co_await asio::async_write(sock, bufs, asio::use_awaitable);
        }
    }
    catch (const std::system_error &e)
    {
        if (e.code() != asio::error::eof &&
            e.code() != asio::error::connection_reset &&
            e.code() != asio::error::operation_aborted)
        {
            LOG(WARNING) << "session: " << e.code().message();
        }
    }
    catch (const std::exception &e)
    {
        LOG(WARNING) << "session: " << e.what();
    }

    std::error_code ec;
    sock.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
    sock.close(ec);
    co_return;
}

XrpcProvider::~XrpcProvider()
{
    LOG(INFO) << "~XrpcProvider()";
    if (acceptor_ioc_ && !acceptor_ioc_->stopped())
    {
        acceptor_ioc_->stop();
    }
    for (auto &g : guards_)
    {
        if (g)
            g->reset();
    }
    for (auto &w : workers_)
    {
        if (w && !w->stopped())
            w->stop();
    }
    for (auto &t : worker_threads_)
    {
        if (t.joinable())
            t.join();
    }
}
