/*
 * snowflake.h
 *
 *  Created on: May 28, 2019
 *      Author: rynzen <chuanrui123@126.com>
 *
 *  This file is part of a cache system of lease mechanism implemenation.
 *
 *  snowflake.h is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as published
 *  by the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  snowflake.h is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with consistent_hashing.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#include <memory>
#include "common.h"
CACHE_NAMESPACE_BEGIN
class SnowFlake :public std::enable_shared_from_this<SnowFlake> {
public:
	//datacenter_id : identity this mechine
	//worker_id     : identify this thread or process
	SnowFlake(uint8_t datacenter_id, uint8_t worker_id):
		machine_id_((((uint64_t)datacenter_id<<15)&0x3E0000)| (((uint64_t)worker_id << 10) & 0x1F0000)),
		sequence()
	{
	}
	uint64_t generate_uniform_id() {
		uint64_t id{};
		uint64_t timestamp = get_time_stamp();
		sequence++;
		id = (((uint64_t)timestamp << 22) & (~0x3F0000)) | machine_id_ | ((uint64_t)sequence & 0xFFF);
		return id;
	}
private:
	uint64_t machine_id_;
	uint8_t  worker_id_;
	uint16_t sequence;
};
CACHE_NAMESPACE_END