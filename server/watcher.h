#ifndef PYXIS_SERVER_WATCHER_H
#define PYXIS_SERVER_WATCHER_H

#include <pyxis/common/types.h>
#include <pyxis/common/watch_event.h>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

namespace pyxis {

typedef boost::function<void (const watch::Event&)> WatchCallback;

class Watcher {
 public:
  Watcher(const std::string& path, sid_t sid, watch::EventType mask, bool recursive);

  void SetWatchCallback(const WatchCallback& cb) { cb_ = cb; }

  bool IsRecursive() { return recursive_; }

  std::string& Path() { return path_; }

  sid_t Sid() {return sid_;}

  watch::EventType Mask() { return mask_; }

  void Notify(const watch::Event& e);

 private:
  std::string path_;
  sid_t sid_;
  watch::EventType mask_;
  bool recursive_;
  WatchCallback cb_;
};

typedef boost::shared_ptr<Watcher> WatcherPtr;
typedef boost::weak_ptr<Watcher> WatcherWeakPtr;
}

#endif
