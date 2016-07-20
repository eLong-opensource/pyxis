#include <pyxis/server/session_manager.h>
#include <pyxis/server/session.h>
#include <glog/logging.h>
#include <algorithm>
#include <inttypes.h>
#include <pyxis/proto/node.pb.h>
#include <pyxis/common/node.h>
#include <pyxis/server/store.h>
#include <toft/base/string/format.h>
#include <toft/base/string/number.h>
#include <leveldb/db.h>
#include <leveldb/write_batch.h>
#include <muduo/base/Thread.h>
#include <boost/bind.hpp>

namespace pyxis {

static void defaultSessionCallback(Session*);
static muduo::Timestamp roundTo(muduo::Timestamp t1, int step);

SessionManager::SessionManager(int interval)
    : sid_(0),
      interval_(interval),
      sessionCallback_(defaultSessionCallback),
      sessions_(),
      timers_(),
      store_()
{
  initSid();
}

SessionManager::~SessionManager()
{
}

void SessionManager::SetSessionCallback(SessionCallback cb)
{
  sessionCallback_ = cb;
}

sid_t SessionManager::GenSid()
{
  return ++sid_;
}

void SessionManager::Add(SessionPtr s)
{
  // 防止leader回退sid
  if (sid_ < s->Sid()) {
    sid_ = s->Sid();
  }

  // 添加session到session持久化
  Status st = store_->AddSession(s);
  CHECK(st.ok()) << st.ToString();

  sessions_.insert(std::make_pair(s->Sid(), s));
  Touch(s->Sid());
}

void SessionManager::Remove(sid_t sid)
{
  SessionPtr s = Get(sid);
  CHECK(s);
  CHECK(!s->IsActive());
  removeFromTimers(sid);
  removeFromSessions(sid);
  Status st = store_->RemoveSession(sid);
  CHECK(st.ok()) << st.ToString();
}

SessionPtr SessionManager::Get(sid_t sid)
{
  SessionList::iterator it = sessions_.find(sid);
  if (it == sessions_.end()) {
    return SessionPtr();
  }
  return it->second;
}

bool SessionManager::GetSessions(muduo::Timestamp when,
                                 SessionSet** set)
{
  TimerList::iterator it = timers_.find(when);
  if (it == timers_.end()) {
    return false;
  }
  *set = &it->second;
  return true;
}

muduo::Timestamp SessionManager::Step()
{
  muduo::Timestamp now = muduo::Timestamp::now();
  TimerList::iterator end = timers_.lower_bound(now);
  for (TimerList::iterator it = timers_.begin();
       it != end; it++) {
    SessionSet& set = it->second;
    for (SessionSet::iterator it = set.begin(); it != set.end(); it++) {
      SessionPtr s = *it;
      if (s->IsActive()) {
        s->Close();
        sessionCallback_(s.get());
      }
    }
  }

  // 经过interval后再检查
  return roundTo(
      muduo::addTime(muduo::Timestamp::now(), static_cast<double>(interval_) / 1000),
      interval_);
}

void SessionManager::Touch(sid_t sid)
{
  SessionPtr s = Get(sid);
  CHECK(s);
  if (!s->IsActive()) {
    LOG(ERROR) << "touch closing session: " << sid;
    return;
  }
  removeFromTimers(sid);
  muduo::Timestamp expire = muduo::addTime(muduo::Timestamp::now(), static_cast<double>(s->Timeout()) / 1000);
  expire = roundTo(expire, interval_);
  s->SetExpireTime(expire);
  VLOG(1) << "touch session " << sid << ". expire at " << expire.toFormattedString();
  SessionSet& set = timers_[expire];
  set.insert(s);
}

// 把所有的session过期时间从现在开始向后延迟一个timeout
// 简单做法:遍历session, touch session, 这样会逐个从set中删除,再逐个添加
// 貌似快速做法:遍历timers, 更改session过期时间
void SessionManager::ResetTimer()
{
  TimerList newTimers;
  for (TimerList::iterator it = timers_.begin(); it != timers_.end(); it++) {
    SessionSet& set = it->second;
    CHECK(!set.empty());
    int timeout = (*set.begin())->Timeout();
    muduo::Timestamp expire = muduo::addTime(muduo::Timestamp::now(),
                                             static_cast<double>(timeout) / 1000);
    for (SessionSet::iterator it = set.begin(); it != set.end(); it++) {
      (*it)->SetExpireTime(expire);
    }
    SessionSet& newSet = newTimers[expire];
    newSet.swap(set);
  }

  timers_.swap(newTimers);
}

void SessionManager::ResetWatchers()
{
  for (std::map<sid_t, SessionPtr>::iterator it = sessions_.begin();
    it != sessions_.end(); it++) {
    it->second->ResetWatch();
  }
}

void SessionManager::ResetPendingPing()
{
  for (std::map<sid_t, SessionPtr>::iterator it = sessions_.begin();
    it != sessions_.end(); it++) {
    it->second->PingBack();
  }
}

Status SessionManager::OnProcRead(const store::ProcDB::ArgList& args,
                                  std::string* value)
{
  store::Node node;
  node.set_flags(kPersist);
  node.set_data("");
  node.set_version(0);

  // list sessions
  if (args.empty()) {
    for (std::map<sid_t, SessionPtr>::iterator it = sessions_.begin();
         it != sessions_.end(); it++) {
      node.add_children(toft::NumberToString(it->first));
    }
    node.SerializeToString(value);
    return Status::OK();
  }

  // get session node
  sid_t sid;
  if (!toft::StringToNumber(args[0], &sid)) {
    return Status::NotFound(args[0]);
  }
  SessionPtr session = Get(sid);
  if (!session) {
    return Status::NotFound(args[0]);
  }
  std::string data;
  if (!session->SerializeToJson(&data)) {
    return Status::Internal("get session error");
  }
  node.set_data(data);
  if (!node.SerializeToString(value)) {
    return Status::Internal("serialize node error");
  }
  return Status::OK();
}

// myid(1) + timestamp_ms(5) + zero(2)
void SessionManager::initSid()
{
  // TODO myid
  uint64_t myid = 0;
  muduo::Timestamp now = muduo::Timestamp::now();
  sid_ = now.microSecondsSinceEpoch() / 1000;
  sid_ = (sid_ << 24 >> 8) | myid << 56;
}

void SessionManager::removeFromTimers(sid_t sid)
{
  SessionPtr s = Get(sid);
  CHECK(s);
  muduo::Timestamp expire = s->ExpireTime();
  TimerList::iterator it = timers_.find(expire);
  if (it == timers_.end()) {
    return;
  }
  SessionSet& set = it->second;
  set.erase(s);
  if (set.empty()) {
    timers_.erase(it);
  }
}

void SessionManager::removeFromSessions(sid_t sid)
{
  sessions_.erase(sid);
}

muduo::Timestamp roundTo(muduo::Timestamp t1, int step)
{
  step = step * 1000;
  double begin = t1.microSecondsSinceEpoch();
  int64_t end = static_cast<int64_t>(begin / step + 0.5) * step;
  return muduo::Timestamp(end);
}

void defaultSessionCallback(Session* s)
{
}

}
