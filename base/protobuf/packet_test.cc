#include <pyxis/base/protobuf/packet.h>
#include <pyxis/base/protobuf/packet.pb.h>
#include <gtest/gtest.h>
#include <boost/scoped_ptr.hpp>

using namespace pyxis;

TEST(packet, encode)
{
  protobuf::Packet msg;
  msg.set_name("pyxis");
  msg.set_data("none");
  std::string buf = protobuf::Encode(msg);
  boost::scoped_ptr<google::protobuf::Message> output;
  output.reset(protobuf::Decode(buf));
  ASSERT_TRUE(NULL != output);
  protobuf::Packet* p = dynamic_cast<protobuf::Packet*>(output.get());
  ASSERT_TRUE(NULL != p);
  EXPECT_EQ("pyxis", p->name());
  EXPECT_EQ("none", p->data());
}
