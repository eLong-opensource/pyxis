#include <pyxis/proto/rpc.pb.h>
#include <pyxis/proto/session.pb.h>
#include <pyxis/server/store.h>
#include <pyxis/server/session.h>
#include <pyxis/server/server.h>
#include <pyxis/common/event_log.h>
#include <pyxis/common/strreader.h>
#include <jsoncpp/json.h>
#include <toft/encoding/proto_json_format.h>
#include <glog/logging.h>
#include <muduo/base/Singleton.h>
#include <thirdparty/leveldb/db.h>
#include <thirdparty/leveldb/write_batch.h>

namespace pyxis {

Session::Session(sid_t sid, int timeout,
  WatchManager* wm, Server* server)
    : sid_(sid),
      timeout_(timeout),
      expire_(),
      join_(muduo::Timestamp::now()),
      lastActive_(muduo::Timestamp::now()),
      active_(true),
      pingEvent_(),
      pendingWatcher_(),
      wm_(wm),
      server_(server),
      watches_()
{
}

Session::~Session()
{
  LOG(INFO) << "~session sid: " << sid_;

  for (std::map<std::string, WatcherPtr>::iterator it = watches_.begin();
       it != watches_.end(); it++) {
    wm_->Remove(it->second);
    LOG(INFO) << "sid: " << sid_ << " removes watcher " << it->second->Path();
  }

  for (std::set<std::string>::iterator it = nodeSet_.begin();
       it != nodeSet_.end(); it++) {
    store::Node node;
    Status st = server_->ReadNode(*it, &node);
    if (st.IsNotFound()) {
      LOG(INFO) << "ephemeral node " << *it
                << " deleted before session close";
      continue;
    }

    if (node.sid() != sid_) {
      LOG(WARNING) << "ephemeral node " << *it << " changes owner from " << sid_
                << " to " << node.sid();
      continue;
    }
    LOG(INFO) << "sid " << sid_ << " delete ephemeral node " << *it;

    st = server_->DeleteNode(*it);
    CHECK(st.ok() || st.IsNotFound()) << st.ToString();
  }
}

void Session::PendingWatcher(const std::string& path, const watch::Event& e)
{
  pendingWatcher_.insert(std::make_pair(path, e));
}

void Session::PingBack()
{
  if (!pingEvent_) {
    return;
  }
  lastActive_ = muduo::Timestamp::now();
  rpc::Response* rep = pingEvent_->Rep;
  if (!pendingWatcher_.empty()) {
    std::multimap<std::string, watch::Event>::iterator it = pendingWatcher_.begin();
    VLOG(1) << "watch back. event:" << it->second.String();
    rep->set_path(it->first);
    rpc::WatchEvent* w = rep->mutable_watch();
    w->set_path(it->second.Node);
    w->set_type(it->second.Type);
    pendingWatcher_.erase(it);

    EventLog& logger = muduo::Singleton<EventLog>::instance();
    logger.LogWatch(sid_, it->second);
  }
  // TODO session连接断开如何处理
  pingEvent_.reset();
}

void Session::toProtobuf(store::Session* proto) const
{
  CHECK(proto != NULL);
  proto->set_sid(sid_);
  proto->set_timeout(timeout_);

  for (std::map<std::string, WatcherPtr>::const_iterator it = watches_.begin();
       it != watches_.end(); it++) {
    const WatcherPtr& p = it->second;
    std::string& path = p->Path();
    store::WatchEvent* w = proto->add_watch();
    w->set_path(path);
    w->set_type(p->Mask());
    w->set_recursive(p->IsRecursive());
  }

  for (std::set<std::string>::iterator it = nodeSet_.begin();
       it != nodeSet_.end(); it++) {
    proto->add_node(*it);
  }
}

bool Session::SerializeToString(std::string* value) const
{
  CHECK(value != NULL);
  store::Session s;
  toProtobuf(&s);
  return s.SerializeToString(value);
}

bool Session::SerializeToJson(std::string* value) const
{
  CHECK(value != NULL);
  store::Session s;
  toProtobuf(&s);
  Json::Value v;
  toft::ProtoJsonFormat::WriteToValue(s, &v);
  v["join"] = uint32_t(join_.secondsSinceEpoch());
  v["last"] = uint32_t(lastActive_.secondsSinceEpoch());
  Json::FastWriter w;
  value->assign(w.write(v));
  return true;
}

}
