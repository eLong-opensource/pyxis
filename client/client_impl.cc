#include <toft/base/string/algorithm.h>
#include <toft/base/string/number.h>
#include <toft/system/threading/event.h>
#include <toft/base/random.h>
#include <muduo/base/Thread.h>
#include <muduo/base/CurrentThread.h>
#include <muduo/net/EventLoop.h>
#include <boost/bind.hpp>
#include <glog/logging.h>
#include <pyxis/client/client_impl.h>
#include <pyxis/proto/rpc.pb.h>
#include <pyxis/proto/node.pb.h>
#include <pyxis/rpc/client.h>
#include <muduo/base/Logging.h>

using namespace muduo::net;

namespace pyxis {
namespace client {

static std::string randomAddr(const AddrList& addrList);

static void removeClient(boost::shared_ptr<rpc::Client>& cli,
                         toft::AutoResetEvent* done)
{
  done->Wait();
  cli->Close();
}

static void resetClient(boost::shared_ptr<rpc::Client>& cli)
{
}

void dummyOutput(const char* msg, int len)
{
}

ClientImpl::ClientImpl(const Options& options, const std::string& addrList, const WatchCallback& cb)
    : loopThread_(new EventLoopThread),
      loop_(loopThread_->startLoop()),
      state_(kClosed),
      addrList_(),
      options_(options),
      cache_(options.CacheDir),
      lastPing_(time(NULL)),
      wm_(cb),
      client_(),
      sid_(options.Sid),
      connTimerId_()
{
  toft::SplitString(addrList, ",", &addrList_);
}

ClientImpl::~ClientImpl()
{
  if (state() == kClosed) {
    return;
  }
  LOG_INFO << "~ClientImpl";
  Status st = unregister();
  if (!st.ok()) {
    LOG_ERROR << "unregister: " << st.ToString();
  }
  setState(kClosed);
  // 只是为了让rpc client在loop线程析构
  toft::AutoResetEvent done;
  loop_->queueInLoop(boost::bind(&removeClient, client_, &done));
  client_.reset();
  // 确保client_在loop线程销毁
  done.Set();

  // waiting for connection disconnection
  usleep(10 * 1000);
  // 确保loop在之前析构
  loopThread_.reset();
}

Status ClientImpl::Start()
{
  if (!options_.EnableDebugLog) {
    muduo::Logger::setOutput(dummyOutput);
  }
  if (options_.EnableCache) {
    Status st = cache_.Init();
    if (!st.ok()) {
      return Status::Internal("open cache failed:" + st.ToString());
    }
  }

  setState(kLooking);
  wm_.Start(1);

  loop_->runInLoop(boost::bind(&ClientImpl::resetConnection,
                               this, randomAddr(addrList_)));
  return Status::OK();
}

Status ClientImpl::Read(const ReadOptions& options, const std::string& path, Node* node)
{
  rpc::Request req;
  rpc::Response rep;
  pyxis::store::Node proto;

  req.mutable_stat()->set_path(path);
  if (options.WatchEvent != watch::kNone) {
    rpc::WatchRequest* w = req.mutable_watch();
    w->set_path(path);
    w->set_watch(options.WatchEvent);
    w->set_recursive(options.Recursive);
    WatcherPtr watcher(new Watcher);
    watcher->Path = path;
    watcher->Mask = watch::EventType(options.WatchEvent);
    watcher->Recursive = options.Recursive;
    watcher->Done = options.Callback;
    wm_.Add(path, watcher);
  }

  Status st = call(&req, &rep);
  if (!st.ok()) {
    // for watcher
    wm_.Remove(path);
    // for cache
    if (options_.EnableCache && st.IsDisconnect()) {
      Status st1 = cache_.Read(path, &proto);
      if (!st1.ok()) {
        LOG(ERROR) << "read cache error:" << st1.ToString();
      } else {
        if (node != NULL) {
          CopyNodeFromProto(node, proto);
        }
        st = Status::Cached(path);
      }
    }

    return st;
  }

  if (!proto.ParseFromString(rep.data())) {
    return Status::Internal("decode stat error");
  }

  if (node != NULL) {
    CopyNodeFromProto(node, proto);
  }
  if (options_.EnableCache) {
    Status st1 = cache_.Write(path, proto);
    if (!st1.ok()) {
      LOG(ERROR) << "write cache error:" << st1.ToString();
    }
  }
  return Status::OK();
}

Status ClientImpl::Write(const WriteOptions& options, const std::string& path, const std::string& data)
{
  rpc::Request req;
  rpc::Response rep;
  req.mutable_write()->set_path(path);
  req.mutable_write()->set_data(data);

  return call(&req, &rep);
}

Status ClientImpl::Create(const WriteOptions& options, const std::string& path, const std::string& data)
{
  rpc::Request req;
  rpc::Response rep;
  req.mutable_create()->set_path(path);
  req.mutable_create()->set_data(data);
  req.mutable_create()->set_flags(options.Flags);

  return call(&req, &rep);
}

Status ClientImpl::Delete(const WriteOptions& options, const std::string& path)
{
  rpc::Request req;
  rpc::Response rep;
  req.mutable_delete_()->set_path(path);
  return call(&req, &rep);
}

Status ClientImpl::Sid(sid_t* sid)
{
  if (sid == NULL) {
    return Status::InvalidArgument("sid null");
  }
  if (state() != kConnected) {
    return Status::Disconnect("connection lost");
  }
  *sid = sid_.Value();
  return Status::OK();
}

static std::string randomAddr(const AddrList& addrList)
{
  int i = toft::Random(time(NULL)).Uniform(addrList.size());
  return addrList[i];
}

Status ClientImpl::unregister()
{
  rpc::Request req;
  rpc::Response rep;
  req.mutable_unregister();
  return call(&req, &rep);
}

Status ClientImpl::call(rpc::Request* req, rpc::Response* rep)
{
  if (state() == kClosed) {
    return Status::Disconnect("closed");
  }
  toft::AutoResetEvent done;
  Status st;
  rpc::RpcCallback cb = boost::bind(
      &ClientImpl::onCall, this, &done);

  loop_->runInLoop(boost::bind(
      &ClientImpl::callInLoop, this, &st, req, rep, cb));

  done.Wait();
  if (!st.ok()) {
    return st;
  }
  return Status::FromRpcError(rep->err());
}

void ClientImpl::callInLoop(Status* st, rpc::Request* req,
                            rpc::Response* rep, const rpc::RpcCallback& cb)
{
  if (state() != kConnected) {
    *st = Status::Disconnect("not connected");
    cb();
    return;
  }
  req->set_sid(sid_);
  client_->Call(st, req, rep, cb);
}

void ClientImpl::onCall(toft::AutoResetEvent* done)
{
  done->Set();
}

void ClientImpl::onConnection(bool connected)
{
  if (connected) {
    LOG_INFO << "connected, begin register";
    loop_->cancel(connTimerId_);
    register_();
  } else {
    LOG_INFO << "connected lost";
    wm_.Triger("", watch::Event("", watch::kDisconnected));
    resetConnection(randomAddr(addrList_));
  }
}

void ClientImpl::onConnectionTimeout(const std::string& addr)
{
  LOG_ERROR << "connect to address " << addr << " timeout.";
  resetConnection(randomAddr(addrList_));
}

void ClientImpl::resetConnection(const std::string addr)
{
  loop_->assertInLoopThread();
  if (state() == kClosed) {
    LOG_INFO << "closed. not reset connection";
    return;
  }
  LOG_INFO << "reset connection to " << addr;
  // 防止在loop在处理channel事件的时候rpc client析构
  // 做延迟处理
  loop_->queueInLoop(boost::bind(
      &resetClient, client_));

  client_.reset(new rpc::Client(loop_, addr));
  client_->SetConnectionCallback(boost::bind(&ClientImpl::onConnection, this, _1));
  client_->SetCallTimeout(options_.Timeout + 1000);
  client_->Connect();
  // set connection timeout
  connTimerId_ = loop_->runAfter(0.5, boost::bind(
      &ClientImpl::onConnectionTimeout, this, addr));
}

void ClientImpl::register_()
{
  StatusPtr st(new Status);
  rpc::RequestPtr req(new rpc::Request);
  rpc::ResponsePtr rep(new rpc::Response);
  if (sid_ != 0) {
    req->set_sid(sid_);
  }
  req->mutable_register_()->set_timeout(options_.Timeout);
  client_->Call(st.get(), req.get(), rep.get(), boost::bind(
      &ClientImpl::onRegister, this, st, req, rep));
}

void ClientImpl::onRegister(StatusPtr st, rpc::RequestPtr req,
                            rpc::ResponsePtr rep)
{
  if (state_ == kClosed) {
    LOG_INFO << "closed ignore register callback";
    return;
  }
  if (st->ok() && !rep->has_err()) {
    //success
    sid_ = rep->sid();
    state_ = kConnected;
    wm_.Triger("", watch::Event("", watch::kConnected));
    LOG_INFO << "register ok. sid:" << sid_;
    ping();
    // reregister watchers.
    muduo::Thread(boost::bind(&ClientImpl::reregisterWatchers, this),
                  "registerWatcherThread").start();
    return;
  }

  if (!st->ok()) {
    LOG_ERROR << "register back. status:" << st->ToString();
  }
  if (rep->has_err()) {
    LOG_ERROR << "register rpc error:" << Status::FromRpcError(rep->err()).ToString();
  }

  std::string addr = randomAddr(addrList_);
  const rpc::Error& err = rep->err();
  if (err.type() == rpc::kNotLeader) {
    CHECK(err.has_data());
    addr = err.data();
  } else if (err.type() == rpc::kSessionExpired) {
    sid_ = 0;
    addr = client_->Addr();
    // TODO reregister watchers
  }
  resetConnection(addr);
}

void ClientImpl::ping()
{
  StatusPtr st(new Status);
  rpc::RequestPtr req(new rpc::Request);
  rpc::ResponsePtr rep(new rpc::Response);
  req->mutable_ping();
  callInLoop(st.get(), req.get(), rep.get(), boost::bind(
      &ClientImpl::onPing, this, st, req, rep));
}

void ClientImpl::onPing(StatusPtr st, RequestPtr req, ResponsePtr rep)
{
  if (state_ == kClosed) {
    LOG_INFO << "closed ignore ping callback";
    return;
  }
  if (st->ok() && !rep->has_err()) {
    if (rep->has_watch()) {
      const rpc::WatchEvent& t = rep->watch();
      watch::Event e(t.path(), watch::EventType(t.type()));
      wm_.Triger(rep->path(), e);
    }
    ping();
    return;
  }

  // ping的timeout进行重试
  if (st->IsTimeout()) {
    LOG_ERROR << "ping timeout, retry.";
    ping();
    return;
  }

  if (!st->ok()) {
    LOG_ERROR << "ping back. status:" << st->ToString();
  }
  if (rep->has_err()) {
    LOG_ERROR << "ping rpc error:" << Status::FromRpcError(rep->err()).ToString();
  }

  if (rep->err().type() == rpc::kSessionExpired) {
    // todo reregister watchers
    sid_ = 0;
    // session过期不换连接
    resetConnection(client_->Addr());
    return;
  }
  resetConnection(randomAddr(addrList_));
}

// block call
void ClientImpl::reregisterWatchers()
{
  wm_.Walk(boost::bind(&ClientImpl::watcherWalker, this, _1));
}

void ClientImpl::watcherWalker(const WatcherPtr& watcher)
{
  rpc::Request req;
  rpc::Response rep;
  req.mutable_watch()->set_path(watcher->Path);
  req.mutable_watch()->set_watch(watcher->Mask);
  req.mutable_watch()->set_recursive(watcher->Recursive);

  Status st = call(&req, &rep);
  if (!st.ok()) {
    LOG_ERROR << "reregister watcher:" << watcher->Mask << "-"
              << watcher->Path
              << " failed:" << st.ToString();
  } else {
    LOG_INFO << "reregister watcher:" << watcher->Mask << "-"
             << watcher->Path
             << " ok.";
  }
}

}
}
