#ifndef PYXIS_SERVER_OPTIONS_H
#define PYXIS_SERVER_OPTIONS_H

#include <string>
#include <vector>

namespace pyxis {

struct Options {
  // leveldb dir
  std::string DBDir;

  // snapshot dir
  std::string SnapshotDir;

  // max session timeout, in ms, default 120s
  int MaxSessionTimeout;

  // min session timeout in ms, default 5s
  int MinSessionTimeout;

  // session check interval, in ms, default 1s
  int SessionCheckInterval;

  // proc root path, default /pyxis
  std::string ProcRoot;

  // myid
  int MyID;

  // address list of pyxis server
  std::vector<std::string> AddrList;

  // size of rpc channel, default 1024
  int RpcChannelSize;

  Options()
      : DBDir(""),
        SnapshotDir(""),
        MaxSessionTimeout(2 * 60 * 1000),
        MinSessionTimeout(5 * 1000),
        SessionCheckInterval(1000),
        ProcRoot("/pyxis"),
        RpcChannelSize(1024)
  {
  }
};

}

#endif
