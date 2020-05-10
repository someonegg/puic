cmake_minimum_required (VERSION 3.2)

project(argtable2 C)

cmake_policy(SET CMP0063 NEW)

set (argtable2_source_dir 2.13)

include_directories(${argtable2_source_dir}/include ${argtable2_source_dir}/src)

include(CheckIncludeFile)

if (CMAKE_COMPILER_IS_GNUC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu89 ${C_FLAGS}")
elseif (MSVC)
	add_definitions(/wd4244 /wd4267 /wd4477 /wd4800 /wd4996)
endif ()

check_include_file("getopt.h" HAVE_GETOPT_H)
if (HAVE_GETOPT_H)
	add_definitions(-DHAVE_GETOPT_H)
endif ()

check_include_file("inttypes.h" HAVE_INTTYPES_H)
if (HAVE_INTTYPES_H)
	add_definitions(-DHAVE_INTTYPES_H)
endif ()

check_include_file("memory.h" HAVE_MEMORY_H)
if (HAVE_MEMORY_H)
	add_definitions(-DHAVE_MEMORY_H)
endif ()

check_include_file("stdlib.h" HAVE_STDLIB_H)
if (HAVE_STDLIB_H)
	add_definitions(-DHAVE_STDLIB_H)
endif ()

check_include_file("strings.h" HAVE_STRINGS_H)
if (HAVE_STRINGS_H)
	add_definitions(-DHAVE_STRINGS_H)
endif ()

check_include_file("string.h" HAVE_STRING_H)
if (HAVE_STRING_H)
	add_definitions(-DHAVE_STRING_H)
endif ()

check_include_file("ctype.h" HAVE_CTYPE_H)
if (HAVE_CTYPE_H)
	add_definitions(-DHAVE_CTYPE_H)
endif ()

if (HAVE_STDLIB_H AND HAVE_STRING_H AND HAVE_CTYPE_H)
	add_definitions(-DSTDC_HEADERS)
endif ()

if (WIN32)
	set(
		ARGTABLE2_SOURCES

		${argtable2_source_dir}/src/arg_dbl.c
		${argtable2_source_dir}/src/arg_end.c
		${argtable2_source_dir}/src/arg_file.c
		${argtable2_source_dir}/src/arg_int.c
		${argtable2_source_dir}/src/arg_lit.c
		${argtable2_source_dir}/src/arg_rem.c
		${argtable2_source_dir}/src/arg_str.c
		${argtable2_source_dir}/src/argtable2.c
	)
else (WIN32)
	set(
		ARGTABLE2_SOURCES

		${argtable2_source_dir}/src/arg_date.c
		${argtable2_source_dir}/src/arg_dbl.c
		${argtable2_source_dir}/src/arg_end.c
		${argtable2_source_dir}/src/arg_file.c
		${argtable2_source_dir}/src/arg_int.c
		${argtable2_source_dir}/src/arg_lit.c
		${argtable2_source_dir}/src/arg_rem.c
		${argtable2_source_dir}/src/arg_rex.c
		${argtable2_source_dir}/src/arg_str.c
		${argtable2_source_dir}/src/argtable2.c
	)
endif (WIN32)

if (NOT HAVE_GETOPT_H)
	set(
		GETOPT_SOURCES

		${argtable2_source_dir}/src/getopt.c
		${argtable2_source_dir}/src/getopt1.c
	)
endif (NOT HAVE_GETOPT_H)

add_library(
	argtab2 # Different to "argtable2"
	STATIC

	${ARGTABLE2_SOURCES}
	${GETOPT_SOURCES}
)
