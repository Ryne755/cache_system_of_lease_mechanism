/*
 * timer_queue.cc
 *
 *  Created on: May 28, 2019
 *      Author: rynzen <chuanrui123@126.com>
 *
 *  This file is part of a cache system of lease mechanism implemenation.
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
#include <cstdarg>
#include <cstdio>
#include <string>
#include <ctime>
#include <chrono>
#include <cstring>

#if defined(OS_WINDOWS)
#include <windows.h>
#define snprintf _snprintf
#else
#include <unistd.h>
#endif
#include "common.h"

CACHE_NAMESPACE_BEGIN
#if defined(OS_WINDOWS)
const char* basename(const char* s) {
	const char* p_last_slash = std::strrchr(s,'/');
	const char* p_base_name = p_last_slash ? p_last_slash + 1 : s;
	return p_base_name;
}
#endif

inline int get_time_cstr(char* timerbuf, uint32_t bufferlen) {
	int ret = 0;
	std::time_t t = std::time(NULL);
	struct tm* c = localtime(&t);
	if (timerbuf)
		ret = snprintf(timerbuf, bufferlen, "%d-%d-%d %d:%d:%d", c->tm_year + 1900, c->tm_mon + 1, c->tm_mday, c->tm_hour, c->tm_min, c->tm_sec);
	return ret;
}

int write_log(const char* file_path, const char* func_name, const int line_no, const char* fmt, ...)
{
	int ret=0;
	va_list args;
#define BUFFER_LENGTH 256
	char buffer[BUFFER_LENGTH] = "";
	char* p = buffer;

	int length = get_time_cstr(p, BUFFER_LENGTH);
	length +=snprintf(p+length,BUFFER_LENGTH -length," %s %s() %d:", basename(file_path),func_name,line_no);
	p += length;

	va_start(args, fmt);
	length += vsnprintf(p, BUFFER_LENGTH-length,fmt,args);
	va_end(args);
#if defined(OS_WINDOWS)
	//HANDLE stderr_handle = ::GetStdHandle(STD_ERROR_HANDLE);
	HANDLE stderr_handle = ::GetStdHandle(STD_OUT_HANDLE);
	DWORD bytes_written = 0;
	::WriteFile(stderr_handle, buffer, length, &bytes_written, 0);
#else
	//::write(STDERR_FILENO, buffer, length);
	::write(STDOUT_FILENO, buffer, length);
#endif

	return length;
}
CACHE_NAMESPACE_END