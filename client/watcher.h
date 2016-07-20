#ifndef PYXIS_CLIENT_WATCHER_H
#define PYXIS_CLIENT_WATCHER_H

#include <string>
#include <pyxis/common/watch_event.h>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

namespace pyxis {

typedef boost::function<void (const watch::Event&)> WatchCallback;

struct Watcher {
  std::string Path;
  watch::EventType Mask;
  bool Recursive;
  WatchCallback Done;
};

typedef boost::shared_ptr<Watcher> WatcherPtr;

}

#endif
