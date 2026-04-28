#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "zkclientpool.h"

// 测试单例获取
TEST(ZkClientPoolTest, GetInstance) {
    ZkClientPool& pool1 = ZkClientPool::getInstance();
    ZkClientPool& pool2 = ZkClientPool::getInstance();
    EXPECT_EQ(&pool1, &pool2);
}

// 测试初始状态
TEST(ZkClientPoolTest, InitialState) {
    ZkClientPool& pool = ZkClientPool::getInstance();
    EXPECT_EQ(pool.availableCount(), 0u);
    EXPECT_EQ(pool.totalCount(), 0u);
}

// 测试 destroy 后连接池为空
TEST(ZkClientPoolTest, DestroyClearsPool) {
    ZkClientPool& pool = ZkClientPool::getInstance();
    // 在没有 ZooKeeper 服务器的情况下调用 destroy
    // 不应崩溃
    pool.destroy();
    EXPECT_EQ(pool.availableCount(), 0u);
    EXPECT_EQ(pool.totalCount(), 0u);
}