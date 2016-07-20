#include <pyxis/server/watcher.h>

namespace pyxis {

Watcher::Watcher(const std::string& path, sid_t sid,
                 watch::EventType mask, bool recursive)
    : path_(path),
      sid_(sid),
      mask_(mask),
      recursive_(recursive)
{
}

void Watcher::Notify(const watch::Event& e)
{
  cb_(e);
}

}
