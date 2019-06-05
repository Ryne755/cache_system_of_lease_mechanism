/*
 * protobuf_message_common.h
 *
 *  Created on: May 28, 2019
 *      Author: rynzen <chuanrui123@126.com>
 *
 *  This file is part of a cache system of lease mechanism implemenation.
 *
 *  protobuf_message_common.h is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as published
 *  by the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  protobuf_message_common.h is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with consistent_hashing.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#include <type_traits>
#include "cache_data_center.h"
#include "common.h"
#include "cache_message.pb.h"
#include "socket_group.h"

using CacheMessageProto::CacheMessageHeader;
using CacheMessageProto::CacheReadRequest;
using CacheMessageProto::CacheUpdateRequest;
using CacheMessageProto::CacheOpResponse;
using CacheMessageProto::CacheMessage;

#define HEADER_VERSION      1
#define HEADER_MAGIC        0x34EC27D9
#define PRINTF_HEADER(t) 	LOG_OUT("MAGIC:0x%x version:%u type:0x%x op_id:0x%x", \
									t.magic(), t.version(), t.type(),t.op_id());

#define PRINTF_MESSAGE_INFO(PREFIX,message) do { LOG_OUT(PREFIX " message type 0x%x op_id 0x%x", \
								message->mutable_header()->type(),message->mutable_header()->op_id());}while(0)

CACHE_NAMESPACE_BEGIN

//depend on this define of proto buffer cache_data ,current is std::string
using CacheDataType=std::decay_t<decltype(std::declval<CacheOpResponse>().cache_data())>;

class CacheMessageRaii
{
public:
	CacheMessageRaii(CacheMessage* message) :message_(message) {}
	~CacheMessageRaii() {
		if (message_->has_header())
			message_->release_header();
		if (message_->has_read_request())
			message_->release_read_request();
		if (message_->has_update_request())
			message_->release_update_request();
		if (message_->has_op_response())
			message_->release_op_response();
		//request->unsafe_arena_release_update_request();
		//response->unsafe_arena_release_header();
		//response->unsafe_arena_release_op_response();
	}
private:
	CacheMessage* message_;
};

inline bool header_available(CacheMessage* message) {
	if (!message || !message->has_header()) {
		LOG_OUT("error invalid message header\n");
		return false;
	}
	const CacheMessageHeader& header = message->header();
	//PRINTF_HEADER(header);
	if (HEADER_MAGIC != header.magic())
	{
		LOG_OUT("error invalid message header magic\n");
		return false;
	}
	if (CacheMessageProto::CacheMessageType type = header.type();
		(type< CacheMessageProto::kReadRequest || type > CacheMessageProto::kInvalidateCache)) {
		LOG_OUT("error type out of range 0x%x\n", type);
		return false;
	}
	if (!header.op_id()) {
		LOG_OUT("error unset op_id\n");
		return false;
	}
	return true;
}

inline uint16_t do_send_cache_message(const std::shared_ptr<ProtoSocket>& socket, const CacheMessage* message) {
	static std::string str{};
	if (nullptr == socket || nullptr == message){
		throw csn::Exception(csn::Exception::kErrorSysRoutine, "null socket or message");
	}
	message->SerializeToString(&str);
	if (str.size() == 0) {
		throw csn::Exception(csn::Exception::kErrorSysRoutine, "SerializeToString() generate a zero length message");
	}
	return socket->do_send(str);
}
CACHE_NAMESPACE_END