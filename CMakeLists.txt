cmake_minimum_required( VERSION 3.25.0 )

project(lxd VERSION 2.0.0.0)

# 修复 Intellisense
# https://discourse.cmake.org/t/imported-projects-dont-resolve-include-paths-in-intellisense/9185
if (BUILDING_CLANG_SLN)
	unset(CMAKE_INCLUDE_SYSTEM_FLAG_CXX)
	unset(_CMAKE_INCLUDE_SYSTEM_FLAG_CXX_WARNING)
	message("/external:I disabled")
endif()

find_package(fmt REQUIRED CONFIG)

set(VERSION_RC ${CMAKE_CURRENT_SOURCE_DIR}/version.rc)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/dll.rc.in
				${VERSION_RC}
				NEWLINE_STYLE UNIX)

message(CMAKE_PREFIX_PATH:${CMAKE_PREFIX_PATH})

set(SOURCE_STRINGS
	strings/ascii.h
	strings/ascii.cc
	strings/match.h
	strings/match.cc
	strings/numbers.h
	strings/numbers.cc
	strings/str_split_internal.h
	strings/str_split.h
	strings/resize_uninitialized.h
	strings/str_cat.h
	strings/str_cat.cc
	strings/str_replace.h
	strings/str_replace.cc
	strings/int128.h
	strings/int128.cc
)
source_group(strings FILES ${SOURCE_STRINGS})

set(SRC_COMMON
	debug.h
	debug.cpp
	encoding.h
	encoding.cpp
	timer.h
	timer.cpp
	base64.h
	base64.cpp
	str.h
	str.cpp
	json.h
	threading.h
	nanoseconds.h
	smallvector.h
	map.h
	map.cpp
	glb.h
	glb.cpp
)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	set(SRC_WIN32
		fileio.h
		fileio.cpp
		crypt.h
		crypt.cpp
		utils.h
		utils.cpp
		http.h
		http.cpp
		AsyncHttp.h
		AsyncHttp.cpp
		WinHTTPWrappers.h
	)
else()
	set(SRC_WIN32 "")
endif()

add_library(${PROJECT_NAME} STATIC
	defines.h
	${SOURCE_STRINGS}
	${SRC_COMMON}
	${SRC_WIN32}
	${VERSION_RC}
)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8" "/fp:fast")
	target_compile_definitions(${PROJECT_NAME} PRIVATE -DUNICODE -D_UNICODE)
	target_link_libraries(${PROJECT_NAME} PUBLIC Winhttp Bcrypt Pathcch)
endif()

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
target_precompile_headers(${PROJECT_NAME} PUBLIC "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/defines.h>")
target_link_libraries(${PROJECT_NAME} PUBLIC fmt::fmt)
