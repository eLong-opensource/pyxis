#include <pyxis/server/store.h>
#include <pyxis/server/proc_db.h>
#include <pyxis/server/session.h>
#include <gtest/gtest.h>
#include <muduo/base/Timestamp.h>
#include <google/profiler.h>
#include <boost/bind.hpp>
#include <stdint.h>

using namespace pyxis;
using namespace pyxis::store;
using namespace muduo;

static const char* gDBName = "/tmp/pyxis_store_test";

class StoreTest : public testing::Test
{
 protected:
  void Load(const std::string& dbpath)
  {
    dbpath_ = dbpath;
    leveldb::DB* persist = OpenLevelDB(dbpath);
    ProcDB* proc = new ProcDB("/pyxis");
    store_ = new Store(persist, proc, "/pyxis");
    ASSERT_TRUE(store_ != NULL);
  }

  void Destroy()
  {
    delete store_;
    leveldb::DestroyDB(dbpath_, leveldb::Options());
    store_ = NULL;
  }

  virtual void SetUp()
  {
    Load(gDBName);
  }

  virtual void TearDown()
  {
    Destroy();
  }
  std::string dbpath_;
  Store* store_;
};

TEST(Store, IsValidPath)
{
  EXPECT_FALSE(IsValidPath(""));
  EXPECT_FALSE(IsValidPath("aos"));
  EXPECT_FALSE(IsValidPath("aos/hello"));
  EXPECT_FALSE(IsValidPath("!@#$%"));
  EXPECT_FALSE(IsValidPath("/!@#$%"));
  EXPECT_FALSE(IsValidPath("/ab#"));
  EXPECT_FALSE(IsValidPath("aos-hello"));
  std::string maxPath(kMaxPathSize, 'a');
  maxPath = "/" + maxPath;
  EXPECT_FALSE(IsValidPath(maxPath));

  EXPECT_TRUE(IsValidPath("/"));
  EXPECT_TRUE(IsValidPath("/aos"));
  EXPECT_TRUE(IsValidPath("/aos/hello"));
  EXPECT_TRUE(IsValidPath("/aos/hello_world"));
  EXPECT_TRUE(IsValidPath("/aos/hello-world"));
}

void testPath(const std::string& path,
              const std::string& expectParent,
              const std::string& expectSon)
{
  std::string parent, son;
  EXPECT_TRUE(SplitPath(path, &parent, &son).ok());
  EXPECT_EQ(expectParent, parent);
  EXPECT_EQ(expectSon, son);
}

TEST(Store, SplitPath)
{
  std::string parent, son;
  EXPECT_FALSE(SplitPath("", &parent, &son).ok());
  EXPECT_FALSE(SplitPath("aos", &parent, &son).ok());
  EXPECT_FALSE(SplitPath("/", &parent, &son).ok());
  EXPECT_FALSE(SplitPath("/aos/", &parent, &son).ok());

  testPath("/aos", "/", "aos");
  testPath("/aos/hello", "/aos", "hello");
}

TEST(Store, AddChild)
{
  store::Node node;
  NewNode(&node, "aos", kPersist);
  EXPECT_EQ(node.children().size(), 0);
  ASSERT_TRUE(AddChild(&node, "son").ok());
  ASSERT_EQ(node.children().size(), 1);
  ASSERT_EQ(node.children(0), "son");
}

TEST(Store, RemoveChild)
{
  store::Node node;
  NewNode(&node, "aos", kPersist);
  EXPECT_FALSE(RemoveChild(&node, "son").ok());
  EXPECT_TRUE(AddChild(&node, "son").ok());
  EXPECT_TRUE(RemoveChild(&node, "son").ok());
  EXPECT_EQ(node.children().size(), 0);
}

TEST_F(StoreTest, PutAndGet)
{
  store::Node node;
  NewNode(&node, "aos", kEphemeral);
  // invalid path
  EXPECT_FALSE(store_->Put("aos", node).ok());

  EXPECT_TRUE(store_->Put("/aos", node).ok());

  store::Node n;
  EXPECT_TRUE(store_->Get("/aos", &n).ok());
  EXPECT_EQ(node.data(), "aos");
  EXPECT_EQ(node.children().size(), 0);
  EXPECT_EQ(node.flags(), kEphemeral);
  EXPECT_EQ(node.version(), 0);
}

TEST_F(StoreTest, CreateInvalidPath)
{
  EXPECT_FALSE(store_->Create("aos", "", kEphemeral).ok());
}

TEST_F(StoreTest, CreateParentNotExists)
{
  EXPECT_FALSE(store_->Create("/aos/status", "nil", kEphemeral).ok());
}

TEST_F(StoreTest, CreateParentNotPersist)
{
  Status st;
  st = store_->Create("/aos", "", kEphemeral);
  ASSERT_TRUE(st.ok()) << st.ToString();
  st = store_->Create("/aos/status", "nil", kPersist);
  EXPECT_FALSE(st.ok()) << st.ToString();
}

