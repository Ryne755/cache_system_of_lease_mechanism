/*
 * message_server.h
 *
 *  Created on: May 28, 2019
 *      Author: rynzen <chuanrui123@126.com>
 *
 *  This file is part of a cache system of lease mechanism implemenation.
 *
 *  message_server.h is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as published
 *  by the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  message_server.h is distributed in the hope that it will be useful,
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
#include <type_traits>
#include "common.h"
#include "socket_group.h"
CACHE_NAMESPACE_BEGIN

class MessageServerImpl:public std::enable_shared_from_this<MessageServerImpl>{
public:
	MessageServerImpl() = default;
	void bind_socket(std::shared_ptr<ProtoSocket> socket) {
		socket_ = socket;
	}
	virtual ~MessageServerImpl() = default;
	virtual void on_receive(const std::string& data)=0;
protected:
	std::shared_ptr<ProtoSocket> socket_;
};

template <typename SocketType>
class MessageServer:public SocketType {
	static_assert(std::is_base_of_v<ProtoSocket, SocketType>, "SocketType should be base of ProtoSocket!!!!");
public:
	MessageServer() = default;
	void set_message_impl(std::shared_ptr<MessageServerImpl> impl) {
		impl_ = impl;
		if(impl_)
			impl_->bind_socket(this->shared_from_this());
	}
	void on_receive(const std::string& data) override
	{
		if(unlikely(!impl_))
			throw csn::Exception(csn::Exception::kErrorIllUsage, "null implment");
		impl_->on_receive(data);
	}
private:
	std::shared_ptr<MessageServerImpl> impl_;
};
CACHE_NAMESPACE_END