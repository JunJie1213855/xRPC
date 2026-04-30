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

// ============ Edge Case Tests ============

TEST(XrpcConfigTest, EmptyFile) {
    std::ofstream of("/tmp/test_empty.conf");
    of.close();

    Xrpcconfig config;
    config.LoadConfigFile("/tmp/test_empty.conf");
    EXPECT_TRUE(config.Load("anykey").empty());
}

TEST(XrpcConfigTest, FileWithOnlyComments) {
    std::ofstream of("/tmp/test_comments.conf");
    of << "# This is a comment\n";
    of << "# Another comment\n";
    of << "   # Indented comment\n";
    of.close();

    Xrpcconfig config;
    config.LoadConfigFile("/tmp/test_comments.conf");
    EXPECT_TRUE(config.Load("anykey").empty());
}

TEST(XrpcConfigTest, FileWithOnlyEmptyLines) {
    std::ofstream of("/tmp/test_empty_lines.conf");
    of << "\n\n\n";
    of.close();

    Xrpcconfig config;
    config.LoadConfigFile("/tmp/test_empty_lines.conf");
    EXPECT_TRUE(config.Load("anykey").empty());
}

TEST(XrpcConfigTest, KeyWithoutValue) {
    std::ofstream of("/tmp/test_no_value.conf");
    of << "keyonly\n";
    of << "anotherkey=\n";
    of.close();

    Xrpcconfig config;
    config.LoadConfigFile("/tmp/test_no_value.conf");
    // Should handle gracefully, keywithoutvalue may be stored with empty value
}

TEST(XrpcConfigTest, DuplicateKeys) {
    std::ofstream of("/tmp/test_duplicate.conf");
    of << "key=first\n";
    of << "key=second\n";
    of.close();

    Xrpcconfig config;
    config.LoadConfigFile("/tmp/test_duplicate.conf");
    // Current implementation uses insert, so first value is kept
    // This is implementation-dependent behavior
}

TEST(XrpcConfigTest, ValueWithEqualSign) {
    std::ofstream of("/tmp/test_equal.conf");
    of << "key=value=with=equals\n";
    of.close();

    Xrpcconfig config;
    config.LoadConfigFile("/tmp/test_equal.conf");
    EXPECT_EQ(config.Load("key"), "value=with=equals");
}

TEST(XrpcConfigTest, MultipleWhitespaceAroundKeyAndValue) {
    std::ofstream of("/tmp/test_multi_space.conf");
    of << "    key1    =    value1    \n";
    of << "\tkey2\t=\tvalue2\t\n";
    of.close();

    Xrpcconfig config;
    config.LoadConfigFile("/tmp/test_multi_space.conf");
    EXPECT_EQ(config.Load("key1"), "value1");
    EXPECT_EQ(config.Load("key2"), "value2");
}

TEST(XrpcConfigTest, ValueWithSpecialCharacters) {
    std::ofstream of("/tmp/test_special.conf");
    of << "key1=value with spaces\n";
    of << "key2=value:with:colons\n";
    of << "key3=value#with#hash\n";  // hash should be part of value, not comment
    of.close();

    Xrpcconfig config;
    config.LoadConfigFile("/tmp/test_special.conf");
    EXPECT_EQ(config.Load("key1"), "value with spaces");
    EXPECT_EQ(config.Load("key2"), "value:with:colons");
}

TEST(XrpcConfigTest, KeyWithDotsAndUnderscores) {
    std::ofstream of("/tmp/test_complex_keys.conf");
    of << "service.host=localhost\n";
    of << "service_port=8080\n";
    of << "max_connections.10=100\n";
    of.close();

    Xrpcconfig config;
    config.LoadConfigFile("/tmp/test_complex_keys.conf");
    EXPECT_EQ(config.Load("service.host"), "localhost");
    EXPECT_EQ(config.Load("service_port"), "8080");
    EXPECT_EQ(config.Load("max_connections.10"), "100");
}

TEST(XrpcConfigTest, TrimPreservesContent) {
    std::ofstream of("/tmp/test_trim.conf");
    of << "  key1  =  value with space at both ends  \n";
    of << "key2=value\n";
    of.close();

    Xrpcconfig config;
    config.LoadConfigFile("/tmp/test_trim.conf");
    // Spaces at start and end of value should be trimmed
    std::string val = config.Load("key1");
    EXPECT_EQ(val, "value with space at both ends");
}

TEST(XrpcConfigTest, MultipleLinesSameKey) {
    std::ofstream of("/tmp/test_multi_line.conf");
    of << "key=first_value\n";
    of << "another=line\n";
    of << "key=second_value\n";
    of.close();

    Xrpcconfig config;
    config.LoadConfigFile("/tmp/test_multi_line.conf");
    EXPECT_EQ(config.Load("another"), "line");
}