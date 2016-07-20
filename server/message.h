/*
 * message.h
 *
 *  Created on: Aug 11, 2014
 *      Author: fan
 */

#ifndef PYXIS_SERVER_MESSAGE_H_
#define PYXIS_SERVER_MESSAGE_H_

#include <boost/shared_ptr.hpp>
#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>

#include <pyxis/proto/rpc.pb.h>
#include <pyxis/base/channel.h>

namespace pyxis {

typedef google::protobuf::Message Message;
typedef boost::shared_ptr<Message> MessagePtr;
typedef Channel<MessagePtr> MessageChannel;
typedef boost::shared_ptr<MessageChannel> MessageChannelPtr;

const std::string kError = "Error";
const size_t kMaxMessageSize = 1024 * 1024 - 1024;

const std::string kRegisterRequest = "RegisterRequest";
const std::string kRegisterResponse = "RegisterReponse";
const std::string kUnRegisterRequest = "UnRegisterRequest";
const std::string kUnRegisterResponse = "UnRegisterResponse";
const std::string kPingRequest = "PingRequest";
const std::string kPingResponse = "PingResponse";

inline const std::string& Type(const Message* msg)
{
    const google::protobuf::Descriptor* desp = msg->GetDescriptor();
    return desp->name();
}

inline void Swap(Message* msg1, Message* msg2)
{
    const google::protobuf::Reflection* ref = msg1->GetReflection();
    ref->Swap(msg1, msg2);
}

}

#endif /* PYXIS_SERVER_MESSAGE_H_ */
