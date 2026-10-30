cmake_minimum_required(VERSION 3.14)

project(cpp-steam-tools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(WIN32)
	set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core Gui)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui)

add_library(cpp-steam-tools SHARED
	include/cpp-steam-tools_global.h
	include/crc.h
	include/vdf_parser.hpp
	include/shortcutsvdf.h
	src/shortcutsvdf.cpp
	include/steamshortcutentry.h
	src/steamshortcutentry.cpp
	include/steamtools.h
	src/steamtools.cpp
	include/shortcutmatch.h
	src/shortcutmatch.cpp
)

target_include_directories(cpp-steam-tools
	PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>
)

target_link_libraries(cpp-steam-tools PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui)
target_compile_definitions(cpp-steam-tools PRIVATE CPPSTEAMTOOLS_LIBRARY)

install(TARGETS cpp-steam-tools
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
	RUNTIME DESTINATION bin
)

option(CPP_STEAM_TOOLS_TESTS "Build cpp-steam-tools unit tests" OFF)
if(CPP_STEAM_TOOLS_TESTS OR CHIAKI_ENABLE_TESTS)
	enable_testing()
	add_executable(cpp-steam-tools-test tests/test_shortcutsvdf.cpp)
	target_link_libraries(cpp-steam-tools-test PRIVATE cpp-steam-tools Qt${QT_VERSION_MAJOR}::Core)
	add_test(NAME cpp-steam-tools COMMAND cpp-steam-tools-test)
	add_executable(cpp-steam-tools-test-steamtools tests/test_steamtools.cpp)
	target_link_libraries(cpp-steam-tools-test-steamtools PRIVATE cpp-steam-tools Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui)
	add_test(NAME cpp-steam-tools-steamtools COMMAND cpp-steam-tools-test-steamtools)
	add_executable(cpp-steam-tools-inspect tests/inspect_steam.cpp)
	target_link_libraries(cpp-steam-tools-inspect PRIVATE cpp-steam-tools Qt${QT_VERSION_MAJOR}::Gui)
endif()
