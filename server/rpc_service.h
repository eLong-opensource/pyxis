#ifndef PYXIS_RPC_SERVICE_H
#define PYXIS_RPC_SERVICE_H

#include <pyxis/proto/rpc.pb.h>
#include <pyxis/rpc/rpc_receiver.h>

namespace pyxis {
class RpcServiceImpl : public rpc::RpcService
{
 public:
  RpcServiceImpl(rpc::RpcReceiver* rr);
  ~RpcServiceImpl();

  void Call(::google::protobuf::RpcController*,
            const rpc::Request* request,
            rpc::Response* response,
            ::google::protobuf::Closure* done);

 private:
  void send(const rpc::EventPtr& event);
  rpc::RpcReceiver* rr_;
};

}

#endif
