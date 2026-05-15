// ZkClientPool live 测试 — 真实连接池 start/get/return/destroy
#include <gtest/gtest.h>
#include <thread>
#include <vector>

#include "test_fixture.h"
#include "test_integration_helpers.h"
#include "zkclientpool.h"

TEST_F(ZkClientPoolLiveTest, StartCreatesNConnections)
{
    EnsureAppInited();
    auto &pool = ZkClientPool::getInstance();
    pool.destroy();  // 清理掉之前可能残留的状态

    pool.start(4);
    EXPECT_EQ(pool.totalCount(), 4u);
    EXPECT_EQ(pool.availableCount(), 4u);

    pool.destroy();
    EXPECT_EQ(pool.totalCount(), 0u);
    EXPECT_EQ(pool.availableCount(), 0u);
}

TEST_F(ZkClientPoolLiveTest, GetAndReturnConnection)
{
    EnsureAppInited();
    auto &pool = ZkClientPool::getInstance();
    pool.destroy();
    pool.start(2);

    auto c1 = pool.getConnection(2000);
    ASSERT_TRUE(c1 != nullptr);
    EXPECT_EQ(pool.availableCount(), 1u);

    auto c2 = pool.getConnection(2000);
    ASSERT_TRUE(c2 != nullptr);
    EXPECT_EQ(pool.availableCount(), 0u);

    pool.returnConnection(c1);
    EXPECT_EQ(pool.availableCount(), 1u);
    pool.returnConnection(c2);
    EXPECT_EQ(pool.availableCount(), 2u);

    pool.destroy();
}

TEST_F(ZkClientPoolLiveTest, ConcurrentGetReturnDoesNotDeadlock)
{
    EnsureAppInited();
    auto &pool = ZkClientPool::getInstance();
    pool.destroy();
    pool.start(4);

    constexpr int kThreads = 8;
    constexpr int kIters = 50;
    std::vector<std::thread> ts;
    for (int i = 0; i < kThreads; ++i)
    {
        ts.emplace_back([&pool] {
            for (int j = 0; j < kIters; ++j)
            {
                auto c = pool.getConnection(2000);
                ASSERT_TRUE(c != nullptr);
                pool.returnConnection(c);
            }
        });
    }
    for (auto &t : ts)
        t.join();

    EXPECT_EQ(pool.availableCount(), 4u);
    pool.destroy();
}
