#include <string>
#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>
#include <pyxis/base/protobuf/packet.pb.h>

namespace pyxis {
namespace protobuf {

using namespace google::protobuf;

std::string Encode(const Message& m)
{
  const Descriptor* desc = m.GetDescriptor();
  Packet p;
  p.set_name(desc->full_name());
  p.set_data(m.SerializeAsString());
  return p.SerializeAsString();
}

Message* Decode(const std::string& b)
{
  Packet p;
  if (!p.ParseFromString(b)) {
    return NULL;
  }
  const Descriptor* desc = DescriptorPool::generated_pool()->FindMessageTypeByName(p.name());
  if (desc == NULL) {
    return NULL;
  }
  const Message* proto = MessageFactory::generated_factory()->GetPrototype(desc);
  if (proto == NULL) {
    return NULL;
  }
  Message* payload = proto->New();
  if (payload == NULL) {
    return NULL;
  }
  if (!payload->ParseFromString(p.data())) {
    delete payload;
    return NULL;
  }

  return payload;
}

}
}
