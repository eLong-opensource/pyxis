#include <pyxis/client/client.h>
#include <gtest/gtest.h>
#include <iostream>
#include <boost/bind.hpp>
#include <toft/system/threading/event.h>
#include <pyxis/common/node.h>
#include <muduo/net/EventLoop.h>
#include <muduo/base/Thread.h>

using namespace pyxis;

class WatchExpecter
{
 public:
  WatchExpecter(const watch::Event& e, int timeout)
      : e_(e),
        timeout_(timeout)
  {
  }

  void Callback(const watch::Event& e)
  {
    if (e == e_) {
      sig_.Set();
    }
  }

  bool Wait()
  {
    return sig_.TimedWait(timeout_);
  }

 private:
  watch::Event e_;
  int timeout_;
  toft::AutoResetEvent sig_;
};

class ClientTest : public testing::Test
{
 protected:
  virtual void SetUp()
  {
    Options options;
    options.Sid = 0;
    options.Timeout = 5000;
    options.EnableCache = false;
    options.EnableDebugLog = true;
    std::string addr = "127.0.0.1:9981";
    Status st = Client::Open(options, addr,
                             boost::bind(&ClientTest::Callback, this, _1),
                             &cli);
    EXPECT_TRUE(st.ok()) << st.ToString();
    EXPECT_TRUE(sig_.TimedWait(5000));
  }

  virtual void TearDown()
  {
    if (cli != NULL) {
      delete cli;
    }
  }

  void Callback(const watch::Event& e)
  {
    if (e.Type == watch::kConnected) {
      sig_.Set();
    }
  }

  Client* cli;
 private:
  toft::AutoResetEvent sig_;
};

TEST_F(ClientTest, open)
{
}

TEST_F(ClientTest, notfound)
{
  Status st = cli->Delete(WriteOptions(), "/not_found_node");
  EXPECT_TRUE(st.IsNotFound()) << st.ToString();

  st = cli->Write(WriteOptions(), "/not_found_node", "");
  EXPECT_TRUE(st.IsNotFound()) << st.ToString();

  st = cli->Read(ReadOptions(), "/not_found_node", NULL);
  EXPECT_TRUE(st.IsNotFound()) << st.ToString();
}

void testPath(Client* cli, const std::string& path)
{
  WriteOptions options;
  options.Flags = kEphemeral;
  Status st = cli->Create(options, path, "");
  EXPECT_TRUE(st.IsInvalidArgument()) << st.ToString();
}

TEST_F(ClientTest, invalidpath)
{
  // empty path
  testPath(cli, "");
  // special path
  testPath(cli, "/");
  testPath(cli, "/pyxis");

  // not start with /
  testPath(cli, "bad_path");
  testPath(cli, "bad/path");
  // invalid char
  testPath(cli, "/#$$%");
  // tail /
  testPath(cli, "/aos/apus/");
  // max path size
  testPath(cli, "/" + std::string(4096, 'a'));
}

TEST_F(ClientTest, exist)
{
  WriteOptions options;
  options.Flags = kEphemeral;
  Status st = cli->Create(options, "/aos_test", "");
  EXPECT_TRUE(st.ok()) << st.ToString();
  st = cli->Create(options, "/aos_test", "");
  EXPECT_TRUE(st.IsExist()) << st.ToString();
}

TEST_F(ClientTest, nodesize)
{
  WriteOptions options;
  options.Flags = kEphemeral;
  std::string data(2 * 1024 * 1024, 'a');
  Status st = cli->Create(options, "/aos_test", data);
  std::cout << st.ToString() << std::endl;
  EXPECT_TRUE(st.IsInvalidArgument()) << st.ToString();

  cli->Write(WriteOptions(), "/aos_test", data);
  std::cout << st.ToString() << std::endl;
  EXPECT_TRUE(st.IsInvalidArgument()) << st.ToString();
}

TEST_F(ClientTest, session)
{
  sid_t sid = 0;
  {
    EXPECT_TRUE(cli->Sid(&sid).ok());
    EXPECT_NE(sid, 0);
    std::cout << sid << std::endl;
  }

  // sleep 5 seconds
  usleep(1000 * 1000 * 5);

  WatchExpecter we(watch::Event("", watch::kConnected), 1000);
  Client* cli1 = NULL;
  Options options;
  options.Sid = sid;
  std::string addr = "127.0.0.1:9981";
  Status st = Client::Open(options, addr,
                           boost::bind(&WatchExpecter::Callback, &we, _1),
                           &cli1);
  EXPECT_TRUE(we.Wait());
  EXPECT_TRUE(st.ok()) << st.ToString();

  sid_t sid1 = 0;
  EXPECT_TRUE(cli1->Sid(&sid1).ok());
  EXPECT_EQ(sid1, sid);
  std::cout << sid1 << std::endl;
  delete cli1;
}

