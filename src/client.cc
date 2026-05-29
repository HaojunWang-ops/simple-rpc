#include "common.h"
#include "rpc_channel.h"
#include "user.pb.h"

#include <google/protobuf/stubs/common.h>
#include <iostream>

int main()
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    MyRpcChannel channel("127.0.0.1", 8000);

    demo::UserService_Stub stub(&channel);

    demo::LoginRequest request;
    request.set_name("haojun");
    request.set_password("123456");

    demo::LoginResponse response;

    SimpleRpcController controller;

    stub.Login(&controller, &request, &response, nullptr);

    if (controller.Failed())
    {
        std::cerr << "rpc failed: "
                  << controller.ErrorText()
                  << "\n";
        return 1;
    }

    std::cout << "response.code = "
              << response.code()
              << "\n";

    std::cout << "response.message = "
              << response.message()
              << "\n";

    std::cout << "response.success = "
              << std::boolalpha
              << response.success()
              << "\n";

    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}