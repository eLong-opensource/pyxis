#ifndef PYXIS_SERVER_SERVER_H
#define PYXIS_SERVER_SERVER_H

#include <list>
#include <muduo/base/BlockingQueue.h>
#include <muduo/base/Thread.h>
#include <boost/scoped_ptr.hpp>
#include <toft/system/atomic/atomic.h>
#include <raft/core/raft.h>
#include <pyxis/rpc/event.h>
#include <pyxis/rpc/rpc_receiver.h>
#include <pyxis/server/session_manager.h>
#include <pyxis/server/watch_manager.h>
#include <pyxis/server/options.h>
#include <pyxis/common/status.h>
#include <leveldb/options.h>
#include <leveldb/db.h>

namespace google {
namespace protobuf {
class Message;
}
}

namespace raft {
class Raft;
}

namespace pyxis {
class Session;
class EventLog;

namespace store {
class Store;
class Session;
class Node;
}

namespace rpc {
class Request;
class Response;
}

}

namespace pyxis {

class Server : public rpc::RpcReceiver
{
 public:
  Server(const Options& options, raft::Raft*);
  ~Server();

  void ReceiveEvent(const rpc::EventPtr& e);
  Status DeleteNode(const std::string& path);
  Status ReadNode(const std::string& path, store::Node* node);

  void Run();

  void Start();

  void Stop();

 private:
  enum State {
    kLeader,
    kWaiter,
    kFollower,
    kStop,
  };

  typedef Channel<int> StateChannel;
  typedef rpc::Request Command;
  typedef boost::shared_ptr<Command> CommandPtr;
  typedef Channel<CommandPtr> CommandChannel;
  typedef boost::shared_ptr<Session> SessionPtr;
  typedef boost::function<void()> Functor;
  typedef Channel<Functor> FunctorChannel;
  typedef std::list<rpc::EventPtr> RpcEventList;

  void mainLoop();
  void leaderLoop();
  void followerLoop();
  void waitLoop();

  // 向各个follower发送同步数据的请求，只有leader调用
  void replicate(const Command& cmd);

  // 大多数follower同步完日志之后的回调函数
  void onSynced(uint64_t, const std::string& data);

  // 集群状态变更之后的回调函数，这个函数会清空所有的待处理请求
  // 包括ping请求
  void onState(int state);

  // 加载快照的回调函数
  uint64_t onLoadSnapshot(uint64_t commitIndex);

  // /pyxis/snapshot虚拟节点的处理函数
  Status onProcTakeSnapshot(const store::ProcDB::ArgList& args, const std::string& value);

  // 生成快照的回调函数，会异步执行生成leveldb的快照
  void onTakeSnapshot(uint64_t commitIndex, const raft::TakeSnapshotDoneCallback& cb);

  // session过期的回调，被session manager调用
  void onSessionClosed(Session* s);

  // 工具函数，向列表里面的session返回ping请求
  void pingBack(const std::set<SessionPtr>& sessions);

  // 处理rpc的请求
  void handleEvent(rpc::EventPtr&);
  void handleRegister(rpc::EventPtr&);
  Status handlePing(rpc::EventPtr&);
  Status handleRead(rpc::EventPtr&);
  Status handleStat(rpc::EventPtr&);

  // 工具函数，返回session
  Status getSession(sid_t sid, SessionPtr* s);

  // 日志同步之后的回调函数，leader和follower都会执行
  void onCommand(CommandPtr&);
  Status onRegister(const rpc::Request* req, sid_t* sid);
  Status onUnRegister(const rpc::Request* req);
  Status onCreate(const rpc::Request* rep);
  Status onDelete(const rpc::Request* req);
  Status onRead(const rpc::Request* req, std::string* data);
  Status onWrite(const rpc::Request* req);
  Status onStat(const rpc::Request* req, std::string* data);
  Status onWatch(const rpc::Request* req);

  // watcher触发之后的回调函数
  void watchCallback(const WatcherWeakPtr& w,
                     const watch::Event& e);


  // 遍历持久化session的处理函数
  void walkSessionFunc(const store::Session& s);

  // 遍历所有节点的处理函数
  void walkTreeFunc(const std::string& p, const store::Node& node);

  // /pyxis/cluster处理函数
  Status onProcCluster(const store::ProcDB::ArgList& args, std::string* value);


  Options options_;

  // levledb db store
  leveldb::DB* db_;

  State state_;

  // 已经广播,尚未返回的quoram请求
  RpcEventList requests_;

  // 待处理的rpc请求
  rpc::EventChannel eventch_;

  // 已经返回的quoram决议的请求 待处理
  CommandChannel cmdch_;

  // 状态转换channel
  StateChannel statech_;

  // task channel
  FunctorChannel taskch_;

  raft::Raft* raft_;

  boost::scoped_ptr<store::Store> store_;

  SessionManager sm_;

  WatchManager wm_;

  // raft log index
  toft::Atomic<uint64_t> logIndex_;

  // event logger
  EventLog& logger_;

  // main thread
  muduo::Thread thread_;
};

}

#endif
