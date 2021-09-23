cmake_minimum_required( VERSION 3.16.0 )

project(lxd VERSION 1.1.0.0)

set(VERSION_RC ${CMAKE_CURRENT_SOURCE_DIR}/version.rc)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/dll.rc.in
                ${VERSION_RC}
                NEWLINE_STYLE UNIX)

message(CMAKE_PREFIX_PATH:${CMAKE_PREFIX_PATH})

add_library( lxd SHARED
	defines.h
	debug.h
	debug.cpp
	encoding.h
	encoding.cpp
	fileio.h
	fileio.cpp
	timer.h
	timer.cpp
	http.h
	http.cpp
	base64.h
	base64.cpp
	crypt.h
	crypt.cpp
	json.h
	threading.h
	nanoseconds.h
	str.h
	str.cpp
	utils.h
	utils.cpp
	AsyncFile.h
	AsyncFile.cpp
	AsyncHttp.h
	AsyncHttp.cpp
	WinHTTPWrappers.h
	${VERSION_RC}
)

target_compile_options( lxd PRIVATE "/utf-8" "/MP" "/wd4251" "/WX")
target_compile_definitions( lxd PUBLIC -DBUILDING_DLL )
target_compile_definitions( lxd PRIVATE -DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS )
target_compile_features( lxd PRIVATE cxx_std_17 )
target_link_libraries( lxd PUBLIC fmt::fmt-header-only Winhttp Bcrypt Pathcch)
