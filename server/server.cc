#include <glog/logging.h>
#include <boost/bind.hpp>
#include <muduo/base/Timestamp.h>
#include <muduo/base/Thread.h>
#include <muduo/base/Singleton.h>
#include <raft/core/raft.h>
#include <toft/storage/path/path.h>
#include <toft/base/string/algorithm.h>
#include <pyxis/common/watch_event.h>
#include <pyxis/server/message.h>
#include <pyxis/server/server.h>
#include <pyxis/base/protobuf/packet.h>
#include <pyxis/base/selector.h>
#include <pyxis/base/timer.h>
#include <pyxis/server/session.h>
#include <pyxis/server/store.h>
#include <pyxis/proto/node.pb.h>
#include <pyxis/proto/session.pb.h>
#include <pyxis/common/node.h>
#include <pyxis/common/status.h>
#include <pyxis/common/event_log.h>
#include <pyxis/server/proc_db.h>

namespace pyxis
{

static void emptyUnregisterCallback(boost::shared_ptr<rpc::Request> req,
                                    boost::shared_ptr<rpc::Response> rep)
{
}

static std::string snapshotDBDir(const std::string& dir, uint64_t index)
{
  char buf[32];
  snprintf(buf, 32, "%020"PRIu64, index);
  return toft::Path::Join(dir, buf);
}

static bool needBoradcast(const rpc::Request* req)
{
  return req->has_unregister() ||
      req->has_register_() ||
      req->has_create() ||
      req->has_delete_() ||
      req->has_write();
}

Server::Server(const Options& options, raft::Raft* raft)
    : options_(options),
      db_(store::OpenLevelDB(options.DBDir)),
      state_(kFollower),
      requests_(),
      eventch_(options.RpcChannelSize),
      cmdch_(),
      statech_(),
      taskch_(),
      raft_(raft),
      store_(),
      sm_(options.SessionCheckInterval),
      wm_(),
      logIndex_(0),
      logger_(muduo::Singleton<EventLog>::instance()),
      thread_(boost::bind(&Server::Run, this), "pyxis")
{
  CHECK(raft);
  raft_->SetSyncedCallback(
      boost::bind(&Server::onSynced, this, _1, _2));
  raft_->SetStateChangedCallback(
      boost::bind(&Server::onState, this, _1));
  raft_->SetTakeSnapshotCallback(
      boost::bind(&Server::onTakeSnapshot, this, _1, _2));
  raft_->SetLoadSnapshotCallback(
      boost::bind(&Server::onLoadSnapshot, this, _1));

  CHECK(db_ != NULL) << "Open db failed";
  store::ProcDB* proc = new store::ProcDB(options.ProcRoot);
  proc->RegisterReadNode(toft::Path::Join(options.ProcRoot, "session"),
                         boost::bind(&SessionManager::OnProcRead, &sm_, _1, _2));
  proc->RegisterWriteNode(toft::Path::Join(options.ProcRoot, "eventlog"),
                         boost::bind(&EventLog::OnProcWrite, &logger_, _1, _2));
  proc->RegisterWriteNode(toft::Path::Join(options.ProcRoot, "snapshot"),
                          boost::bind(&Server::onProcTakeSnapshot, this, _1, _2));

  proc->RegisterReadNode(toft::Path::Join(options.ProcRoot, "cluster"),
                          boost::bind(&Server::onProcCluster, this, _1, _2));


  store_.reset(new store::Store(db_, proc, options.ProcRoot));
  proc->RegisterReadNode(toft::Path::Join(options.ProcRoot, "db"),
                         boost::bind(&store::Store::OnProcDBStatusRead, store_.get(), _1, _2));

  sm_.SetSessionCallback(boost::bind(&Server::onSessionClosed, this, _1));
  sm_.SetStore(store_.get());
}

Server::~Server()
{
}

void Server::ReceiveEvent(const rpc::EventPtr& e)
{
  eventch_.Put(e);
}

void Server::Run()
{
  mainLoop();
}

void Server::Start()
{
  thread_.start();
}

void Server::Stop()
{
  statech_.Put(raft::STOP);
  thread_.join();
}

void Server::replicate(const Command& cmd)
{
  raft_->Broadcast(cmd.SerializeAsString());
}

void Server::onState(int state)
{
  // 清空待处理队列
  while (!requests_.empty()) {
    rpc::EventPtr e = requests_.front();
    Status::Again("net error, try again").ToRpcError(e->Rep->mutable_err());
    requests_.pop_front();
  }
  sm_.ResetPendingPing();
  statech_.Put(state);
}

void Server::onSynced(uint64_t index, const std::string& data)
{
  logIndex_ = index;
  Status st = store_->SetCommitIndex(index);
  CHECK(st.ok()) << st.ToString();

  CommandPtr cmd(new Command);
  CHECK(cmd->ParseFromString(data)) << "parse log data error";
  cmdch_.Put(cmd);
}

void Server::walkSessionFunc(const store::Session& s)
{
  EventLog& logger = muduo::Singleton<EventLog>::instance();
  SessionPtr sp(new Session(s.sid(), s.timeout(), &wm_, this));
  sm_.Add(sp);
  VLOG(2) << "restore session " << s.sid() << " timeout " << s.timeout();
  logger.LogSession(s.sid(), "restore");
}

void Server::walkTreeFunc(const std::string& p, const store::Node& node)
{
  if (node.flags() & kEphemeral) {
    SessionPtr s = sm_.Get(node.sid());
    // 如果节点的session不存在就创建一个默认的session，等待重连，如果没有心跳等待自然过期
    if (!s) {
      LOG(WARNING) << "ephemeral node " << p << " sid " << node.sid() << " gone";
      s.reset(new Session(node.sid(), DEFAULT_TIMEOUT, &wm_, this));
      sm_.Add(s);
      LOG(INFO) << "restore session " << node.sid() << " from ephemeral node";
      EventLog& logger = muduo::Singleton<EventLog>::instance();
      logger.LogSession(node.sid(), "restoree");
    }
    s->BindNode(p);
  }
}

// at booststrap, thread safe
uint64_t Server::onLoadSnapshot(uint64_t commitIndex)
{
  Status st = store_->WalkSessions(boost::bind(&Server::walkSessionFunc, this, _1));
  CHECK(st.ok()) << st.ToString();
  st = store_->WalkTree(boost::bind(&Server::walkTreeFunc, this, _1, _2));
  CHECK(st.ok()) << st.ToString();

  uint64_t idx;
  st = store_->GetCommitIndex(&idx);
  CHECK(st.ok()) << st.ToString();
  return idx;
}

// 生成完快照的回调，打打日志
void takeSnapshotCallback(uint64_t idx, muduo::Timestamp begin,
                            const raft::TakeSnapshotDoneCallback& cb)
{
  EventLog& logger = muduo::Singleton<EventLog>::instance();

  double used = muduo::timeDifference(muduo::Timestamp::now(), begin);
  char buf[32];
  snprintf(buf, 32, "%010ld", idx);
  logger.Log("-", "snapshot", 0, 0, buf, "", used);
  cb();
}

void Server::onTakeSnapshot(uint64_t commitIndex, const raft::TakeSnapshotDoneCallback& cb)
{
  std::string dbDir = snapshotDBDir(options_.SnapshotDir, commitIndex);
  raft::TakeSnapshotDoneCallback done(boost::bind(&takeSnapshotCallback,
                                                  commitIndex, muduo::Timestamp::now(), cb));
  muduo::Thread thread(boost::bind(&store::Store::TakeSnapshot, store_.get(), dbDir, done));
  thread.start();
}

void emptySnapshotCallback(){}
Status Server::onProcTakeSnapshot(const store::ProcDB::ArgList& args, const std::string& value)
{
  uint64_t idx;
  Status st = store_->GetCommitIndex(&idx);
  if (!st.ok()) {
    return st;
  }
  onTakeSnapshot(idx, emptySnapshotCallback);
  return Status::OK();
}

Status Server::onProcCluster(const store::ProcDB::ArgList& args, std::string* value)
{
  store::Node node;
  node.set_flags(kPersist);
  node.set_data("");
  node.set_version(0);

  if (args.size() == 0) {
    node.add_children("leader");
    node.add_children("addrlist");
    node.SerializeToString(value);
    return Status::OK();
  }

  if (args[0] == "addrlist") {
    std::string addr = toft::JoinStrings(options_.AddrList, ",");
    node.set_data(addr);
  } else if (args[0] == "leader") {
    int leaderid = raft_->LeaderId();
    if (leaderid != 0) {
      std::string leaderAddr = options_.AddrList[leaderid - 1];
      node.set_data(leaderAddr);
    }
  }
  node.SerializeToString(value);
  return Status::OK();
}

// session销毁的回调 在这里模拟一次客户端的注销请求
void Server::onSessionClosed(Session* s)
{
  CHECK(!s->IsActive());
  logger_.LogSession(s->Sid(), "expire");
  boost::shared_ptr<rpc::Request> req(new rpc::Request);
  boost::shared_ptr<rpc::Response> rep(new rpc::Response);
  google::protobuf::Closure* done = NewCallback(&emptyUnregisterCallback, req, rep);
  rpc::EventPtr event(new rpc::Event(req.get(), rep.get(), done, "-"));
  req->set_sid(s->Sid());
  req->mutable_unregister();
  handleEvent(event);
}

void Server::mainLoop()
{
  for (;;) {
    VLOG(1) << "now state " << state_;
    switch (state_) {
      case kFollower:
        followerLoop();
        break;
      case kWaiter:
        waitLoop();
        break;
      case kLeader:
        leaderLoop();
        break;
      case kStop:
        LOG(INFO) << "state kStop, server stoped.";
        return;
      default:
        LOG(INFO) << "unknown state " << state_;
        return;
    }
  }
}

void Server::leaderLoop()
{
  LOG(INFO) << "Now state leader";
  Timer pingbackTimer;
  Timer timeoutTimer;
  muduo::Timestamp timeout;

  LOG(INFO) << "reset session timers";
  sm_.ResetTimer();

  LOG(INFO) << "reset session's watchers";
  sm_.ResetWatchers();

  LOG(INFO) << "reset watchers of watch manager";
  wm_.Reset();

  timeoutTimer.At(sm_.Step());
  Selector slt;
  slt.Register(&timeoutTimer);
  slt.Register(&cmdch_);
  slt.Register(&eventch_);
  slt.Register(&statech_);
  slt.Register(&taskch_);
  for (;;) {
    Selectable* choosen = slt.Wait();
    CHECK(choosen != NULL);

    if (choosen == &taskch_) {
      Functor task;
      taskch_.Take(&task);
      task();
    }

    if (choosen == &statech_) {
      int state = 0;
      statech_.Take(&state);
      LOG(INFO) << "receive raft state notify " << state;
      if (state == raft::STOP) {
        state_ = kStop;
        break;
      }
      state_ = kFollower;
      break;
    }

    if (choosen == &eventch_) {
      rpc::EventPtr e;
      eventch_.Take(&e);
      handleEvent(e);
    }

    if (choosen == &cmdch_) {
      CommandPtr cmd;
      cmdch_.Take(&cmd);
      onCommand(cmd);
    }

    if (choosen == &timeoutTimer) {
      timeout = sm_.Step();
      timeoutTimer.At(timeout);
      SessionManager::SessionSet* set;
      if (sm_.GetSessions(timeout, &set)) {
        pingBack(*set);
      }
    }
  }
}

void Server::followerLoop()
{
  LOG(INFO) << "Now state follower";
  Selector slt;
  slt.Register(&cmdch_);
  slt.Register(&eventch_);
  slt.Register(&statech_);
  slt.Register(&taskch_);
  for (;;) {
    Selectable* choosen = slt.Wait();
    CHECK(choosen != NULL);

    if (choosen == &taskch_) {
      Functor task;
      taskch_.Take(&task);
      task();
    }

    if (choosen == &statech_) {
      int state = 0;
      statech_.Take(&state);
      LOG(INFO) << "receive raft state notify " << state;
      if (state == raft::STOP) {
        state_ = kStop;
        break;
      }
      if (state == raft::LEADER) {
        state_ = kWaiter;
        break;
      }
    }

    if (choosen == &cmdch_) {
      CommandPtr cmd;
      cmdch_.Take(&cmd);
      onCommand(cmd);
    }

    if (choosen == &eventch_) {
      rpc::EventPtr e;
      eventch_.Take(&e);
      int leaderid = raft_->LeaderId();
      if (leaderid == 0) {
        Status::Again("try again").ToRpcError(e->Rep->mutable_err());
      } else {
        std::string leaderAddr = options_.AddrList[leaderid - 1];
        Status::NotLeader(leaderAddr).ToRpcError(e->Rep->mutable_err());
      }
    }
  }
}

void Server::waitLoop()
{
  LOG(INFO) << "Now state wait";
  // 发送一个ping包 这个ping包的sid字段使用当前时间戳 然后等待这个包的广播
  // 当这个包被成功广播 证明之前的日志都被提交了 可以正常提供服务
  uint64_t tag = muduo::Timestamp::now().microSecondsSinceEpoch();
  rpc::Request ping;
  ping.set_sid(tag);
  ping.mutable_ping();
  replicate(ping);
  Selector slt;
  slt.Register(&statech_);
  slt.Register(&cmdch_);
  for (;;) {
    Selectable* choosen = slt.Wait();
    if (choosen == NULL) {
      continue;
    }

    if (choosen == &statech_) {
      int state = 0;
      statech_.Take(&state);
      LOG(INFO) << "receive raft state notify " << state;
      if (state == raft::STOP) {
        state_ = kStop;
        break;
      }
      CHECK(state != kLeader);
      state_ = kFollower;
      break;
    }
    if (choosen == &cmdch_) {
      CommandPtr cmd;
      cmdch_.Take(&cmd);
      if (cmd->has_ping() && cmd->sid() == tag) {
        state_ = kLeader;
        break;
      }
      onCommand(cmd);
    }
  }
}

void Server::pingBack(const std::set<SessionPtr>& sessions)
{
  for (std::set<SessionPtr>::iterator it = sessions.begin();
       it != sessions.end(); ++it) {
    (*it)->PingBack();
  }
}

void Server::handleEvent(rpc::EventPtr& e)
{
  const rpc::Request* req = e->Req;
  rpc::Response* rep = e->Rep;

  // 注册需要单独判断 由于sid不能判断过期 同时广播的时候需要修改request
  if (req->has_register_()) {
    handleRegister(e);
    return;
  }

  Status st;
  sid_t sid = req->sid();
  if (!sm_.Get(sid)) {
    Status::SessionExpired("session expired").ToRpcError(rep->mutable_err());
    return;
  }

  // 所有非ping和unregister的请求也会更新会话过期时间
  if (!req->has_ping() && !req->has_unregister()) {
    sm_.Touch(sid);
  }

  // 需要广播的进入待处理队列
  if (needBoradcast(req)) {
    replicate(*req);
    requests_.push_back(e);
    return;
  }

  // ping自己处理是否立即返回
  if (req->has_ping()) {
    st = handlePing(e);
  } else if (req->has_read()) {
    st = handleRead(e);
  } else if (req->has_stat()) {
    st = handleStat(e);
  } else if (req->has_watch()) {
    st = onWatch(e->Req);
  }

  if (!st.ok()) {
    st.ToRpcError(rep->mutable_err());
  }
}

Status Server::getSession(sid_t sid, SessionPtr* s)
{
  *s = sm_.Get(sid);
  if (!*s) {
    return Status::SessionExpired("session expired");
  }
  return Status::OK();
}

// 使用过期sid或空的sid都直接返回, 没有sid字段则分配一个sid
// 然后广播
void Server::handleRegister(rpc::EventPtr& e)
{
  const rpc::Request* req = e->Req;
  rpc::Response* rep = e->Rep;

  int timeout = req->register_().timeout();
  if (timeout < options_.MinSessionTimeout) {
    timeout = options_.MinSessionTimeout;
  }
  if (timeout > options_.MaxSessionTimeout) {
    timeout = options_.MaxSessionTimeout;
  }

  if (req->has_sid()) {
    sid_t sid = req->sid();
    SessionPtr s = sm_.Get(sid);
    if (s) {
      LOG(INFO) << "Use sid " << sid << " register success";
      rep->set_sid(sid);
      // 注册成功更新timeout，因为session可能是从临时节点构造的，timeout是默认值
      s->SetTimeout(timeout);
    } else {
      LOG(INFO) << "Used sid " << req->sid() << " register failed. expired session";
      rpc::Error* err = rep->mutable_err();
      err->set_type(rpc::kSessionExpired);
    }
    return;
  }

  // 复用register request来作为新建session的命令
  sid_t sid = sm_.GenSid();
  rpc::Request cmd;
  cmd.set_sid(sid);
  cmd.mutable_register_()->set_timeout(timeout);
  requests_.push_back(e);
  replicate(cmd);
  return;
}

Status Server::handlePing(rpc::EventPtr& e)
{
  SessionPtr s;
  Status st = getSession(e->Req->sid(), &s);
  if (!st.ok()) {
    return st;
  }

  // 更新session过期时间,然后查看是否有未发送的watcher
  // 如果没有要发送的watcher, sesion保留event引用,不会出发callback
  // 有要发送的watcher, session将没有event的引用,再触发event的callback
  sm_.Touch(s->Sid());
  s->HoldPing(e);
  if (s->HasPendingWatcher()) {
    s->PingBack();
  }
  return Status::OK();
}

Status Server::handleRead(rpc::EventPtr& e)
{
  SessionPtr s;
  Status st = getSession(e->Req->sid(), &s);
  if (!st.ok()) {
    return st;
  }

  std::string data;
  st = store_->Read(e->Req->read().path(), &data);
  if (!st.ok()) {
    return st;
  }
  e->Rep->set_data(data);
  if (e->Req->has_watch()) {
    return onWatch(e->Req);
  }
  return Status::OK();
}

Status Server::handleStat(rpc::EventPtr& e)
{
  SessionPtr s;
  Status st = getSession(e->Req->sid(), &s);
  if (!st.ok()) {
    return st;
  }

  store::Node node;
  st = store_->Get(e->Req->stat().path(), &node);
  if (!st.ok()) {
    // stat调用可以关注节点的创建
    if (!(st.IsNotFound() && e->Req->has_watch())) {
      return st;
    }
    Status st1 = onWatch(e->Req);
    if (!st1.ok()) {
      return st1;
    }
    return st;
  }
  node.SerializeToString(e->Rep->mutable_data());

  if (e->Req->has_watch()) {
    return onWatch(e->Req);
  }
  return Status::OK();
}

void Server::onCommand(CommandPtr& cmd)
{
  LOG_EVERY_N(INFO, 1000) << "on command " << google::COUNTER;
  rpc::Request* req = cmd.get();
  rpc::Response rep;
  rpc::EventPtr e;
  if (state_ == kLeader) {
    CHECK(!requests_.empty()) << "request queue empty";
    e = requests_.front();
    requests_.pop_front();
  }

  Status st;
  if (req->has_register_()) {
    sid_t sid;
    st = onRegister(req, &sid);
    if (st.ok()) {
      rep.set_sid(sid);
    }
  } else if (req->has_unregister()) {
    st = onUnRegister(req);
  } else if (req->has_create()) {
    st = onCreate(req);
  } else if (req->has_delete_()) {
    st = onDelete(req);
  } else if (req->has_read()) {
    st = onRead(req, rep.mutable_data());
  } else if (req->has_write()) {
    st = onWrite(req);
  } else if (req->has_stat()) {
    st = onStat(req, rep.mutable_data());
  } else if (req->has_watch()) {
    st = onWatch(req);
  } else {
    // TODO
  }

  if (!st.ok()) {
    st.ToRpcError(rep.mutable_err());
  }

  if (e) {
    e->Rep->Swap(&rep);
  }
}

Status Server::onRegister(const rpc::Request* req, sid_t* sid)
{
  uint32_t timeout = req->register_().timeout();
  SessionPtr s(new Session(req->sid(), timeout, &wm_, this));
  sm_.Add(s);

  logger_.LogSession(s->Sid(), "new");
  if (sid != NULL) {
    *sid = s->Sid();
  }
  return Status::OK();
}

Status Server::onUnRegister(const rpc::Request* req)
{
  SessionPtr s;
  Status st = getSession(req->sid(), &s);
  if (!st.ok()) {
    return st;
  }
  // 来自读取日志的调用,session还是激活状态
  logger_.LogSession(s->Sid(), "close");
  s->Close();
  sm_.Remove(s->Sid());
  return st;
}

/* - 节点不存在
 * - 父节点必须存在
 * - 父节点不可以为临时节点
 */
Status Server::onCreate(const rpc::Request* req)
{
  SessionPtr s;
  Status st = getSession(req->sid(), &s);
  // 回放日志的时候，创建这个节点的session可能已经不存在了，对于临时节点，session必须存在
  if (!st.ok() && (req->create().flags() & kEphemeral)) {
    LOG(ERROR) << "create ephemeral node " << req->create().data() << "failed. reason: sid not exists. sid:" << req->sid();
    return st;
  }

  const std::string& path = req->create().path();
  st = store_->Create(path, req->create().data(), req->create().flags(), req->sid());
  if (!st.ok()) {
    return st;
  }

  if (req->create().flags() & kEphemeral) {
    CHECK(s);
    s->BindNode(path);
  }

  std::string parent, son;
  store::SplitPath(path, &parent, &son);
  wm_.Triger(watch::Event(path, watch::kCreate));
  wm_.Triger(watch::Event(parent, watch::kChildren));
  return st;
}

// 为了让session注销的时候也可以删除节点 把这部分逻辑提取出来
Status Server::DeleteNode(const std::string& path)
{
  Status st = store_->Delete(path);
  if (!st.ok()) {
    return st;
  }
  std::string parent, son;
  store::SplitPath(path, &parent, &son);
  wm_.Triger(watch::Event(path, watch::kDelete));
  wm_.Triger(watch::Event(parent, watch::kChildren));
  return Status::OK();
}

// 找不到其他地方来让session注销的时候可以获取到节点的信息
Status Server::ReadNode(const std::string& path, store::Node* node)
{
  return store_->Get(path, node);
}

Status Server::onDelete(const rpc::Request* req)
{
  store::Node node;
  const std::string& path = req->delete_().path();
  Status st = store_->Get(path, &node);
  if (!st.ok()) {
    return st;
  }
  st = DeleteNode(path);
  if (!st.ok()) {
    return st;
  }
  SessionPtr s;
  st = getSession(req->sid(), &s);
  if (!st.ok()) {
    LOG(ERROR) << "node deleted with session gone";
    return st;
  }
  s->RemoveNode(path);
  return st;
}

Status Server::onRead(const rpc::Request* req, std::string* data)
{
  CHECK(req->has_watch());
  CHECK_NOTNULL(data);
  Status st = store_->Read(req->read().path(), data);
  if (!st.ok()) {
    return st;
  }

  return onWatch(req);
}

Status Server::onWatch(const rpc::Request* req)
{
  SessionPtr s;
  Status st = getSession(req->sid(), &s);
  if (!st.ok()) {
    return st;
  }
  const rpc::WatchRequest& wreq = req->watch();
  const std::string& path = wreq.path();
  if (s->HasWatch(path)) {
    return Status::OK();
  }
  WatcherPtr w(new Watcher(
      path,
      s->Sid(),
      watch::EventType(wreq.watch()),
      wreq.recursive()));

  w->SetWatchCallback(boost::bind(
      &Server::watchCallback, this, WatcherWeakPtr(w), _1));
  wm_.Add(path, w);
  s->BindWatch(w);

  return st;
}

Status Server::onWrite(const rpc::Request* req)
{
  Status st = store_->Write(req->write().path(), req->write().data());
  if (!st.ok()) {
    return st;
  }
  wm_.Triger(watch::Event(req->write().path(), watch::kModify));
  return st;
}

Status Server::onStat(const rpc::Request* req, std::string* data)
{
  CHECK(req->has_watch());

  store::Node node;
  Status st = store_->Get(req->stat().path(), &node);
  if (!st.ok()) {
    // stat调用可以关注节点的创建
    if (!(st.IsNotFound() && req->has_watch())) {
      return st;
    }
    Status st1 = onWatch(req);
    if (!st1.ok()) {
      return st1;
    }
    return st;
  }
  CHECK_NOTNULL(data);
  node.SerializeToString(data);
  return onWatch(req);
}

void Server::watchCallback(const WatcherWeakPtr& wp, const watch::Event& e)
{
  WatcherPtr w = wp.lock();
  if (!w) {
    return;
  }

  SessionPtr s = sm_.Get(w->Sid());
  if (!s) {
    return;
  }

  // only leader needs ping back
  if (state_ == kLeader) {
    s->PendingWatcher(w->Path(), e);
    s->PingBack();
  }
  s->RemoveWatch(w);
}

}
