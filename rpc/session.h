#ifndef PYXIS_RPC_SESSION_H
#define PYXIS_RPC_SESSION_H

#include <pyxis/rpc/codec.h>
#include <pyxis/rpc/rpc_receiver.h>
#include <boost/enable_shared_from_this.hpp>
#include <muduo/base/Timestamp.h>

namespace pyxis {
namespace rpc {

class Session : public boost::enable_shared_from_this<Session>
{
 public:
  Session(const muduo::net::TcpConnectionPtr& conn, RpcReceiver* rr);
  ~Session();

 private:

  struct Call {
    muduo::Timestamp Start;
    std::string Addr;
    RequestPtr Req;
    ResponsePtr Rep;
  };

  void onRpcMessage(const muduo::net::TcpConnectionPtr& conn,
                    const RequestPtr& message,
                    const muduo::Timestamp receiveTime);

  void onRpcDone(boost::weak_ptr<Session> wptr,
                 Call call);

  RequestCodec codec_;

  muduo::net::TcpConnectionPtr conn_;

  RpcReceiver* rr_;
};

}
}
#endif
