set (PROJECT_NAME sample)

if(MSVC)
	add_definitions(/std:c++latest)
else()
	add_definitions(-std=c++17) 
	if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
		add_definitions(-g)
		message("build type Debug ....")
	elseif(${CMAKE_BUILD_TYPE} MATCHES "Release")
		add_definitions(-Os)
		message("build type Release ....")
	endif()
endif()

include_directories(${_NETLINK_INCLUDE_DIR})
include_directories(${_CACHE_INCLUDE_DIR})

link_libraries(${_CACHE_LIBRARIES})
add_executable(sample_server server.cc)
add_executable(sample_client client.cc)