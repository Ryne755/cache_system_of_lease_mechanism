/*
 * protobuf_message_server_impl.h
 *
 *  Created on: May 28, 2019
 *      Author: rynzen <chuanrui123@126.com>
 *
 *  This file is part of a cache system of lease mechanism implemenation.
 *
 *  protobuf_message_server_impl.h is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as published
 *  by the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  protobuf_message_server_impl.h is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with consistent_hashing.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#include <iostream>
#include <memory>
#include <string>
#include <functional>
#include <type_traits>
#include <google/protobuf/arena.h>
#include "cache_data_center.h"
#include "common.h"
#include "cache_message.pb.h"
#include "socket_group.h"
#include "protobuf_message_common.h"
#include "message_server.h"
CACHE_NAMESPACE_BEGIN

class WaitCacheAck :public CacheMessageRaii
{
	using CustomCallHandler = std::function<void(void)>;
public:
	WaitCacheAck(const std::shared_ptr<ProtoSocket>& socket, CacheMessage* message, CustomCallHandler handle = nullptr) :
		CacheMessageRaii(message), socket_(socket), message_(message), handle_(std::move(handle)), timer_id_() {}
	~WaitCacheAck() = default;
	//resend cache response message
	void timer_handle() {
		do_send_cache_message(socket_, message_);
		if (handle_)
			handle_();
	}
	void set_custom_handle(CustomCallHandler handle) {
		handle_ = std::move(handle);
	}
	void set_timer_id(std::size_t timer_id) {
		timer_id_ = timer_id;
	}
	std::size_t timer_id() {
		return timer_id_;
	}
private:
	CacheMessage* message_;
	std::shared_ptr<ProtoSocket> socket_;
	CustomCallHandler handle_;
	std::size_t timer_id_;
};

class CacheWaitAcktManager {
	enum WaitAckTimeout {
		//500 ms expiretimer
		kDefaultTimeout = 500,
	};
public:
	void register_wait_ack(const std::shared_ptr<ProtoSocket>& socket, CacheMessage* message)
	{
		if (message == nullptr || !message->has_header())
			throw csn::Exception(csn::Exception::kErrorSysRoutine, "message or message header should not be null");

		std::shared_ptr<WaitCacheAck> ack = std::make_shared<WaitCacheAck>(socket, message);
		std::size_t timer_id = csn::TimerQueue::get_timer_queue()->add_timer(\
			std::bind(&WaitCacheAck::timer_handle, ack.get()), kDefaultTimeout, 1);
		uint64_t op_id = message->header().op_id();
		ack->set_custom_handle(std::bind(&CacheWaitAcktManager::expire_handle, this, op_id));
		ack->set_timer_id(timer_id);
		map_.emplace(op_id, ack);
	}
	void unregister_wait_ack(uint64_t op_id) {
		auto it = map_.find(op_id);
		if (it == map_.end()) {
			//TODO rynzen, miss some race condition check
			LOG_OUT("assume it was timeout and retransferred 0x%x", op_id);
			return;
		}

		std::size_t timer_id = it->second->timer_id();
		csn::TimerQueue::get_timer_queue()->del_timer(timer_id);
		map_.erase(it);
	}
	static CacheWaitAcktManager* get_wait_ack_manager() {
		static CacheWaitAcktManager mng{};
		return &mng;
	}
private:
	void expire_handle(uint64_t op_id) {
		auto it = map_.find(op_id);
		if (it == map_.end()) {
			throw csn::Exception(csn::Exception::kErrorSysRoutine, "WaitCacheAck should not be null");
		}
		map_.erase(it);
	}
	CacheWaitAcktManager() = default;
	std::map < uint64_t, std::shared_ptr<WaitCacheAck>> map_;
};
class CacheOperationInterface {
public:
	CacheOperationInterface(const std::shared_ptr<csn::CacheDataCenter<CacheDataType>>& center) :
		center_(center->shared_from_this()) {};
	virtual void on_process(const std::shared_ptr<ProtoSocket>& socket, CacheMessage* request) {
		LOG_OUT("rcv other cache operation %u", request->mutable_header()->type());
		CacheMessageRaii req_raii(request);
	}
	virtual ~CacheOperationInterface() = default;
protected:
	//set response body
	void prepare_op_response(CacheMessage* response, std::time_t timestamp,
		uint32_t cache_id, CacheDataType cache_data,
		csn::OpResult ret)
	{
		CacheOpResponse* op_response = response->mutable_op_response();
		op_response->set_expire(timestamp);
		op_response->set_cache_id(cache_id);
		op_response->set_cache_data(std::move(cache_data));
		op_response->set_result(ret);
	}
	void register_wait_ack(const std::shared_ptr<ProtoSocket>& socket, CacheMessage* message)
	{
		CacheWaitAcktManager::get_wait_ack_manager()->register_wait_ack(socket, message);
	}
	void unregister_wait_ack(uint64_t op_id)
	{
		CacheWaitAcktManager::get_wait_ack_manager()->unregister_wait_ack(op_id);
	}
	std::shared_ptr<csn::CacheDataCenter<CacheDataType>> center_;
};

class CacheAckOperation :public CacheOperationInterface {
public:
	CacheAckOperation(const std::shared_ptr<csn::CacheDataCenter<CacheDataType>>& center) :
		CacheOperationInterface(center) {};
	void on_process(const std::shared_ptr<ProtoSocket>& socket, CacheMessage* request) override {
		if (!request || !request->has_header()) {
			LOG_OUT("check ack failure !!!!");
		}
		PRINTF_MESSAGE_INFO("rcv", request);
		CacheMessageRaii req_raii(request);
		unregister_wait_ack(request->mutable_header()->op_id());
	}
	~CacheAckOperation() = default;
};
class CacheReadRequestOperation :public CacheOperationInterface {
public:
	CacheReadRequestOperation(const std::shared_ptr<csn::CacheDataCenter<CacheDataType>>& center) :
		CacheOperationInterface(center), timestamp_(), cache_id_(), cache_data_(), ret_() {}
	void on_process(const std::shared_ptr<ProtoSocket>& socket, CacheMessage* request) override {
		if (!request || !request->has_read_request()) {
			LOG_OUT("check read_request failure !!!!");
			return;
		}
		PRINTF_MESSAGE_INFO("rcv", request);
		CacheMessageRaii req_raii(request);
		query_cache_center(request);
		CacheMessage* response = prepare_response_message(request);
		register_wait_ack(socket, response);
		do_send_cache_message(socket, response);
		PRINTF_MESSAGE_INFO("send", response);
	}
	~CacheReadRequestOperation() = default;
private:
	CacheMessage* prepare_response_message(CacheMessage* request) {
		google::protobuf::Arena* arena_ptr = request->GetArena();
		if (!arena_ptr) {
			LOG_OUT("get Arena failure !!!!");
			throw csn::Exception(csn::Exception::kErrorSysRoutine, "arena_ptr should not be null");
		}
		CacheMessage* response = google::protobuf::Arena::CreateMessage<CacheMessage>(arena_ptr);
		response->unsafe_arena_set_allocated_header(request->unsafe_arena_release_header());
		//set response header
		CacheMessageHeader* header = response->mutable_header();
		header->set_type(CacheMessageProto::kReadResponse);
		//set response body
		prepare_op_response(response, timestamp_, cache_id_, cache_data_, ret_);
		return response;
	}
	csn::OpResult query_cache_center(CacheMessage* message) {
		cache_id_ = message->read_request().cache_id();
		ret_ = center_->read_op(cache_id_, message->header().op_id(), &timestamp_, &cache_data_);
		return ret_;
	}
private:
	std::time_t   timestamp_;
	uint32_t      cache_id_;
	CacheDataType   cache_data_;
	csn::OpResult ret_;
};

class CacheUpdateRequestOperation :public CacheOperationInterface {
	struct UpdateResult {
		std::time_t     timestamp;
		uint32_t        cache_id;
		CacheDataType   cache_data;
		csn::OpResult   ret;
	};
public:
	CacheUpdateRequestOperation(const std::shared_ptr<csn::CacheDataCenter<CacheDataType>>& center) :
		CacheOperationInterface(center),defer_messages_() {}
	~CacheUpdateRequestOperation() = default;
	void on_process(const std::shared_ptr<ProtoSocket>& socket, CacheMessage* request) {
		if (!request || !request->has_update_request()) {
			LOG_OUT("check update_request failure !!!!");
			return;
		}
		PRINTF_MESSAGE_INFO("rcv", request);
		UpdateResult result{};
		if (update_cache_center(request,socket,&result) != csn::kOperationDefer) {
			CacheMessageRaii req_raii(request);
			CacheMessage* response = prepare_response_message(request, result);
			register_wait_ack(socket, response);
			PRINTF_MESSAGE_INFO("send", response);
			do_send_cache_message(socket, response);
		}
	}
private:
	CacheMessage* prepare_response_message(CacheMessage* request, const UpdateResult& result) {
		google::protobuf::Arena* arena_ptr = request->GetArena();
		if (!arena_ptr) {
			LOG_OUT("get Arena failure !!!!");
			throw csn::Exception(csn::Exception::kErrorSysRoutine, "Arena should not be null");
		}
		CacheMessage* response = google::protobuf::Arena::CreateMessage<CacheMessage>(arena_ptr);
		response->unsafe_arena_set_allocated_header(request->unsafe_arena_release_header());
		//set response header
		CacheMessageHeader* header = response->mutable_header();
		header->set_type(CacheMessageProto::kUpdateResponse);

		prepare_op_response(response, result.timestamp, result.cache_id, result.cache_data, result.ret);
		return response;
	}
	void update_handle(std::shared_ptr<ProtoSocket> socket, csn::OpResult ret, uint32_t op_id, std::time_t expire) {
		using iterator=std::map<uint32_t, CacheMessage*>::iterator;
		if (ret != csn::OpResult::kOperationOk) {
			throw csn::Exception(csn::Exception::kErrorSysRoutine, "update callback throw a routine error");
		}
		iterator it = defer_messages_.find(op_id);
		if (it == defer_messages_.end()) {
			throw csn::Exception(csn::Exception::kErrorSysRoutine, "defer_messages_ should not be null");
		}
		CacheMessage* message = it->second;
		CacheMessageRaii msg_raii(message);
		UpdateResult result{};
		
		result.cache_id = message->update_request().cache_id();
		result.ret = ret;
		result.timestamp = expire;
		result.cache_data = message->update_request().cache_data();

		CacheMessage* response = prepare_response_message(message, result);
		register_wait_ack(socket, response);
		PRINTF_MESSAGE_INFO("send", response);
		do_send_cache_message(socket, response);
		//TODO rynzen, use raii modify erase messge
		defer_messages_.erase(it);
	}
	//result:no stuff when return csn::kOperationDefer
	csn::OpResult update_cache_center(CacheMessage* message, std::shared_ptr<ProtoSocket> socket, UpdateResult* result) {
		using namespace std::placeholders;
		CacheUpdateRequest* request = message->mutable_update_request();
		uint32_t op_id = message->header().op_id();
		OpResult ret{};
		ret = center_->update_op(request->cache_id(), request->cache_data(),
			op_id, std::bind(&CacheUpdateRequestOperation::update_handle, this, socket, _1, _2, _3),
			&result->timestamp);
		if (ret == csn::kOperationDefer) {
			defer_messages_.emplace(op_id, message);
		}
		else {
			result->cache_id = request->cache_id();
			result->ret= ret;
			result->cache_data = request->cache_data();
		}
		return ret;
	}
private:
	std::map<uint32_t, CacheMessage*> defer_messages_;
};


class ProtobufMessageServerImpl :public MessageServerImpl {
	enum CacheMessageCount {
		kCacheMessageCount = (CacheMessageProto::kInvalidateCache - CacheMessageProto::kReadRequest + 1),
	};
public:
	ProtobufMessageServerImpl() :MessageServerImpl{},arena_ {}, 
		center_(std::make_shared<CacheDataCenter<CacheDataType>>()),
		message_op_{
		std::make_shared<CacheReadRequestOperation>(center_),
		std::make_shared<CacheOperationInterface>(center_),
		std::make_shared<CacheUpdateRequestOperation>(center_),
		std::make_shared<CacheOperationInterface>(center_),
		std::make_shared<CacheAckOperation>(center_),
		std::make_shared<CacheOperationInterface>(center_)
	}{}
	~ProtobufMessageServerImpl() = default;
	void on_receive(const std::string& data) override
	{
		CacheMessage* request = google::protobuf::Arena::CreateMessage<CacheMessage>(&arena_);
		if (!request->ParseFromString(data) || !header_available(request))
		{
			LOG_OUT("error parsing message\n");
			return;
		}
		//LOG_OUT("on_receive 0x%x\n", request->header().type());
		uint32_t index = (uint32_t)(request->header().type() - CacheMessageProto::kReadRequest);
		if (index >= kCacheMessageCount)
			throw csn::Exception(csn::Exception::kErrorSysRoutine, "message type out of range !!!!!!!");
		message_op_[index]->on_process(socket_, request);
	}
	std::shared_ptr<csn::CacheDataCenter<CacheDataType>> data_center() { return center_; }
private:
	google::protobuf::Arena arena_;
	std::shared_ptr<csn::CacheDataCenter<CacheDataType>> center_;
	std::array<std::shared_ptr<CacheOperationInterface>, kCacheMessageCount> message_op_;
};
CACHE_NAMESPACE_END