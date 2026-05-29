#pragma once 

#include <arpa/inet.h>
#include <errno.h>
#include <google/protobuf/service.h>
#include <string.h>
#include <unistd.h>

inline bool ReadN(int fd, void* buf, size_t n)
{
    char* p = static_cast<char*> (buf);
    size_t left = n;

    while (left > 0)
    {
        int ret = ::read(fd, p, left);
        if (ret > 0) {
            p += ret;
            left -= ret;
        }else if (ret <= 0)
        {
            if (errno == EAGAIN)
            {
                continue;       
            }
            else
            {
                return false; 
            }
        }
    }
    return true;
}

inline bool WriteN(int fd, void* buf, size_t n)
{
    char* p = static_cast<char*> (buf);
    size_t left = n;

    while (left > 0)
    {
        int ret = ::write(fd, p, left);
        if (ret > 0)
        {
            p += ret;
            left -= ret;
        }
        else if(ret <=0)
        {
            if (errno == EAGAIN)
            {
                continue;
            }
            else
            {
                return false;
            }
        }
    }
    return true;
}

class SimpleRpcController : public google::protobuf::RpcController {
public:
    void Reset() override {
        failed_ = false;
        error_text_.clear();
    }

    bool Failed() const override {
        return failed_;
    }

    std::string ErrorText() const override {
        return error_text_;
    }

    void StartCancel() override {}

    void SetFailed(const std::string& reason) override {
        failed_ = true;
        error_text_ = reason;
    }

    bool IsCanceled() const override {
        return false;
    }

    void NotifyOnCancel(google::protobuf::Closure* callback) override {}
private:
    bool failed_ = false;
    std::string error_text_;
};