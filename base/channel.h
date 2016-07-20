#ifndef PYXIS_BASE_CHANNEL_H
#define PYXIS_BASE_CHANNEL_H

#include <sys/eventfd.h>
#include <unistd.h>
#include <deque>

#include <boost/noncopyable.hpp>
#include <muduo/base/Mutex.h>
#include <muduo/base/Condition.h>
#include <glog/logging.h>
#include <pyxis/base/selectable.h>

namespace pyxis {

/* 加强版blockqueue。
 * Features:
 * 1.支援多线程的blockqueue
 * 2.支援selector查询。
 * 3.关闭channel只关闭写，读可以把剩下的都读完。
 * 分布在多个线程的selector同时查询，成功后会竞争
 */

template<typename T>
class Channel: public Selectable, public boost::noncopyable {
 public:
  Channel(size_t size = 0xFFFFFFFF)
      : mutex_(),
        notEmpty_(mutex_),
        notFull_(mutex_),
        size_(size),
        queue_(),
        closed_(false)
  {
    efd_ = ::eventfd(0, EFD_CLOEXEC | EFD_SEMAPHORE);
    PCHECK(efd_ > 0) << "Failed in eventfd";
  }

  ~Channel()
  {
    closed_ = true;
    ::close(efd_);
  }

  bool Put(const T& x)
  {
    muduo::MutexLockGuard guard(mutex_);
    if (closed_) {
      return false;
    }

    while (queue_.size() >= size_) {
      notFull_.wait();
    }

    queue_.push_back(x);
    uint64_t one = 1;
    PCHECK(write(efd_, &one, sizeof(one)) == sizeof(one)) << "put error";
    notEmpty_.notify();
    return true;
  }

  bool Take(T* t)
  {
    uint64_t one = 0;
    muduo::MutexLockGuard guard(mutex_);

    while (queue_.empty() && !closed_) {
      notEmpty_.wait();
    }

    if (queue_.empty() && closed_) {
      return false;
    }

    CHECK(!queue_.empty());
    int n = read(efd_, &one, sizeof(one));
    if (n != sizeof(one) && !closed_) {
      PCHECK(n == sizeof(one)) << "take error";
    }
    T front(queue_.front());
    queue_.pop_front();
    *t = front;

    notFull_.notify();

    return true;
  }

  bool Empty() const
  {
    muduo::MutexLockGuard guard(mutex_);
    return queue_.empty();
  }

  bool Full() const
  {
    muduo::MutexLockGuard guard(mutex_);
    return queue_.full();
  }

  bool Size() const
  {
    muduo::MutexLockGuard guard(mutex_);
    return queue_.size();
  }

  int Fd() const
  {
    return efd_;
  }

  bool IsReadable()
  {
    muduo::MutexLockGuard guard(mutex_);
    return !queue_.empty();
  }

  void Close()
  {
    muduo::MutexLockGuard guard(mutex_);
    closed_ = true;
    ::close(efd_);
    notEmpty_.notifyAll();
  }

 private:
  int efd_;
  mutable muduo::MutexLock mutex_;
  muduo::Condition notEmpty_;
  muduo::Condition notFull_;
  size_t size_;
  std::deque<T> queue_;
  std::string name_;
  bool closed_;
};

} // end of namespace

#endif
