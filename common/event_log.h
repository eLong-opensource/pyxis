#ifndef PYXIS_SERVER_EVENT_LOG_H
#define PYXIS_SERVER_EVENT_LOG_H

#include <string>
#include <toft/base/string/string_piece.h>
#include <stdio.h>
#include <time.h>
#include <muduo/base/Mutex.h>
#include <pyxis/server/proc_db.h>
#include <pyxis/common/types.h>


namespace pyxis {
namespace watch {
class Event;
}
}

namespace pyxis {
namespace rpc {
class Request;
class Response;
}
}


namespace pyxis {

class EventLog
{
 public:
  EventLog(time_t flushInterval = 3);
  ~EventLog();

  void SetLogFile(const std::string& p);

  void Reload();

  // 用于切割日志文件
  Status OnProcWrite(const store::ProcDB::ArgList& args,
                    const std::string& value);

  void LogWatch(sid_t sid, const watch::Event& e);
  void LogRpc(const std::string& addr, rpc::Request* req, rpc::Response* rep, double used);
  void LogSession(sid_t sid, toft::StringPiece what);
  void Log(toft::StringPiece addr, toft::StringPiece event,
           sid_t sid, uint64_t xid, toft::StringPiece param,
           toft::StringPiece err, double used);

 private:
  void getActionAndNode(const rpc::Request* e, std::string* action,
                        std::string* node);


  std::string logFilePath_;
  FILE* fp_;
  time_t flushInterval_;
  time_t lastflush_;
  muduo::MutexLock mutex_;
};

}

#endif
