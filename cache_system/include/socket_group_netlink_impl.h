/*
 * socket_group_netlink_impl.h
 *
 *  Created on: May 28, 2019
 *      Author: rynzen <chuanrui123@126.com>
 *
 *  This file is part of a cache system of lease mechanism implemenation.
 *
 *  socket_group_netlink_impl.h is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as published
 *  by the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  socket_group_netlink_impl.h is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with consistent_hashing.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#include <type_traits>
#include <functional>
#include "socket_group.h"
#include "netLink.h"

CACHE_NAMESPACE_BEGIN

//! UDP Socket abstraction layer
class UdpSocket : public netLink::Socket, public ProtoSocket{
	typedef netLink::Socket super;
	friend class SocketManager;
public:
	virtual ~UdpSocket() = default;
	//virtual void on_receive(const std::string& data) = 0;
	uint16_t do_send(const std::string& str) override {
		return send(str.c_str(), str.size());
	}
	void initialize(const std::string& host_local, uint16_t local_port,
		const std::string& remote_host, uint16_t remote_port) override
	{
		initAsUdpPeer(host_local, local_port);
		hostRemote = remote_host;
		portRemote = remote_port;
		setInputBufferSize(0);
	}
};

class SocketGroupNetlinkImpl :public SocketGroupImpl<UdpSocket> {
	enum {
		kMaxUdpPacketSize = 1492,
		kMaxBufferSize = 2000
	};
public:
	SocketGroupNetlinkImpl() :buffer_(new char[kMaxBufferSize] {},
		[](char* buffer) {delete[] buffer; }), manager_{}
	{
#ifdef WINVER
		netLink::init();
#endif
		manager_.onReceiveRaw = std::bind(&SocketGroupNetlinkImpl::on_receive, this, std::placeholders::_1, std::placeholders::_2);
	}
	~SocketGroupNetlinkImpl() = default;
	void register_socket(std::shared_ptr<UdpSocket> socket) override {
		if (socket == nullptr) {
			throw csn::Exception(csn::Exception::kErrorSysRoutine, "SocketGroupNetlinkImpl insert a nullptr");
		}
		manager_.sockets.insert(socket);
	}
	void unregister_socket(std::shared_ptr<UdpSocket> socket) override {
		//TODO rynzen should consider race condition here
		manager_.sockets.erase(socket);
	}
	//Let the SocketManager poll from all sockets, events will be triggered here
	void listen(double waitUpToSeconds = 0.0) override {
		manager_.listen(waitUpToSeconds);
	}
protected:
	void on_receive(netLink::SocketManager* manager, std::shared_ptr<netLink::Socket> socket)
	{
		uint16_t length = socket->sgetn(buffer_.get(), kMaxUdpPacketSize);
		if (unlikely(length == 0))
			throw csn::Exception(csn::Exception::kErrorRead, "error socket group error read");
		std::shared_ptr<UdpSocket> udpsocket = std::dynamic_pointer_cast<UdpSocket>(socket);
		if (unlikely(udpsocket == nullptr)) {
			throw csn::Exception(csn::Exception::kErrorSysRoutine, "unsuport original socket!!!!");
		}
		buffer_.get()[length] = '\0';
		udpsocket->on_receive(buffer_.get());
	}
	std::unique_ptr<char[], std::function<void(char*)>>  buffer_;
	netLink::SocketManager								 manager_;
};
CACHE_NAMESPACE_END