#include <pyxis/common/address.h>
#include <stdio.h>
#include <stdint.h>

namespace pyxis {

muduo::net::InetAddress ToMuduoAddr(const std::string& from)
{
  muduo::net::InetAddress to(0);
  std::string portstr;
  size_t pos = from.find(":");
  if (pos == std::string::npos) {
    return to;
  }

  std::string ip;
  uint16_t port;
  ip = from.substr(0, pos);
  if (ip.empty()) {
    ip = "0.0.0.0";
  }

  portstr = from.substr(pos + 1);
  if (sscanf(portstr.c_str(), "%hu", &port) < 1) {
    return to;
  }
  return muduo::net::InetAddress(ip, port);
}

}
