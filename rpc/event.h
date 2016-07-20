/*
 * event.h
 *
 *  Created on: Dec 17, 2013
 *      Author: fan
 */

#ifndef PYXIS_RPC_EVENT_H_
#define PYXIS_RPC_EVENT_H_

#include <boost/shared_ptr.hpp>
#include <pyxis/base/channel.h>

namespace google {
namespace protobuf {
class Closure;
}
}

namespace pyxis {
namespace rpc {
class Request;
class Response;
}
}

namespace pyxis {
namespace rpc {

typedef ::google::protobuf::Closure EventCallback;

// request和reponse一直有效，直到callback被触发。
struct Event {
  const rpc::Request* Req;
  rpc::Response* Rep;
  const std::string Addr;

  Event(const rpc::Request* req, rpc::Response* rep, EventCallback* cb, const std::string& addr)
      : Req(req),
        Rep(rep),
        Addr(addr),
        done_(cb)
  {
  }

  Event()
      : Req(NULL),
        Rep(NULL),
        Addr(),
        done_(NULL)
  {
  }

  void Done();

  ~Event();
 private:
  EventCallback* done_;
};

typedef boost::shared_ptr<Event> EventPtr;
typedef Channel<EventPtr> EventChannel;
typedef boost::shared_ptr<EventChannel> EventChannelPtr;
}
}

#endif /* EVENT_H_ */
