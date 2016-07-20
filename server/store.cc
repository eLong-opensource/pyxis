#include <glog/logging.h>
#include <gflags/gflags.h>
#include <leveldb/write_batch.h>
#include <leveldb/filter_policy.h>
#include <leveldb/cache.h>
#include <toft/base/string/algorithm.h>
#include <algorithm>

#include <pyxis/server/store.h>

DEFINE_int32(cache_size, 100, "leveldb cache size, in MB");
DEFINE_int32(max_node_size, 1024, "node size, in KB");

namespace pyxis {
namespace store {

const std::string SESSION_PREFIX = "ss";
const std::string COMMIT_PREFIX = "commit";

Store::Store(leveldb::DB* persist, leveldb::DB* proc,
             const std::string& procRoot)
    : persist_(persist),
      proc_(proc),
      procRoot_(procRoot)
{
  init();
}

Store::~Store()
{
}

void Store::init()
{
  // 初始化根节点
  std::string data;
  if (persist_->Get(leveldb::ReadOptions(), "/", &data).ok()) {
    return;
  }
  Node root;
  NewNode(&root, "", kPersist);
  root.add_children(toft::StringTrimLeft(procRoot_, "/"));
  CHECK(Put("/", root).ok());
}

Status Store::Create(const std::string& path, const std::string& data, int flags, sid_t sid)
{
  if (toft::StringStartsWith(path, procRoot_)) {
    return Status::InvalidArgument("can't create node under pyxis node");
  }
  if (data.size() > static_cast<size_t>(FLAGS_max_node_size * 1024)) {
    return Status::InvalidArgument("node size exceed");
  }
  std::string parentName, sonName;
  Status st = SplitPath(path, &parentName, &sonName);
  if (!st.ok()) {
    return st;
  }
  Node parent, son;

  // check path not exists
  st = Get(path, &son);
  if (st.ok()) {
    return Status::Exist(path);
  }

  if (st.IsInvalidArgument()) {
    return st;
  }

  st = Get(parentName, &parent);
  if (!st.ok()) {
    return st;
  }
  // 是否是临时节点
  if (parent.flags() & kEphemeral) {
    return Status::InvalidArgument("parent not persistent node");
  }

  // add children
  st = AddChild(&parent, sonName);
  CHECK(st.ok());
  if (parent.ByteSize() > FLAGS_max_node_size * 1024) {
    return Status::InvalidArgument("node size exceed");
  }

  // new node
  NewNode(&son, data, NodeFlags(flags));
  if (sid > 0) {
    son.set_sid(sid);
  }

  // atomic write
  leveldb::WriteBatch batch;
  batch.Put(path, son.SerializeAsString());
  batch.Put(parentName, parent.SerializeAsString());
  leveldb::Status status = persist_->Write(leveldb::WriteOptions(), &batch);
  if (!status.ok()) {
    return Status::Internal(status.ToString());
  }
  return Status::OK();
}

Status Store::Delete(const std::string& path)
{
  if (toft::StringStartsWith(path, procRoot_)) {
    return Status::InvalidArgument("can't delete node under pyxis node");
  }
  std::string parentName, sonName;
  Status st = SplitPath(path, &parentName, &sonName);
  if (!st.ok()) {
    return st;
  }
  Node parent, son;

  // check path exists
  st = Get(path, &son);
  if (!st.ok()) {
    if (st.IsNotFound()) {
      return Status::NotFound(path);
    }
    return st;
  }

  // 是否有子结点
  if (son.children().size() != 0) {
    return Status::InvalidArgument("children not empty");
  }

  // 清除在父节点上的记录
  st = Get(parentName, &parent);
  if (!st.ok()) {
    return st;
  }
  st = RemoveChild(&parent, sonName);
  CHECK(st.ok()) << st.ToString();

  // atomic write
  leveldb::WriteBatch batch;
  batch.Delete(path);
  batch.Put(parentName, parent.SerializeAsString());
  leveldb::Status status = persist_->Write(leveldb::WriteOptions(), &batch);
  if (!status.ok()) {
    return Status::Internal(st.ToString());
  }
  return Status::OK();
}

Status Store::Read(const std::string& path, std::string* data)
{
  CHECK(data != NULL);
  Node node;
  Status st = Get(path, &node);
  if (!st.ok()) {
    return st;
  }

  *data = node.data();
  return Status::OK();
}

Status Store::Write(const std::string& path, const std::string& data)
{
  Node node;
  Status st = Get(path, &node);
  if (!st.ok()) {
    return st;
  }

  node.set_data(data);
  node.set_version(node.version() + 1);

  return Put(path, node);
}

Status Store::Get(const std::string& path, Node* node)
{
  leveldb::DB* db = persist_.get();
  if (toft::StringStartsWith(path, procRoot_)) {
    db = proc_.get();
  }
  // valid path
  if (!IsValidPath(path)) {
    return Status::InvalidArgument(path);
  }

  std::string data;
  leveldb::Status st = db->Get(leveldb::ReadOptions(), path, &data);
  if (!st.ok()) {
    if (st.IsNotFound()) {
      return Status::NotFound(path);
    }
    return Status::Internal(path + " " + st.ToString());
  }

  if (node != NULL) {
    if (!node->ParseFromString(data)) {
      return Status::Internal(path + " bad data");
    }
  }

  return Status::OK();
}

Status Store::Put(const std::string& path, const Node& node)
{
  leveldb::DB* db = persist_.get();
  if (toft::StringStartsWith(path, procRoot_)) {
    db = proc_.get();
  }
  if (!IsValidPath(path)) {
    return Status::InvalidArgument("invalid path");
  }

  if (node.ByteSize() > FLAGS_max_node_size * 1024) {
    return Status::InvalidArgument("node size exceed");
  }
  std::string data;
  if (!node.SerializeToString(&data)) {
    return Status::Internal(path + " bad data");
  }

  leveldb::Status st = db->Put(leveldb::WriteOptions(), path, data);
  if (!st.ok()) {
    return Status::Internal(path + " " + st.ToString());
  }
  return Status::OK();
}

Status Store::WalkTree(const WalkTreeFunc& func)
{
  std::string start = "/";
  std::string end = "/\xFF";
  leveldb::Iterator* it = persist_->NewIterator(leveldb::ReadOptions());
  Node node;

  for (it->Seek(start);
       it->Valid() && it->key().ToString() < end;
       it->Next()) {
    const std::string& key = it->key().ToString();
    node.ParseFromArray(it->value().data(), it->value().size());
    func(key, node);
  }
  leveldb::Status st = it->status();
  delete it;

  if (!st.ok()) {
    return Status::Internal(st.ToString());
  }
  return Status::OK();
}

Status Store::AddSession(const SessionPtr& ss)
{
  std::string key, value;
  sid_t sid = ss->Sid();
  key.append(SESSION_PREFIX);
  key.append((const char*)&sid, sizeof(sid));

  ss->SerializeToString(&value);
  leveldb::Status st = persist_->Put(leveldb::WriteOptions(), key, value);
  if (!st.ok()) {
    return Status::Internal(st.ToString());
  }
  return Status::OK();
}

Status Store::RemoveSession(sid_t sid)
{
  std::string key;
  key.append(SESSION_PREFIX);
  key.append((const char*)&sid, sizeof(sid));

  leveldb::Status st = persist_->Delete(leveldb::WriteOptions(), key);
  if (!st.ok()) {
    return Status::Internal(st.ToString());
  }
  return Status::OK();
}

Status Store::WalkSessions(const WalkSessionFunc& func)
{
  std::string start = SESSION_PREFIX;
  std::string end = start;
  // max sid 0xFF x 8
  end.append(8, 0xFF);

  leveldb::Iterator* it = persist_->NewIterator(leveldb::ReadOptions());
  Session s;

  for (it->Seek(start);
       it->Valid();
       it->Next()) {
    leveldb::Slice key = it->key();
    if (!(key.ToString() < end)) {
      break;
    }
    CHECK(key.size() == (SESSION_PREFIX.size() + 8)) << key.ToString();
    sid_t sid;
    memcpy(&sid, key.data() + SESSION_PREFIX.size(), sizeof(sid));
    s.ParseFromArray(it->value().data(), it->value().size());
    CHECK(sid == s.sid()) << "sid:" << sid << " s.sid():" << s.sid();
    func(s);
  }
  leveldb::Status st = it->status();
  delete it;

  if (!st.ok()) {
    return Status::Internal(st.ToString());
  }
  return Status::OK();
}

Status Store::SetCommitIndex(uint64_t idx)
{
  std::string key, value;
  key.append(COMMIT_PREFIX);
  value.resize(sizeof(idx));
  memcpy(&value[0], &idx, sizeof(idx));

  leveldb::Status st = persist_->Put(leveldb::WriteOptions(), key, value);
  if (!st.ok()) {
    return Status::Internal(st.ToString());
  }
  return Status::OK();
}

Status Store::GetCommitIndex(uint64_t* idx)
{
  std::string key, value;
  key.append(COMMIT_PREFIX);

  leveldb::Status st = persist_->Get(leveldb::ReadOptions(), key, &value);
  if (st.IsNotFound()) {
    *idx = 0;
    return Status::OK();
  }
  if (!st.ok()) {
    return Status::Internal(st.ToString());
  }
  CHECK(value.size() == sizeof(*idx)) << value;
  memcpy(idx, &value[0], sizeof(*idx));
  return Status::OK();
}

void Store::TakeSnapshot(const std::string& path, const boost::function<void()>& done)
{
  leveldb::DB* newdb = NULL;
  leveldb::Options options;
  options.create_if_missing = true;
  leveldb::Status st = leveldb::DB::Open(options, path, &newdb);
  CHECK(st.ok()) << st.ToString();

  leveldb::Iterator* it = persist_->NewIterator(leveldb::ReadOptions());
  Node node;

  leveldb::WriteOptions woptions;
  leveldb::WriteBatch batch;
  int batchLimit = 1024;
  int batchCount = 0;
  for (it->SeekToFirst();
       it->Valid();
       it->Next()) {
    batch.Put(it->key(), it->value());
    batchCount++;
    if (batchCount >= batchLimit) {
      st = newdb->Write(woptions, &batch);
      batch.Clear();
      batchCount = 0;
    }
  }
  if (batchCount > 0) {
    st = newdb->Write(woptions, &batch);
  }
  leveldb::Status st1 = it->status();
  delete it;
  delete newdb;

  CHECK(st.ok() && st1.ok()) << st.ToString() << " " << st1.ToString();
  done();
}

Status Store::OnProcDBStatusRead(const ProcDB::ArgList& args, std::string* value)
{
  store::Node node;
  node.set_flags(kPersist);
  node.set_data("");
  node.set_version(0);

  if (args.size() == 0) {
    node.add_children("leveldb.num-files-at-level<N>");
    node.add_children("leveldb.stats");
    node.add_children("leveldb.sstables");
    node.SerializeToString(value);
    return Status::OK();
  }

  std::string d;
  if (!persist_->GetProperty(args[0], &d)) {
    return Status::InvalidArgument("bad request " + args[0]);
  }
  node.set_data(d);
  node.SerializeToString(value);
  return Status::OK();
}

leveldb::DB* OpenLevelDB(const std::string& path)
{
  static leveldb::Cache* cache = leveldb::NewLRUCache(FLAGS_cache_size * 1024 * 1024);
  static const leveldb::FilterPolicy* filter = leveldb::NewBloomFilterPolicy(10);
  leveldb::DB* db;
  leveldb::Options options;
  options.create_if_missing = true;
  options.filter_policy = filter;
  options.block_cache = cache;
  leveldb::Status st = leveldb::DB::Open(options, path, &db);
  CHECK(st.ok()) << st.ToString();
  return db;
}

static bool invalidChar(char c)
{
  return !(std::isalnum(c) || c == '_' || c =='/' || c == '-' || c == '.');
}

bool IsValidPath(const std::string& path)
{
  return !path.empty() &&
      path[0] == '/' &&
      path.size() < kMaxPathSize &&
      std::find_if(path.begin(), path.end(), invalidChar) == path.end();
}

Status SplitPath(const std::string& path, std::string* parent, std::string* son)
{
  size_t lastslash = path.rfind("/");
  if (lastslash == std::string::npos) {
    return Status::InvalidArgument(path);
  }
  *parent = path.substr(0, lastslash);
  *son = path.substr(lastslash + 1);
  if (parent->empty()) {
    *parent = "/";
  }
  if (son->empty()) {
    return Status::InvalidArgument(path);
  }
  return Status::OK();
}

Status AddChild(Node* node, const std::string& son)
{
  CHECK(node != NULL);
  // FIXME check node exists
  node->add_children(son);
  return Status::OK();
}

Status RemoveChild(Node* node, const std::string& son)
{
  CHECK(node != NULL);
  google::protobuf::RepeatedPtrField<std::string>* children = node->mutable_children();
  for (int i = 0; i < children->size(); i++) {
    if (children->Get(i) == son) {
      children->SwapElements(i, children->size() - 1);
      children->RemoveLast();
      return Status::OK();
    }
  }
  return Status::NotFound(son + " not found in " + " children");
}

void NewNode(Node* node, const std::string& data, NodeFlags flag)
{
  CHECK(node != NULL);

  node->set_data(data);
  node->set_flags(flag);
  node->set_version(0);
}

}
}
