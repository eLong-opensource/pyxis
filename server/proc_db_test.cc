#include <gtest/gtest.h>
#include <boost/bind.hpp>
#include <pyxis/server/proc_db.h>
#include <toft/base/string/format.h>
#include <toft/base/string/algorithm.h>

using leveldb::WriteOptions;
using leveldb::ReadOptions;
using namespace pyxis;
using namespace pyxis::store;

class ProcDBTest
{
 public:
  Status ReadArg0(const ProcDB::ArgList& args, std::string* value)
  {
    if (args.size() != 0) {
      return Status::InvalidArgument(toft::StringPrint("expect args:0 got %d", args.size()));
    }
    value->assign("");
    return Status::OK();
  }

  Status ReadArg1(const ProcDB::ArgList& args, std::string* value)
  {
    if (args.size() != 1) {
      return Status::InvalidArgument(toft::StringPrint("expect args:1 got %d", args.size()));
    }
    value->assign(args[0]);
    return Status::OK();
  }

  Status ReadArgN(const ProcDB::ArgList& args, std::string* value)
  {
    if (args.size() <= 1) {
      return Status::InvalidArgument(toft::StringPrint("expect args gt 1 got %d", args.size()));
    }
    value->assign(toft::JoinStrings(args, "/"));
    return Status::OK();
  }

  Status WriteArg0(const ProcDB::ArgList& args, const std::string& value)
  {
    if (args.size() != 0) {
      return Status::InvalidArgument(toft::StringPrint("expect args:0 got %d", args.size()));
    }
    return Status::OK();
  }

  Status WriteArg1(const ProcDB::ArgList& args, const std::string& value)
  {
    if (args.size() != 1) {
      return Status::InvalidArgument(toft::StringPrint("expect args:1 got %d", args.size()));
    }
    if (args[0] != value) {
      return Status::InvalidArgument(toft::StringPrint("expect %s got %s", args[0], value));
    }
    return Status::OK();
  }

  Status WriteArgN(const ProcDB::ArgList& args, const std::string& value)
  {
    if (args.size() <= 1) {
      return Status::InvalidArgument(toft::StringPrint("expect args gt 1 got %d", args.size()));
    }
    std::string expect = toft::JoinStrings(args, "/");
    if(expect != value) {
      return Status::InvalidArgument(toft::StringPrint("expect %s got %s", expect, value));
    }
    return Status::OK();
  }



};

TEST(ProcDBTest, ReadArg0)
{
  ProcDB db("/pyxis");
  ProcDBTest test;
  db.RegisterReadNode("/pyxis/size", boost::bind(&ProcDBTest::ReadArg0,
                                                 &test, _1, _2));
  std::string value;
  leveldb::Status st = db.Get(ReadOptions(), "/pyxis/size/0", &value);
  ASSERT_FALSE(st.ok()) << st.ToString();

  st = db.Get(ReadOptions(), "/pyxis/size", &value);
  ASSERT_TRUE(st.ok()) << st.ToString();
  ASSERT_EQ(value, "");
}

TEST(ProcDBTest, ReadArg1)
{
  ProcDB db("/pyxis");
  ProcDBTest test;
  db.RegisterReadNode("/pyxis/session", boost::bind(&ProcDBTest::ReadArg1,
                                                 &test, _1, _2));
  std::string value;
  leveldb::Status st = db.Get(ReadOptions(), "/pyxis/session/0/status", &value);
  ASSERT_FALSE(st.ok()) << st.ToString();

  st = db.Get(ReadOptions(), "/pyxis/session/0", &value);
  ASSERT_TRUE(st.ok()) << st.ToString();
  ASSERT_EQ("0", value);
}

TEST(ProcDBTest, ReadArgN)
{
  ProcDB db("/pyxis");
  ProcDBTest test;
  db.RegisterReadNode("/pyxis/session", boost::bind(&ProcDBTest::ReadArgN,
                                                 &test, _1, _2));
  std::string value;
  leveldb::Status st = db.Get(ReadOptions(), "/pyxis/session/0", &value);
  ASSERT_FALSE(st.ok()) << st.ToString();

  st = db.Get(ReadOptions(), "/pyxis/session/0/status/nodes", &value);
  ASSERT_TRUE(st.ok()) << st.ToString();
  ASSERT_EQ("0/status/nodes", value);
}

TEST(ProcDBTest, WriteArg0)
{
  ProcDB db("/pyxis");
  ProcDBTest test;
  db.RegisterWriteNode("/pyxis/config", boost::bind(&ProcDBTest::WriteArg0,
                                                 &test, _1, _2));
  leveldb::Status st = db.Put(WriteOptions(), "/pyxis/config/0", "");
  ASSERT_FALSE(st.ok()) << st.ToString();

  st = db.Put(WriteOptions(), "/pyxis/config", "");
  ASSERT_TRUE(st.ok()) << st.ToString();
}

TEST(ProcDBTest, WriteArg1)
{
  ProcDB db("/pyxis");
  ProcDBTest test;
  db.RegisterWriteNode("/pyxis/config", boost::bind(&ProcDBTest::WriteArg1,
                                                 &test, _1, _2));
  leveldb::Status st = db.Put(WriteOptions(), "/pyxis/config/timeout", "nil");
  ASSERT_FALSE(st.ok()) << st.ToString();

  st = db.Put(WriteOptions(), "/pyxis/config/timeout", "timeout");
  ASSERT_TRUE(st.ok()) << st.ToString();
}

TEST(ProcDBTest, WriteArgN)
{
  ProcDB db("/pyxis");
  ProcDBTest test;
  db.RegisterWriteNode("/pyxis/config", boost::bind(&ProcDBTest::WriteArgN,
                                                 &test, _1, _2));
  leveldb::Status st = db.Put(WriteOptions(), "/pyxis/config/session/timeout", "session/timeout/0");
  ASSERT_FALSE(st.ok()) << st.ToString();

  st = db.Put(WriteOptions(), "/pyxis/config/session/timeout", "session/timeout");
  ASSERT_TRUE(st.ok()) << st.ToString();
}
