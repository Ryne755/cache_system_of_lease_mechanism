/*
 * cache_state_machine.h
 *
 *  Created on: May 28, 2019
 *      Author: rynzen <chuanrui123@126.com>
 *
 *  This file is part of a cache system of lease mechanism implemenation.
 *
 *  cache_state_machine.h is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as published
 *  by the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  cache_state_machine.h is distributed in the hope that it will be useful,
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
#include "timer_queue.h"
#include "common.h"
#include "cache_data_center.h"

CACHE_NAMESPACE_BEGIN
class CacheStateManager {
private:
	enum class CacheState:uint32_t{ kCacheIdle = 0, kCacheGuaranteed, kCacheUpdateProtected, kCacheStateCount };
	class CacheStateInterface :public std::enable_shared_from_this<CacheStateInterface> {
	public:
		//CacheStateInterface is private defined and used inside the CacheStateManager
		//so just use a refenence of CacheStateManager
		CacheStateInterface(CacheStateManager& mng) :mng_(mng) {}
		virtual ~CacheStateInterface() = default;
		//op_id:  request operation id 
		//return: OpResult
		virtual OpResult update_op(uint32_t op_id/*IN*/, UpdateCallHandler f/*IN*/, std::time_t* tp/*OUT*/) = 0;
		//op_id:  request operation id 
		//return: expire timepoint for this operation,usually is now()+kDefaultExpireMillisecond,
		//		  when enter CacheUpdateProtectedState,operation returns now();
		virtual OpResult read_op(uint32_t op_id/*IN*/, std::time_t* tp/*OUT*/) = 0;

		//enter this with operation id
		virtual void enter_state() = 0;
	protected:
		CacheStateManager& mng_;
	};
	class CacheIdleState :public CacheStateInterface {
	public:
		CacheIdleState(CacheStateManager& mng) :CacheStateInterface(mng) {}
		OpResult update_op(uint32_t op_id/*IN*/, UpdateCallHandler f/*IN*/, std::time_t* tp/*OUT*/) override {
			mng_.set_cache_state(CacheState::kCacheGuaranteed);
			*tp = mng_.expire_time();
			return kOperationOk;
		}
		OpResult read_op(uint32_t op_id/*IN*/, std::time_t* tp/*OUT*/) override {
			mng_.set_cache_state(CacheState::kCacheGuaranteed);
			*tp = mng_.expire_time();
			return kOperationOk;
		}
		void enter_state() override {
			mng_.set_timer_id(0);
			mng_.set_expire_time(0);
		}
	};
	struct CacheGuaranteedState :public CacheStateInterface {
	public:
		CacheGuaranteedState(CacheStateManager& mng) :CacheStateInterface(mng) {}
		OpResult update_op(uint32_t op_id/*IN*/, UpdateCallHandler f/*IN*/, std::time_t* tp/*OUT*/) override {
			if (!op_id || !f)
				throw Exception(Exception::kErrorIllArgument, "CacheGuaranteedState update_op with error argument");
			mng_.set_op_id(op_id);
			mng_.set_call_handle(f);
			mng_.set_cache_state(CacheState::kCacheUpdateProtected);
			*tp = mng_.expire_time();
			return  kOperationDefer;
		}
		OpResult read_op(uint32_t op_id/*IN*/, std::time_t* tp/*OUT*/) override {
			mng_.set_op_id(op_id);
			mng_.set_cache_state(CacheState::kCacheGuaranteed);
			*tp = mng_.expire_time();
			return kOperationOk;
		}
		void enter_state() override {
			mng_.start_expire([this]() {
				mng_.set_cache_state(CacheState::kCacheIdle); },
				kDefaultExpireMillisecond);
		}
	};
	class CacheUpdateProtectedState :public CacheStateInterface {
	public:
		CacheUpdateProtectedState(CacheStateManager& mng) :CacheStateInterface(mng) {}
		OpResult update_op(uint32_t op_id/*IN*/, UpdateCallHandler f/*IN*/, std::time_t* tp/*OUT*/) override {
			if (!op_id)
				throw Exception(Exception::kErrorIllArgument, "CacheUpdateProtectedState update_op with error argument");
			*tp = mng_.expire_time();
			return kOperationRetry;
		}
		OpResult read_op(uint32_t op_id/*IN*/, std::time_t* tp/*OUT*/) override {
			*tp = get_time_stamp();
			return kOperationOk;
		}
		void enter_state() override {
			int32_t  timer_delay = mng_.expire_time() - get_time_stamp();
			if (timer_delay <= 0)
				throw Exception(Exception::kErrorSysRoutine, "timer is expire,should not run here");

			auto function = [this]() {
				mng_.set_cache_state(CacheState::kCacheGuaranteed);
				(mng_.get_call_handle())(kOperationOk, mng_.op_id(), mng_.expire_time());
			};
			mng_.start_expire(std::move(function), (uint32_t)timer_delay);
		}
	};
