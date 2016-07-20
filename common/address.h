#ifndef PYXIS_COMMON_ADDRESS_H
#define PYXIS_COMMON_ADDRESS_H

#include <string>
#include <muduo/net/InetAddress.h>

namespace pyxis {

muduo::net::InetAddress ToMuduoAddr(const std::string& from);

}

#endif
