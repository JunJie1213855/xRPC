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

inline void EnsureAppInited()
{
    static bool inited = false;
    if (inited)
        return;

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
