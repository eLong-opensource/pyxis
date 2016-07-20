#include <pyxis/rpc/server.h>
#include <pyxis/rpc/session.h>
#include <pyxis/common/address.h>
#include <muduo/net/TcpConnection.h>

using namespace muduo::net;

namespace pyxis {
namespace rpc {

Server::Server(EventLoop* loop, const std::string& addr, RpcReceiver* rr)
    : sid_(0),
      sessions_(),
      rr_(rr),
      server_(loop, ToMuduoAddr(addr), "pyxis.rpc")
{
  server_.setConnectionCallback(boost::bind(
      &Server::onConnection, this, _1));
}

Server::~Server()
{
}

void Server::Start()
{
  server_.start();
}

void Server::onConnection(const muduo::net::TcpConnectionPtr& conn)
{
  if (conn->connected()) {
    conn->setContext(sid_);
    SessionPtr session(new Session(conn, rr_));
    sessions_.insert(std::make_pair(sid_, session));
    sid_++;
  } else {
    sessions_.erase(boost::any_cast<uint64_t>(conn->getContext()));
  }
}

}
}
