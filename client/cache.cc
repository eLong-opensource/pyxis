#include <pyxis/client/cache.h>
#include <pyxis/proto/node.pb.h>
#include <toft/storage/path/path.h>
#include <glog/logging.h>

namespace pyxis {
namespace client {

Cache::Cache(const std::string& dir)
    : dir_(dir),
      db_(NULL)
{
}

Cache::~Cache()
{
  if (db_) {
    delete db_;
  }
}

Status Cache::Init()
{
  leveldb::Options options;
  options.create_if_missing = true;
  leveldb::Status st = leveldb::DB::Open(options, dir_, &db_);
  if (!st.ok()) {
    return Status::Internal("open cache failed:" + st.ToString());
  }
  return Status::OK();
}

Status Cache::Read(const std::string& path, store::Node* node)
{
  CHECK_NOTNULL(db_);
  if (node == NULL) {
    return Status::InvalidArgument("store::Node");
  }
  std::string data;
  leveldb::Status st = db_->Get(leveldb::ReadOptions(),
                                path,
                                &data);
  if (!st.ok()) {
    if (st.IsNotFound()) {
      return Status::NotFound(path);
    }
    return Status::Internal(st.ToString());
  }

  if (!node->ParseFromString(data)) {
    return Status::Internal("parse node:" + path);
  }
  return Status::OK();
}

Status Cache::Write(const std::string& path, const store::Node& node)
{
  CHECK_NOTNULL(db_);
  std::string data;
  if (!node.SerializeToString(&data)) {
    return Status::Internal("serialize node:" + path);
  }

  leveldb::Status st = db_->Put(leveldb::WriteOptions(),
                                path,
                                data);
  if (!st.ok()) {
    return Status::Internal(st.ToString());
  }

  return Status::OK();
}

}
}
