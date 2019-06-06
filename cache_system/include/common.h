/*
 * common.h
 *
 *  Created on: May 28, 2019
 *      Author: rynzen <chuanrui123@126.com>
 *
 *  This file is part of a cache system of lease mechanism implemenation.
 *
 *  common.h is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as published
 *  by the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  common.h is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with consistent_hashing.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <stdint.h>
#include <chrono>
#include <string>

#define CACHE_NAMESPACE_BEGIN namespace csn {
#define CACHE_NAMESPACE_END }

#if defined(_WIN32) || defined(__WIN32__) || defined(_MSC_VER)
#define OS_WINDOWS
#define likely(x)   (x)
#define unlikely(x) (!x)
#else
#define OS_LINUX
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

CACHE_NAMESPACE_BEGIN
enum OpResult {
	kOperationOk = 0,  //read or update success
	kOperationDefer,   //call_handle after all Guaranteed operation expire 
	kOperationRetry,
	kOperationErrorArgument,
	kOperationErrorNoData,
};

class Exception {
public:
	enum Code {
		kErrorOutOfRange,
		kErrorIllArgument,
		kErrorSysRoutine,
		kErrorReadFormat,
		kErrorIllUsage,
		kErrorRead,
		kErrorWrite,
	};
	Exception(Code c, std::string describe) :code_(c), describe_(describe) {}
	Code code() { return code_; }
	const char* what() { return describe_.c_str(); }
private:
	Code code_;
	std::string describe_;
};
CACHE_NAMESPACE_END

CACHE_NAMESPACE_BEGIN

inline std::time_t get_time_stamp(uint32_t milliseconds) {
	std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tp =
		std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() +
			std::chrono::milliseconds(milliseconds));
	return tp.time_since_epoch().count();
}
inline std::time_t get_time_stamp()
{
	return get_time_stamp(0);
}

inline int32_t expire_milliseconds_of_timestamp(std::time_t t)
{
     return (int32_t)(t-get_time_stamp(0));
}

extern int write_log(const char* file_path, const char* func_name, const int line_no, const char* fmt, ...);

CACHE_NAMESPACE_END

#if defined(OS_WINDOWS)
#define LOG_OUT(fmt,...)  do{ \
	csn::write_log(__FILE__,__FUNCTION__, __LINE__, fmt"\r\n",__VA_ARGS__); \
}while(0)
#else
#define LOG_OUT(fmt, args ...)  do{ \
	csn::write_log(__FILE__,__FUNCTION__, __LINE__, fmt"\r\n", ##args); \
}while(0)
#endif

#include <exception>
#define CATCH_EXPTIONS catch (netLink::Exception e) { LOG_OUT("netLink::Exception code:%d",e.code);} \
	catch (csn::Exception e) { LOG_OUT("csn::Exception code:%d describe:%s", e.code(), e.what()); } \
	catch (std::exception e) { LOG_OUT("std::exception code:%s",e.what());} 
