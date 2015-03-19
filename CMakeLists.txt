cmake_minimum_required(VERSION 2.8.4)
project(similar)

cmake_policy(SET CMP0054 NEW) # Only interpret ``if()`` arguments as variables or keywords when unquoted.

set(CMAKE_CXX_FLAGS "--std=c++11 --lto")
set(CMAKE_LINK_FLAGS "--lto")

set(SRC
	main.cpp
	directory_walker.cpp
	hasher.cpp
	spanhash.cpp
	SHA1.cpp
	progress.cpp
)

if(NOT "${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
	set(SRC ${SRC}
		directory_unix.cpp
	)
endif()

add_executable(similar ${SRC})

install(TARGETS similar DESTINATION /usr/bin)