TEST_F(ClientTest, create)
{
  WriteOptions options;
  options.Flags = kEphemeral;
  Status st = cli->Create(options, "/aos_test", "");
  EXPECT_TRUE(st.ok()) << st.ToString();
  sid_t sid = 0;
  EXPECT_TRUE(cli->Sid(&sid).ok());
  EXPECT_NE(sid, 0);
}

TEST_F(ClientTest, delete)
{
  WriteOptions options;
  options.Flags = kEphemeral;
  Status st = cli->Create(options, "/aos_test", "");
  EXPECT_TRUE(st.ok()) << st.ToString();
  st = cli->Delete(options, "/aos_test");
  EXPECT_TRUE(st.ok()) << st.ToString();
}

TEST_F(ClientTest, write_read)
{
  WriteOptions options;
  options.Flags = kEphemeral;
  Status st = cli->Create(options, "/aos_test", "");
  EXPECT_TRUE(st.ok()) << st.ToString();
  st = cli->Write(options, "/aos_test", "hello");
  EXPECT_TRUE(st.ok()) << st.ToString();
  pyxis::Node node;
  st = cli->Read(ReadOptions(), "/aos_test", &node);
  EXPECT_TRUE(st.ok()) << st.ToString();
  EXPECT_EQ("hello", node.Data);
}

TEST_F(ClientTest, stat)
{
  WriteOptions options;
  options.Flags = kEphemeral;
  Status st = cli->Create(options, "/aos_test", "");
  EXPECT_TRUE(st.ok()) << st.ToString();
  Node node;
  st = cli->Read(ReadOptions(), "/", &node);
  EXPECT_TRUE(st.ok()) << st.ToString();
  std::vector<std::string>& children = node.Children;
  EXPECT_NE(children.size(), 0);
  EXPECT_FALSE(std::find(children.begin(),
                         children.end(),
                         "aos_test") == node.Children.end());
  for (size_t i =0; i<children.size(); i++) {
    std::cout << children[i] << std::endl;
  }
}

TEST_F(ClientTest, create_watch)
{
  ReadOptions roptions;
  WatchExpecter we(watch::Event("/create_watch", watch::kCreate), 1000);
  roptions.WatchEvent = watch::kCreate;
  roptions.Recursive = true;
  roptions.Callback = boost::bind(&WatchExpecter::Callback, &we, _1);
  Status st = cli->Read(roptions, "/", NULL);
  EXPECT_TRUE(st.ok()) << st.ToString();

  WriteOptions woptions;
  woptions.Flags = kEphemeral;
  st = cli->Create(woptions, "/create_watch", "");
  EXPECT_TRUE(st.ok()) << st.ToString();
  EXPECT_TRUE(we.Wait());
}

TEST_F(ClientTest, delete_watch)
{
  WriteOptions woptions;
  woptions.Flags = kEphemeral;
  Status st = cli->Create(woptions, "/aos_test", "");
  EXPECT_TRUE(st.ok()) << st.ToString();

  ReadOptions roptions;
  WatchExpecter we(watch::Event("/aos_test", watch::kDelete), 1000);
  roptions.WatchEvent = watch::kDelete;
  roptions.Callback = boost::bind(&WatchExpecter::Callback, &we, _1);
  st = cli->Read(roptions, "/aos_test", NULL);
  EXPECT_TRUE(st.ok()) << st.ToString();

  st = cli->Delete(WriteOptions(), "/aos_test");
  EXPECT_TRUE(st.ok()) << st.ToString();
  EXPECT_TRUE(we.Wait());
}

TEST_F(ClientTest, modify_watch)
{
  WriteOptions woptions;
  woptions.Flags = kEphemeral;
  Status st = cli->Create(woptions, "/aos_test", "");
  EXPECT_TRUE(st.ok()) << st.ToString();

  ReadOptions roptions;
  WatchExpecter we(watch::Event("/aos_test", watch::kModify), 1000);
  roptions.WatchEvent = watch::kModify;
  roptions.Callback = boost::bind(&WatchExpecter::Callback, &we, _1);
  st = cli->Read(roptions, "/aos_test", NULL);
  EXPECT_TRUE(st.ok()) << st.ToString();

  st = cli->Write(WriteOptions(), "/aos_test", "");
  EXPECT_TRUE(st.ok()) << st.ToString();
  EXPECT_TRUE(we.Wait());
}

TEST_F(ClientTest, children_watch)
{
  ReadOptions roptions;
  WatchExpecter we(watch::Event("/", watch::kChildren), 1000);
  roptions.WatchEvent = watch::kChildren;
  roptions.Callback = boost::bind(&WatchExpecter::Callback, &we, _1);
  Status st = cli->Read(roptions, "/", NULL);
  EXPECT_TRUE(st.ok()) << st.ToString();

  WriteOptions woptions;
  woptions.Flags = kEphemeral;
  st = cli->Create(woptions, "/aos_test", "");
  EXPECT_TRUE(st.ok()) << st.ToString();
  EXPECT_TRUE(we.Wait());
}


