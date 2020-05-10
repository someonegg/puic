cmake_minimum_required (VERSION 3.2)

project(protobuf C CXX)

cmake_policy(SET CMP0063 NEW)

set (protobuf_source_dir 3.3.0)

include_directories(${protobuf_source_dir}/src ${protobuf_source_dir})

add_definitions(-DGOOGLE_PROTOBUF_CMAKE_BUILD)

find_package(Threads REQUIRED)
if (CMAKE_USE_PTHREADS_INIT)
 	add_definitions(-DHAVE_PTHREAD)
endif ()

if (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	set(C_CXX_FLAGS "-Wall -Wno-sign-compare")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_CXX_FLAGS}")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 ${C_CXX_FLAGS}")
elseif (MSVC)
	# Allow big object
	add_definitions(/bigobj)
	add_definitions(/wd4244 /wd4267 /wd4018 /wd4355 /wd4800 /wd4251 /wd4996 /wd4146 /wd4305 /wd4309)
endif ()

set(protobuf_lite_files
	${protobuf_source_dir}/src/google/protobuf/arena.cc
	${protobuf_source_dir}/src/google/protobuf/arenastring.cc
	${protobuf_source_dir}/src/google/protobuf/extension_set.cc
	${protobuf_source_dir}/src/google/protobuf/generated_message_util.cc
	${protobuf_source_dir}/src/google/protobuf/io/coded_stream.cc
	${protobuf_source_dir}/src/google/protobuf/io/zero_copy_stream.cc
	${protobuf_source_dir}/src/google/protobuf/io/zero_copy_stream_impl_lite.cc
	${protobuf_source_dir}/src/google/protobuf/message_lite.cc
	${protobuf_source_dir}/src/google/protobuf/repeated_field.cc
	${protobuf_source_dir}/src/google/protobuf/stubs/atomicops_internals_x86_gcc.cc
	${protobuf_source_dir}/src/google/protobuf/stubs/atomicops_internals_x86_msvc.cc
	${protobuf_source_dir}/src/google/protobuf/stubs/bytestream.cc
	${protobuf_source_dir}/src/google/protobuf/stubs/common.cc
	${protobuf_source_dir}/src/google/protobuf/stubs/int128.cc
	${protobuf_source_dir}/src/google/protobuf/stubs/once.cc
	${protobuf_source_dir}/src/google/protobuf/stubs/status.cc
	${protobuf_source_dir}/src/google/protobuf/stubs/statusor.cc
	${protobuf_source_dir}/src/google/protobuf/stubs/stringpiece.cc
	${protobuf_source_dir}/src/google/protobuf/stubs/stringprintf.cc
	${protobuf_source_dir}/src/google/protobuf/stubs/structurally_valid.cc
	${protobuf_source_dir}/src/google/protobuf/stubs/strutil.cc
	${protobuf_source_dir}/src/google/protobuf/stubs/time.cc
	${protobuf_source_dir}/src/google/protobuf/wire_format_lite.cc
)

add_library(
	protobuflite # Different to "protobuf-lite".
	STATIC

	${protobuf_lite_files}
)
