// 测试线协议帧的字节布局：[4B total_len][4B header_len][header][args]
// 这些测试不依赖任何网络/zk，只用 htonl/ntohl 校验帧格式与 README 文档一致。
#include <arpa/inet.h>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "Xrpcheader.pb.h"

namespace
{
// 按生产代码（Xrpcchannel.cc CallImpl）相同的方式构造请求帧
std::string BuildRequestFrame(const std::string &service_name,
                              const std::string &method_name,
                              const std::string &args_bytes)
{
    Xrpc::RpcHeader hdr;
    hdr.set_service_name(service_name);
    hdr.set_method_name(method_name);
    hdr.set_args_size(static_cast<uint32_t>(args_bytes.size()));

    std::string header_bytes;
    hdr.SerializeToString(&header_bytes);

    uint32_t header_size = static_cast<uint32_t>(header_bytes.size());
    uint32_t total_len = 4u + header_size + static_cast<uint32_t>(args_bytes.size());
    uint32_t net_total_len = htonl(total_len);
    uint32_t net_header_len = htonl(header_size);

    std::string frame;
    frame.reserve(4 + total_len);
    frame.append(reinterpret_cast<const char *>(&net_total_len), 4);
    frame.append(reinterpret_cast<const char *>(&net_header_len), 4);
    frame.append(header_bytes);
    frame.append(args_bytes);
    return frame;
}
}  // namespace

TEST(XrpcFrameTest, RequestFrameLayout)
{
    std::string args = "ARGSPAYLOAD";
    std::string frame = BuildRequestFrame("UserServiceRpc", "Login", args);

    // 解析 total_len
    ASSERT_GE(frame.size(), 8u);
    uint32_t total_len_net = 0;
    std::memcpy(&total_len_net, frame.data(), 4);
    uint32_t total_len = ntohl(total_len_net);
    EXPECT_EQ(frame.size(), 4u + total_len);

    // 解析 header_len
    uint32_t header_len_net = 0;
    std::memcpy(&header_len_net, frame.data() + 4, 4);
    uint32_t header_len = ntohl(header_len_net);

    // 边界检查
    EXPECT_LE(4u + header_len, total_len);

    // 解析 header
    Xrpc::RpcHeader hdr;
    ASSERT_TRUE(hdr.ParseFromArray(frame.data() + 8, static_cast<int>(header_len)));
    EXPECT_EQ(hdr.service_name(), "UserServiceRpc");
    EXPECT_EQ(hdr.method_name(), "Login");
    EXPECT_EQ(hdr.args_size(), args.size());

    // 解析 args（紧跟 header 之后）
    size_t args_offset = 8u + header_len;
    size_t args_size = total_len - 4u - header_len;
    ASSERT_EQ(args_offset + args_size, frame.size());
    std::string decoded_args(frame.data() + args_offset, args_size);
    EXPECT_EQ(decoded_args, args);
}

TEST(XrpcFrameTest, RequestFrameByteOrderIsNetwork)
{
    // 用一个 total_len 不止低 8 位的值校验大端编码
    std::string args(300, 'a');
    std::string frame = BuildRequestFrame("S", "M", args);

    // 高字节应当出现在低地址
    uint8_t b0 = static_cast<uint8_t>(frame[0]);
    uint8_t b3 = static_cast<uint8_t>(frame[3]);
    // total_len 一定 > 256，所以 b3（低字节）应当非零
    EXPECT_NE(b3, 0u);
    // 小端 host 下 b0 应当为 0；走 htonl 转换后大端 b0 也应当为 0（因为值 < 16M）
    EXPECT_EQ(b0, 0u);
}

TEST(XrpcFrameTest, EmptyArgsFrameIsWellFormed)
{
    std::string frame = BuildRequestFrame("S", "M", "");
    ASSERT_GE(frame.size(), 8u);

    uint32_t total_len_net = 0;
    std::memcpy(&total_len_net, frame.data(), 4);
    uint32_t total_len = ntohl(total_len_net);

    uint32_t header_len_net = 0;
    std::memcpy(&header_len_net, frame.data() + 4, 4);
    uint32_t header_len = ntohl(header_len_net);

    // total_len == 4 + header_len （args 为空）
    EXPECT_EQ(total_len, 4u + header_len);
}

TEST(XrpcFrameTest, ResponseFrameLayout)
{
    // 响应帧: [4B resp_len][resp_bytes]
    std::string payload = "RESPONSE_PAYLOAD_BYTES";
    uint32_t len = static_cast<uint32_t>(payload.size());
    uint32_t net_len = htonl(len);

    std::string frame;
    frame.append(reinterpret_cast<const char *>(&net_len), 4);
    frame.append(payload);

    ASSERT_EQ(frame.size(), 4u + payload.size());

    uint32_t decoded_len_net = 0;
    std::memcpy(&decoded_len_net, frame.data(), 4);
    EXPECT_EQ(ntohl(decoded_len_net), payload.size());

    std::string decoded(frame.data() + 4, payload.size());
    EXPECT_EQ(decoded, payload);
}
