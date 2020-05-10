
if (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	set(C_CXX_FLAGS "-Wall")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_CXX_FLAGS}")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 ${C_CXX_FLAGS}")
elseif (MSVC)
	add_definitions(/wd4244 /wd4267 /wd4800 /wd4996)
endif ()

if (CMAKE_COMPILER_IS_GNUCXX)
	set(CMAKE_EXE_LINKER_FLAGS "-static-libstdc++ -static-libgcc")
endif ()

if (WIN32)
	add_definitions(-DNOMINMAX)
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	include_directories(/usr/local/include)
	link_directories(/usr/local/lib)
endif ()

include_directories(./src ../common ../puicclient/include ../third_party/libuv/1.18.0/include ../third_party/argtable2/2.13/include ../third_party/spdlog/include)

set(
	PUICAGENT_SOURCES

	src/main.cc
	src/multi_proxy_main.cc
	src/proxy_main.cc
	src/puic_agent.cc
	src/agent_server.cc
)

add_executable(
	puicagent

	${PUICAGENT_SOURCES}
)

target_link_libraries(puicagent puicclient argtab2)
