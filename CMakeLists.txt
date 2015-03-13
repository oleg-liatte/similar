cmake_minimum_required(VERSION 2.8.4)
project(similar)

cmake_policy(SET CMP0054 NEW) # Only interpret ``if()`` arguments as variables or keywords when unquoted.

set(CMAKE_CXX_FLAGS "--std=c++11")

set(SRC
	main.cpp
	directory.cpp
	hasher.cpp
	spanhash.cpp
	SHA1.cpp
	progress.cpp
	match_map.cpp
)

message(STATUS "CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
if("${CMAKE_SYSTEM_NAME}" STREQUAL "CYGWIN")
	set(SRC ${SRC}
		directory_lister_unix.cpp
	)
endif()

add_executable(similar ${SRC})

