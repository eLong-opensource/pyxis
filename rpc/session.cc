#include <pyxis/rpc/session.h>
#include <pyxis/proto/rpc.pb.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/base/Singleton.h>
#include <google/protobuf/stubs/common.h>
#include <pyxis/rpc/event.h>
#include <pyxis/common/event_log.h>

namespace pyxis {
namespace rpc {

Session::Session(const muduo::net::TcpConnectionPtr& conn,
                 RpcReceiver* rr)
    : codec_(boost::bind(&Session::onRpcMessage, this, _1, _2, _3)),
      conn_(conn),
      rr_(rr)
{
  conn->setMessageCallback(boost::bind(&RequestCodec::onMessage,
                                       &codec_, _1, _2, _3));
}

Session::~Session()
{
}

void Session::onRpcMessage(const muduo::net::TcpConnectionPtr& conn,
                           const RequestPtr& message,
                           const muduo::Timestamp receiveTime)
{
  Call call;
  call.Start = muduo::Timestamp::now();
  call.Addr = conn->peerAddress().toIp().c_str();
  call.Req = message;
  call.Rep.reset(new Response);

  boost::weak_ptr<Session> wptr(shared_from_this());

  google::protobuf::Closure* done = google::protobuf::NewCallback(
      this, &Session::onRpcDone, wptr, call);

  EventPtr event(new Event(call.Req.get(),
                           call.Rep.get(),
                           done,
                           conn->peerAddress().toIp().c_str()));

  rr_->ReceiveEvent(event);
}

void Session::onRpcDone(boost::weak_ptr<Session> wptr,
                        Call call)
{
  boost::shared_ptr<Session> s(wptr.lock());
  if (s) {
    call.Rep->set_xid(call.Req->xid());
    s->codec_.send(s->conn_, *call.Rep);

    EventLog& logger = muduo::Singleton<EventLog>::instance();
    double used = muduo::timeDifference(muduo::Timestamp::now(), call.Start);
    logger.LogRpc(call.Addr, call.Req.get(), call.Rep.get(), used);
  }
}


}
}
