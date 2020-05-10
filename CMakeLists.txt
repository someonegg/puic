cmake_minimum_required (VERSION 3.9)

set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN TRUE)

project (puic)

cmake_policy(SET CMP0063 NEW)

message("SYSTEM ${CMAKE_SYSTEM} : PROCESSOR ${CMAKE_SYSTEM_PROCESSOR}")
if (IOS)
	message("IOS : ${CMAKE_OSX_ARCHITECTURES}")
endif ()

if (NOT WIN32 AND NOT ANDROID)
	set(C_CXX_FLAGS "-pthread")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_CXX_FLAGS}")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${C_CXX_FLAGS}")
endif ()

add_definitions(-D__STDC_FORMAT_MACROS)
add_definitions(-D__STDC_LIMIT_MACROS)
add_definitions(-D__STDC_CONSTANT_MACROS)

if (WIN32)
	# On Windows, prefer cl over gcc if both are available. By default most of
	# the CMake generators prefer gcc, even on Windows.
	set(CMAKE_GENERATOR_CC cl)

	add_compile_options("/source-charset:utf-8")

	add_definitions(-DWIN32_LEAN_AND_MEAN)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	add_definitions(-D_WIN32_WINNT=0x0600)
endif ()

if (MSVC)
	add_definitions(-D_HAS_EXCEPTIONS=0)
endif ()

if (IOS)
	set(C_CXX_FLAGS "-Wno-shorten-64-to-32")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_CXX_FLAGS}")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${C_CXX_FLAGS}")
endif ()

add_subdirectory(puicbase)
add_subdirectory(puicclient)
add_subdirectory(puicserver)
if (NOT IOS AND NOT ANDROID)
	add_subdirectory(puicagent)
	add_subdirectory(puicproxy)
endif ()

if (NOT IOS AND NOT ANDROID)
	add_subdirectory(third_party/argtable2)
endif ()
add_subdirectory(third_party/protobuf)
add_subdirectory(third_party/libuv)

if (NOT PUIC_NO_EMBEDDED_BORINGSSL)
	add_subdirectory(third_party/boringssl)
endif ()
