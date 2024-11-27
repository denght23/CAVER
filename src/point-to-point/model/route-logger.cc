// RouteLogger.cc
#include "route-logger.h"
#include <iostream>
#include <sys/stat.h>  // 用于创建目录
#include <cstring>     // 用于处理字符串

// 初始化静态成员变量
std::string RouteLogger::logFolder = "./logs";

void RouteLogger::SetLogFolder(const std::string& folderPath) {
    logFolder = folderPath;

    // 创建文件夹（如果不存在）
    //CreateFolder(logFolder);
}

void RouteLogger::RecordRoute(int nodeId, int flowId, uint32_t outDev) {
    // 创建文件名，基于 nodeId 命名
    std::string logFileName = logFolder + "/node_" + std::to_string(nodeId) + "_log.txt";
    std::ofstream logFile(logFileName, std::ios::app);

    if (!logFile.is_open()) {
        std::cerr << "Failed to open log file: " << logFileName << std::endl;
        return;
    }

    // 写入日志信息
    logFile << "Node ID: " << nodeId 
            << ", Flow ID: " << flowId 
            << ", OutDev: " << outDev 
            << std::endl;

    logFile.close();
}
