CMAKE_MINIMUM_REQUIRED(VERSION 2.8.9)
PROJECT(VideoCapture)

cmake_policy(SET CMP0043 NEW) ## ignore the content of the COMPILE_DEFINITIONS_<CONFIG> target property
cmake_policy(SET CMP0020 NEW) ## auto link with qtmain under windows

## Allow C++11
include(CheckCXXCompilerFlag)
get_filename_component(currentBuildTool ${CMAKE_BUILD_TOOL} NAME_WE)
if(${currentBuildTool} MATCHES "(msdev|devenv|nmake|MSBuild)")
    add_definitions(/W4)
elseif(${currentBuildTool} MATCHES "(make|gmake)")
    add_definitions(-Wall -g)
	CHECK_CXX_COMPILER_FLAG("-std=gnu++11" COMPILER_SUPPORTS_CXX11)
	CHECK_CXX_COMPILER_FLAG("-std=gnu++0x" COMPILER_SUPPORTS_CXX0X)
	if(COMPILER_SUPPORTS_CXX11)
		add_definitions(-std=gnu++11)
	elseif(COMPILER_SUPPORTS_CXX0X)
		add_definitions(-std=gnu++0x)
	else()
		message(SEND_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
	endif()
elseif(APPLE) ## TODO: do a better test and send error on unsupported c++11 compiler
    add_definitions(-std=c++11 -stdlib=libc++)
endif()


## find packages
find_package(OpenGL)

############
## Find Qt5
############
## WORK AROUND with windows : try to auto find and set Qt5_DIR
if(WIN32)
	file(GLOB qt5versionPathList "C:/Qt/Qt5.*")
	if(qt5versionPathList)
		list(LENGTH qt5versionPathList qt5versionPathListCount)
		if(${qt5versionPathListCount} GREATER "1" AND NOT Qt5_DISBALE_FIND_CMAKE_WARNING)
			message(WARNING "Many Qt5 version has been found (check manually the right one with Qt5_DIR cmake variable).")
			message("If you are sure of your qt installation set, you can disable this warning by checking Qt5_DISBALE_FIND_CMAKE_WARNING variable")
			option(Qt5_DISBALE_FIND_CMAKE_WARNING "" OFF)
		endif()
	endif()
	if(NOT Qt5_DIR AND qt5versionPathList)
		foreach(qt5versionPath ${qt5versionPathList})
			file(GLOB qt5versionSubPathList "${qt5versionPath}/5.*")
			file(GLOB qt5versionSubPathList "${qt5versionSubPathList}/*")
			if(qt5versionSubPathList)
				foreach(qt5versionSubPath ${qt5versionSubPathList})
					message(STATUS "Plausible Qt5 instllation dir : ${qt5versionSubPath}")
					set(Qt5_DIR "${qt5versionSubPath}/lib/cmake/Qt5") ## choose last one
				endforeach()
			endif()
		endforeach()
	endif()
endif()

## WORK AROUND: QT5.2.x OpenGL problem to find glu32 on windows... we need WINSDK to let qt find it
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(WINSDK_LIB "C:\\Program Files (x86)\\Windows Kits\\8.0\\Lib\\win8\\um\\x64")
elseif(WIN32)
	set(WINSDK_LIB "C:\\Program Files (x86)\\Windows Kits\\8.0\\Lib\\win8\\um\\x86")
endif()
if(WIN32 AND EXISTS ${WINSDK_LIB})
	list(APPEND CMAKE_PREFIX_PATH "C:\\Program Files (x86)\\Windows Kits\\8.0\\Lib\\win8\\um\\x86")
elseif(WIN32)
	message(WARNING "You need Windows SDK to let Qt5 find OpenGL glu32, something like :\n${WINSDK_LIB}")
endif()

if(EXISTS ${Qt5_DIR})
	message(STATUS "Qt5_DIR = ${Qt5_DIR}")
else()
	message(STATUS "Qt5_DIR not set...")
endif()
set(Qt5_DIR ${Qt5_DIR} CACHE PATH "Path to <Qt5 installation>/lib/cmake/Qt5")

set(CMAKE_AUTOMOC ON) # Instruct CMake to run moc automatically when needed.
find_package(Qt5 COMPONENTS OpenGL Widgets) ## then you can use : qt5_use_modules(<target> <component> <...>)

if(WIN32 AND "${QT_VERSION_MAJOR}.${QT_VERSION_MINOR}.${QT_VERSION_PATCH}" MATCHES "5.2.1")
	message(WARNING "Be careful, there were bugs on 5.2.1 win32 version : http://stackoverflow.com/questions/14008737/qt5-qconfig-h-weird")
endif()



add_library(${PROJECT_NAME} 		STATIC 	FFmpegVideoRecorderProcess.h FFmpegVideoRecorderProcess.cpp)
#add_executable(${PROJECT_NAME}_Test 		Example.cpp)



if(0)
	add_definitions(-DHAS_QT)
	qt5_use_modules(${PROJECT_NAME} OpenGL)
#	qt5_use_modules(${PROJECT_NAME}_Test OpenGL)
elseif(OpenGL_FOUND OR OPENGL_FOUND)
	#target_link_libraries(${OPENGL_LIBRARIES})
else()
	message(SEND_ERROR "Qt5 not found, please set Qt5_DIR to <Qt5 installation>/lib/cmake/Qt5")
endif()