#ifndef PYXIS_BASE_PROTOBUF_H
#define PYXIS_BASE_PROTOBUF_H

#include <string>
#include <google/protobuf/message.h>

namespace pyxis {
namespace protobuf {

// 将一个protobuffer message 打包为一个string
// 之后可以使用Decode解开
std::string Encode(const google::protobuf::Message& m);

google::protobuf::Message* Decode(const std::string& b);

}
}
#endif
