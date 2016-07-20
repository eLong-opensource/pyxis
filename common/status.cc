#include <pyxis/common/status.h>
#include <pyxis/proto/rpc.pb.h>

namespace pyxis {

std::string Status::ToString()
{
  switch (code_) {
    case kOk: return "";
    case kNotFound: return "NotFound:" + msg_;
    case kNotLeader: return "NotLeader:" + msg_;
    case kDisconnect: return "Disconnect:" + msg_;
    case kSessionExpired: return "SessionExpired:" + msg_;
    case kInvalidArgument: return "InvalidArgument:" + msg_;
    case kAgain: return "Again:" + msg_;
    case kExist: return "Exist:" + msg_;
    case kInternal: return "Internal:" + msg_;
    case kTimeout: return "Timeout:" + msg_;
    case kCached: return "Cached:" + msg_;
    default:
      return "???";
  }
}

void Status::ToRpcError(rpc::Error* err)
{
  err->set_data(msg_);
  switch(code_) {
    case kOk: err->set_type(rpc::kNone); break;
    case kNotLeader: err->set_type(rpc::kNotLeader); break;
    case kNotFound: err->set_type(rpc::kNotFound); break;
    case kSessionExpired: err->set_type(rpc::kSessionExpired); break;
    case kInvalidArgument: err->set_type(rpc::kInvalidArgument); break;
    case kAgain: err->set_type(rpc::kAgain); break;
    case kExist: err->set_type(rpc::kExist); break;
    default: err->set_type(rpc::kInternal); break;
  }
}

Status Status::FromRpcError(const rpc::Error& err)
{
  switch (err.type()) {
    case rpc::kNone: return Status::OK();
    case rpc::kNotFound: return Status::NotFound(err.data());
    case rpc::kNotLeader: return Status::NotLeader(err.data());
    case rpc::kSessionExpired: return Status::SessionExpired(err.data());
    case rpc::kInvalidArgument: return Status::InvalidArgument(err.data());
    case rpc::kAgain: return Status::Again(err.data());
    case rpc::kExist: return Status::Exist(err.data());
    default: return Status::Internal(err.data());
  }
}

}
