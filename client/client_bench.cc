#include <pyxis/client/client.h>
#include <gtest/gtest.h>
#include <boost/bind.hpp>
#include <toft/system/threading/event.h>

using namespace pyxis;

class ClientBench : public testing::Test
{
  protected:
  virtual void SetUp()
  {
    Options options;
    options.Sid = 0;
    options.Timeout = 5000;
    options.EnableCache = false;
    options.EnableDebugLog = false;
    std::string addr = "127.0.0.1:9981,127.0.0.1:9982,127.0.0.1:9983";
    Status st = Client::Open(options, addr,
                             boost::bind(&ClientBench::Callback, this, _1),
                             &cli);
    EXPECT_TRUE(st.ok()) << st.ToString();
    EXPECT_TRUE(sig_.TimedWait(5000));

    // create /bench node
    WriteOptions woptions;
    woptions.Flags = kPersist;
    st = cli->Create(woptions, "/bench", "");
    EXPECT_TRUE(st.ok() || st.IsExist()) << st.ToString();
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

class Walker
{
 public:
  Walker(int level, int size);
};

TEST_F(ClientBench, Create)
{
  const int size = 1000;

  WriteOptions options;
  options.Flags = kEphemeral;
  char name[32];
  for (int i = 0; i < size; i++) {
    snprintf(name, 32, "/bench/node%d", i);
    Status st = cli->Create(options, name, "");
    ASSERT_TRUE(st.ok()) << st.ToString();
  }
}

TEST_F(ClientBench, Read)
{
  const int size = 10000;

  WriteOptions options;
  options.Flags = kEphemeral;
  Status st = cli->Create(options, "/bench/read", "");
  ASSERT_TRUE(st.ok()) << st.ToString();
  for (int i = 0; i < size; i++) {
    pyxis::Node node;
    ReadOptions options;
    options.WatchEvent = watch::kModify;
    Status st = cli->Read(options, "/bench/read", &node);
    ASSERT_TRUE(st.ok()) << st.ToString();
  }
}
