# CMakeList.txt: 顶层 CMake 项目文件，在此处执行全局配置
# 并包含子项目。
#

set (PROJECT_NAME cache_system)

if(MSVC)
	message("BUILD IN MSVC...")
	add_definitions(/std:c++latest)
elseif("${CMAKE_SYSTEM}" MATCHES "Linux")
	message("BUILD IN linux...")
	add_definitions(-std=c++17)
	if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
		add_definitions(-g)
		message("build type Debug ....")
	elseif(${CMAKE_BUILD_TYPE} MATCHES "Release")
		add_definitions(-Os)
		message("build type Release ....")
	endif()
else(UNIX)
	message("BUILD IN unix...")
	#TODO
endif()

if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
ADD_DEFINITIONS(-DBITS_PER_LONG=64)
else()
ADD_DEFINITIONS(-DBITS_PER_LONG=32)
endif()

if(MSVC)
set(_PROTO_PATH "E:/netdisk/C++/resource/vcpkg/packages/protobuf_x64-windows")
find_package(protobuf  CONFIG REQUIRED
	         PATHS ${_PROTO_PATH})
include_directories(${_PROTO_PATH}/include)
link_directories(${_PROTO_PATH}/lib)
find_package( netLink REQUIRED )
else()
find_package(Protobuf REQUIRED)
include_directories(/usr/local/include)
link_directories(/usr/local/lib)
endif()

#find_package(protobuf CONFIG REQUIRED)
# target_link_libraries(main PRIVATE protobuf::libprotoc protobuf::libprotobuf)

set(PROTO_PATH ${CMAKE_SOURCE_DIR}/cache_system/proto)
set(PROTO_FILE "${PROTO_PATH}/cache_message.proto")
set(cache_proto_hdrs "${PROTO_PATH}/cache_message.pb.h")
set(cache_proto_srcs "${PROTO_PATH}/cache_message.pb.cc")
if(MSVC)
set(PROTOBUF_PROTOC_EXECUTABLE "E:/netdisk/C++/resource/vcpkg/installed/x64-windows/tools/protobuf/protoc.exe")
endif()
add_custom_command(
OUTPUT "${cache_proto_hdrs}"
	   "${cache_proto_srcs}"
COMMAND ${PROTOBUF_PROTOC_EXECUTABLE}
ARGS "--proto_path=${PROTO_PATH}"
	 "--cpp_out=${PROTO_PATH}"
	 "${PROTO_FILE}"
)

set(_PROTOBUF_FILES ${cache_proto_srcs} ${cache_proto_hdrs})

include_directories(PUBLIC include ${PROTO_PATH})
include_directories(${_NETLINK_INCLUDE_DIR})
include_directories(${PROTO_PATH})

set(CACHE_SRCS "src/timer_queue.cc" 
	           "src/common.cc") 
#[[
#link_libraries(grpc++_unsecure grpc gpr ${PROTOBUF_LIBRARY})
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package (Threads REQUIRED)

if(MSVC)
#target_link_libraries(main PRIVATE protobuf::libprotoc protobuf::libprotobuf)
link_libraries(${_CACHE_LIBRARIES} protobuf::libprotoc protobuf::libprotobuf ${netLinkLib})
else()
link_libraries(${_CACHE_LIBRARIES} Threads::Threads )
endif()
]]

link_libraries(${PROTOBUF_LIBRARY} ${_NETLINK_LIBRARIES} )
add_library(cache_shared SHARED ${_PROTOBUF_FILES} ${CACHE_SRCS})
add_library(cache_static STATIC ${_PROTOBUF_FILES} ${CACHE_SRCS})
set_target_properties(cache_shared PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
set_target_properties(cache_static PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
