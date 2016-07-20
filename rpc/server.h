#ifndef PYXIS_RPC_SERVER_H
#define PYXIS_RPC_SERVER_H

#include <map>
#include <muduo/net/TcpServer.h>
#include <muduo/net/protobuf/ProtobufCodecLite.h>
#include <pyxis/rpc/rpc_receiver.h>

namespace pyxis {
namespace rpc {

class Session;
typedef boost::shared_ptr<Session> SessionPtr;

class Server {
 public:
  Server(muduo::net::EventLoop* loop,
         const std::string& addr,
         RpcReceiver* rr);
  ~Server();

  void Start();

 private:

  void onConnection(const muduo::net::TcpConnectionPtr& conn);

 private:
  uint64_t sid_;
  std::map<uint64_t, SessionPtr> sessions_;
  RpcReceiver* rr_;
  muduo::net::TcpServer server_;
};

}
}

#endif
