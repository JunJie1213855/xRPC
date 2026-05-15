// 测试 RpcHeader protobuf 协议头：序列化/反序列化、字段边界、损坏数据
#include <gtest/gtest.h>
#include <string>

#include "Xrpcheader.pb.h"

TEST(XrpcHeaderTest, RoundTripBasic)
{
    Xrpc::RpcHeader src;
    src.set_service_name("UserServiceRpc");
    src.set_method_name("Login");
    src.set_args_size(42);

    std::string bytes;
    ASSERT_TRUE(src.SerializeToString(&bytes));
    ASSERT_FALSE(bytes.empty());

    Xrpc::RpcHeader dst;
    ASSERT_TRUE(dst.ParseFromString(bytes));
    EXPECT_EQ(dst.service_name(), "UserServiceRpc");
    EXPECT_EQ(dst.method_name(), "Login");
    EXPECT_EQ(dst.args_size(), 42u);
}

TEST(XrpcHeaderTest, EmptyFieldsRoundTrip)
{
    Xrpc::RpcHeader src;  // 所有字段默认值
    std::string bytes;
    ASSERT_TRUE(src.SerializeToString(&bytes));

    Xrpc::RpcHeader dst;
    ASSERT_TRUE(dst.ParseFromString(bytes));
    EXPECT_EQ(dst.service_name(), "");
    EXPECT_EQ(dst.method_name(), "");
    EXPECT_EQ(dst.args_size(), 0u);
}

TEST(XrpcHeaderTest, LongNamesRoundTrip)
{
    Xrpc::RpcHeader src;
    src.set_service_name(std::string(1024, 's'));
    src.set_method_name(std::string(512, 'm'));
    src.set_args_size(0xFFFFFFFFu);  // 边界值

    std::string bytes;
    ASSERT_TRUE(src.SerializeToString(&bytes));

    Xrpc::RpcHeader dst;
    ASSERT_TRUE(dst.ParseFromString(bytes));
    EXPECT_EQ(dst.service_name().size(), 1024u);
    EXPECT_EQ(dst.method_name().size(), 512u);
    EXPECT_EQ(dst.args_size(), 0xFFFFFFFFu);
}

TEST(XrpcHeaderTest, ParseGarbageReturnsFalse)
{
    Xrpc::RpcHeader dst;
    // 一段不可能合法的 protobuf 二进制（错位的 tag/wire type）
    std::string garbage("\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF", 8);
    EXPECT_FALSE(dst.ParseFromString(garbage));
}

TEST(XrpcHeaderTest, ParseTruncatedReturnsFalse)
{
    Xrpc::RpcHeader src;
    src.set_service_name("X");
    src.set_method_name("Y");
    src.set_args_size(7);

    std::string bytes;
    ASSERT_TRUE(src.SerializeToString(&bytes));
    ASSERT_GT(bytes.size(), 2u);
    // 截断到只剩一半
    std::string truncated = bytes.substr(0, bytes.size() / 2);

    Xrpc::RpcHeader dst;
    EXPECT_FALSE(dst.ParseFromString(truncated));
}
