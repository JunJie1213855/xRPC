#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <string>

// 测试夹具基类
class ZkConnectionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// 测试夹具基类
class ZkClientPoolTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};