cmake_minimum_required (VERSION 3.2)

project(libuv C)

cmake_policy(SET CMP0063 NEW)

set (libuv_source_dir 1.18.0)

include_directories(${libuv_source_dir}/include ${libuv_source_dir}/src)

# UDP_DGRAM_MAXSIZE=1500
add_definitions(-DUV_UDP_MTU_DGRAM)

if (CMAKE_COMPILER_IS_GNUC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
	set(C_FLAGS "-Wall")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu89 ${C_FLAGS}")
elseif (MSVC)
	add_definitions(/wd4244 /wd4267 /wd4477 /wd4800 /wd4996)
endif ()

set(
	COMMON_SOURCES

	${libuv_source_dir}/src/fs-poll.c
	${libuv_source_dir}/src/inet.c
	${libuv_source_dir}/src/threadpool.c
	${libuv_source_dir}/src/uv-common.c
	${libuv_source_dir}/src/version.c
)

set (
	UNIX_COMMON_SOURCES

	${libuv_source_dir}/src/unix/async.c
	${libuv_source_dir}/src/unix/core.c
	${libuv_source_dir}/src/unix/dl.c
	${libuv_source_dir}/src/unix/fs.c
	${libuv_source_dir}/src/unix/getaddrinfo.c
	${libuv_source_dir}/src/unix/getnameinfo.c
	${libuv_source_dir}/src/unix/loop.c
	${libuv_source_dir}/src/unix/loop-watcher.c
	${libuv_source_dir}/src/unix/pipe.c
	${libuv_source_dir}/src/unix/poll.c
	${libuv_source_dir}/src/unix/process.c
	${libuv_source_dir}/src/unix/proctitle.c
	${libuv_source_dir}/src/unix/signal.c
	${libuv_source_dir}/src/unix/stream.c
	${libuv_source_dir}/src/unix/tcp.c
	${libuv_source_dir}/src/unix/thread.c
	${libuv_source_dir}/src/unix/timer.c
	${libuv_source_dir}/src/unix/tty.c
	${libuv_source_dir}/src/unix/udp.c
)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	include_directories(${libuv_source_dir}/src/unix)

	add_definitions(-D_GNU_SOURCE)
	add_definitions(-DUV_ENABLE_UDP_MMSG)

	set(
		LIBUV_SOURCES

		${COMMON_SOURCES}
		${UNIX_COMMON_SOURCES}

		${libuv_source_dir}/src/unix/linux-core.c
		${libuv_source_dir}/src/unix/linux-inotify.c
		${libuv_source_dir}/src/unix/linux-syscalls.c
		${libuv_source_dir}/src/unix/procfs-exepath.c
		${libuv_source_dir}/src/unix/sysinfo-loadavg.c
		${libuv_source_dir}/src/unix/sysinfo-memory.c
	)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Android")
	include_directories(${libuv_source_dir}/src/unix)

	add_definitions(-D_GNU_SOURCE)

	set(
		LIBUV_SOURCES

		${COMMON_SOURCES}
		${UNIX_COMMON_SOURCES}

		${libuv_source_dir}/src/unix/linux-core.c
		${libuv_source_dir}/src/unix/linux-inotify.c
		${libuv_source_dir}/src/unix/linux-syscalls.c
		${libuv_source_dir}/src/unix/procfs-exepath.c
		${libuv_source_dir}/src/unix/sysinfo-loadavg.c
		${libuv_source_dir}/src/unix/sysinfo-memory.c
		${libuv_source_dir}/src/unix/android-ifaddrs.c
		${libuv_source_dir}/src/unix/pthread-fixes.c
	)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	include_directories(${libuv_source_dir}/src/unix)

	add_definitions(-D_DARWIN_USE_64_BIT_INODE=1)
	add_definitions(-D_DARWIN_UNLIMITED_SELECT)

	set(
		LIBUV_SOURCES

		${COMMON_SOURCES}
		${UNIX_COMMON_SOURCES}

		${libuv_source_dir}/src/unix/bsd-ifaddrs.c
		${libuv_source_dir}/src/unix/darwin.c
		${libuv_source_dir}/src/unix/darwin-proctitle.c
		${libuv_source_dir}/src/unix/fsevents.c
		${libuv_source_dir}/src/unix/kqueue.c	
	)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	include_directories(${libuv_source_dir}/src/win)

	add_definitions(-D_GNU_SOURCE)

	set(
		LIBUV_SOURCES

		${COMMON_SOURCES}

		${libuv_source_dir}/src/win/async.c
		${libuv_source_dir}/src/win/core.c
		${libuv_source_dir}/src/win/detect-wakeup.c
		${libuv_source_dir}/src/win/dl.c
		${libuv_source_dir}/src/win/error.c
		${libuv_source_dir}/src/win/fs.c
		${libuv_source_dir}/src/win/fs-event.c
		${libuv_source_dir}/src/win/getaddrinfo.c
		${libuv_source_dir}/src/win/getnameinfo.c
		${libuv_source_dir}/src/win/handle.c
		${libuv_source_dir}/src/win/loop-watcher.c
		${libuv_source_dir}/src/win/pipe.c
		${libuv_source_dir}/src/win/poll.c
		${libuv_source_dir}/src/win/process.c
		${libuv_source_dir}/src/win/process-stdio.c
		${libuv_source_dir}/src/win/req.c
		${libuv_source_dir}/src/win/signal.c
		${libuv_source_dir}/src/win/snprintf.c
		${libuv_source_dir}/src/win/stream.c
		${libuv_source_dir}/src/win/tcp.c
		${libuv_source_dir}/src/win/thread.c
		${libuv_source_dir}/src/win/timer.c
		${libuv_source_dir}/src/win/tty.c
		${libuv_source_dir}/src/win/udp.c
		${libuv_source_dir}/src/win/util.c
		${libuv_source_dir}/src/win/winapi.c
		${libuv_source_dir}/src/win/winsock.c
	)
endif ()

add_library(
	uvext # Different to "uv"
	STATIC

	${LIBUV_SOURCES}
)