TEST_F(ClientTest, prefix_watch)
{
  WriteOptions woptions;
  woptions.Flags = kEphemeral;
  Status st = cli->Create(woptions, "/prefix_watch", "");
  EXPECT_TRUE(st.ok()) << st.ToString();

  ReadOptions roptions;
  WatchExpecter we(watch::Event("/prefix_watch", watch::kModify), 1000);
  roptions.WatchEvent = watch::kAll;
  roptions.Recursive = true;
  roptions.Callback = boost::bind(&WatchExpecter::Callback, &we, _1);
  st = cli->Read(roptions, "/", NULL);
  EXPECT_TRUE(st.ok()) << st.ToString();

  st = cli->Write(WriteOptions(), "/prefix_watch", "");
  EXPECT_TRUE(st.ok()) << st.ToString();
  EXPECT_TRUE(we.Wait());
}

TEST_F(ClientTest, prefix_watch_filter)
{
  WriteOptions woptions;
  woptions.Flags = kEphemeral;
  Status st = cli->Create(woptions, "/prefix_watch_filter", "");
  EXPECT_TRUE(st.ok()) << st.ToString();

  ReadOptions roptions;
  WatchExpecter we(watch::Event("/prefix_watch_filter", watch::kModify), 1000);
  roptions.WatchEvent = watch::kCreate;
  roptions.Recursive = true;
  roptions.Callback = boost::bind(&WatchExpecter::Callback, &we, _1);
  st = cli->Read(roptions, "/", NULL);
  EXPECT_TRUE(st.ok()) << st.ToString();

  st = cli->Write(WriteOptions(), "/prefix_watch_filter", "");
  EXPECT_TRUE(st.ok()) << st.ToString();
  EXPECT_FALSE(we.Wait());
}

class SomeService
{
 public:
  SomeService(const std::string& name, const std::string& lockNode)
      : cli_(NULL),
        lockNode_(lockNode),
        name_(name),
        onConnect_(watch::Event("", watch::kConnected), 2000)
  {
  }

  ~SomeService()
  {
    if (cli_ != NULL) {
      delete cli_;
    }
    std::cout << name_ << " done" << std::endl;
  }

  void Start()
  {
    muduo::Thread thread(boost::bind(&SomeService::Run, this),
                         name_.c_str());
    thread.start();
  }

  void Run()
  {
    std::cout << "Service " << name_ << " started" << std::endl;
    Options options;
    options.EnableCache = false;
    options.Timeout = 5000;
    std::string addr = "127.0.0.1:9981";
    Status st = Client::Open(options, addr,
                             boost::bind(&WatchExpecter::Callback, &onConnect_, _1),
                             &cli_);
    EXPECT_TRUE(st.ok()) << st.ToString();
    EXPECT_TRUE(onConnect_.Wait());
    // triger an elect
    onLeaderDown(watch::Event());
  }

  void Crash()
  {
    std::cout << "Service " << name_ << " crash" << std::endl;
    delete cli_;
    cli_ = NULL;
  }

  void onLeaderDown(const watch::Event& e)
  {
    std::cout << "Service " << name_ << " begin electing" << std::endl;
    WriteOptions options;
    options.Flags = kEphemeral;
    Status st = cli_->Create(options, lockNode_, name_);
    EXPECT_TRUE(st.ok() || st.IsExist()) << st.ToString();
    if (st.ok()) {
      std::cout << "I'm leader: " << name_ << std::endl;
    }
    if (st.IsExist()) {
      ReadOptions options;
      options.WatchEvent = watch::kDelete;
      options.Callback = boost::bind(&SomeService::onLeaderDown, this, _1);
      pyxis::Node leader;
      st = cli_->Read(options, lockNode_, &leader);
      EXPECT_TRUE(st.ok()) << st.ToString();
      std::cout << "leader: " << leader.Data << std::endl;
    }
  }

 private:
  Client* cli_;
  std::string lockNode_;
  std::string name_;
  WatchExpecter onConnect_;
};

void quit(muduo::net::EventLoop* loop)
{
  std::cout << "loop quit" << std::endl;
  loop->quit();
}

TEST(example, leader)
{
  muduo::net::EventLoop loop;
  SomeService a("ServiceA", "/leader");
  SomeService b("ServiceB", "/leader");
  SomeService c("ServiceC", "/leader");

  loop.runAfter(1, boost::bind(&SomeService::Crash, &b));
  loop.runAfter(1.5, boost::bind(&SomeService::Start, &c));
  loop.runAfter(4, boost::bind(&quit, &loop));

  a.Start();
  b.Start();

  loop.loop();

}
