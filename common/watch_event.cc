#include <pyxis/common/watch_event.h>

namespace pyxis {
namespace watch {

void BitsToSet(std::set<EventType>* set, int mask)
{
  EventType map[] = {watch::kNone, watch::kCreate, watch::kDelete,
               watch::kModify, watch::kChildren};
  for (EventType* p = map; p != map + sizeof(map) / sizeof(EventType); p++) {
    if (mask & *p) {
      set->insert(*p);
    }
  }
}

int SetToBits(const std::set<EventType>& set)
{
  int mask = 0;
  for (std::set<EventType>::iterator it = set.begin();
       it != set.end(); it++) {
    mask |= *it;
  }
  return mask;
}

std::string Event::String() const
{
  std::string typ;
  switch (Type) {
    case kNone:
      typ = "None";break;
    case kCreate:
      typ = "Create";break;
    case kDelete:
      typ = "Delete";break;
    case kModify:
      typ = "Modify";break;
    case kChildren:
      typ = "Children";break;
    case kConnected:
      typ = "Connected";break;
    case kDisconnected:
      typ = "Disconnected";break;
    default:
      typ = "???";
  }
  return "[" + Node + ", " + typ + "]";
}

}
}