TEST_F(StoreTest, CreateDuplicate)
{
  Status st;
  st = store_->Create("/aos", "", kEphemeral);
  ASSERT_TRUE(st.ok()) << st.ToString();
  st = store_->Create("/aos", "", kPersist);
  EXPECT_FALSE(st.ok()) << st.ToString();
}

TEST_F(StoreTest, CreateSuccess)
{
  Status st;
  st = store_->Create("/aos", "aos", kEphemeral);
  ASSERT_TRUE(st.ok()) << st.ToString();
  store::Node node;
  st = store_->Get("/aos", &node);
  ASSERT_TRUE(st.ok()) << st.ToString();

  EXPECT_EQ(node.data(), "aos");
  EXPECT_EQ(node.flags(), kEphemeral);
  EXPECT_EQ(node.children().size(), 0);
  EXPECT_EQ(node.version(), 0);
}

TEST_F(StoreTest, DeleteInvalidPath)
{
  EXPECT_FALSE(store_->Delete("aos").ok());
}

TEST_F(StoreTest, DeleteNodeNotExists)
{
  EXPECT_FALSE(store_->Delete("/aos").ok());
}

TEST_F(StoreTest, DeleteParentNotExists)
{
  store::Node node;
  NewNode(&node, "nil", kPersist);
  store_->Put("/aos/status", node);
  EXPECT_FALSE(store_->Delete("/aos/status").ok());
}

TEST_F(StoreTest, DeleteHasChildren)
{
  ASSERT_TRUE(store_->Create("/aos", "", kPersist).ok());
  ASSERT_TRUE(store_->Create("/aos/status", "", kPersist).ok());
  Status st;
  st = store_->Delete("/aos");
  EXPECT_FALSE(st.ok()) << st.ToString();
  std::cerr << st.ToString() << std::endl;
}

TEST_F(StoreTest, DeleteSuccess)
{
  ASSERT_TRUE(store_->Create("/aos", "", kPersist).ok());
  ASSERT_TRUE(store_->Create("/aos/status", "", kPersist).ok());
  ASSERT_TRUE(store_->Delete("/aos/status").ok());
  ASSERT_FALSE(store_->Delete("/aos/status").ok());
}

TEST_F(StoreTest, ReadNotExists)
{
  std::string data;
  ASSERT_FALSE(store_->Read("/aos", &data).ok());
}

TEST_F(StoreTest, ReadSuccess)
{
  ASSERT_TRUE(store_->Create("/aos", "aos", kPersist).ok());
  std::string data;
  ASSERT_TRUE(store_->Read("/aos", &data).ok());
  ASSERT_EQ(data, "aos");
}

TEST_F(StoreTest, WriteNotExists)
{
  ASSERT_FALSE(store_->Write("/aos", "").ok());
}

TEST_F(StoreTest, WriteSuccess)
{
  ASSERT_TRUE(store_->Create("/aos", "", kPersist).ok());
  ASSERT_TRUE(store_->Write("/aos", "aos").ok());
  std::string data;
  ASSERT_TRUE(store_->Read("/aos", &data).ok());
  ASSERT_EQ(data, "aos");
}

class NodeCreator
{
 public:
  NodeCreator(Store* store, int size)
      : size_(size),
        store_(store)
  {
    char buf[32];
    for (int i=0; i<size_; i++) {
      snprintf(buf, 32, "/node%010d", i);
      store_->Create(buf, "", kEphemeral);
    }
  }

  ~NodeCreator()
  {
    char buf[32];
    for (int i=0; i<size_; i++) {
      snprintf(buf, 32, "/node%010d", i);
      store_->Delete(buf);
    }
  }

 private:
  int size_;
  Store* store_;
};

/*
TEST_F(StoreTest, BenchCreate)
{
  const int size = 1000;
  std::cout << "create " << size << " nodes" << std::endl;
  Timestamp begin = Timestamp::now();
  NodeCreator n(store_, size);
  std::cout << "time used: " << timeDifference(Timestamp::now(), begin) << std::endl;
}

TEST_F(StoreTest, BenchRead)
{
  const int size = 10000;
  NodeCreator n(store_, 1);
  std::string data;
  char buf[32];
  std::cout << "read " << size << " times" << std::endl;
  Timestamp begin = Timestamp::now();
  for (int i=0; i<size; i++) {
    snprintf(buf, 32, "/node0");
    store_->Read(buf, &data);
  }
  std::cout << "time used: " << timeDifference(Timestamp::now(), begin) << std::endl;
}

TEST_F(StoreTest, BenchWrite)
{
  const int size = 10000;
  NodeCreator n(store_, 1);
  std::string data(1024, 0);
  char buf[32];
  std::cout << "write " << size << " times" << std::endl;
  Timestamp begin = Timestamp::now();
  for (int i=0; i<size; i++) {
    snprintf(buf, 32, "/node0");
    store_->Write(buf, data);
  }
  std::cout << "time used: " << timeDifference(Timestamp::now(), begin) << std::endl;
}
*/

