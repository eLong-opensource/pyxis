#ifndef PYXIS_CLIENT_CACHE_H
#define PYXIS_CLIENT_CACHE_H

#include <string>
#include <pyxis/common/status.h>
#include <pyxis/proto/node.pb.h>
#include <leveldb/db.h>

namespace pyxis {
namespace client {

class Cache {

 public:
  Cache(const std::string& dir);
  ~Cache();

  Status Init();
  Status Read(const std::string& path, store::Node* node);
  Status Write(const std::string& path, const store::Node& node);

 private:
  std::string dir_;
  leveldb::DB* db_;
};

}
}
#endif
