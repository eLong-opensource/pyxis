#ifndef PYXIS_SERVER_SESSION_H
#define PYXIS_SERVER_SESSION_H

#include <set>
#include <map>
#include <vector>
#include <boost/shared_ptr.hpp>
#include <muduo/base/Timestamp.h>
#include <pyxis/rpc/event.h>
#include <pyxis/server/watcher.h>
#include <pyxis/common/watch_event.h>
#include <pyxis/common/types.h>
#include <pyxis/common/status.h>

#define DEFAULT_TIMEOUT (4 * 60 * 1000)

namespace pyxis {
class Server;
class WatchManager;
namespace store {
class Session;
}
}

namespace leveldb {
class DB;
class Snapshot;
}

namespace pyxis {

class Session {
 public:
  Session(sid_t sid, int timeout, WatchManager* wm, Server* server);
  ~Session();

  sid_t Sid() const { return sid_; }

  void SetTimeout(int t) { timeout_ = t; }

  int Timeout() const { return timeout_; }

  muduo::Timestamp ExpireTime() const { return expire_; }

  void SetExpireTime(muduo::Timestamp when) { expire_ = when; }

  void Close() { active_ = false; }

  bool IsActive() const { return active_; }

  void HoldPing(const rpc::EventPtr& event) { pingEvent_ = event; }

  bool HasPendingPing() const { return pingEvent_; }

  bool HasPendingWatcher() const { return !pendingWatcher_.empty(); }

  void PendingWatcher(const std::string& path, const watch::Event& e);

  void PingBack();

  void BindWatch(const WatcherPtr& w) {
    watches_[w->Path()] = w;
  }

  void RemoveWatch(const WatcherPtr& w) {
    watches_.erase(w->Path());
  }

  bool HasWatch(const std::string& p) const { return watches_.find(p) != watches_.end(); }

  void ResetWatch() { watches_.clear(); }

  void BindNode(const std::string& path) {
    nodeSet_.insert(path);
  }

  void RemoveNode(const std::string& path) {
    nodeSet_.erase(path);
  }

  bool SerializeToString(std::string* value) const ;

  bool SerializeToJson(std::string* value) const ;

 private:
  void toProtobuf(store::Session* s) const ;

  sid_t sid_;
  int timeout_;
  muduo::Timestamp expire_;

  muduo::Timestamp join_;

  muduo::Timestamp lastActive_;

  bool active_;

  // hold住的心跳请求
  rpc::EventPtr pingEvent_;

  // 等待返回给客户端的watcher
  std::multimap<std::string, watch::Event> pendingWatcher_;

  // reference to watch manager
  WatchManager* wm_;

  // reference to server
  Server* server_;

  // 订阅的watcher
  std::map<std::string, WatcherPtr> watches_;

  // 创建的临时节点
  std::set<std::string> nodeSet_;
};

typedef boost::shared_ptr<Session> SessionPtr;

}

#endif
