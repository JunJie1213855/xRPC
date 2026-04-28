# 测试目录 CMakeLists.txt
enable_testing()

# 源文件
set(TEST_SOURCES
    test_fixture.h
    zkconnection_test.cc
    zkclientpool_test.cc
)

# 创建测试可执行文件
add_executable(zk_test ${TEST_SOURCES})

# 使用系统自带的 GTest
target_link_libraries(zk_test
    /usr/lib/x86_64-linux-gnu/libgtest.a
    /usr/lib/x86_64-linux-gnu/libgtest_main.a
    krpc_core
    pthread
    zookeeper_mt
    glog
)

# 设置包含目录
target_include_directories(zk_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src/include
    /usr/include
)

# 注册测试
add_test(NAME zk_test COMMAND zk_test)