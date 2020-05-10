
if (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	set(C_CXX_FLAGS "-Wall")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_CXX_FLAGS}")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 ${C_CXX_FLAGS}")
elseif (MSVC)
	add_definitions(/wd4244 /wd4267 /wd4800 /wd4996)
endif ()

if (WIN32)
	add_definitions(-DNOMINMAX)
	add_definitions(-DUNICODE)
	add_definitions(-D_UNICODE)
endif ()

include_directories(./include ./src ../puicbase ../third_party/libuv/1.18.0/include)

set(
	PUICSERVER_SOURCES

	src/puic_server.cc
	src/puic_server_lib.cc
)

add_library(
	puicserver

	${PUICSERVER_SOURCES}
)

if (BUILD_SHARED_LIBS)
	add_definitions(-DPUICSERVER_SHARED_LIBRARY)
	add_definitions(-DPUICSERVER_IMPLEMENTATION)
endif ()

target_link_libraries(puicserver puicbase uvext)
