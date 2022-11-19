cmake_minimum_required( VERSION 3.16.0 )

project(lxd VERSION 2.0.0.0)

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
	strings/str_split.cc
	strings/resize_uninitialized.h
	strings/str_cat.h
	strings/str_cat.cc
	strings/str_replace.h
	strings/str_replace.cc
)
source_group(strings FILES ${SOURCE_STRINGS})

set(SRC_COMMON
	debug.h
	debug.cpp
	encoding.h
	encoding.cpp
	fileio.h
	fileio.cpp
	timer.h
	timer.cpp
	base64.h
	base64.cpp
	str.h
	str.cpp
	utils.h
	utils.cpp
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
		http.h
		http.cpp
		crypt.h
		crypt.cpp
		AsyncFile.h
		AsyncFile.cpp
		AsyncHttp.h
		AsyncHttp.cpp
		WinHTTPWrappers.h
	)
else()
	set(SRC_WIN32 "")
endif()

add_library( lxd SHARED
	defines.h
	${SOURCE_STRINGS}
	${SRC_COMMON}
	${SRC_WIN32}
	${VERSION_RC}
)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	target_compile_options(lxd PRIVATE "/utf-8" "/MP" "/W4" "/wd4251")
	target_compile_definitions(lxd PRIVATE -DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS)
	target_link_libraries(lxd PUBLIC Winhttp Bcrypt Pathcch)
endif()
target_compile_definitions(lxd PRIVATE -DBUILDING_DLL)
target_compile_features( lxd PRIVATE cxx_std_20)
target_precompile_headers(lxd PUBLIC defines.h)
target_link_libraries(lxd PUBLIC fmt::fmt-header-only)
