#include <pyxis/rpc/server.h>
#include <pyxis/rpc/client.h>
#include <muduo/net/EventLoopThread.h>
#include <muduo/net/EventLoop.h>
#include <pyxis/proto/rpc.pb.h>
#include <gtest/gtest.h>

using namespace muduo::net;
using namespace pyxis;

class MockServer : public rpc::RpcReceiver
{
 public:
  MockServer(EventLoop* loop, std::string addr)
      : loop_(loop),
        server_(new rpc::Server(loop, addr, this))
  {
  }

  ~MockServer()
  {
    Stop();
  }

  void ReceiveEvent(const rpc::EventPtr& e)
  {
    // auto done
    if (e->Req->has_ping()) {
      loop_->runAfter(1, boost::bind(&MockServer::sleep, this, e));
    }
    if (e->Req->has_create()) {
      e->Rep->set_data(e->Req->create().path());
    }
  }

  void Start() {
    loop_->runInLoop(boost::bind(
        &rpc::Server::Start, server_));
  }

  void Stop() {
    loop_->runInLoop(boost::bind(
        &MockServer::del, this, server_));
  }

 private:
  void del(rpc::Server* ptr) { delete ptr; }
  void sleep(const rpc::EventPtr& e) {}
  EventLoop* loop_;
  rpc::Server* server_;
};

TEST(client, conn_timeout)
{
  EventLoopThread thread;
  EventLoop* loop = thread.startLoop();
  loop->runAfter(1, boost::bind(
      &EventLoop::quit, loop));
  rpc::Client client(loop, ":45001");
  Status st = client.Connect(0.02);
  EXPECT_TRUE(st.IsTimeout()) << st.ToString();
}

TEST(client, call_timeout)
{
  EventLoopThread thread;
  EventLoop* loop = thread.startLoop();
  loop->runAfter(2, boost::bind(
      &EventLoop::quit, loop));

  MockServer server(loop, ":45001");
  server.Start();

  rpc::Client client(loop, ":45001");
  client.SetCallTimeout(0.5);
  Status st = client.Connect(0.02);
  EXPECT_TRUE(st.ok()) << st.ToString();

  rpc::Request req;
  rpc::Response rep;
  // request block;
  req.mutable_ping();
  st = client.Call(&req, &rep);
  EXPECT_TRUE(st.IsTimeout()) << st.ToString();
}

void done(Status* st, rpc::Request* req, rpc::Response* rep,
          toft::AutoResetEvent* event)
{
  EXPECT_TRUE(st->ok()) << st->ToString();
  EXPECT_EQ(req->create().path(), rep->data());
  event->Set();
  delete st;
  delete req;
  delete rep;
}

TEST(client, async)
{
  EventLoopThread thread;
  EventLoop* loop = thread.startLoop();
  loop->runAfter(2, boost::bind(
      &EventLoop::quit, loop));

  MockServer server(loop, ":45001");
  server.Start();

  rpc::Client client(loop, ":45001");
  client.SetCallTimeout(0.5);
  Status st = client.Connect(0.02);
  EXPECT_TRUE(st.ok()) << st.ToString();

  Status* stptr = new Status;
  rpc::Request* req = new rpc::Request;
  rpc::Response* rep = new rpc::Response;
  req->mutable_create()->set_path("async");
  req->mutable_create()->set_flags(0);
  toft::AutoResetEvent event;
  client.Call(stptr, req, rep,
              boost::bind(&done, stptr, req, rep, &event));
  EXPECT_TRUE(event.TimedWait(1000));
}
