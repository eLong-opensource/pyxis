/*
 * timer.h
 *
 *  Created on: Dec 17, 2013
 *      Author: fan
 */

#ifndef PYXIS_BASE_TIMER_H_
#define PYXIS_BASE_TIMER_H_

#include <string>
#include <sys/timerfd.h>
#include <boost/noncopyable.hpp>
#include <muduo/base/Mutex.h>
#include <muduo/base/Timestamp.h>
#include <glog/logging.h>
#include <pyxis/base/selectable.h>
#include <pyxis/base/selector.h>

namespace pyxis {
class Timer: public Selectable, public boost::noncopyable {
 public:
  Timer()
      : closed_(false)
  {
    tfd_ = ::timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    PCHECK(tfd_ > 0) << "create timerfd error";
  }

  ~Timer()
  {
    ::close(tfd_);
  }

  void After(int64_t ms)
  {
    struct itimerspec new_value;
    new_value.it_interval.tv_nsec = 0;
    new_value.it_interval.tv_sec = 0;
    new_value.it_value.tv_nsec = static_cast<long>((ms % 1000) * 1000000);
    new_value.it_value.tv_sec = static_cast<time_t>(ms / 1000);
    int ret = ::timerfd_settime(tfd_, 0, &new_value, NULL);
    PCHECK(ret == 0) << "set timer error";
  }

  void At(const muduo::Timestamp& when)
  {
    muduo::Timestamp now = muduo::Timestamp::now();
    if (when < now) {
      return;
    }
    double diff = muduo::timeDifference(when, now);
    After(static_cast<int64_t>(diff * 1000));
  }

  void Wait()
  {
    Selector selector;
    selector.Poll(this, -1);
  }

  int Fd() const
  {
    return tfd_;
  }

  bool IsReadable()
  {
    uint64_t one;
    return read(tfd_, &one, sizeof(one)) > 0;
  }

 private:
  int tfd_;
  bool closed_;
  muduo::MutexLock mutex_;
};

typedef boost::shared_ptr<Timer> TimerPtr;
}

#endif /* TIMER_H_ */
