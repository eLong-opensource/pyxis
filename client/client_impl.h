#ifndef PYXIS_CLIENT_CLIENT_IMPL_H
#define PYXIS_CLIENT_CLIENT_IMPL_H

#include <string>
#include <vector>
#include <boost/function.hpp>
#include <boost/scoped_ptr.hpp>
#include <muduo/base/Thread.h>
#include <muduo/net/EventLoopThread.h>
#include <muduo/base/Mutex.h>
#include <muduo/net/TimerId.h>
#include <pyxis/common/types.h>
#include <pyxis/proto/rpc.pb.h>
#include <pyxis/common/watch_event.h>
#include <pyxis/common/status.h>
#include <pyxis/client/watch_manager.h>
#include <pyxis/client/options.h>
#include <pyxis/client/cache.h>
#include <pyxis/client/client.h>
#include <pyxis/rpc/client.h>
#include <toft/system/atomic/atomic.h>
#include <toft/system/threading/event.h>

namespace toft {
class ManualResetEvent;
}

namespace muduo {
namespace net {
class EventLoop;
}
}

namespace pyxis {
namespace client {

class Conn;

typedef std::vector<std::string> AddrList;
typedef boost::shared_ptr<Status> StatusPtr;
typedef boost::shared_ptr<rpc::Request> RequestPtr;
typedef boost::shared_ptr<rpc::Response> ResponsePtr;

class ClientImpl : public Client {
 public:
  ClientImpl(const Options& option, const std::string& addrList, const WatchCallback& cb);
  ~ClientImpl();

  Status Start();

  Status Create(const WriteOptions& options, const std::string& path, const std::string& data);

  Status Delete(const WriteOptions& options, const std::string& path);

  Status Write(const WriteOptions& options, const std::string& path, const std::string& data);

  Status Read(const ReadOptions& options, const std::string& path, Node* node);

  Status Sid(sid_t* sid);

  time_t LastPing() { return lastPing_; }

 private:
  enum State {
    kLooking,
    kConnected,
    kClosed,
  };

  Status unregister();

  void onConnection(bool connected);

  void onConnectionTimeout(const std::string& addr);

  void resetConnection(const std::string addr);

  void register_();

  void onRegister(StatusPtr st, RequestPtr req, ResponsePtr rep);

  Status call(rpc::Request* req, rpc::Response* rep);

  void callInLoop(Status* st, rpc::Request* req,
                  rpc::Response* rep, const rpc::RpcCallback& cb);

  void onCall(toft::AutoResetEvent* done);

  void ping();

  void onPing(StatusPtr st, RequestPtr req, ResponsePtr rep);

  void reregisterWatchers();

  void watcherWalker(const WatcherPtr&);

  State state() { return state_.Value();}

  void setState(State st) { state_ = st; }

  boost::scoped_ptr<muduo::net::EventLoopThread> loopThread_;

  muduo::net::EventLoop* loop_;

  toft::Atomic<State> state_;

  AddrList addrList_;

  // session timeout in ms
  Options options_;

  Cache cache_;

  time_t lastPing_;

  WatchManager wm_;

  boost::shared_ptr<rpc::Client> client_;

  toft::Atomic<sid_t> sid_;

  muduo::net::TimerId connTimerId_;
};

}
}

#endif
