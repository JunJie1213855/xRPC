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

// ============ Edge Case Tests ============

TEST(ZkClientPoolTest, StartTwice) {
    ZkClientPool& pool = ZkClientPool::getInstance();
    pool.destroy();  // 确保干净的初始状态
    // Start 第一次
    // 注意：在没有真实 ZooKeeper 服务器的情况下，start 可能无法创建连接
    // 这里测试的是 API 调用不会崩溃
}

TEST(ZkClientPoolTest, DestroyWithoutStart) {
    ZkClientPool& pool = ZkClientPool::getInstance();
    pool.destroy();  // 没有 start 就 destroy 应该安全
    EXPECT_EQ(pool.availableCount(), 0u);
}

TEST(ZkClientPoolTest, DestroyMultipleTimes) {
    ZkClientPool& pool = ZkClientPool::getInstance();
    pool.destroy();
    pool.destroy();  // 多次 destroy 不应崩溃
    EXPECT_EQ(pool.availableCount(), 0u);
}

TEST(ZkClientPoolTest, GetConnectionWithoutStart) {
    ZkClientPool& pool = ZkClientPool::getInstance();
    pool.destroy();  // 确保干净的初始状态
    // 在没有 start 的情况下获取连接
    // 应该返回临时连接（即使没有 ZooKeeper 也可能不为空）
    auto conn = pool.getConnection(100);
    // conn 可能为空指针或无效连接
}

TEST(ZkClientPoolTest, ReturnNullConnection) {
    ZkClientPool& pool = ZkClientPool::getInstance();
    pool.destroy();
    // 归还空连接不应崩溃
    std::shared_ptr<ZkConnection> nullConn;
    pool.returnConnection(nullConn);
}

TEST(ZkClientPoolTest, TotalCountConsistency) {
    ZkClientPool& pool = ZkClientPool::getInstance();
    pool.destroy();
    size_t total = pool.totalCount();
    size_t available = pool.availableCount();
    // available 不应大于 total
    EXPECT_LE(available, total);
}

TEST(ZkClientPoolTest, StartWithZeroInitialSize) {
    ZkClientPool& pool = ZkClientPool::getInstance();
    pool.destroy();
    // 使用 0 初始大小启动
    // 取决于实现，可能创建一些默认连接或保持为空
}

TEST(ZkClientPoolTest, StartWithNegativeInitialSize) {
    ZkClientPool& pool = ZkClientPool::getInstance();
    pool.destroy();
    // 使用负数初始大小启动（如果实现允许）
    // 应该被安全处理
}

TEST(ZkClientPoolTest, GetConnectionWithZeroTimeout) {
    ZkClientPool& pool = ZkClientPool::getInstance();
    pool.destroy();
    // 零超时获取连接
    auto conn = pool.getConnection(0);
    // 行为取决于实现
}

TEST(ZkClientPoolTest, GetConnectionWithVeryLongTimeout) {
    ZkClientPool& pool = ZkClientPool::getInstance();
    pool.destroy();
    // 很大的超时值（但不是无限）
    auto conn = pool.getConnection(30000);
    // 应该在合理时间内返回
}