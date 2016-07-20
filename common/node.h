#ifndef PYXIS_COMMON_NODE_H
#define PYXIS_COMMON_NODE_H

#include <stdint.h>
#include <string>
#include <vector>

namespace pyxis {
namespace store {
class Node;
}
}

namespace pyxis {

enum NodeFlags {
  kPersist = 0x00,
  kEphemeral = 0x01
};

struct Node {
  std::string Data;
  std::vector<std::string> Children;
  NodeFlags Flags;
  uint64_t Version;
  uint64_t Sid;
};

void CopyNodeFromProto(Node* node, const store::Node& proto);

}

#endif
