#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "zkconnection.h"

// 测试 ZkConnection 构造函数
TEST(ZkConnectionTest, Constructor) {
    ZkConnection conn;
    EXPECT_FALSE(conn.isConnected());
    EXPECT_TRUE(conn.getHandle() == nullptr);
}

// 测试 ZkConnection 析构（不崩溃）
TEST(ZkConnectionTest, Destructor) {
    {
        ZkConnection conn;
        // 析构时不应崩溃
    }
    // 已离开作用域，析构已完成
}

// 测试 isExpired 初始状态
TEST(ZkConnectionTest, IsExpiredInitiallyFalse) {
    ZkConnection conn;
    EXPECT_FALSE(conn.isExpired());
}

// 测试 close 在未连接时调用
TEST(ZkConnectionTest, CloseWithoutConnection) {
    ZkConnection conn;
    conn.close();
    EXPECT_FALSE(conn.isConnected());
}

// 测试 getData 在未连接时返回空字符串
TEST(ZkConnectionTest, GetDataWithoutConnection) {
    ZkConnection conn;
    std::string result = conn.getData("/test");
    EXPECT_EQ(result, "");
}

// 测试 createZnode 在未连接时返回 false
TEST(ZkConnectionTest, CreateZnodeWithoutConnection) {
    ZkConnection conn;
    bool result = conn.createZnode("/test", "data", 4);
    EXPECT_FALSE(result);
}