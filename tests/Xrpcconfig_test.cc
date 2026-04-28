#include <gtest/gtest.h>
#include "Xrpcconfig.h"
#include <fstream>

TEST(XrpcConfigTest, LoadExistingKey) {
    // 创建临时配置文件
    std::ofstream of("/tmp/test.conf");
    of << "testkey=testvalue\n";
    of.close();

    Xrpcconfig config;
    config.LoadConfigFile("/tmp/test.conf");
    std::string result = config.Load("testkey");
    EXPECT_EQ(result, "testvalue");
}

TEST(XrpcConfigTest, LoadNonExistingKey) {
    Xrpcconfig config;
    std::string result = config.Load("nonexistent_key_12345");
    EXPECT_TRUE(result.empty());
}

TEST(XrpcConfigTest, TrimWhitespace) {
    std::ofstream of("/tmp/test.conf");
    of << "key1 = value1\n";
    of << "key2=value2\n";
    of << "  key3  =  value3  \n";
    of.close();

    Xrpcconfig config;
    config.LoadConfigFile("/tmp/test.conf");
    EXPECT_EQ(config.Load("key1"), "value1");
    EXPECT_EQ(config.Load("key2"), "value2");
    EXPECT_EQ(config.Load("key3"), "value3");
}