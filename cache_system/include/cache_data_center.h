/*
 * cache_state_machine.h
 *
 *  Created on: May 28, 2019
 *      Author: rynzen <chuanrui123@126.com>
 *
 *  This file is part of a cache system of lease mechanism implemenation.
 *
 *  cache_data_center.h is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as published
 *  by the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  cache_data_center.h is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with consistent_hashing.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#include <functional>
#include "common.h"
CACHE_NAMESPACE_BEGIN
using UpdateCallHandler=std::function<void(OpResult status, uint32_t op_id, std::time_t expire)>;
const uint32_t kDefaultExpireMillisecond = 10000u;
CACHE_NAMESPACE_END

#include <string>
#include <unordered_map>
#include <memory>
#include <type_traits>
#include "cache_state_manager.h"

CACHE_NAMESPACE_BEGIN
template <typename T>
struct CacheElement {
	using ValueType=typename std::remove_cv<T>::type;
	static_assert(!std::is_reference_v<ValueType>&& !std::is_const_v<ValueType>, "value type should not be reference or const");
	//simple value 
	CacheElement() :value_(), state_(std::make_unique<CacheStateManager>()) {}
	void call_handle(OpResult status, uint32_t op_id, std::time_t expire) {
		if (status == OpResult::kOperationOk)
			value_ = std::move(temp_value_);
		call_(status, op_id, expire);
	}

	OpResult read_op(uint32_t op_id/*IN*/, std::time_t* expire/*OUT*/, ValueType* value) {
		*value = value_;
		return state_->read_op(op_id, expire);
	}
	template< typename U, typename = std::enable_if_t<std::is_same_v<std::decay_t<U>, ValueType>>>
	OpResult update_op(U&& value, uint32_t op_id/*IN*/, UpdateCallHandler f, std::time_t* expire) {
		OpResult r = state_->update_op(&CacheElement::call_handle, this, op_id, expire);
		if (OpResult::kOperationOk == r)
			value_ = std::move(value);
		else if (OpResult::kOperationDefer == r) {
			temp_value_ = std::move(value);
			op_id_ = op_id;
			call_ = std::move(f);
		}
		return r;
	}
private:
	//for defer call back
	uint32_t							 op_id_;
	//acturally storage in cache
	ValueType							 value_;
	ValueType							 temp_value_;
	UpdateCallHandler                    call_;
	std::unique_ptr<CacheStateManager>   state_;
};

template <typename T>
class CacheDataCenter : public std::enable_shared_from_this<CacheDataCenter<T>> {
public:
	using ValueType=T;
	using iterator=typename std::unordered_map<uint64_t, CacheElement<ValueType>>::iterator;
	static_assert(!std::is_reference_v<ValueType> && !std::is_const_v<ValueType>, "value type should not be reference or const");

	CacheDataCenter() = default;
	
	OpResult read_op(uint64_t cache_id, uint32_t op_id/*IN*/, std::time_t* expire/*OUT*/, ValueType* value) {
		iterator it = map_.find(cache_id);
		if (it == map_.end()) {
			return OpResult::kOperationErrorNoData;
		}
		return it->second.read_op(op_id, expire, value);
	}
	//
	template< typename U>
	OpResult update_op(uint64_t cache_id/*IN*/, U&& value/*IN*/, uint32_t op_id/*IN*/, UpdateCallHandler f/*IN*/, std::time_t* expire/*OUT*/) {
		iterator it = map_.find(cache_id);
		if (it == map_.end()) {
			//no value
			std::pair<iterator, bool> pair= map_.emplace(cache_id, CacheElement<ValueType>());
			if (pair.second == false)
				throw Exception(Exception::kErrorSysRoutine,"unordered_map insert data error !!!");
			it = pair.first;
		}
		return it->second.update_op(std::forward<U>(value), op_id, std::move(f), expire);
	}
private:
	std::unordered_map<uint64_t, CacheElement<ValueType>> map_;
};
CACHE_NAMESPACE_END
