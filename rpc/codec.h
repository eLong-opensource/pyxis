#ifndef PYXIS_RPC_CODEC_H
#define PYXIS_RPC_CODEC_H

#include <muduo/net/protobuf/ProtobufCodecLite.h>

namespace pyxis {
namespace rpc {

extern const char kRPCTag[]; // "pyxi"

class Request;
class Response;

typedef boost::shared_ptr<Request> RequestPtr;
typedef boost::shared_ptr<Response> ResponsePtr;

typedef muduo::net::ProtobufCodecLiteT<Request, kRPCTag> RequestCodec;
typedef muduo::net::ProtobufCodecLiteT<Response, kRPCTag> ResponseCodec;

}
}

#endif
