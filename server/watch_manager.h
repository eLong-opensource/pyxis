#ifndef PYXIS_SERVER_WATCH_MANAGER_H
#define PYXIS_SERVER_WATCH_MANAGER_H

#include <pyxis/common/types.h>
#include <pyxis/common/watch_event.h>
#include <pyxis/server/watcher.h>
#include <map>
#include <boost/function.hpp>

namespace pyxis {

class WatchManager {
 public:
  WatchManager();
  ~WatchManager();

  // 添加一个watcher
  void Add(const std::string& path, const WatcherPtr& watcher);

  // 删除一个watcher
  void Remove(const WatcherPtr& watcher);

  // 触发一个事件
  void Triger(const watch::Event& e);

  // 是否有订阅
  bool HasWatcher(const std::string& path)
  {
    return watchers_.find(path) != watchers_.end();
  }

  // 清空所有订阅的事件
  void Reset() { watchers_.clear();}

 private:

  void trigerInternal(const std::string& path,
                      const watch::Event& e, bool fromChild);

  // 订阅事件到订阅者的一个映射
  typedef std::multimap<std::string, WatcherPtr> WatchList;

  WatchList watchers_;
};
}
#endif
