#ifndef PYXIS_COMMON_STATUS_H
#define PYXIS_COMMON_STATUS_H

#include <string>

namespace pyxis {
namespace rpc {
class Error;
}
}

namespace pyxis {

class Status
{
 public:
  Status() : code_(kOk) {}
  ~Status() {}

  Status(const Status& s) : code_(s.code_), msg_(s.msg_) {}
  void operator = (const Status& s)
  {
    code_ = s.code_;
    msg_ = s.msg_;
  }

  void ToRpcError(rpc::Error* err);

  std::string ToString();

  bool ok() { return code_ == kOk; }
  bool IsNotFound() { return code_ == kNotFound; }
  bool IsAgain() { return code_ == kAgain; }
  bool IsInvalidArgument() { return code_ == kInvalidArgument; }
  bool IsDisconnect() { return code_ == kDisconnect; }
  bool IsExist() { return code_ == kExist; }
  bool IsTimeout() { return code_ == kTimeout; }
  bool IsCached() { return code_ == kCached; }

  static Status OK() { return Status(); }

  static Status NotLeader(const std::string& msg) {
    return Status(kNotLeader, msg);
  }
  static Status NotFound(const std::string& msg) {
    return Status(kNotFound, msg);
  }
  static Status Internal(const std::string& msg) {
    return Status(kInternal, msg);
  }

  static Status SessionExpired(const std::string& msg) {
    return Status(kSessionExpired, msg);
  }

  static Status InvalidArgument(const std::string& msg) {
    return Status(kInvalidArgument, msg);
  }

  static Status Again(const std::string& msg) {
    return Status(kAgain, msg);
  }

  static Status Exist(const std::string& msg) {
    return Status(kExist, msg);
  }

  static Status Disconnect(const std::string& msg) {
    return Status(kDisconnect, msg);
  }

  static Status Timeout(const std::string& msg) {
    return Status(kTimeout, msg);
  }

  static Status Cached(const std::string& msg) {
    return Status(kCached, msg);
  }

  static Status FromRpcError(const rpc::Error& err);

 private:
  enum Code {
    kOk = 0,
    kNotLeader = 1,
    kNotFound = 2,
    kSessionExpired = 4,
    kInvalidArgument = 5,
    kInternal = 6,
    kAgain = 7,
    kExist = 8,
    kDisconnect = 9,
    kTimeout = 10,
    kCached = 11,
  };

  Status(Code code, const std::string& msg) : code_(code), msg_(msg) {}

  Code code_;
  std::string msg_;
};

}

#endif
