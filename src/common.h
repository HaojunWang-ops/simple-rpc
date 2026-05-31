#pragma once 

#include <arpa/inet.h>
#include <errno.h>
#include <google/protobuf/service.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <iomanip>

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

//将二进制字符串以可读十六进制格式打印出来
inline void PrintHex(const std::string& name, const std::string& s)
{
    std::cout << name << " size= " << s.size() << " hex= ";

    for (unsigned char c : s){
        std::cout << std::hex
                  << std::setw(2)
                  << std::setfill('0')
                  << static_cast<int> (c)
                  <<" ";
    }

    std::cout << std::dec << "\n";
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