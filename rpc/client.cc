#include <pyxis/rpc/client.h>
#include <pyxis/proto/rpc.pb.h>
#include <pyxis/common/address.h>
#include <muduo/net/EventLoop.h>
#include <toft/system/threading/event.h>
#include <glog/logging.h>
#include <muduo/base/Logging.h>

using namespace muduo::net;

namespace pyxis {
namespace rpc {

void emptyConnectionCallback(const TcpConnectionPtr& conn)
{
  LOG_INFO << "emptyConnectionCallback";
}

void emptyMessageCallback(const TcpConnectionPtr& conn,
                          Buffer* buffer,
                          const muduo::Timestamp receiveTime)
{
  LOG_INFO << "emptyRpcMessageCallback";
}

Client::Client(EventLoop* loop,
               const std::string& addr)
    : loop_(loop),
      addr_(addr),
      closing_(false),
      connected_(false),
      codec_(boost::bind(&Client::onRpcMessage, this, _1, _2, _3)),
      client_(loop, ToMuduoAddr(addr), "pyxis.rpc"),
      xid_(0),
      pendingCalls_(),
      callTimeout_(10000)
{
  client_.setConnectionCallback(boost::bind(
      &Client::onConnection, this, _1));
}

Client::~Client()
{
  loop_->assertInLoopThread();
  LOG_INFO << "rpc::Client::~Client " << this;
  if (!closing_) {
    Close();
  }
}

void Client::Connect()
{
  client_.connect();
}

void Client::Close()
{
  loop_->assertInLoopThread();
  closing_ = true;
  connected_ = false;
  TcpConnectionPtr conn = client_.connection();
  if (conn) {
    conn->setConnectionCallback(emptyConnectionCallback);
    conn->setMessageCallback(emptyMessageCallback);
    conn->forceClose();
  }
  clearCalls();
}

void Client::Call(Status* st, Request* req,
                  Response* rep, const RpcCallback& done)
{
  loop_->queueInLoop(boost::bind(
      &Client::callInLoop, this, st, req, rep, done));
}

void Client::callInLoop(Status* st, Request* req,
                        Response* rep,
                        const RpcCallback& done)
{
  if (!connected_) {
    *st = Status::Disconnect("not connected");
    req->Clear();
    done();
    return;
  }
  RpcCall call;
  call.St = st;
  call.Req = req;
  call.Rep = rep;
  call.Done = done;
  req->set_xid(xid_);
  call.Tid = loop_->runAfter(callTimeout_ / 1000.0, boost::bind(
      &Client::onCallTimeout, this, req->xid()));
  pendingCalls_.insert(std::make_pair(xid_, call));
  LOG_TRACE << "new rpc call, xid:" << xid_;
  xid_++;

  codec_.send(client_.connection(), *req);
}

void Client::onConnection(const TcpConnectionPtr& conn)
{
  loop_->assertInLoopThread();
  if (!conn->connected()) {
    LOG_ERROR << "connection lost";
    connected_ = false;
    clearCalls();
    return;
  }
  conn->setMessageCallback(boost::bind(
      &ResponseCodec::onMessage,
      &codec_, _1, _2, _3));
  connected_ = true;
  connectionCallback_(true);
}

void Client::onRpcMessage(const TcpConnectionPtr& conn,
                          const ResponsePtr& message,
                          const muduo::Timestamp receiveTime)
{
  uint64_t xid = message->xid();
  std::map<uint64_t, RpcCall>::iterator it = pendingCalls_.find(xid);
  if (it == pendingCalls_.end()) {
    return;
  }
  RpcCall call = it->second;
  pendingCalls_.erase(it);
  call.Rep->CopyFrom(*message.get());
  *call.St = Status::OK();
  LOG_TRACE << "rpc call done, xid:" << message->xid();
  LOG_TRACE << "pending rpc calls:" << pendingCalls_.size();
  loop_->cancel(call.Tid);
  call.Done();
}

void Client::onCallTimeout(uint64_t xid)
{
  std::map<uint64_t, RpcCall>::iterator it = pendingCalls_.find(xid);
  if (it == pendingCalls_.end()) {
    return;
  }
  RpcCall call = it->second;
  pendingCalls_.erase(it);
  *call.St = Status::Timeout("rpc call timeout");
  call.Done();
}

void Client::clearCalls()
{
  CHECK(connected_ == false);
  loop_->assertInLoopThread();
  std::map<uint64_t, RpcCall> calls;
  calls.swap(pendingCalls_);
  std::map<uint64_t, RpcCall>::iterator it = calls.begin();
  for (; it != calls.end(); it++) {
    RpcCall& call = it->second;
    *call.St = Status::Disconnect("rpc call reset");
    call.Req->Clear();
    loop_->cancel(call.Tid);
    LOG_TRACE << "rpc call reset, xid:" << call.Req->xid();
    call.Done();
  }
}

}
}
