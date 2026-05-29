#pragma once

#include <google/protobuf/service.h>
#include <string>
#include <unordered_map>

class RpcProvider {
public:
    void NotifyService(google::protobuf::Service* service);
    void Run(const std::string& ip, uint16_t port);
private:
    struct ServiceInfo {
        google::protobuf::Service* service = nullptr;

        std::unordered_map<std::string, const google::protobuf::MethodDescriptor*> methods;
    };

    void HandleClient(int connfd);

private:
    std::unordered_map<std::string, ServiceInfo> services_;
};
