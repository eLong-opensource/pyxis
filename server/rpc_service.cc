#include <pyxis/server/rpc_service.h>
#include <sofa/pbrpc/rpc_controller.h>
#include <glog/logging.h>

namespace pyxis {

RpcServiceImpl::RpcServiceImpl(rpc::RpcReceiver* rr)
    : rr_(rr)
{
}

RpcServiceImpl::~RpcServiceImpl()
{
}

void RpcServiceImpl::send(const rpc::EventPtr& event)
{
  rr_->ReceiveEvent(event);
}

void RpcServiceImpl::Call(::google::protobuf::RpcController* ctl,
                          const rpc::Request* req,
                          rpc::Response* rep,
                          ::google::protobuf::Closure* done)
{
  sofa::pbrpc::RpcController* sofactl = dynamic_cast<sofa::pbrpc::RpcController*>(ctl);
  CHECK_NOTNULL(sofactl);
  rep->set_xid(req->xid());
  send(rpc::EventPtr(new rpc::Event(req, rep, done, sofactl->RemoteAddress())));
}


}
