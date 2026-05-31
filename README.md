# Simple Rpc Demo

一个基于 Protocol Buffers `cc_generic_services` 的 c++ RPC 最小原型。
当前版本使用阻塞式 TCP 实现服务端与客户端通信，支持通过 Protobuf 生成的`Service` / `Stub` / `RpcChannel` 完成一次远程方法调用。
本项目的目标不是实现完整的 RPC 框架，而是完成 RPC 的核心链路。
```text
client stub 调用  
↓  
RpcChannel::CallMethod()  
↓  
序列化 request  
↓  
构造 RpcHeader  
↓  
TCP 发送  
↓  
服务端解析 RpcHeader  
↓  
查找 service / method  
↓  
反序列化 request  
↓  
Service::CallMethod()  
↓  
业务方法执行  
↓  
序列化 response  
↓  
返回客户端
```
## 当前功能

- 使用 `.proto` 定义业务接口 `UserService`
- 支持 `Login(LoginRequest) returns (LoginResponse)`
- 实现客户端侧 `MyRpcChannel`
- 实现服务端侧 `RpcProvider`
- 支持服务注册：`NotifyService()`
- 支持方法分发：`service_name + method_name -> MethodDescriptor`
- 使用 `GetRequestPrototype(method).New()` 动态创建 request 对象
- 使用 `GetResponsePrototype(method).New()` 动态创建 response 对象
- 使用 `Closure` 在业务处理完成后回写 response
- 提供 `run.sh` 一键编译、启动服务端、运行客户端
## 项目结构

```
rpc_demo/
├── CMakeLists.txt
├── rpc_header.proto
├── user.proto
├── run.sh
└── src
    ├── common.h
    ├── rpc_channel.h
    ├── rpc_channel.cc
    ├── rpc_provider.h
    ├── rpc_provider.cc
    ├── server.cc
    └── client.cc
```
## 文件说明
### `user_proto`
定义业务 RPC 接口：
```c++
service UserService {
	rpc Login(LoginRequest) returns (LoginResponse);
}
```
生成：
+ `demo::LoginRequest`
+ `demo::LoginResponse`
+ `demo::UserService`
+ `demo::UserService_stub`

### `rpc_header.proto`
定义 RPC 框架头
```c++
message RpcHeader {
	string service_name = 1;
	string method_name = 2;
	uint32_t args_size = 3;
}
```
请求格式：
```text
[4 bytes header_size][RpcHeader bytes][request args bytes]
```
相应格式：
```text
[4 bytes response_size][response bytes]
```

### `rpc_channel.cc`
客户端 RPC 通道
主要负责：
- 从 `MethodDescriptor` 获取 `service_name` 和 `method_name`
- 序列化 request
- 构造 `RpcHeader`
- 建立 TCP 连接
- 发送请求包
- 接收响应包
- 反序列化 response

### `rpc_provider.cc`
主要负责：
+ 注册 service
+  启动 TCP server
+ 读取 RPC 请求
+ 解析 `RpcHeader`
+ 根据 `service_name` 查找 service
+ 根据 `method_name` 查找 method
+ 创建 request / response 对象
+ 调用 `service->CallMethod()`
+ 通过 `Closure` 回写 response
### `server.cc`
服务端入口。
实现业务类：
```
class UserServiceImpl : public demo::UserService
```
并注册到 `RpcProvider`：
```
provider.NotifyService(&user_service);provider.Run("0.0.0.0", 8000);
```

### `client.cc`
客户端入口。
通过生成的 Stub 发起调用：
```
demo::UserService_Stub stub(&channel);stub.Login(&controller, &request, &response, nullptr);
```
表面是本地函数调用，实际进入 `MyRpcChannel::CallMethod()`，通过网络发送 RPC 请求。
## RPC中各个类的协作关系


                      ┌──────────────────────────┐
                      │ google::protobuf::Service│ (抽象)
                      │ + CallMethod()           │
                      │ + GetRequestPrototype()  │
                      │ + GetResponsePrototype() │
                      └──────────┬───────────────┘
                                 │ 继承
                      ┌──────────▼───────────────┐
                      │       UserService        │ (生成，抽象)
                      │ + Echo() = 0             │
                      │ + CallMethod() 已实现     │  ← 根据方法描述符分发
                      └─────┬──────────┬─────────┘
                 继承        │          │ 继承
         ┌──────────▼──────┐ │  ┌───────▼────────────────┐
         │ UserServiceImpl │ │  │   UserService_Stub     │ (生成)
         │ (用户手写)       │ │  │   - channel_           │
         │ + Login() 实现   │ │  │   + Echo() {           │
         └─────────────────┘ │  │       channel_->Call... │
                             │  │     }                  │
                             │  └───────────┬────────────┘
                             │              │ 拥有
                             │      ┌───────▼────────────┐
                             │      │    RpcChannel      │ (抽象，你必须实现)
                             │      │ + CallMethod() = 0 │
                             │      └────────────────────┘
                             │
                     ┌───────▼────────────┐
                     │   RpcController    │ (抽象，你必须实现)
                     │ + SetFailed()      │
                     │ + StartCancel()    │
                     └────────────────────┘
                     ┌────────────────────┐
                     │     Closure        │ (抽象，框架/用户实现)
                     │ + Run()            │
                     └────────────────────┘
## 完整调用链

```text
服务端启动
    ↓
provider.NotifyService(&user_service)
    ↓
注册 demo.UserService.Login 的 MethodDescriptor
    ↓
provider.Run()
    ↓
等待客户端连接


客户端：
stub.Login(...)
    ↓
MyRpcChannel::CallMethod(...)
    ↓
发送 service_name = demo.UserService
    ↓
发送 method_name = Login
    ↓
发送 LoginRequest bytes


服务端：
HandleClient()
    ↓
解析 RpcHeader
    ↓
查 services_["demo.UserService"]
    ↓
查 methods["Login"]
    ↓
创建 LoginRequest / LoginResponse
    ↓
request->ParseFromString(args)
    ↓
service->CallMethod(method, ...)
    ↓
Protobuf 生成的 CallMethod 分发
    ↓
UserServiceImpl::Login(...)
    ↓
填充 response
    ↓
done->Run()
    ↓
发送 LoginResponse 给客户端
```

## 编译运行

安装依赖：

```
sudo apt update
sudo apt install -y protobuf-compiler libprotobuf-dev cmake g++
```

一键运行：

```
chmod +x run.sh
./run.sh
```

或者手动编译：

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

启动服务端：

```
./build/server
```

启动客户端：

```
./build/client
```

## 示例输出

客户端：

```
response.code = 0
response.message = login success
response.success = true
```

服务端：

```
register service: demo.UserService
RpcProvider listen on 0.0.0.0:8000
rpc_provider: demo.UserService.Login
UserServiceImpl::Login called
name = haojunpassword = 123456
```
## 当前限制

当前版本只是 RPC 最小闭环，仍有明显限制：
- 阻塞式 TCP
- 一个连接只处理一次 RPC
- 没有并发处理
- 没有超时机制
- 没有完整错误响应协议
- 没有连接复用
- 没有异步 callback 调度
- 没有接入 mini-muduo Reactor
- 没有服务发现、负载均衡、重试、心跳