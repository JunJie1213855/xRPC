#include <gtest/gtest.h>
#include "Xrpcchannel.h"
#include "Xrpccontroller.h"

// ============ Constructor Tests ============

TEST(XrpcChannelTest, ConstructorWithFalse) {
    XrpcChannel channel(false);
}

TEST(XrpcChannelTest, ConstructorWithTrue) {
    XrpcChannel channel(true);
}

TEST(XrpcChannelTest, CopyConstructorDisabled) {
    // 拷贝构造函数应该被禁用
    XrpcChannel channel1(false);
    // 下面的代码在编译时应该失败（如果取消注释）
    // XrpcChannel channel2(channel1);
}

TEST(XrpcChannelTest, AssignmentOperatorDisabled) {
    // 赋值运算符应该被禁用
    XrpcChannel channel1(false);
    XrpcChannel channel2(false);
    // 下面的代码在编译时应该失败（如果取消注释）
    // channel1 = channel2;
}

// ============ XrpcController Integration Tests ============

TEST(XrpcChannelTest, ControllerInitialState) {
    XrpcController controller;
    EXPECT_FALSE(controller.Failed());
    EXPECT_TRUE(controller.ErrorText().empty());
}

TEST(XrpcChannelTest, ControllerSetFailed) {
    XrpcController controller;
    controller.SetFailed("test error");
    EXPECT_TRUE(controller.Failed());
    EXPECT_EQ(controller.ErrorText(), "test error");
}

TEST(XrpcChannelTest, ControllerResetAfterFailed) {
    XrpcController controller;
    controller.SetFailed("test error");
    controller.Reset();
    EXPECT_FALSE(controller.Failed());
    EXPECT_TRUE(controller.ErrorText().empty());
}

TEST(XrpcChannelTest, ControllerMultipleSetFailed) {
    XrpcController controller;
    controller.SetFailed("first error");
    controller.SetFailed("second error");
    EXPECT_TRUE(controller.Failed());
    EXPECT_EQ(controller.ErrorText(), "second error");
}

TEST(XrpcChannelTest, ControllerStartCancelNotImplemented) {
    XrpcController controller;
    // StartCancel 未实现，调用不应崩溃
    controller.StartCancel();
}

TEST(XrpcChannelTest, ControllerIsCanceledReturnsFalse) {
    XrpcController controller;
    EXPECT_FALSE(controller.IsCanceled());
}

TEST(XrpcChannelTest, ControllerNotifyOnCancelNotImplemented) {
    XrpcController controller;
    // NotifyOnCancel 未实现，调用不应崩溃
    controller.NotifyOnCancel(nullptr);
}