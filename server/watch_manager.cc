#include <glog/logging.h>
#include <pyxis/server/watch_manager.h>

namespace pyxis {

WatchManager::WatchManager()
    : watchers_()
{
}

WatchManager::~WatchManager()
{
}

void WatchManager::Add(const std::string& path, const WatcherPtr& watcher)
{
  watchers_.insert(std::make_pair(path, watcher));
}

void WatchManager::Remove(const WatcherPtr& w)
{
  std::pair<WatchList::iterator, WatchList::iterator> ret = watchers_.equal_range(w->Path());
  for (WatchList::iterator it = ret.first; it != ret.second; it++) {
    if (w->Sid() == it->second->Sid()) {
      watchers_.erase(it);
      return;
    }
  }
}

void WatchManager::Triger(const watch::Event& event)
{
  std::string path = event.Node;
  trigerInternal(path, event, false);
  path = path.substr(0, path.rfind("/"));
  while (!path.empty()) {
    trigerInternal(path, event, true);
    path = path.substr(0, path.rfind("/"));
  }
  trigerInternal("/", event, true);
}

void WatchManager::trigerInternal(const std::string& path, const watch::Event& e, bool fromChild)
{
  std::pair<WatchList::iterator, WatchList::iterator> ret = watchers_.equal_range(path);

  for (WatchList::iterator it = ret.first; it != ret.second;) {
    const WatcherPtr& w = it->second;
    if ((!fromChild || w->IsRecursive()) && (w->Mask() & e.Type)) {
      w->Notify(e);
      watchers_.erase(it++);
    } else {
      it++;
    }
  }
}

}
