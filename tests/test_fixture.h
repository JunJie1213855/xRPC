#pragma once

#include <chrono>
#include <cstdlib>
#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "zkconnection.h"

// 检测当前环境是否能连上 ZooKeeper（默认 127.0.0.1:2181）。
// 在 CI 的 unit-only job 中 zk 不可用，需要的测试用 GTEST_SKIP 跳过。
inline bool ZkAvailable()
{
    const char *host = std::getenv("XRPC_ZK_HOST");
    std::string addr = host ? host : "127.0.0.1:2181";
    ZkConnection probe;
    return probe.connect(addr, /*timeout_ms=*/2000);
}

inline std::string ZkHost()
{
    const char *host = std::getenv("XRPC_ZK_HOST");
    return host ? host : "127.0.0.1:2181";
}

// 需要 zk 的 fixture：自动检测并跳过
class ZkLiveTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        if (!ZkAvailable())
        {
            GTEST_SKIP() << "ZooKeeper not reachable at " << ZkHost();
        }
    }
};

class ZkClientPoolLiveTest : public ZkLiveTest {};
class RpcE2ELiveTest : public ZkLiveTest {};
