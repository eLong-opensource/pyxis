#include <pyxis/client/client.h>
#include <pyxis/client/client_impl.h>

namespace pyxis {

Client::~Client()
{
}

Status Client::Open(const Options& options,
                    const std::string& addrList,
                    const WatchCallback& cb,
                    Client** cli)
{
  if (cli == NULL) {
    return Status::InvalidArgument("client");
  }
  *cli = NULL;
  client::ClientImpl* impl = new client::ClientImpl(options, addrList, cb);
  Status st = impl->Start();
  if (!st.ok()) {
    usleep(1000);
    delete impl;
    return st;
  }
  *cli = impl;
  return st;
}


}
