#include "rpc_channel.h"

#include "common.h"
#include "rpc_header.pb.h"

#include <arpa/inet.h>
#include <google/protobuf/descriptor.h>
#include <string>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

MyRpcChannel::MyRpcChannel(std::string ip, std::uint16_t port)
    : ip_(ip), port_(port)
{
}

void MyRpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                              google::protobuf::RpcController* controller,
                              const google::protobuf::Message* request,
                              google::protobuf::Message* response,
                              google::protobuf::Closure* done)
{
    const std::string service_name = method->service()->full_name();
    const std::string method_name = method->name();

    std::string args;
    if (!request->SerializeToString(&args)){
        if (controller){
            controller->SetFailed("request SerializeToString failed");
        }
        return;
    }

    rpc::RpcHeader header;
    header.set_service_name(service_name);
    header.set_method_name(method_name);
    header.set_args_size(static_cast<uint32_t>(args.size()));

    std::string header_str;
    if (!header.SerializeToString(&header_str)){
        if (controller){
            controller->SetFailed("header SerializedToString failed");
        }
        return;
    }

    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd <= 0) {
        if (controller){
            controller->SetFailed("create socket failed");
        }
        return;
    }

    //printf("original: 0x%04x (%u)\n", port_, port_);
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port_);
    //printf("after htons: 0x%04x (%u)\n", addr.sin_port, addr.sin_port);

    if (::inet_pton(AF_INET, ip_.c_str(), &addr.sin_addr) <= 0){
        if (controller){
            controller->SetFailed("invalid server ip");
        }
        ::close(fd);
        return;
    }

    /*std::cout << "!!!!\n";
    std::cout << ntohs(addr.sin_port) << std::endl;
    char ip_str[INET_ADDRSTRLEN];
    const char* result = inet_ntop(AF_INET, &addr.sin_addr, ip_str, sizeof(ip_str));
    std::cout << ip_str << std::endl;
    std::cout << "!!!!!\n";*/

    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0){
        perror("");
        if (controller){
            controller->SetFailed("connect failed");
        }
        ::close(fd);
        return;
    }

    uint32_t header_size = static_cast<uint32_t>(header_str.size());
    uint32_t net_header_size = ::htonl(header_size);

    if (!WriteN(fd, &net_header_size, sizeof(net_header_size)) || 
        !WriteN(fd, header_str.data(), header_str.size()) || 
        !WriteN(fd, args.data(), args.size()))
    {
        if (controller){
            controller->SetFailed("send rpc request failed");
        }
        ::close(fd);
        return;
    }

   uint32_t net_response_header_size = 0;
   if (!ReadN(fd, &net_response_header_size, sizeof(net_response_header_size)))
   {
    if (controller){
        controller->SetFailed("read response_header_size failed");
    }
    ::close(fd);
    return;
   } 

   uint32_t response_header_size = ::ntohl(net_response_header_size);

   std::string response_header_str(response_header_size, '\0');

   if (!ReadN(fd, response_header_str.data(), response_header_size))
   {
    if (controller){
        controller->SetFailed("read response header failed");
    }
    ::close(fd);
    return;
   }

   rpc::ResponseRpcHeader response_header;

   if (!response_header.ParseFromString(response_header_str)){
    if (controller){
        controller->SetFailed("parse response header failed");
    }
    ::close(fd);
    return;
   }

   if (response_header.error_code() != 0)
   {
    if (controller)
    {
        controller->SetFailed(response_header.error_text());
    }
    ::close(fd);
    return;
   }

   uint32_t response_size = response_header.response_size();

   std::string response_str(response_size, '\0');

   if (!ReadN(fd, response_str.data(), response_size)){
    if (controller){
        controller->SetFailed("read response failed");
    }
    ::close(fd);
    return;
   }

    if (!response->ParseFromString(response_str)){
        if (controller){
            controller->SetFailed("parse response body failed");
        }
        ::close(fd);
        return;
    }

    if(done){
        done->Run();
    }

    ::close(fd);
}
