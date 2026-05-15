# 测试目录 CMakeLists.txt
enable_testing()

# 源文件
set(TEST_SOURCES
    test_fixture.h
    zkconnection_test.cc
    zkclientpool_test.cc
    Xrpcconfig_test.cc
    Xrpccontroller_test.cc
    Xrpcchannel_test.cc
    Xrpcapplication_test.cc
)

# 创建测试可执行文件
add_executable(zk_test ${TEST_SOURCES})

# 使用 /usr/local 下的 GTest（系统 gtest 已被删除）
target_link_libraries(zk_test
    /usr/local/lib/libgtest.a
    /usr/local/lib/libgtest_main.a
    xrpc_core
    pthread
    zookeeper_mt
    glog
)

# 设置包含目录
target_include_directories(zk_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src/include
)

# 注册测试
add_test(NAME zk_test COMMAND zk_test)