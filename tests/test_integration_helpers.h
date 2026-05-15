// Integration / E2E 测试共享 helper：
//   - 确保 XrpcApplication 已加载临时配置（zk host 来自 XRPC_ZK_HOST 或默认 127.0.0.1:2181）
//   - 必须在所有需要 ZkClientPool / XrpcChannel / XrpcProvider 的测试 SetUp 中调用
#pragma once

#include <cstdio>
#include <cstdlib>
#include <gtest/gtest.h>
#include <string>
#include <unistd.h>

#include "Xrpcapplication.h"
#include "test_fixture.h"
#include "zkclientpool.h"

// 注意：每次调用都保证 ZkClientPool 处于 started 状态。
//
// 必要性：ZkClientPoolLiveTest.ConcurrentGetReturnDoesNotDeadlock 等用例会调用
// pool.destroy()。destroy 后 pool 的 m_running=false，此时 getConnection 会回退到
// 创建一次性临时连接；returnConnection 因 m_running=false 直接丢弃。临时连接
// 析构后 zk session 关闭，其创建的 EPHEMERAL znode 会被 zk 立即删除——
// 导致 RpcE2E 测试客户端查询时 path "is not exist"。
inline void EnsureAppInited()
{
    static bool inited = false;
    if (!inited)
    {
        auto host_full = ZkHost();
        auto colon = host_full.find(':');
        std::string zkip = colon == std::string::npos ? host_full
                                                      : host_full.substr(0, colon);
        std::string zkport = colon == std::string::npos ? "2181"
                                                        : host_full.substr(colon + 1);

        char tmpl[] = "/tmp/xrpc_e2e_conf_XXXXXX";
        int fd = mkstemp(tmpl);
        ASSERT_GE(fd, 0);
        close(fd);
        {
            FILE *f = std::fopen(tmpl, "w");
            std::fprintf(f, "rpcserverip=127.0.0.1\nrpcserverport=8801\n");
            std::fprintf(f, "zookeeperip=%s\nzookeeperport=%s\n",
                         zkip.c_str(), zkport.c_str());
            std::fclose(f);
        }

        char arg0[] = "test";
        char arg1[] = "-i";
        char *argv[] = {arg0, arg1, tmpl, nullptr};
        XrpcApplication::Init(3, argv);
        inited = true;
    }

    // 即使是后续调用：若前序测试 destroy() 过 pool，必须重新 start，
    // 否则 EPHEMERAL znode 会在临时连接析构时立即消失。
    auto &pool = ZkClientPool::getInstance();
    if (pool.totalCount() == 0)
    {
        pool.start(4);
    }
}
