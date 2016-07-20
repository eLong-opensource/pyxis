#ifndef PYXIS_SERVER_STORE_H
#define PYXIS_SERVER_STORE_H

#include <string>
#include <thirdparty/leveldb/db.h>

#include <pyxis/proto/node.pb.h>
#include <pyxis/proto/session.pb.h>
#include <pyxis/common/node.h>
#include <pyxis/common/status.h>
#include <pyxis/common/types.h>
#include <pyxis/server/session.h>
#include <pyxis/server/proc_db.h>

#include <boost/scoped_ptr.hpp>
#include <boost/function.hpp>

namespace pyxis {
namespace store {

const size_t kMaxPathSize = 4096;

typedef boost::function<void (std::string, const Node& node)> WalkTreeFunc;
typedef boost::function<void (const Session& session)> WalkSessionFunc;

class Store {
 public:

  Store(leveldb::DB* persist, leveldb::DB* proc, const std::string& procRoot);

  ~Store();

  Status Create(const std::string& path, const std::string& data, int flag, sid_t sid = 0);

  Status Delete(const std::string& path);

  Status Read(const std::string& path, std::string* data);

  Status Write(const std::string& path, const std::string& data);

  Status Get(const std::string& path, Node* node);

  Status Put(const std::string& path, const Node& node);

  Status WalkTree(const WalkTreeFunc& func);

  Status AddSession(const SessionPtr& ss);

  Status RemoveSession(sid_t sid);

  Status WalkSessions(const WalkSessionFunc& func);

  // 更新提交id
  Status SetCommitIndex(uint64_t idx);

  Status GetCommitIndex(uint64_t* idx);

  // 为当前的存储做快照，用path做db的目录
  void TakeSnapshot(const std::string& path, const boost::function<void()>& done);

  // 读取数据库状态的节点
  Status OnProcDBStatusRead(const ProcDB::ArgList& args, std::string* value);


 private:
  void init();
  boost::scoped_ptr<leveldb::DB> persist_;
  boost::scoped_ptr<leveldb::DB> proc_;
  std::string procRoot_;

};

leveldb::DB* OpenLevelDB(const std::string& path);

bool IsValidPath(const std::string& path);

Status SplitPath(const std::string& path, std::string* parent, std::string* son);

Status AddChild(Node* node, const std::string& son);

Status RemoveChild(Node* node, const std::string& son);

void NewNode(store::Node* node, const std::string& data, NodeFlags flag);

}
}

#endif
