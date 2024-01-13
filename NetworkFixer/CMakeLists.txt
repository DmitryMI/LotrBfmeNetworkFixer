
project ( NetworkFixer )

# set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")

# include libdetours headers:
include_directories ( ${DETOURS_DIR}/Detours/src )

set (srcs
	network_fixer.cpp
	pipe_sink.hpp
	hook_gethostbyname.hpp
)

add_library ( ${PROJECT_NAME} SHARED ${hdrs} ${srcs} ${rsrc})

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 23)
endif()

# link with libdetours.lib
target_link_libraries ( ${PROJECT_NAME} PRIVATE ${DETOURS_LIB} )
add_dependencies(${PROJECT_NAME} DetoursLib)

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

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    find_package(spdlog REQUIRED)
	target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    target_include_directories(${PROJECT_NAME} PRIVATE "${SPDLOG_ROOT}/../include/")
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE Ws2_32.lib)

target_compile_definitions(${PROJECT_NAME} PRIVATE _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS)

INSTALL( TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX} COMPONENT ${PROJECT_NAME} )
