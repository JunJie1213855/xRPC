#include <gtest/gtest.h>
#include "Xrpcchannel.h"
#include "Xrpccontroller.h"

TEST(XrpcChannelTest, ConstructorWithFalse) {
    XrpcChannel channel(false);
}

TEST(XrpcChannelTest, ConstructorWithTrue) {
    XrpcChannel channel(true);
}