#include <gtest/gtest.h>
#include "Xrpccontroller.h"

TEST(XrpcControllerTest, FailedInitiallyFalse) {
    XrpcController controller;
    EXPECT_FALSE(controller.Failed());
}

TEST(XrpcControllerTest, FailedAfterSetFailed) {
    XrpcController controller;
    controller.SetFailed("test error");
    EXPECT_TRUE(controller.Failed());
}

TEST(XrpcControllerTest, ErrorTextInitiallyEmpty) {
    XrpcController controller;
    EXPECT_TRUE(controller.ErrorText().empty());
}

TEST(XrpcControllerTest, ErrorTextAfterSetFailed) {
    XrpcController controller;
    controller.SetFailed("test error message");
    EXPECT_EQ(controller.ErrorText(), "test error message");
}

TEST(XrpcControllerTest, SetFailedTwice) {
    XrpcController controller;
    controller.SetFailed("first error");
    controller.SetFailed("second error");
    EXPECT_TRUE(controller.Failed());
    // 实际实现每次 SetFailed 都覆盖，而不是保留第一次的
    EXPECT_EQ(controller.ErrorText(), "second error");
}

TEST(XrpcControllerTest, Reset) {
    XrpcController controller;
    controller.SetFailed("error");
    EXPECT_TRUE(controller.Failed());
    controller.Reset();
    EXPECT_FALSE(controller.Failed());
    EXPECT_TRUE(controller.ErrorText().empty());
}