void walkSessionFunc(std::map<uint64_t, store::Session>* ss, const store::Session& s)
{
  (*ss)[s.sid()] = s;
}

void getSessions(store::Store* store_, std::map<uint64_t, store::Session>* ss)
{
  store_->WalkSessions(boost::bind(walkSessionFunc, ss, _1));
}

void testStoreSession(store::Store* store)
{
  for (int i=0; i<10; i++) {
    pyxis::SessionPtr s(new pyxis::Session(i, 10000, NULL, NULL));
    store->AddSession(s);
  }
  std::map<uint64_t, store::Session> ss;
  getSessions(store, &ss);
  EXPECT_EQ(10, ss.size());
  for (int i=0; i<10; i++) {
    EXPECT_EQ(i, ss[i].sid());
    EXPECT_EQ(10000, ss[i].timeout());
  }
}

TEST_F(StoreTest, AddSession)
{
  testStoreSession(store_);
}

TEST_F(StoreTest, RemoveSession)
{
  for (int i=0; i<10; i++) {
    pyxis::SessionPtr s(new pyxis::Session(i, 10000, NULL, NULL));
    store_->AddSession(s);
  }

  for (int i=5; i<10; i++) {
    store_->RemoveSession(i);
  }
  std::map<uint64_t, store::Session> ss;
  getSessions(store_, &ss);
  EXPECT_EQ(5, ss.size());
  for (int i=0; i<5; i++) {
    EXPECT_EQ(i, ss[i].sid());
    EXPECT_EQ(10000, ss[i].timeout());
  }
}

TEST_F(StoreTest, CommitIndex)
{
  uint64_t idx;
  store_->SetCommitIndex(10);
  store_->GetCommitIndex(&idx);
  EXPECT_EQ(10, idx);
}

void walkTreeFunc(std::map<std::string, store::Node>* m,
  const std::string& p, const store::Node& n)
{
  (*m)[p] = n;
}

void testWalkTree(store::Store* store)
{
  char buf[32];
  for (int i=0; i<10; i++) {
    snprintf(buf, sizeof(buf), "/node%d", i);
    store->Create(buf, "", kPersist);
  }
  std::map<std::string, store::Node> m;
  store->WalkTree(boost::bind(walkTreeFunc, &m, _1, _2));

  // 加上根目录
  EXPECT_EQ(11, m.size());

  for (int i=0; i<10; i++) {
    snprintf(buf, sizeof(buf), "/node%d", i);
    EXPECT_TRUE(m.find(buf) != m.end());
  }
}

TEST_F(StoreTest, WalkTree)
{
  testWalkTree(store_);
}

TEST_F(StoreTest, MixSessionTree)
{
  testWalkTree(store_);
  testStoreSession(store_);
  testWalkTree(store_);
}

void emptyDone(){}

TEST_F(StoreTest, Snapshot)
{
  std::string snapshotPath = "/tmp/pyxis_snapshot_test";
  int treeSize = 2000;
  int sessionSize = 1000;
  int commitIndex = 1000;

  // add node;
  for (int i=0; i<treeSize; i++) {
    char buf[32];
    snprintf(buf, 32, "/node%010d", i);
    store_->Create(buf, buf, kEphemeral);
  }
  // add session
  for (int i=0; i<sessionSize; i++) {
    pyxis::SessionPtr s(new pyxis::Session(i, 10000, NULL, NULL));
    store_->AddSession(s);
  }
  // add CommitIndex
  store_->SetCommitIndex(commitIndex);

  // take snapshot
  store_->TakeSnapshot(snapshotPath, emptyDone);

  // reload snapshot
  Destroy();
  Load(snapshotPath);

  // check tree
  std::map<std::string, store::Node> m;
  store_->WalkTree(boost::bind(walkTreeFunc, &m, _1, _2));
  // 需要考虑root节点
  ASSERT_EQ(treeSize + 1, m.size());
  for (int i=0; i<treeSize; i++) {
    char buf[32];
    snprintf(buf, 32, "/node%010d", i);
    std::map<std::string, store::Node>::iterator it = m.find(buf);
    ASSERT_TRUE(it != m.end());
    ASSERT_EQ(buf, it->first);
    store::Node& n = it->second;
    ASSERT_EQ(buf, n.data());
  }

  // check session
  std::map<uint64_t, store::Session> ss;
  store_->WalkSessions(boost::bind(walkSessionFunc, &ss, _1));
  for (int i=0; i<sessionSize; i++) {
    ASSERT_EQ(i, ss[i].sid());
    ASSERT_EQ(10000, ss[i].timeout());
  }

  // check commitindex
  uint64_t idx;
  store_->GetCommitIndex(&idx);
  ASSERT_EQ(commitIndex, idx);
}
