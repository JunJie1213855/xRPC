#include "Xrpcapplication.h"
#include "zkclientpool.h"
#include <cstdlib>
#include <unistd.h>
#include "XrpcLogger.h"

/**
 * @brief
 */
Xrpcconfig XrpcApplication::m_config; // 全局配置对象
// std::mutex XrpcApplication::m_mutex;  // 用于线程安全的互斥锁
// XrpcApplication *XrpcApplication::m_application = nullptr; // 单例对象指针，初始为空

std::shared_ptr<XrpcApplication> XrpcApplication::m_application = nullptr; // 单例对象指针，初始为空

// 初始化函数，用于解析命令行参数并加载配置文件
void XrpcApplication::Init(int argc, char **argv)
{
    if (argc < 2)
    { // 如果命令行参数少于2个，说明没有指定配置文件
        // std::cout << "格式: command -i <配置文件路径>" << std::endl;
        LOG(INFO) << "格式: command -i <配置文件路径>";
        exit(EXIT_FAILURE); // 退出程序
    }

    int o;
    std::string config_file;
    // 使用getopt解析命令行参数，-i表示指定配置文件
    while (-1 != (o = getopt(argc, argv, "i:")))
    {
        switch (o)
        {
        case 'i':                 // 如果参数是-i，后面的值就是配置文件的路径
            config_file = optarg; // 将配置文件路径保存到config_file
            break;
        case '?': // 如果出现未知参数（不是-i），提示正确格式并退出
            // std::cout << "格式: command -i <配置文件路径>" << std::endl;
            LOG(INFO) << "格式: command -i <配置文件路径>";
            exit(EXIT_FAILURE);
            break;
        case ':': // 如果-i后面没有跟参数，提示正确格式并退出
            // std::cout << "格式: command -i <配置文件路径>" << std::endl;
            LOG(INFO) << "格式: command -i <配置文件路径>";
            exit(EXIT_FAILURE);
            break;
        default:
            break;
        }
    }

    // 加载配置文件
    m_config.LoadConfigFile(config_file.c_str());

    // 启动 ZooKeeper 连接池
    ZkClientPool::getInstance().start(4);
}

// 获取单例对象的引用，保证全局只有一个实例
XrpcApplication &XrpcApplication::GetInstance()
{
    static std::once_flag flag;
    std::call_once(flag, [&]()
                   { m_application = std::shared_ptr<XrpcApplication>(new XrpcApplication); });
    return *m_application; // 返回单例对象的引用
}

// 获取全局配置对象的引用
Xrpcconfig &XrpcApplication::GetConfig()
{
    return m_config;
}

// 析构函数，销毁连接池
XrpcApplication::~XrpcApplication()
{
    ZkClientPool::getInstance().destroy();
}