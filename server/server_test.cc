#include <sys/stat.h>
#include <time.h>
#include <gtest/gtest.h>
#include <leveldb/db.h>

#include <muduo/base/Singleton.h>
#include <muduo/base/Condition.h>

#include <boost/any.hpp>

#include <pyxis/common/event_log.h>
#include <pyxis/common/watch_event.h>
#include <pyxis/common/node.h>

#include <pyxis/server/server.h>
#include <pyxis/server/fake_raft.h>

#include <pyxis/rpc/event.h>

#include <pyxis/proto/rpc.pb.h>

using namespace pyxis;
using namespace muduo;

struct CallData
{
  boost::shared_ptr<rpc::Request> req;
  boost::shared_ptr<rpc::Response> rep;
};

class Future
{
public:
  Future(): cond_(mutex_), done_(false) {}
  ~Future(){}
  void Done() {
    MutexLockGuard lock(mutex_);
    done_ = true;
    cond_.notifyAll();
  }

  bool Wait(int seconds = 0) {
    MutexLockGuard lock(mutex_);
    bool succ = true;
    while (!done_)
    {
      if (seconds == 0) {
        cond_.wait();
      } else {
        succ = cond_.waitForSeconds(seconds);
        return !succ;
      }
    }
    return succ;
  }

  void SetContext(const boost::any& context) {
    context_ = context;
  }

  const boost::any& Context() {
    return context_;
  }

private:
  muduo::MutexLock mutex_;
  muduo::Condition cond_;
  bool done_;
  boost::any context_;
};

typedef boost::shared_ptr<Future> FuturePtr;

void callDone(FuturePtr f)
{
  f->Done();
}

class ServerTest : public testing::Test
{
 protected:
  virtual void SetUp()
  {
    Start();
  }

  virtual void TearDown()
  {
    ASSERT_TRUE(server_ != NULL);
    ASSERT_TRUE(raft_ != NULL);
    Stop();
    delete server_;
    delete raft_;
    leveldb::DestroyDB(options_.DBDir, leveldb::Options());
  }

  void Start()
  {
    ::mkdir("log", 0755);
    options_.DBDir = "/tmp/pyxis_server_test_db";
    options_.MinSessionTimeout = 2000;
    raft_ = new FakeRaft();
    server_ = new Server(options_, raft_);
    raft_->Start();
    server_->Start();
    ::usleep(10000);
  }

  void Stop()
  {
    server_->Stop();
  }

  // Restart 重启服务但不删除数据库
  void Restart()
  {
    Stop();
    delete server_;
    delete raft_;
    Start();
  }

  FuturePtr Go(rpc::Request* req, rpc::Response* rep)
  {
    FuturePtr f(new Future);
    {
      rpc::EventPtr e(new rpc::Event(req, rep, google::protobuf::NewCallback(&callDone, f), ""));
      server_->ReceiveEvent(e);
    }
    return f;
  }

  void Call(rpc::Request* req, rpc::Response* rep, int timeout = 10)
  {
    FuturePtr f = Go(req, rep);
    EXPECT_TRUE(f->Wait(timeout));
  }

  sid_t Register(sid_t sid = 0, int timeout = 0, int err = rpc::kNone)
  {
    rpc::Request req;
    rpc::Response rep;
    if (sid != 0) {
      req.set_sid(sid);
    }
    req.set_xid(0);
    rpc::RegisterRequest* r = req.mutable_register_();
    r->set_timeout(timeout);
    Call(&req, &rep);
    EXPECT_EQ(err, rep.err().type()) << rep.err().data();
    return rep.sid();
  }

  void UnRegister(sid_t sid)
  {
    rpc::Request req;
    rpc::Response rep;
    req.set_sid(sid);
    req.set_xid(0);
    req.mutable_unregister();
    Call(&req, &rep);
    EXPECT_FALSE(rep.has_err()) << rep.err().data();
  }

  void Ping(sid_t sid, int err = rpc::kNone)
  {
    rpc::Request req;
    rpc::Response rep;
    req.set_sid(sid);
    req.set_xid(0);
    req.mutable_ping();
    Call(&req, &rep);
    EXPECT_EQ(err, rep.err().type()) << rep.err().data();
  }

  void Create(sid_t sid, const std::string& path,
              const std::string& data, int flags,
              int err = rpc::kNone)
  {
    rpc::Request req;
    rpc::Response rep;
    req.set_sid(sid);
    req.set_xid(0);
    rpc::CreateRequest* c = req.mutable_create();
    c->set_path(path);
    c->set_data(data);
    c->set_flags(flags);
    Call(&req, &rep);
    EXPECT_EQ(err, rep.err().type()) << rep.err().data();
  }

  void Delete(sid_t sid, const std::string& path,
              int err = rpc::kNone)
  {
    rpc::Request req;
    rpc::Response rep;
    req.set_sid(sid);
    req.set_xid(0);
    rpc::DeleteRequest* c = req.mutable_delete_();
    c->set_path(path);
    Call(&req, &rep);
    EXPECT_EQ(err, rep.err().type()) << rep.err().data();
  }

  void Stat(sid_t sid, const std::string& p,
            std::string* data, int err = rpc::kNone)
  {
    rpc::Request req;
    rpc::Response rep;
    req.set_sid(sid);
    req.set_xid(0);
    rpc::StatRequest* c = req.mutable_stat();
    c->set_path(p);
    Call(&req, &rep);
    ASSERT_EQ(err, rep.err().type()) << rep.err().data();
    if (data != NULL) {
      *data = rep.data();
    }
  }

