project ( DetoursLib )

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT /W4 /WX /Zi /MT /Gy /Gm- /Zl /Od")

option(DETOURS_CL_17_OR_NEWER "DETOURS_CL_17_OR_NEWER" ON)
option(DETOURS_WIN_7 "DETOURS_WIN_7" OFF)
option (DETOURS_SOURCE_BROWSING "DETOURS_SOURCE_BROWSING" ON)

if ((NOT ${DETOURS_WIN_7}) AND ${DETOURS_CL_17_OR_NEWER})
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /D_USING_V110_SDK71_")
endif()

if(DETOURS_SOURCE_BROWSING)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /FR")
else()
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /DWIN32_LEAN_AND_MEAN /D_WIN32_WINNT=0x501")
endif()

include_directories (
	detours/src
)

set (srcs
	detours/src/creatwth.cpp
	detours/src/disolarm.cpp
	detours/src/disolx64.cpp
	detours/src/modules.cpp
	detours/src/detours.cpp
	detours/src/disolarm64.cpp
	detours/src/disolx86.cpp
	detours/src/disasm.cpp
	detours/src/disolia64.cpp
	detours/src/image.cpp
)

set (hdrs
	detours/src/detours.h
	detours/src/detver.h
	detours/src/uimports.cpp
)

set_property (SOURCE detours/src/uimports.cpp APPEND PROPERTY HEADER_FILE_ONLY true)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	add_definitions(-D_AMD64_)
else()
	add_definitions(-D_X86_)
endif()

add_library ( ${PROJECT_NAME} STATIC ${hdrs} ${srcs} )