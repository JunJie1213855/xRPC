#ifndef _Xrpcapplication_H
#define _Xrpcapplication_H
#include "Xrpcconfig.h"
#include "Xrpcchannel.h"
#include "Xrpccontroller.h"
#include <mutex>

/**
 * @brief Krpc基础类，负责框架的一些初始化操作，初始化全局config类，保持云config类
 */
class XrpcApplication : public std::enable_shared_from_this<XrpcApplication>
{
public:
    static void Init(int argc, char **argv);
    static XrpcApplication &GetInstance();
    static Xrpcconfig &GetConfig();
    ~XrpcApplication() {}

protected:
    XrpcApplication() {}
    XrpcApplication(const XrpcApplication &) = delete;
    XrpcApplication(XrpcApplication &&) = delete;

private:
    static Xrpcconfig m_config;
    static std::shared_ptr<XrpcApplication> m_application;
};
#endif
