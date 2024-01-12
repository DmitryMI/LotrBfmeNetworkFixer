set(BOOST_ROOT "C:/FilesAndFolders/boost_1_80_0" CACHE STRING "Path to boost source code")
set(SPDLOG_ROOT "C:/FilesAndFolders/spdlog/build" CACHE STRING "Path to spdlog build")

cmake_path(GET CMAKE_CURRENT_SOURCE_DIR FILENAME current_source_dir_name)
message(STATUS ${current_source_dir_name})

project(${current_source_dir_name})

add_executable (${PROJECT_NAME} "launcher.cpp" "pipe_server.hpp"  "pipe_server.cpp")

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 23)

# Boost
set(Boost_USE_STATIC_LIBS ON)
find_package( Boost 1.80 REQUIRED COMPONENTS program_options)
target_include_directories(${PROJECT_NAME} PRIVATE ${Boost_INCLUDE_DIR} )
target_link_libraries( ${PROJECT_NAME} PRIVATE ${Boost_LIBRARIES} )

# Spdlog
if(DEFINED CACHE{SPDLOG_ROOT})
	message(STATUS "Using SPDLOG_ROOT from cmake cache: ${SPDLOG_ROOT}")
	list(APPEND CMAKE_PREFIX_PATH ${SPDLOG_ROOT})
elseif(DEFINED ENV{SPDLOG_ROOT})
	message(STATUS "Using SPDLOG_ROOT from environment variable: $ENV{SPDLOG_ROOT}")
	list(APPEND CMAKE_PREFIX_PATH $ENV{SPDLOG_ROOT})
else()
	message(STATUS "SPDLOG_ROOT not set!")
endif()
find_package(spdlog REQUIRED)
target_compile_definitions(${PROJECT_NAME} PRIVATE _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS)

target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)
target_link_libraries(${PROJECT_NAME} PRIVATE Ws2_32.lib)

