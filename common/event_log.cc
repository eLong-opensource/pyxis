#include <toft/storage/path/path.h>
#include <glog/logging.h>
#include <gflags/gflags.h>
#include <muduo/base/Timestamp.h>
#include <stdio.h>
#include <inttypes.h>
#include <pyxis/proto/rpc.pb.h>
#include <pyxis/proto/node.pb.h>
#include <pyxis/common/watch_event.h>
#include <pyxis/common/event_log.h>
#include <pyxis/common/status.h>

namespace pyxis {
EventLog::EventLog(time_t flushInterval)
    : logFilePath_("log/event.log"),
      fp_(NULL),
      flushInterval_(flushInterval),
      lastflush_(time(NULL)),
      mutex_()
{
  Reload();
}

EventLog::~EventLog()
{
  if (fp_ != NULL) {
    fclose(fp_);
  }
}

void EventLog::SetLogFile(const std::string& p)
{
  logFilePath_ = p;
}

void EventLog::Reload()
{
  if (fp_ != NULL) {
    fclose(fp_);
  }
  fp_ = fopen(logFilePath_.c_str(), "ae");
  PCHECK(fp_ != NULL);
}

void EventLog::LogWatch(sid_t sid, const watch::Event& e)
{
  Log("-", "wevent", sid, 0, e.String(), "", 0);
}

void EventLog::LogRpc(const std::string& addr, rpc::Request* req, rpc::Response* rep, double used)
{
  std::string event;
  std::string param;
  getActionAndNode(req, &event, &param);
  Log(addr, event, req->sid(), req->xid(), param, Status::FromRpcError(rep->err()).ToString(), used);
}

void EventLog::LogSession(sid_t sid, toft::StringPiece what)
{
  Log("-", "sess", sid, 0, what, "", 0);
}

Status EventLog::OnProcWrite(const store::ProcDB::ArgList& args,
                            const std::string& value)
{
  Reload();
  return Status::OK();
}

void EventLog::Log(toft::StringPiece addr, toft::StringPiece event,
                   sid_t sid, uint64_t xid, toft::StringPiece param, toft::StringPiece err,
                   double used)
{
  muduo::MutexLockGuard guard(mutex_);
  muduo::string timestamp(muduo::Timestamp::now().toFormattedString());
  int n = fprintf(fp_, "%s %s" " %020"PRIu64 " %05"PRIu64 " %-6s \"%s\" \"%s\" %f\n", timestamp.c_str(),
                  addr.data(), sid, xid, event.data(), param.data(), err.data(), used);
  PCHECK(n > 0);
  time_t now = time(NULL);
  if (now - lastflush_ > flushInterval_) {
    fflush(fp_);
  }
  lastflush_ = now;
}

void EventLog::getActionAndNode(const rpc::Request* req, std::string* action,
                                std::string* node)
{
  if (req->has_ping()) {
    action->assign("ping");
    node->assign("-");
  } else if (req->has_register_()) {
    action->assign("reg");
    node->assign("-");
  } else if (req->has_unregister()) {
    action->assign("unreg");
    node->assign("-");
  } else if (req->has_stat()) {
    action->assign("stat");
    node->assign(req->stat().path());
  } else if (req->has_read()) {
    action->assign("read");
    node->assign(req->read().path());
  } else if (req->has_write()) {
    action->assign("write");
    node->assign(req->write().path());
  } else if (req->has_create()) {
    action->assign("create");
    node->assign(req->create().path());
  } else if (req->has_delete_()) {
    action->assign("delete");
    node->assign(req->delete_().path());
  } else if (req->has_watch()) {
    action->assign("watch");
    node->assign(req->watch().path());
  } else {
    action->assign("???");
    node->assign("???");
  }

  if (req->has_watch()) {
    *action += "w";
  }
}

}
