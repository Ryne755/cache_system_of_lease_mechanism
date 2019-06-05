/*
 * timer_queue.h
 *
 *  Created on: May 28, 2019
 *      Author: rynzen <chuanrui123@126.com>
 *
 *  This file is part of a cache system of lease mechanism implemenation.
 *
 *  timer_queue.h is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as published
 *  by the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  timer_queue.h is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with consistent_hashing.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <vector>
#include <algorithm>
#include <mutex>
#include <memory>
#include <functional>
#include "common.h"
// 
// threadsafe tiny timer,maybe remove lock and manipulate it with threadlocal in future
// Example:
//		//tick in a thread
//      int epfd = epoll_create(1);
//		while (1){
//			epoll_wait(epfd, NULL, 1, 1000);
//			TimerQueue::get_timer_queue()->tick();
//		}
//		//create a timer with a simple lambda function, 3 second expire, repeat 10 times
//      TimerQueue::get_timer_queue()->add_timer([]() {LOG_OUT("timer1 run"); }, 3000, 10);

CACHE_NAMESPACE_BEGIN
class TimerQueue
{
public:
	using TimerCallHandler = std::function<void(void)>;
	class TimerCompare;
	class EventTimer
	{
	public:
		//callback: std::function<void(void)>
		//timeout_ms:timer expire millisecond 
		//repeat: timer repeat counts
		explicit EventTimer(TimerCallHandler&& callback, uint32_t timeout_ms, uint32_t repeat = 1) :
			callback_(std::move(callback)),
			timeout_ms_(timeout_ms),
			expire_time_(get_time_stamp(timeout_ms)),
			repeat_(repeat) {}

	private:
		//just for repeat_ > 1
		void reset()
		{
			expire_time_ = get_time_stamp(timeout_ms_);
		}
		bool expired() { return expire_time_ <= get_time_stamp(); }
		bool operator<(const EventTimer& timer) { return expire_time_ < timer.expire_time_; }
		//timer_id,identified by this
		size_t timer_id() { return reinterpret_cast<size_t>(this); }
	private:
		friend class TimerQueue;
		friend class TimerCompare;
		uint32_t repeat_;
		uint32_t timeout_ms_;
		std::time_t expire_time_;
		TimerCallHandler callback_;
	};
	struct TimerCompare {
		bool operator()(const std::unique_ptr<EventTimer>& t1, const std::unique_ptr<EventTimer>& t2) {
			return (t2->expire_time_) < (t1->expire_time_);
		}
	};
	//return : timer id
	//callback: std::function<void(void)>
	//timeout_ms:timer expire millisecond 
	//repeat: timer repeat counts
	const size_t add_timer(TimerCallHandler&& callback,
		uint32_t timeout_ms, uint32_t repeat = 1);
	//
	void del_timer(const size_t timer_id);
	void tick();

	static TimerQueue* get_timer_queue() {
		return &queue_;
	}
private:
	std::vector<std::unique_ptr<EventTimer>> timer_queue_;
	TimerCompare  compare_;
	std::mutex    mutex_;

	TimerQueue() = default;
	static TimerQueue queue_;
};
CACHE_NAMESPACE_END