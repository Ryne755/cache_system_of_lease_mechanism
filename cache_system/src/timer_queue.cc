/*
 * timer_queue.cc
 *
 *  Created on: May 28, 2019
 *      Author: rynzen <chuanrui123@126.com>
 *
 *  This file is part of a cache system if lease mechanism implemenation.
 *
 *  timer_queue.cc is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as published
 *  by the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  timer_queue.cc is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with consistent_hashing.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <vector>
#include <algorithm>
#include <functional>
#include <chrono>
#include <utility>
#include <mutex>
#include <memory>
#include "common.h"
#include "timer_queue.h"

CACHE_NAMESPACE_BEGIN
TimerQueue TimerQueue::queue_{};

const size_t TimerQueue::add_timer(TimerCallHandler&& callback,
	uint32_t timeout_ms, uint32_t repeat) {
	if (!repeat)
		throw Exception(Exception::kErrorIllArgument, "error timer repeat count");

	std::lock_guard<std::mutex> lock(mutex_);
	const std::unique_ptr<EventTimer>& timer = timer_queue_.emplace_back(
		std::make_unique<EventTimer>(std::forward<TimerCallHandler>(callback),
			timeout_ms, repeat));
	size_t timer_id = timer->timer_id();
	//heap up
	//warnning:would change timer reference !!!
	std::push_heap(timer_queue_.begin(), timer_queue_.end(), compare_);
	return timer_id;
}
//
void TimerQueue::del_timer(const size_t timer_id) {

	std::lock_guard<std::mutex> lock(mutex_);
	for (auto& t : timer_queue_) {
		if (t->timer_id() == timer_id) {
			//just make timer unavailable
			t->repeat_ = 0;
		}
	}
}
void TimerQueue::tick() {
	if (timer_queue_.empty())
		return;
	std::unique_ptr<EventTimer>& timer = timer_queue_.front();
	while (timer->expired()) {
		//here we actually delete a timer  
		std::lock_guard<std::mutex> lock(mutex_);
		if (timer->repeat_ > 0) {
			timer->callback_();
			timer->repeat_--;
		}
		if (!timer->repeat_) {
			//heap down
			std::pop_heap(timer_queue_.begin(), timer_queue_.end(), compare_);
			timer_queue_.pop_back();
		}
		else
		{
			timer->reset();
			//heap down
			std::pop_heap(timer_queue_.begin(), timer_queue_.end(), compare_);
			//heap up
			std::push_heap(timer_queue_.begin(), timer_queue_.end(), compare_);
		}
		if (timer_queue_.empty())
			break;
		std::unique_ptr<EventTimer>& timer = timer_queue_.front();
	}
}
CACHE_NAMESPACE_END