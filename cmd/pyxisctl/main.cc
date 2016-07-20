#include <gflags/gflags.h>
#include <glog/logging.h>
#include <toft/base/string/algorithm.h>
#include <pyxis/client/client.h>
#include <pyxis/common/types.h>
#include <pyxis/common/watch_event.h>
#include <stdio.h>
#include <iostream>

DEFINE_string(addr, "", "server address");
DEFINE_int32(timeout, 5, "session timeout in s");
DEFINE_int64(sid, 0, "session id");
DEFINE_string(cache, "/tmp/pyxisctl", "cache dir");
DEFINE_bool(enable_cache, true, "enable cache");
DEFINE_bool(debug, false, "enable debug log");

pyxis::Client* client = NULL;

void WatchFunc(const pyxis::watch::Event& e)
{
  printf("watcher - path:%s, event:%d\n", e.Node.c_str(), e.Type);
  if (e.Type == pyxis::watch::kModify) {
    pyxis::ReadOptions options;
    options.WatchEvent = pyxis::watch::kModify;
    pyxis::Status st = client->Read(options, e.Node, NULL);
  }
}

int main(int argc, char* argv[])
{
  google::InitGoogleLogging(argv[0]);
  google::ParseCommandLineFlags(&argc, &argv, true);
  pyxis::Options options;
  options.Sid = FLAGS_sid;
  options.EnableCache = FLAGS_enable_cache;
  options.CacheDir = FLAGS_cache;
  options.Timeout = FLAGS_timeout * 1000;
  options.EnableDebugLog = FLAGS_debug;

  pyxis::Status st = pyxis::Client::Open(options, FLAGS_addr, WatchFunc, &client);
  if (!st.ok()) {
    std::cerr << "open client error" << st.ToString();
    abort();
  }
  std::string line;
  while (true) {
    std::cout << "pyxis> ";
    if (!std::getline(std::cin, line)) {
      break;
    }
    std::vector<std::string> args;
    toft::SplitString(line, " ", &args);
    if (line.empty()) {
      continue;
    }
    if (args[0] == "write") {
      if (args.size() < 3) {
        std::cerr << "wrong args" << std::endl;
        continue;
      }
      st = client->Write(pyxis::WriteOptions(), args[1], args[2]);
      if (!st.ok()) {
        std::cerr << st.ToString() << std::endl;
      }
    } else if (args[0] == "read") {
      if (args.size() < 2) {
        std::cerr << "wrong args" << std::endl;
        continue;
      }
      pyxis::ReadOptions options;
      options.WatchEvent = pyxis::watch::kModify;
      pyxis::Node node;
      st = client->Read(options, args[1], &node);
      if (!st.ok()) {
        if (st.IsCached()) {
          std::cout << "cache: " << node.Data << std::endl;
        }
        std::cerr << st.ToString() << std::endl;
        continue;
      }
      std::cout << "Data:" << node.Data << std::endl;
    } else if (args[0] == "create") {
      if (args.size() < 2) {
        std::cerr << "wrong args" << std::endl;
        continue;
      }
      st = client->Create(pyxis::WriteOptions(), args[1], "");
      if (!st.ok()) {
        std::cerr << st.ToString() << std::endl;
      }
      continue;
    }
  }
  return 0;
}
