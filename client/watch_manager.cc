#include <pyxis/client/watch_manager.h>
#include <boost/bind.hpp>

namespace pyxis {
namespace client {

WatchManager::WatchManager(const WatchCallback& cb)
    : defaultWatch_(new Watcher),
      watchers_(),
      mutex_(),
      threadPool_("pyxis.client.watchpool")
{
  defaultWatch_->Done = cb;
}

WatchManager::~WatchManager()
{
  threadPool_.stop();
}

void WatchManager::Start(int poolSize)
{
  threadPool_.start(poolSize);
}

void WatchManager::Clear()
{
  muduo::MutexLockGuard guard(mutex_);
  watchers_.clear();
}

void WatchManager::Walk(const WalkCallback& cb)
{
  WatchMap t;
  {
      muduo::MutexLockGuard guard(mutex_);
      t = watchers_;
  }
  WatchMap::iterator it;
  for (it = t.begin(); it != t.end(); it++) {
    cb(it->second);
  }
}

void WatchManager::Add(const std::string& path,
                       const WatcherPtr& watcher)
{
  muduo::MutexLockGuard guard(mutex_);
  watchers_.insert(std::make_pair(path, watcher));
}

void WatchManager::Remove(const std::string& path)
{
  muduo::MutexLockGuard guard(mutex_);
  watchers_.erase(path);
}

void WatchManager::Triger(const std::string& path, const watch::Event& e)
{
  WatcherPtr watcher;
  {
    muduo::MutexLockGuard guard(mutex_);
    std::map<std::string, WatcherPtr>::iterator it;
    it = watchers_.find(path);
    if (it == watchers_.end()) {
      watcher = defaultWatch_;
    } else {
      watcher = it->second;
      watchers_.erase(it);
    }
  }
  threadPool_.run(boost::bind(&WatchManager::trigerInternal, this,
                              watcher, e));
}

void WatchManager::trigerInternal(const WatcherPtr& watcher,
                                  const watch::Event& e)
{
  watcher->Done(e);
}

}
}
