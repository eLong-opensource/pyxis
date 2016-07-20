#ifndef PYXIS_RPC_RPC_RECEIVER_H
#define PYXIS_RPC_RPC_RECEIVER_H

#include <pyxis/rpc/event.h>

namespace pyxis {
namespace rpc {

class RpcReceiver
{
 public:
  virtual ~RpcReceiver() {}
  virtual void ReceiveEvent(const EventPtr& e) = 0;
};

}
}

#endif
