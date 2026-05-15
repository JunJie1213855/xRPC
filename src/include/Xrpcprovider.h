#ifndef _Xrpcprovider_H__
#define _Xrpcprovider_H__

#include <google/protobuf/descriptor.h>
#include <google/protobuf/service.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

#include <asio/awaitable.hpp>
#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

/**
 * @brief 服务端 RPC 提供者，使用 standalone asio + C++20 协程实现。
 *  1. 服务注册：将每个服务的 service/method 节点写入 ZooKeeper，节点值为 ip:port。
 *  2. 接收请求：协程读取 [4B total_len][4B header_len][header][args] 帧，
 *     解析 RpcHeader 找到 service+method，调用对应实现，再回写 [4B resp_len][resp]。
 *  3. 线程模型：1 个 acceptor io_context + N 个 worker io_context（子 reactor 风格），
 *     新连接 round-robin 分发到 worker，会话内部天然串行执行。
 */
class XrpcProvider
{
public:
    XrpcProvider() = default;
    ~XrpcProvider();

    XrpcProvider(const XrpcProvider &) = delete;
    XrpcProvider &operator=(const XrpcProvider &) = delete;

    // 注册服务对象及其方法
    void NotifyService(google::protobuf::Service *service);

    // 启动 RPC 服务节点，阻塞到 io_context 退出
    void Run();

private:
    struct ServiceInfo
    {
        google::protobuf::Service *service;
        std::unordered_map<std::string, const google::protobuf::MethodDescriptor *> method_map;
    };

    // worker io_context 数量（对齐原 muduo setThreadNum(4)）
    static constexpr std::size_t kWorkerCount = 4;

    using WorkGuard = asio::executor_work_guard<asio::io_context::executor_type>;

    // 协程：在 acceptor io_context 上 accept 新连接并 round-robin 分发到 worker
    asio::awaitable<void> AcceptLoop(std::string ip, uint16_t port);

    // 协程：单个连接的会话循环（读帧 → 派发 → 回包）
    asio::awaitable<void> Session(asio::ip::tcp::socket sock);

    // 注册 service_map 中的服务到 ZooKeeper（沿用原同步逻辑）
    bool RegisterToZookeeper(const std::string &ip, uint16_t port);

    std::unordered_map<std::string, ServiceInfo> service_map;

    std::unique_ptr<asio::io_context> acceptor_ioc_;
    std::array<std::unique_ptr<asio::io_context>, kWorkerCount> workers_;
    std::array<std::unique_ptr<WorkGuard>, kWorkerCount> guards_;
    std::array<std::thread, kWorkerCount> worker_threads_;
    std::atomic<std::size_t> next_worker_{0};
};

#endif
