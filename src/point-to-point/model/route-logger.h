// RouteLogger.h
#ifndef ROUTE_LOGGER_H
#define ROUTE_LOGGER_H

#include <string>
#include <fstream>

class RouteLogger {
public:
    // 设置日志文件夹
    static void SetLogFolder(const std::string& folderPath);

    // 记录路由信息
    static void RecordRoute(int nodeId, int flowId, uint32_t outDev);

private:
    // 创建文件夹（如果不存在）
    // 日志文件夹路径
    static std::string logFolder;
};

#endif // ROUTE_LOGGER_H
