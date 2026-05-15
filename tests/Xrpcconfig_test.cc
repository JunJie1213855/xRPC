// Xrpcconfig 单元测试 — 解析、查找、边界
#include <cstdio>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <unistd.h>

#include "Xrpcconfig.h"

namespace
{
// 写一个临时配置文件并返回路径
std::string WriteTempConf(const std::string &content)
{
    char tmpl[] = "/tmp/xrpc_conf_XXXXXX";
    int fd = mkstemp(tmpl);
    EXPECT_GE(fd, 0);
    if (fd >= 0)
        close(fd);
    std::ofstream of(tmpl);
    of << content;
    of.close();
    return tmpl;
}
}  // namespace

TEST(XrpcConfigTest, LoadExistingKey)
{
    auto path = WriteTempConf("testkey=testvalue\n");
    Xrpcconfig config;
    config.LoadConfigFile(path.c_str());
    EXPECT_EQ(config.Load("testkey"), "testvalue");
}

TEST(XrpcConfigTest, LoadNonExistingKey)
{
    Xrpcconfig config;
    EXPECT_TRUE(config.Load("nonexistent_key_12345").empty());
}

TEST(XrpcConfigTest, TrimWhitespace)
{
    auto path = WriteTempConf(
        "key1 = value1\n"
        "key2=value2\n"
        "  key3  =  value3  \n");
    Xrpcconfig config;
    config.LoadConfigFile(path.c_str());
    EXPECT_EQ(config.Load("key1"), "value1");
    EXPECT_EQ(config.Load("key2"), "value2");
    EXPECT_EQ(config.Load("key3"), "value3");
}

TEST(XrpcConfigTest, SkipsCommentsAndBlankLines)
{
    auto path = WriteTempConf(
        "# this is a comment\n"
        "\n"
        "rpcserverip=127.0.0.1\n"
        "   \n"
        "# rpcserverport=should_be_ignored\n"
        "rpcserverport=8000\n");
    Xrpcconfig config;
    config.LoadConfigFile(path.c_str());
    EXPECT_EQ(config.Load("rpcserverip"), "127.0.0.1");
    EXPECT_EQ(config.Load("rpcserverport"), "8000");
    // 注释中的键不应被解析为合法配置
    EXPECT_TRUE(config.Load("# rpcserverport").empty());
}

TEST(XrpcConfigTest, MissingFileDoesNotCrash)
{
    Xrpcconfig config;
    // 不存在的路径：当前实现仅记录日志，查询应得空
    config.LoadConfigFile("/tmp/__definitely_not_present_xrpc__.conf");
    EXPECT_TRUE(config.Load("anykey").empty());
}

TEST(XrpcConfigTest, MalformedLinesDoNotBreakLoader)
{
    auto path = WriteTempConf(
        "this_line_has_no_equal_sign\n"
        "validkey=validvalue\n");
    Xrpcconfig config;
    config.LoadConfigFile(path.c_str());
    EXPECT_EQ(config.Load("validkey"), "validvalue");
}
