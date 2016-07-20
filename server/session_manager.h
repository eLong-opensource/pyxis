#ifndef PYXIS_SERVER_SESSION_MANAGER_H
#define PYXIS_SERVER_SESSION_MANAGER_H

#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <map>
#include <set>
#include <muduo/base/Timestamp.h>
#include <pyxis/common/types.h>
#include <pyxis/common/status.h>
#include <pyxis/server/proc_db.h>

namespace pyxis {
namespace store {
class Store;
}

class Session;

typedef boost::function<void (Session*)> SessionCallback;

class SessionManager {
 public:

  SessionManager(int interval);
  ~SessionManager();

  typedef boost::shared_ptr<Session> SessionPtr;
  typedef std::set<SessionPtr> SessionSet;

  void SetStore(store::Store* s) { store_ = s; }

  // session过期被删除时被调用
  void SetSessionCallback(SessionCallback cb);

  // 生成下一个sid
  sid_t GenSid();

  // 增加一个session 返回sid
  void Add(SessionPtr s);

  // 删除一个session
  void Remove(sid_t sid);

  SessionPtr Get(sid_t sid);

  // 检查过期session 返回下次检查时间
  muduo::Timestamp Step();

  // 某个时间超时的sessions
  bool GetSessions(muduo::Timestamp when, SessionSet** set);

  // 更改session的过期时间
  void Touch(sid_t sid);

  // 用于follower切换leader,对session重新计时
  void ResetTimer();

  // 用于重置所有session的watcher
  void ResetWatchers();

  // 重置绑定在session上的ping请求
  void ResetPendingPing();

  // proc的钩子函数
  // path: /pyxis/session
  Status OnProcRead(const store::ProcDB::ArgList& args, std::string* value);

 private:
  void initSid();
  void removeFromTimers(sid_t sid);
  void removeFromSessions(sid_t sid);
  typedef std::pair<muduo::Timestamp, SessionSet> TimerListEntry;
  typedef std::map<muduo::Timestamp, SessionSet> TimerList;
  typedef std::map<sid_t, SessionPtr> SessionList;
  sid_t sid_;
  int interval_;
  SessionCallback sessionCallback_;
  SessionList sessions_;
  TimerList timers_;
  store::Store* store_;
};

}// end of namespace

#endif
