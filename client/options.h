#ifndef PYXIS_CLIENT_OPTIONS_H
#define PYXIS_CLIENT_OPTIONS_H

#include <stdint.h>
#include <string>
#include <boost/function.hpp>
#include <pyxis/common/watch_event.h>
#include <pyxis/common/node.h>
#include <pyxis/common/types.h>

namespace pyxis {

typedef boost::function<void (const pyxis::watch::Event&)> WatchCallback;

struct Options {
  // session timeout, in ms. default 30s
  uint32_t Timeout;

  // default ./cache
  std::string CacheDir;

  // default true
  bool EnableCache;

  // 当服务挂掉的时候,可以利用曾经的sid注册,
  // 利用提供的sid注册, 默认为0
  sid_t Sid;

  // 开启debug日志, 默认不开启
  bool EnableDebugLog;

  Options()
      : Timeout(30 * 1000),
        CacheDir("./cache"),
        EnableCache(true),
        Sid(0),
        EnableDebugLog(false)
  {
  }
};

struct WriteOptions {
  // 创建节点的flags,默认为kPersist
  NodeFlags Flags;

  WriteOptions()
      : Flags(kPersist)
  {
  }
};

struct ReadOptions {
  // 监听事件, 默认不监听
  uint32_t WatchEvent;

  // 是否递归监听事件
  bool Recursive;

  // 监听的回调,默认使用全局回调
  WatchCallback Callback;

  ReadOptions()
      : WatchEvent(watch::kNone),
        Recursive(false),
        Callback()
  {
  }
};

}

#endif
