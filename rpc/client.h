#ifndef PYXIS_RPC_CLIENT_H
#define PYXIS_RPC_CLIENT_H

#include <muduo/net/TcpClient.h>
#include <muduo/net/TimerId.h>
#include <pyxis/rpc/codec.h>
#include <pyxis/common/status.h>
#include <map>
#include <set>

namespace pyxis {
namespace rpc {

typedef boost::function<void()> RpcCallback;
typedef boost::function<void(bool)> ConnectionCallback;

class Client {
 public:
  Client(muduo::net::EventLoop* loop,
         const std::string& addr);
  ~Client();

  // rpc call timeout in ms. default 5s
  void SetCallTimeout(int timeout) { callTimeout_ = timeout; }

  void SetConnectionCallback(const ConnectionCallback& cb)
  {
    connectionCallback_ = cb;
  }

  void Connect();

  void Close();

  // async call
  void Call(Status* st, Request* req,
            Response* rep,
            const RpcCallback& done);

  std::string Addr() { return addr_; }

 private:

  void callInLoop(Status* st, Request* req,
                  Response* rep,
                  const RpcCallback& done);

  void onConnection(const muduo::net::TcpConnectionPtr& conn);

  void onRpcMessage(const muduo::net::TcpConnectionPtr& conn,
                    const ResponsePtr& message,
                    const muduo::Timestamp receiveTime);

  void onCallTimeout(uint64_t xid);

  void clearCalls();

  struct RpcCall {
    Status* St;
    Request* Req;
    Response* Rep;
    RpcCallback Done;
    muduo::net::TimerId Tid;
  };
  muduo::net::EventLoop* loop_;

  std::string addr_;

  // 用户主动调用close
  bool closing_;
  // 是否连接上server
  bool connected_;

  ResponseCodec codec_;
  muduo::net::TcpClient client_;
  uint64_t xid_;
  std::map<uint64_t, RpcCall> pendingCalls_;

  int callTimeout_;

  ConnectionCallback connectionCallback_;
};

}
}

#endif
