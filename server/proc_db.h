#ifndef PYXIS_SERVER_PROC_DB_H
#define PYXIS_SERVER_PROC_DB_H

#include <vector>
#include <map>
#include <boost/function.hpp>
#include <leveldb/db.h>
#include <pyxis/common/types.h>
#include <pyxis/common/status.h>

namespace pyxis {
namespace store {

class ProcDB : public leveldb::DB {

 public:

  typedef std::vector<std::string> ArgList;
  typedef boost::function<pyxis::Status (const ArgList&, std::string*)> ReadNodeCallback;
  typedef boost::function<pyxis::Status (const ArgList&, const std::string&)> WriteNodeCallback;

  ProcDB(const std::string& root);
  ~ProcDB();

  void RegisterReadNode(const std::string& path, const ReadNodeCallback& cb);

  void RegisterWriteNode(const std::string& path, const WriteNodeCallback& cb);


  /******************** interface of leveldb::DB ******************/

  leveldb::Status Put(const leveldb::WriteOptions& options,
                              const leveldb::Slice& key,
                              const leveldb::Slice& value);

  leveldb::Status Delete(const leveldb::WriteOptions& options,
                        const leveldb::Slice& key);

  leveldb::Status Write(const leveldb::WriteOptions& options,
                        leveldb::WriteBatch* updates);

  leveldb::Status Get(const leveldb::ReadOptions& options,
                      const leveldb::Slice& key, std::string* value);

  leveldb::Iterator* NewIterator(const leveldb::ReadOptions& options)
  {
    return NULL;
  }

  const leveldb::Snapshot* GetSnapshot()
  {
    return NULL;
  }

  void ReleaseSnapshot(const leveldb::Snapshot* snapshot)
  {
  }

  bool GetProperty(const leveldb::Slice& property, std::string* value)
  {
    return true;
  }

  void GetApproximateSizes(const leveldb::Range* range, int n,
                           uint64_t* size)
  {
  }

  void CompactRange(const leveldb::Slice* begin, const leveldb::Slice* end)
  {
  }

  /* end leveldb::DB */

 private:
  Status emptyReadCallback(const ArgList& args, std::string* value);

  pyxis::Status onProcRead(const ArgList& args, std::string* value);
  std::string root_;
  std::map<std::string, ReadNodeCallback> readMap_;
  std::map<std::string, WriteNodeCallback> writeMap_;
};

}
}
#endif
