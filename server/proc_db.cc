#include <pyxis/server/proc_db.h>
#include <toft/base/string/string_piece.h>
#include <toft/base/string/algorithm.h>
#include <boost/bind.hpp>
#include <pyxis/common/node.h>
#include <pyxis/proto/node.pb.h>

using namespace leveldb;

namespace pyxis {
namespace store {

ProcDB::ProcDB(const std::string& root)
    : root_(root)
{
  RegisterReadNode("/pyxis", boost::bind(&ProcDB::onProcRead,
                                         this, _1, _2));
}

ProcDB::~ProcDB()
{
}

void ProcDB::RegisterReadNode(const std::string& path, const ReadNodeCallback& cb)
{
  readMap_[path] = cb;
}

void ProcDB::RegisterWriteNode(const std::string& path, const WriteNodeCallback& cb)
{
    writeMap_[path] = cb;
    readMap_[path] = boost::bind(&ProcDB::emptyReadCallback,
                                 this, _1, _2);
}


leveldb::Status ProcDB::Put(const WriteOptions& options,
                   const Slice& key,
                   const Slice& value)
{
  toft::StringPiece path(key.data());
  std::map<std::string, WriteNodeCallback>::reverse_iterator it;
  for (it = writeMap_.rbegin(); it != writeMap_.rend(); it++) {
    if (path.starts_with(it->first)) {
      break;
    }
  }
  if (it == writeMap_.rend()) {
    return leveldb::Status::NotFound(path.data());
  }

  toft::StringPiece prefix = it->first;
  WriteNodeCallback& callback = it->second;

  toft::StringPiece argstr = path.substr(prefix.size());
  std::vector<std::string> args;
  toft::SplitString(argstr, "/", &args);
  Status st = callback(args, value.data());
  if (!st.ok()) {
    if (st.IsNotFound()) {
      return leveldb::Status::NotFound(st.ToString());
    }
    return leveldb::Status::InvalidArgument(st.ToString());
  }
  return leveldb::Status::OK();
}

leveldb::Status ProcDB::Delete(const WriteOptions& options,
                      const Slice& key)
{
  return leveldb::Status::NotSupported("method not allowed");
}

leveldb::Status ProcDB::Write(const WriteOptions& options,
                     WriteBatch* updates)
{
  return leveldb::Status::NotSupported("method not allowed");
}

leveldb::Status ProcDB::Get(const ReadOptions& options,
                   const Slice& key, std::string* value)
{
  toft::StringPiece path(key.data());
  std::map<std::string, ReadNodeCallback>::reverse_iterator it;
  for (it = readMap_.rbegin(); it != readMap_.rend(); it++) {
    if (path.starts_with(it->first)) {
      break;
    }
  }
  if (it == readMap_.rend()) {
    return leveldb::Status::NotFound(path.data());
  }
  toft::StringPiece prefix = it->first;
  ReadNodeCallback& callback = it->second;

  toft::StringPiece argstr = path.substr(prefix.size());
  std::vector<std::string> args;
  toft::SplitString(argstr, "/", &args);
  Status st = callback(args, value);
  if (!st.ok()) {
    if (st.IsNotFound()) {
      return leveldb::Status::NotFound(st.ToString());
    }
    return leveldb::Status::InvalidArgument(st.ToString());
  }
  return leveldb::Status::OK();
}

Status ProcDB::onProcRead(const ArgList& args, std::string* value)
{
  if (!args.empty()) {
    return Status::NotFound("");
  }
  Node node;
  node.set_flags(kPersist);
  node.set_data("");
  node.set_version(0);

  std::set<std::string> children;
  for (std::map<std::string, ReadNodeCallback>::iterator it = readMap_.begin();
       it != readMap_.end(); it++) {
    children.insert(it->first);
  }

  for (std::map<std::string, WriteNodeCallback>::iterator it = writeMap_.begin();
       it != writeMap_.end(); it++) {
    children.insert(it->first);
  }

  for (std::set<std::string>::iterator it = children.begin();
       it != children.end(); it++) {
    if (*it == root_) {
      continue;
    }
    const std::string& s = it->substr(root_.size());
    node.add_children(toft::StringTrimLeft(s, "/"));
  }

  node.SerializeToString(value);
  return Status::OK();
}

Status ProcDB::emptyReadCallback(const ArgList& args, std::string* value)
{
  Node node;
  node.set_data("write only");
  node.SerializeToString(value);
  return Status::OK();
}


}
}
