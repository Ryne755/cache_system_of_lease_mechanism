
/*
 * message_client.h
 *
 *  Created on: May 28, 2019
 *      Author: rynzen <chuanrui123@126.com>
 *
 *  This file is part of a cache system if lease mechanism implemenation.
 *
 *  message_client.h is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as published
 *  by the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  message_client.h is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with consistent_hashing.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#include <memory>
#include <string>
#include <functional>
#include "common.h"
#include "socket_group.h"

CACHE_NAMESPACE_BEGIN

class MessageClientImpl :public std::enable_shared_from_this<MessageClientImpl> {
public:
	using CallbackHandleType=std::function<void(csn::OpResult result, std::time_t expire,
		uint32_t cache_id, CacheDataType cache_data)>;
	MessageClientImpl() = default;
	void bind_socket(std::shared_ptr<ProtoSocket> socket) {
		socket_ = socket;
	}
	virtual ~MessageClientImpl() = default;
	virtual void on_receive( const std::string& data) = 0;
	virtual void read_cache_async(uint32_t cache_id, CallbackHandleType handle) = 0;
	virtual void update_cache_async(uint32_t cache_id, CacheDataType cache_data, CallbackHandleType handle)=0;
protected:
	std::shared_ptr<ProtoSocket> socket_;
};

template <typename SocketType>
class MessageClient :public SocketType {
	static_assert(std::is_base_of_v<ProtoSocket, SocketType>, "SocketType should be base of ProtoSocket!!!!");
public:
	MessageClient() = default;
	void set_message_impl(std::shared_ptr<MessageClientImpl> impl) {
		impl_ = impl;
		if (impl_)
			impl_->bind_socket(this->shared_from_this());
	}
	void on_receive(const std::string& data) override
	{
		if (!impl_)
			throw csn::Exception(csn::Exception::kErrorIllUsage, "on_receive null implment");
		impl_->on_receive(data);
	}
	void read_cache_async(uint32_t cache_id, MessageClientImpl::CallbackHandleType handle) {
		if (!impl_)
			throw csn::Exception(csn::Exception::kErrorIllUsage, "read_cache_async null implment");
		impl_->read_cache_async(cache_id, std::move(handle));
	}
	void update_cache_async(uint32_t cache_id, CacheDataType cache_data,MessageClientImpl::CallbackHandleType handle) {
		if (!impl_)
			throw csn::Exception(csn::Exception::kErrorIllUsage, "update_cache_async null implment");
		impl_->update_cache_async(cache_id, cache_data,std::move(handle));
	}
private:
	std::shared_ptr<MessageClientImpl> impl_;
};
CACHE_NAMESPACE_END