  void Write(sid_t sid, const std::string& p,
            const std::string& data, int err = rpc::kNone)
  {
    rpc::Request req;
    rpc::Response rep;
    req.set_sid(sid);
    req.set_xid(0);
    rpc::WriteRequest* c = req.mutable_write();
    c->set_path(p);
    c->set_data(data);
    Call(&req, &rep);
    EXPECT_EQ(err, rep.err().type()) << rep.err().data();
  }

  FuturePtr Watch(sid_t sid, const std::string& p,
                  watch::EventType type, bool isRecursive)
  {

    {
      rpc::Request req;
      rpc::Response rep;
      req.set_sid(sid);
      rpc::StatRequest* s = req.mutable_stat();
      s->set_path(p);
      rpc::WatchRequest* w = req.mutable_watch();
      w->set_path(p);
      w->set_watch(type);
      w->set_recursive(isRecursive);
      Call(&req, &rep);
      EXPECT_TRUE(rep.err().type() == rpc::kNone ||
                  rep.err().type() == rpc::kNotFound) << rep.err().data();
    }
    {
      rpc::Request* req = new rpc::Request;
      rpc::Response* rep = new rpc::Response;
      req->set_sid(sid);
      req->mutable_ping();
      CallData data;
      data.req.reset(req);
      data.rep.reset(rep);
      FuturePtr f = Go(req, rep);
      f->SetContext(data);
      return f;
    }
  }

  void WaitWatch(FuturePtr f, const std::string& p,
                 watch::EventType type, int err = rpc::kNone)
  {
    ASSERT_TRUE(f->Wait(3)) << "wait watch timeout";
    CallData data = boost::any_cast<CallData>(f->Context());
    rpc::Response* rep = data.rep.get();
    ASSERT_EQ(err, rep->err().type());
    if (err == rpc::kNone) {
      ASSERT_TRUE(rep->has_watch());
      EXPECT_EQ(p, rep->watch().path());
      EXPECT_EQ(type, rep->watch().type());
    }
  }

  Options options_;
  Server* server_;
  FakeRaft* raft_;
};

TEST_F(ServerTest, SessionRestart)
{
  sid_t sid = Register();
  Restart();
  sid_t sid1 = Register(sid);
  EXPECT_EQ(sid1, sid);
}

TEST_F(ServerTest, PingExpired)
{
  Ping(1, rpc::kSessionExpired);
  sid_t sid = Register();
  Ping(sid);
  ::sleep(2);
  Ping(sid, rpc::kSessionExpired);
}

TEST_F(ServerTest, Register)
{
  sid_t sid = Register();
  Ping(sid);
}

TEST_F(ServerTest, UnRegister)
{
  sid_t sid = Register();
  Ping(sid);
  UnRegister(sid);
  Ping(sid, rpc::kSessionExpired);
}

TEST_F(ServerTest, ClientRestart)
{
  sid_t sid = Register();
  rpc::Request req;
  rpc::Response rep;
  req.set_sid(sid);
  req.set_xid(0);
  req.mutable_ping();
  FuturePtr f = Go(&req, &rep);

  sid_t sid1 = Register(sid);
  ASSERT_EQ(sid, sid1);
  Ping(sid1);

  EXPECT_TRUE(f->Wait(1));
}

TEST_F(ServerTest, Watcher)
{
  /*
    1. 创建/parent (注册watcher之前)
    2. 创建/parent/node
    3. 修改/parent/node
    4. 删除/parent/node
  */
  struct testCase {
    sid_t sid;
    std::string watchPath;
    watch::EventType watchType;
    bool isRecursive;
    std::string expectPath;
    watch::EventType expectType;
    FuturePtr done;
  } table[] = {
    // 创建/parent/node触发的事件
    {Register(), "/parent/node", watch::kCreate, false, "/parent/node", watch::kCreate},
    {Register(), "/parent/node", watch::kAll, false, "/parent/node", watch::kCreate},
    {Register(), "/parent", watch::kChildren, false, "/parent", watch::kChildren},
    {Register(), "/parent/node", watch::kCreate, true, "/parent/node", watch::kCreate},

    // 修改/parent/node触发的事件
    {Register(), "/parent/node", watch::kModify, false, "/parent/node", watch::kModify},

    // 删除/parent/node触发的事件
    {Register(), "/parent/node", watch::kDelete, false, "/parent/node", watch::kDelete},
  };

  int N = sizeof(table) / sizeof(testCase);

  sid_t sid = Register();
  // 创建/parent节点
  Create(sid, "/parent", "", 0);

  // 添加监听
  for (int i=0; i<N; i++) {
    struct testCase& t = table[i];
    t.done = Watch(t.sid, t.watchPath, t.watchType, t.isRecursive);
  }

  // 创建/parent/node节点
  Create(sid, "/parent/node", "", 0);

  // 修改/parent/node节点
  Write(sid, "/parent/node", "tmp");

  // 删除/parent/node节点
  Delete(sid, "/parent/node");

  for (int i=0; i<N; i++) {
    struct testCase& t = table[i];
    WaitWatch(t.done, t.expectPath, t.expectType);
  }
}

// 测试临时节点与会话挂钩
TEST_F(ServerTest, EphemeralAutoDelete)
{
  sid_t sid = Register();
  Create(sid, "/tmp", "", kEphemeral);
  UnRegister(sid);
  Stat(Register(), "/tmp", NULL, rpc::kNotFound);
}

// 测试临时节点不能创建子节点
TEST_F(ServerTest, EphemeralFather)
{
  sid_t sid = Register();
  Create(sid, "/tmp", "", kEphemeral);
  Create(sid, "/tmp/node", "", kPersist, rpc::kInvalidArgument);
}
