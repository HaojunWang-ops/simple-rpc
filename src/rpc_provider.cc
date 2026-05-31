#include "rpc_provider.h"

#include "common.h"
#include "rpc_header.pb.h"

#include <arpa/inet.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <iostream>
#include <memory>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

static bool SendRpcResponse(int connfd, int error_code, const std::string &error_text, const google::protobuf::Message *response)
{
    std::string response_body;

    if (error_code == 0 && response != nullptr)
    {
        if (!response->SerializeToString(&response_body))
        {
            return false;
        }
    }

    rpc::ResponseRpcHeader response_header;
    response_header.set_error_code(error_code);
    response_header.set_error_text(error_text);
    response_header.set_response_size(static_cast<uint32_t>(response_body.size()));

    std::string response_header_str;

    if (!response_header.SerializeToString(&response_header_str))
    {
        return false;
    }

    uint32_t response_header_size = static_cast<uint32_t>(response_header_str.size());
    uint32_t net_response_header_size = ::htonl(response_header_size);

    if (!WriteN(connfd, &net_response_header_size, sizeof(net_response_header_size)) || !WriteN(connfd, response_header_str.data(), response_header_size) || !WriteN(connfd, response_body.data(), response_body.size()))
    {
        return false;
    }

    return true;
}

static void SendRpcError(int connfd, int error_code, const std::string &error_text)
{
    SendRpcResponse(connfd, error_code, error_text, nullptr);
}

class SendResponseClosure : public google::protobuf::Closure
{
public:
    SendResponseClosure(int connfd, SimpleRpcController *controller, google::protobuf::Message *request, google::protobuf::Message *response)
        : connfd_(connfd),
          controller_(controller),
          request_(request),
          response_(response)
    {
    }

    // 回调处理，负责发送response
    void Run() override
    {
        //业务方法执行了，业务失败
        if (controller_ && controller_->Failed())
        {
            SendRpcResponse(connfd_, 2001, controller_->ErrorText(), nullptr);
        }
        //业务成功执行，返回reponse
        else
        {
            SendRpcResponse(connfd_, 0, "", response_);
        }

        // request / response / controller / closure 都由 SendResponseClosure::Run() 释放
        delete controller_;
        delete request_;
        delete response_;
        delete this;
    }

private:
    int connfd_;
    SimpleRpcController *controller_;
    google::protobuf::Message *request_;
    google::protobuf::Message *response_;
};

// 将service注册
void RpcProvider::NotifyService(google::protobuf::Service *service)
{
    const google::protobuf::ServiceDescriptor *service_desc = service->GetDescriptor();

    struct ServiceInfo service_info;
    service_info.service = service;

    for (int i = 0; i < service_desc->method_count(); i++)
    {
        const google::protobuf::MethodDescriptor *method = service_desc->method(i);
        service_info.methods.emplace(method->name(), method);
    }

    std::string service_name = service_desc->full_name();    
    
    /*
    for (const auto& pair : service_info.methods)
    {
        std::cout << pair.first << " ";
    }
    std::cout << "\n";
    */
    
    services_.emplace(service_name, std::move(service_info)); // move is useful?

    std::cout << "register service: " << service_name << std::endl;
}

// 设置监听listenfd + 循环监听
void RpcProvider::Run(const std::string &ip, uint16_t port)
{
    int listenfd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0)
    {
        std::cerr << "create listen socket failed\n";
        return;
    }

    int opt = 1;
    ::setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);

    if (::inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0)
    {
        std::cerr << "invalid bind ip\n";
        ::close(listenfd);
        return;
    }

    if (::bind(listenfd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0)
    {
        std::cerr << "bind failed\n";
        ::close(listenfd);
        return;
    }

    if (::listen(listenfd, 128) < 0)
    {
        std::cerr << "listen failed\n";
        ::close(listenfd);
        return;
    }

    std::cout << "RpcProvider listen on " << ip << ":" << "port" << std::endl;

    // 循环监听
    while (true)
    {
        struct sockaddr_in peer{};
        socklen_t len = sizeof(peer);

        int connfd = ::accept(listenfd, reinterpret_cast<sockaddr *>(&peer), &len);
        if (connfd < 0)
        {
            continue;
        }

        HandleClient(connfd);

        ::close(connfd);
    }
}

// 处理收到的request
void RpcProvider::HandleClient(int connfd)
{
    uint32_t net_header_size = 0;
    if (!ReadN(connfd, &net_header_size, sizeof(net_header_size)))
    {
        std::cerr << "ReadN header_size failed\n";
        return;
    }

    uint32_t header_size = ::ntohl(net_header_size);
    // std::cout << header_size;

    std::string header_str(header_size, '\0');
    if (!ReadN(connfd, header_str.data(), header_size))
    {
        std::cerr << "ReadN header failed\n";
        return;
    }

    // std::cout << header_str << "\n";

    rpc::RpcHeader header;
    if (!header.ParseFromString(header_str))
    {
        std::cerr << "parse rpc header failed\n";
        return;
    }

    const std::string service_name = header.service_name();
    const std::string method_name = header.method_name();
    uint32_t args_size = header.args_size();
    std::cout << "rpc_provider: " << service_name << " " << method_name << "\n";
    std::string args(args_size, '\0');
    if (!ReadN(connfd, args.data(), args.size()))
    {
        std::cerr << "read args failed\n";
        return;
    }

    auto service_it = services_.find(service_name);
    if (service_it == services_.end())
    {
        std::string error = "service not found" + service_name + '\n';
        std::cerr << error;

        SendRpcError(connfd, 1001, error);
        return;
    }

    auto method_it = service_it->second.methods.find(method_name);
    if (method_it == service_it->second.methods.end())
    {
        std::string error = "method not found" + method_name + "\n";
        std::cerr << error;

        SendRpcError(connfd, 1002, error);
        return;
    }

    google::protobuf::Service *service = service_it->second.service;
    const google::protobuf::MethodDescriptor *method_desc = method_it->second;

    google::protobuf::Message *request = service->GetRequestPrototype(method_desc).New();
    google::protobuf::Message *response = service->GetResponsePrototype(method_desc).New();

    if (!request->ParseFromString(args))
    {
        std::string error = "parse request failed\n";
        std::cerr << error;

        SendRpcError(connfd, 1003, error);

        delete request;
        delete response;
        return;
    }

    SimpleRpcController* controller = new SimpleRpcController;
    SendResponseClosure *done = new SendResponseClosure(connfd, controller, request, response);
    service->CallMethod(method_desc, controller, request, response, done); // 真正调用Login方法
}
