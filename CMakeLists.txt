cmake_minimum_required(VERSION 2.8.4)
project(similar)

cmake_policy(SET CMP0054 NEW) # Only interpret ``if()`` arguments as variables or keywords when unquoted.

set(CMAKE_CXX_FLAGS "-std=c++11 -flto -pthread")

set(SRC
	main.cpp
	directory_unix.cpp
	directory_walker.cpp
	hasher.cpp
	spanhash.cpp
	SHA1.cpp
	progress.cpp
	async_manager.cpp
)

add_executable(similar ${SRC})

target_link_libraries(similar
	pthread
)

install(TARGETS similar DESTINATION /usr/bin)
