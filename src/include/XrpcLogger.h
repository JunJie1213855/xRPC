#ifndef _XrpcLogger_H__
#define _XrpcLogger_H__

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <sstream>
#include <cstdlib>

// RAII wrapper：初始化/销毁 spdlog 全局 logger（兼容原 glog 接口）
class XrpcLogger
{
public:
  explicit XrpcLogger(const char* /*argv0*/ = "xrpc")
  {
    auto sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>("xrpc", sink);
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::trace);
    // %^ ... %$ 之间的内容由 spdlog 渲染为对应等级的颜色
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%-8l%$] %v");
  }
  ~XrpcLogger()
  {
    spdlog::shutdown();
  }
  XrpcLogger(const XrpcLogger&) = delete;
  XrpcLogger& operator=(const XrpcLogger&) = delete;
};

// 流式日志消息辅助类：析构时将累积的内容交给 spdlog 输出
// 生命周期绑定到单条语句，线程安全由 spdlog 的 sink 保证
class XrpcLogStream
{
public:
  explicit XrpcLogStream(spdlog::level::level_enum lvl) : lvl_(lvl) {}

  ~XrpcLogStream()
  {
    spdlog::log(lvl_, "{}", oss_.str());
    if (lvl_ == spdlog::level::critical)
    {
      spdlog::shutdown();
      std::abort();
    }
  }

  template <typename T>
  XrpcLogStream& operator<<(const T& v)
  {
    oss_ << v;
    return *this;
  }

private:
  spdlog::level::level_enum lvl_;
  std::ostringstream oss_;
};

// glog 等级 → spdlog 等级映射
#define XRPC_SPDLVL_INFO    spdlog::level::info
#define XRPC_SPDLVL_WARNING spdlog::level::warn
#define XRPC_SPDLVL_ERROR   spdlog::level::err
#define XRPC_SPDLVL_FATAL   spdlog::level::critical

// 保持与 glog 完全相同的调用语法：LOG(INFO) << "msg " << var;
#define LOG(severity) XrpcLogStream(XRPC_SPDLVL_##severity)

#endif
