#include <gtest/gtest.h>
#include "Xrpcapplication.h"

TEST(XrpcApplicationTest, GetInstance) {
    XrpcApplication& app1 = XrpcApplication::GetInstance();
    XrpcApplication& app2 = XrpcApplication::GetInstance();
    EXPECT_EQ(&app1, &app2);
}