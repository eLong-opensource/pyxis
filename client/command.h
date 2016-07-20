#ifndef PYXIS_CLIENT_COMMAND_H
#define PYXIS_CLIENT_COMMAND_H

#include <string>
#include <pyxis/common/watch_event.h>

namespace google {
namespace protobuf {
class Closure;
}
}

namespace pyxis {
namespace client {

class RequestType {
  enum {
    kCreate = 0x00000001,
    kDelete = 0x00000002,
    kWrite  = 0x00000004,
    kRead   = 0x00000008,
    kStat   = 0x00000010,
    kPing   = 0x00000020,
    kWatch  = 0x00000040,
    kBatch  = 0x00000080,
  };
};

struct Request {
  uint32_t Type;
  std::string Path;
  std::string Data;
  uint32_t Watcher;
};

struct Response {
  std::string Path;
  std::string Data;
  uint32_t Watcher;
};

struct Command {
  Request* Req;
  Response* rep;
  google::protobuf::Closure* done;
};

}
}
#endif