public:
	explicit CacheStateManager() :timer_id_{},expire_time_ {}, op_id_{}, callhandle_{}, 
		current_state_(CacheState::kCacheIdle),
		states_{std::make_shared<CacheIdleState>(*this),
		std::make_shared<CacheGuaranteedState>(*this),
		std::make_shared<CacheUpdateProtectedState>(*this)}
	{
	}
	~CacheStateManager() { stop_expire(); }
	template <typename Function, typename ClassType>
	OpResult update_op(const Function& f/*IN*/, ClassType* t, uint32_t op_id/*IN*/, std::time_t* tp/*OUT*/) {
		using namespace std::placeholders;
		return cache_state()->update_op(op_id, std::bind(f, t, _1, _2, _3), tp);
	}
	template <typename Function>
	OpResult update_op(Function f/*IN*/, uint32_t op_id/*IN*/, std::time_t* tp/*OUT*/) {
		return cache_state()->update_op(op_id, f, tp);
	}

	OpResult read_op(uint32_t op_id/*IN*/, std::time_t* tp/*OUT*/) {
		return cache_state()->read_op(op_id, tp);
	}
	std::shared_ptr<CacheStateInterface> cache_state()  noexcept {
		return states_[static_cast<uint32_t>(current_state_)]->shared_from_this();
	}
	void set_cache_state(CacheState state){
		states_[static_cast<uint32_t>(state)]->enter_state();
		current_state_ = state;
	}

	size_t timer_id() { return timer_id_; }
	void   set_timer_id(size_t timer_id) { timer_id_ = timer_id; }

	std::time_t	 expire_time() { return expire_time_; }
	void   set_expire_time(size_t expire_time) { expire_time_ = expire_time; }

	uint32_t op_id() { return op_id_; }
	void   set_op_id(uint32_t op_id) { op_id_ = op_id; }

	UpdateCallHandler& get_call_handle() { return callhandle_; }
	void   set_call_handle(UpdateCallHandler call_handle) { callhandle_ = call_handle; }

	void start_expire(TimerQueue::TimerCallHandler&& call_back, uint32_t timeout_ms) {
		stop_expire();
		expire_time_ = get_time_stamp(timeout_ms);
		timer_id_ = TimerQueue::get_timer_queue()->add_timer(std::move(call_back), timeout_ms, 1);
	}
	void stop_expire() {
		if (timer_id_ && expire_time_ > get_time_stamp()) {
			TimerQueue::get_timer_queue()->del_timer(timer_id_);
			timer_id_ = 0;
		}
	}
private:
	size_t									timer_id_;
	std::time_t								expire_time_;
	uint32_t								op_id_;
	UpdateCallHandler						callhandle_;
	CacheState							    current_state_;
	std::array<std::shared_ptr<CacheStateInterface>, static_cast<uint32_t>(CacheState::kCacheStateCount)> states_;
};
CACHE_NAMESPACE_END