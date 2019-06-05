/*
 * soucket_group.h
 *
 *  Created on: May 28, 2019
 *      Author: rynzen <chuanrui123@126.com>
 *
 *  This file is part of a cache system of lease mechanism implemenation.
 *
 *  soucket_group.h is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as published
 *  by the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  soucket_group.h is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with consistent_hashing.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#include <memory>
#include <functional>
#include <type_traits>
#include <stdint.h>

CACHE_NAMESPACE_BEGIN
//! Socket abstraction layer
class ProtoSocket:public std::enable_shared_from_this<ProtoSocket> {
public:
	virtual ~ProtoSocket() = default;
	virtual void on_receive(const std::string& data) = 0;
	virtual uint16_t do_send(const std::string& data) = 0;
	virtual void initialize(const std::string& host_local, uint16_t local_port,
		const std::string& remote_host, uint16_t remote_port)=0;
};

//! SocketGroupImpl abstraction layer
template <typename T>
class SocketGroupImpl :public std::enable_shared_from_this<SocketGroupImpl<T>> {
public:
	static_assert(std::is_base_of_v<ProtoSocket, T>,"socket type should be base of ProtoSocket!!!!");
	using SocketType=T;
	SocketGroupImpl() = default;
	virtual ~SocketGroupImpl() = default;
	virtual void register_socket(std::shared_ptr<SocketType> socket) = 0;
	virtual void unregister_socket(std::shared_ptr<SocketType> socket) = 0;
	virtual void listen(double waitUpToSeconds = 0.0) = 0;
};

template <class SocketGroupImplType>
class SocketGroup {
	using SocketType=typename SocketGroupImplType::SocketType;
	static_assert(std::is_base_of_v<SocketGroupImpl<SocketType>, SocketGroupImplType> , "implement type should be base of SocketGroupImplType!!!!");
public:
	SocketGroup():impl_(std::make_shared<SocketGroupImplType>()){}
	void register_socket(std::shared_ptr<SocketType> socket) {
		if (socket == nullptr) {
			throw csn::Exception(csn::Exception::kErrorSysRoutine, "insert a nullptr");
		}
		if (!impl_)
			throw csn::Exception(csn::Exception::kErrorIllUsage, "impl should be set first !!!");
		impl_->register_socket(socket);
	}
	void unregister_socket(std::shared_ptr<SocketType> socket) {
		if (socket == nullptr)
			throw csn::Exception(csn::Exception::kErrorSysRoutine, "insert a nullptr");
		if(impl_)
			impl_->unregister_socket(socket);
	}
	//Let the SocketGroup poll from all sockets, events will be triggered here
	void listen(double waitUpToSeconds = 0.0) {
		if (!impl_)
			throw csn::Exception(csn::Exception::kErrorIllUsage, "impl should be set first !!!");
		impl_->listen(waitUpToSeconds);
	}
private:
	std::shared_ptr<SocketGroupImplType> impl_;
};
CACHE_NAMESPACE_END