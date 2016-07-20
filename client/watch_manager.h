#ifndef PYXIS_CLIENT_WATCH_MANAGER_H
#define PYXIS_CLIENT_WATCH_MANAGER_H

#include <pyxis/common/watch_event.h>
#include <pyxis/client/options.h>
#include <pyxis/client/watcher.h>
#include <muduo/base/ThreadPool.h>
#include <muduo/base/Mutex.h>
#include <map>

namespace pyxis {
namespace client {

class WatchManager
{
 public:
  typedef boost::function<void (const WatcherPtr&)> WalkCallback;
  typedef std::map<std::string, WatcherPtr> WatchMap;

  WatchManager(const WatchCallback& defaultWatch);

  ~WatchManager();

  void Start(int poolSize);

  void Clear();

  void Add(const std::string& path, const WatcherPtr& watcher);

  void Remove(const std::string& path);

  void Walk(const WalkCallback& cb);

  void Triger(const std::string& path, const watch::Event& e);

 private:
  void trigerInternal(const WatcherPtr& watcher,
                      const watch::Event& e);
  WatcherPtr defaultWatch_;
  WatchMap watchers_;
  muduo::MutexLock mutex_;
  muduo::ThreadPool threadPool_;
};

}
}
#endif
