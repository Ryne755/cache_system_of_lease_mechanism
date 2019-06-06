/*
 * protobuf_message_client_impl.h
 *
 *  Created on: May 28, 2019
 *      Author: rynzen <chuanrui123@126.com>
 *
 *  This file is part of a cache system of lease mechanism implemenation.
 *
 *  protobuf_message_client_impl.h is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as published
 *  by the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  protobuf_message_client_impl.h is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with consistent_hashing.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#include <google/protobuf/arena.h>
#include "cache_data_center.h"
#include "snowflake.h"
#include "cache_message.pb.h"
#include "common.h"
#include "protobuf_message_common.h"
#include "message_client.h"
CACHE_NAMESPACE_BEGIN
class CacheClientOperation {
public:
	using CallbackHandleType=std::function<void(csn::OpResult result, std::time_t expire, uint32_t cache_id, CacheDataType cache_data)>;
	CacheClientOperation(std::shared_ptr<google::protobuf::Arena> arena,
		uint64_t op_id, CallbackHandleType handle,
		std::shared_ptr<ProtoSocket> socket) :arena_(arena),
		op_id_(op_id), cache_id_(), handle_(std::move(handle)), socket_(socket) {}
	virtual ~CacheClientOperation() = default;
	virtual void process_response(CacheMessage* response) {
		google::protobuf::Arena* arena_ptr = response->GetArena();
		if (!arena_ptr) {
			LOG_OUT("get Arena failure !!!!");
			throw csn::Exception(csn::Exception::kErrorSysRoutine, "arena_ptr should not be null");
		}
		if (!response->has_op_response()) {
			throw csn::Exception(csn::Exception::kErrorSysRoutine, "ill format op_response");
		}
		PRINTF_MESSAGE_INFO("rcv", response);
		CacheMessageRaii res_raii(response);
		CacheMessageProto::CacheOpResponse* op_response = response->mutable_op_response();
		if (cache_id_ != op_response->cache_id()) {
			LOG_OUT("match error cache_id %u when expect cache_id %u", op_response->cache_id(), cache_id_);
			throw csn::Exception(csn::Exception::kErrorSysRoutine, "match error cache_id");
		}
		handle_((csn::OpResult)op_response->result(), op_response->expire(), op_response->cache_id(), op_response->cache_data());
		//send ack to server
		do_send_ack(response);
	}
	void do_send_ack(CacheMessage* response) {
		CacheMessage* ack = google::protobuf::Arena::CreateMessage<CacheMessage>(arena_.get());
		CacheMessageRaii ack_raii(ack);
		ack->unsafe_arena_set_allocated_header(response->unsafe_arena_release_header());
		prepare_header(CacheMessageProto::kOperationAck, ack);

		do_send_cache_message(socket_, ack);
		PRINTF_MESSAGE_INFO("send", ack);
	}
protected:
	void prepare_header(CacheMessageProto::CacheMessageType type/*IN*/, CacheMessage* message/*OUT*/) {
		//set response header
		CacheMessageHeader* header = message->mutable_header();
		header->set_type(type);
		header->set_magic(HEADER_MAGIC);
		header->set_version(HEADER_VERSION);
		header->set_op_id(op_id_);
	}
	virtual void prepare_request(CacheMessage* message/*OUT*/, uint32_t expire_time_ms) {}
protected:
	std::shared_ptr<google::protobuf::Arena> arena_;
	uint64_t						 op_id_;
	uint32_t						 cache_id_;
	CallbackHandleType				 handle_;
	std::shared_ptr<ProtoSocket>	 socket_;
};

