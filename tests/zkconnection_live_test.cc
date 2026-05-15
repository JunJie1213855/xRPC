// ZkConnection live 测试 — 真实 connect/createZnode/getData round-trip
// 需要环境变量 XRPC_ZK_HOST（默认 127.0.0.1:2181）或本机有 ZooKeeper。
#include <chrono>
#include <gtest/gtest.h>
#include <string>
#include <thread>

#include "test_fixture.h"
#include "zkconnection.h"

namespace
{
std::string MakeUniqueZnodePath()
{
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return "/xrpc_test_" + std::to_string(now);
}
}  // namespace

TEST_F(ZkLiveTest, ConnectSucceeds)
{
    ZkConnection conn;
    EXPECT_TRUE(conn.connect(ZkHost(), 3000));
    EXPECT_TRUE(conn.isConnected());
    EXPECT_NE(conn.getHandle(), nullptr);
}

TEST_F(ZkLiveTest, ConnectInvalidHostFails)
{
    ZkConnection conn;
    // 显式给一个保留地址 + 错误端口，必须失败且不阻塞太久
    EXPECT_FALSE(conn.connect("127.0.0.1:1", 1000));
    EXPECT_FALSE(conn.isConnected());
}

TEST_F(ZkLiveTest, CreateAndReadZnode)
{
    ZkConnection conn;
    ASSERT_TRUE(conn.connect(ZkHost(), 3000));

    std::string path = MakeUniqueZnodePath();
    std::string payload = "127.0.0.1:8000";
    ASSERT_TRUE(conn.createZnode(path.c_str(), payload.data(),
                                  static_cast<int>(payload.size())));

    std::string got = conn.getData(path.c_str());
    EXPECT_EQ(got, payload);
}

TEST_F(ZkLiveTest, GetDataOnMissingPathReturnsEmpty)
{
    ZkConnection conn;
    ASSERT_TRUE(conn.connect(ZkHost(), 3000));
    EXPECT_TRUE(conn.getData("/__no_such_path_xrpc__").empty());
}
