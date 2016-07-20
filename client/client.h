#ifndef PYXIS_CLIENT_CLIENT_H
#define PYXIS_CLIENT_CLIENT_H

#include <pyxis/client/options.h>
#include <pyxis/common/status.h>
#include <time.h>

namespace pyxis {

class Client {
 public:
  Client() {}
  virtual ~Client();

  static Status Open(const Options& options,
                     const std::string& addrList,
                     const WatchCallback& cb,
                     Client** client);

  virtual Status Create(const WriteOptions& options,
                        const std::string& path,
                        const std::string& data) = 0;

  virtual Status Delete(const WriteOptions& options,
                        const std::string& path) = 0;

  virtual Status Write(const WriteOptions& options,
                       const std::string& path,
                       const std::string& data) = 0;

  virtual Status Read(const ReadOptions& options,
                      const std::string& path,
                      Node* node) = 0;

  virtual Status Sid(sid_t* sid) = 0;

  // last ping time
  virtual time_t LastPing() = 0;

 private:
  Client(const Client&);
  void operator= (const Client&);
};

}

#endif
