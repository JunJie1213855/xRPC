# 测试目录 CMakeLists.txt
enable_testing()

# 找 GTest：优先使用 find_package（CI 与本机自动适配 /usr 或 /usr/local），
# 找不到时再回退到固定路径
find_package(GTest QUIET)

if(GTest_FOUND)
    set(XRPC_GTEST_TARGETS GTest::gtest GTest::gtest_main)
elseif(EXISTS /usr/local/lib/libgtest.a)
    set(XRPC_GTEST_TARGETS
        /usr/local/lib/libgtest.a
        /usr/local/lib/libgtest_main.a)
elseif(EXISTS /usr/lib/x86_64-linux-gnu/libgtest.a)
    set(XRPC_GTEST_TARGETS
        /usr/lib/x86_64-linux-gnu/libgtest.a
        /usr/lib/x86_64-linux-gnu/libgtest_main.a)
else()
    message(FATAL_ERROR "GoogleTest not found")
endif()

# ---------- 单元测试可执行文件（无需 zk 也能跑） ----------
set(UNIT_SOURCES
    test_fixture.h
    Xrpcheader_test.cc
    Xrpcframe_test.cc
    Xrpcconfig_test.cc
    Xrpccontroller_test.cc
    Xrpcchannel_test.cc
    Xrpcapplication_test.cc
    zkconnection_test.cc
    zkclientpool_test.cc
)
add_executable(xrpc_unit_test ${UNIT_SOURCES})
target_link_libraries(xrpc_unit_test
    ${XRPC_GTEST_TARGETS}
    xrpc_core
    pthread
    zookeeper_mt
    spdlog::spdlog
    PkgConfig::PROTOBUF
)
target_include_directories(xrpc_unit_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src/include
    ${CMAKE_SOURCE_DIR}/src   # 含 Xrpcheader.pb.h
)

add_test(NAME xrpc_unit_test COMMAND xrpc_unit_test)
set_tests_properties(xrpc_unit_test PROPERTIES LABELS "unit")

# ---------- 集成 + E2E 测试（需要 zk 服务可达；不可达时单个用例 GTEST_SKIP） ----------
set(INTEGRATION_SOURCES
    test_fixture.h
    zkconnection_live_test.cc
    zkclientpool_live_test.cc
    Xrpcrpc_e2e_test.cc
    ${CMAKE_SOURCE_DIR}/example/user.pb.cc
)
add_executable(xrpc_integration_test ${INTEGRATION_SOURCES})
target_link_libraries(xrpc_integration_test
    ${XRPC_GTEST_TARGETS}
    xrpc_core
    pthread
    zookeeper_mt
    spdlog::spdlog
    PkgConfig::PROTOBUF
)
target_include_directories(xrpc_integration_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)

add_test(NAME xrpc_integration_test COMMAND xrpc_integration_test)
set_tests_properties(xrpc_integration_test PROPERTIES LABELS "integration")
