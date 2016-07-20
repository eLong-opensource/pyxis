#ifndef PYXIS_COMMON_WATCH_EVENT_H
#define PYXIS_COMMON_WATCH_EVENT_H

#include <set>
#include <string>
#include <pyxis/common/types.h>

namespace pyxis {
namespace watch {

enum EventType {
  kAll          = 0xFFFFFFFF,
  kNone         = 0x00000000,
  kCreate       = 0x00000001,
  kDelete       = 0x00000002,
  kModify       = 0x00000004,
  kChildren     = 0x00000008,
  kConnected    = 0x00000010,
  kDisconnected = 0x00000020
};

class Event {
 public:

  std::string Node;
  EventType Type;

  Event()
      : Node(),
        Type(kNone)
  {
  }

  Event(const std::string& node, EventType type)
      : Node(node),
        Type(type)
  {
  }

  bool operator <(const Event& w) const
  {
    if (Node < w.Node) {
      return true;
    }
    if ((Node == w.Node) && (Type < w.Type)) {
      return true;
    }
    return false;
  }

  bool operator ==(const Event& e) const
  {
    return Node == e.Node && Type == e.Type;
  }

  std::string String() const ;
};

void BitsToSet(std::set<EventType>* set, int mask);

}
}

#endif
