#include "rpc_provider.h"
#include "user.pb.h"

#include <google/protobuf/stubs/common.h>
#include <iostream>

class UserServiceImpl : public demo::UserService
{
public:
    void Login(google::protobuf::RpcController *controller,
               const demo::LoginRequest *request,
               demo::LoginResponse *response,
               google::protobuf::Closure *done) override
    {
        std::cout << "UserServiceImpl::Login called\n";
        std::cout << "name = " << request->name() << "\n";
        std::cout << "password = " << request->password() << std::endl;

        if (request->name() == "haojun" &&
            request->password() == "123456")
        {
            response->set_code(0);
            response->set_message("login success");
            response->set_success(true);
        }
        else
        {
            response->set_code(1);
            response->set_message("login failed");
            response->set_success(false);
        }

        if (done)
        {
            done->Run();
        }
    }
};

int main(){
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    UserServiceImpl user_service;
    RpcProvider provider;
    provider.NotifyService(&user_service);

    provider.Run("0.0.0.0", 8000);

    google::protobuf::ShutdownProtobufLibrary();
    
    return 0;
}