class CacheClientReadOpration :public CacheClientOperation {
public:
	CacheClientReadOpration(std::shared_ptr<google::protobuf::Arena> arena,
		uint64_t op_id, CallbackHandleType handle, std::shared_ptr<ProtoSocket> socket) :
		CacheClientOperation(arena, op_id, handle, socket) {}
	void do_send_request(uint32_t cache_id, uint32_t expire_time_ms)
	{
		CacheMessage* request = google::protobuf::Arena::CreateMessage<CacheMessage>(arena_.get());
		CacheMessageRaii req_raii(request);
		cache_id_ = cache_id;
		prepare_header(CacheMessageProto::kReadRequest, request);
		prepare_request(request, expire_time_ms);
		do_send_cache_message(socket_, request);
	}
protected:
	void prepare_request(CacheMessage* message/*OUT*/, uint32_t expire_time_ms) override {
		CacheMessageProto::CacheReadRequest* read_request = message->mutable_read_request();
		read_request->set_cache_id(cache_id_);
		read_request->set_timestamp(get_time_stamp());
		read_request->set_expire(expire_time_ms);
	}
};
class CacheClientUpdateOpration :public CacheClientOperation {
public:
	CacheClientUpdateOpration(std::shared_ptr<google::protobuf::Arena> arena,
		uint64_t op_id, CallbackHandleType handle,
		std::shared_ptr<ProtoSocket> socket) :
		CacheClientOperation(arena, op_id, handle, socket) {}
	void do_send_request(uint32_t cache_id, CacheDataType cache_data, uint32_t expire_time_ms)
	{
		CacheMessage* request = google::protobuf::Arena::CreateMessage<CacheMessage>(arena_.get());
		CacheMessageRaii req_raii(request);
		cache_id_ = cache_id;
		cache_data_ = cache_data;
		prepare_header(CacheMessageProto::kUpdateRequest, request);
		prepare_request(request, expire_time_ms);
		do_send_cache_message(socket_, request);
	}
protected:
	void prepare_request(CacheMessage* message/*OUT*/, uint32_t expire_time_ms) override {
		CacheMessageProto::CacheUpdateRequest* update_request = message->mutable_update_request();
		update_request->set_cache_id(cache_id_);
		update_request->set_cache_data(cache_data_);
		update_request->set_timestamp(get_time_stamp());
		update_request->set_expire(expire_time_ms);
	}
private:
	CacheDataType cache_data_;
};

class ProtobufMessageClientImpl :public MessageClientImpl {
public:
	ProtobufMessageClientImpl(uint8_t datacenter_id, uint8_t worker_id) :
		MessageClientImpl(), arena_(std::make_shared<google::protobuf::Arena>()),
		requests_{}, snowflake_(datacenter_id, worker_id){}
	void on_receive(const std::string& data) override
	{
		CacheMessage* message = google::protobuf::Arena::CreateMessage<CacheMessage>(arena_.get());
		if (!message->ParseFromString(data))
		{
			LOG_OUT("error failure ParseFromString\n");
			return;
		}
		if (unlikely(!header_available(message))) {
			return;
		}
		const CacheMessageHeader& header = message->header();
		auto iterator = requests_.find(header.op_id());
		if (iterator == requests_.end())
		{
			LOG_OUT("can't find match requests of op_id 0x%x!!!!!!\n", header.op_id());
			PRINTF_HEADER(header);
			return;
		}
		//CacheClientOperation
		iterator->second->process_response(message);
		requests_.erase(iterator);
	}
	void read_cache_async(uint32_t cache_id, CallbackHandleType handle) override {
		uint64_t op_id = snowflake_.generate_uniform_id();
		std::shared_ptr<CacheClientReadOpration> op = std::make_shared<CacheClientReadOpration>(arena_,
			op_id, std::move(handle), socket_);
		requests_.emplace(op_id, op);
		//TODO rynzen, temporary set 200ms expire time
		op->do_send_request(cache_id, 200);
	}
	void update_cache_async(uint32_t cache_id, CacheDataType cache_data, CallbackHandleType handle) override {
		uint64_t op_id = snowflake_.generate_uniform_id();
		std::shared_ptr<CacheClientUpdateOpration> op = std::make_shared<CacheClientUpdateOpration>(arena_,
			op_id, std::move(handle), socket_);
		requests_.emplace(op_id, op);
		//TODO rynzen, temporary set 200ms expire time
		op->do_send_request(cache_id, std::move(cache_data), 200);
	}
private:
	std::shared_ptr<google::protobuf::Arena>  arena_;
	std::map<uint64_t, std::shared_ptr<CacheClientOperation>> requests_;
	//to generate uniform id
	SnowFlake	snowflake_;
};
CACHE_NAMESPACE_END