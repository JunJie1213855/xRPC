// End-to-end RPC 测试：在同一进程内同时跑 XrpcProvider + XrpcChannel，
// 走真实 zk 注册、协程 accept+session、客户端协程 connect+read+write 全链路。
//
// 用例:
//  1. 启动 provider 监听 127.0.0.1:<port>，注册到 zk。
//  2. 用 channel 调用 UserServiceRpc.Login，验证响应字段。
//  3. 并发 50 次同时调用，验证多线程都成功。
//
// 需要 zk 可达；不可达则全部 SKIP。
//
// 注意：复用 example/user.proto 生成的 stub，避免再写一份测试用 proto。
#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "Xrpcapplication.h"
#include "Xrpcchannel.h"
#include "Xrpccontroller.h"
#include "Xrpcprovider.h"
#include "test_fixture.h"
#include "test_integration_helpers.h"
#include "zkclientpool.h"

// example/user.pb.h 由 protoc 生成；测试 CMakeLists 会引入相同的 user.pb.cc
#include "../example/user.pb.h"

namespace
{
// 一个内嵌的 UserService 实现，Login 直接根据用户名返回固定结果，Register 返回失败
class EchoUserService : public Kuser::UserServiceRpc
{
public:
    std::atomic<int> login_calls{0};

    void Login(google::protobuf::RpcController *,
               const Kuser::LoginRequest *request,
               Kuser::LoginResponse *response,
               google::protobuf::Closure *done) override
    {
        login_calls.fetch_add(1);
        bool ok = request->name() == "zhangsan" && request->pwd() == "123456";
        response->mutable_result()->set_errcode(ok ? 0 : 1);
        response->mutable_result()->set_errmsg(ok ? "" : "auth failed");
        response->set_success(ok);
        done->Run();
    }

    void Register(google::protobuf::RpcController *,
                  const Kuser::RegisterRequest *,
                  Kuser::RegisterResponse *response,
                  google::protobuf::Closure *done) override
    {
        response->mutable_result()->set_errcode(1);
        response->mutable_result()->set_errmsg("not supported in e2e test");
        response->set_success(false);
        done->Run();
    }
};

// 在临时端口起一个 provider，注册一个 EchoUserService 服务；
// 返回 unique_ptr 以便测试用例显式销毁来触发 stop。
struct ProviderHandle
{
    std::unique_ptr<XrpcProvider> provider;
    std::unique_ptr<EchoUserService> svc;
    std::thread run_thread;

    ~ProviderHandle()
    {
        provider.reset();          // 析构 → 停 acceptor + worker
        if (run_thread.joinable()) // Run() 在 acceptor 停后会返回
            run_thread.join();
    }
};

}  // namespace

TEST_F(RpcE2ELiveTest, LoginRoundTrip)
{
    EnsureAppInited();

    ProviderHandle p;
    p.svc = std::make_unique<EchoUserService>();
    p.provider = std::make_unique<XrpcProvider>();
    p.provider->NotifyService(p.svc.get());

    p.run_thread = std::thread([&p] { p.provider->Run(); });

    // 等 provider 在 zk 注册并开始监听
    std::this_thread::sleep_for(std::chrono::milliseconds(800));

    // 调用 Login
    Kuser::UserServiceRpc_Stub stub(new XrpcChannel(false));
    Kuser::LoginRequest req;
    req.set_name("zhangsan");
    req.set_pwd("123456");
    Kuser::LoginResponse resp;
    XrpcController ctrl;

    stub.Login(&ctrl, &req, &resp, nullptr);

    ASSERT_FALSE(ctrl.Failed()) << ctrl.ErrorText();
    EXPECT_TRUE(resp.success());
    EXPECT_EQ(resp.result().errcode(), 0);
    EXPECT_GE(p.svc->login_calls.load(), 1);
}

TEST_F(RpcE2ELiveTest, LoginConcurrent50)
{
    EnsureAppInited();

    ProviderHandle p;
    p.svc = std::make_unique<EchoUserService>();
    p.provider = std::make_unique<XrpcProvider>();
    p.provider->NotifyService(p.svc.get());

    p.run_thread = std::thread([&p] { p.provider->Run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(800));

    constexpr int kThreads = 10;
    constexpr int kIters = 5;
    std::atomic<int> ok{0}, fail{0};

    std::vector<std::thread> ts;
    for (int i = 0; i < kThreads; ++i)
    {
        ts.emplace_back([&ok, &fail] {
            Kuser::UserServiceRpc_Stub stub(new XrpcChannel(false));
            Kuser::LoginRequest req;
            req.set_name("zhangsan");
            req.set_pwd("123456");
            for (int j = 0; j < kIters; ++j)
            {
                Kuser::LoginResponse resp;
                XrpcController ctrl;
                stub.Login(&ctrl, &req, &resp, nullptr);
                if (!ctrl.Failed() && resp.success())
                    ok.fetch_add(1);
                else
                    fail.fetch_add(1);
            }
        });
    }
    for (auto &t : ts)
        t.join();

    EXPECT_EQ(ok.load(), kThreads * kIters);
    EXPECT_EQ(fail.load(), 0);
}

TEST_F(RpcE2ELiveTest, LoginFailsWhenServerDown)
{
    EnsureAppInited();
    // 注意：先确保 provider 已注册过一次（让 zk 里有 /UserServiceRpc/Login 数据）
    {
        ProviderHandle p;
        p.svc = std::make_unique<EchoUserService>();
        p.provider = std::make_unique<XrpcProvider>();
        p.provider->NotifyService(p.svc.get());
        p.run_thread = std::thread([&p] { p.provider->Run(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
        // p 离开作用域 → 注销 (临时节点) + 端口释放
    }
    // 给 zk session 一点时间清理临时节点
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    Kuser::UserServiceRpc_Stub stub(new XrpcChannel(false));
    Kuser::LoginRequest req;
    req.set_name("zhangsan");
    req.set_pwd("123456");
    Kuser::LoginResponse resp;
    XrpcController ctrl;

    stub.Login(&ctrl, &req, &resp, nullptr);
    // 服务端不在：要么 zk 查不到地址、要么 connect 失败，总之 controller.Failed() == true
    EXPECT_TRUE(ctrl.Failed());
}
