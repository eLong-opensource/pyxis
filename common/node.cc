#include <pyxis/common/node.h>
#include <pyxis/proto/node.pb.h>

namespace pyxis {

void CopyNodeFromProto(Node* node, const store::Node& proto)
{
  node->Data = proto.data();
  node->Flags = NodeFlags(proto.flags());
  node->Version = proto.version();
  node->Sid = proto.sid();
  for (int i = 0; i<proto.children().size(); i++) {
    node->Children.push_back(proto.children(i));
  }
}

}
