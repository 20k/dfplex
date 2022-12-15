get_directory_property(isPlugin PARENT_DIRECTORY)

project(dfplex)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 4.9)
		option(DFPLEX_IXW "Use IXWebSocket instead of websocketpp" OFF)
	else()
		# GCC 4.9 is not compatible with IXWebSockets
		option(DFPLEX_IXW "Use IXWebSocket instead of websocketpp" OFF)
	endif()
else()
	option(DFPLEX_IXW "Use IXWebSocket instead of websocketpp" OFF)
endif()

if (NOT isPlugin)
	cmake_minimum_required(VERSION 2.8)
	set(DFHACK_VER 0.34.11)
	set(DFHACK_REL r5)
	add_definitions(-DDFHACK_VERSION="${DFHACK_VER}-${DFHACK_REL}")
	# dfhack path
	set(DH dfhack)
endif()

# A list of source files
file (GLOB SRC "*.cpp")

# A list of headers
file (GLOB HDRS "*.hpp")

set_source_files_properties(${HDRS} PROPERTIES HEADER_FILE_ONLY TRUE)

# mash them together (headers are marked as headers and nothing will try to compile them)
list(APPEND SRC ${HDRS})

set(PROJECT_LIBS)

#linux
if(UNIX)
	add_definitions(
		-std=c++11
		-DLINUX_BUILD
	)
	if (NOT DFPLEX_IXW)
		add_definitions(-D_WEBSOCKETPP_CPP11_STL_)
	endif()
	list(APPEND PROJECT_LIBS pthread)
	set(DF_ROOT ${CMAKE_SOURCE_DIR}/df_linux)
# windows
else(UNIX)
	set(WFDIR ${CMAKE_CURRENT_BINARY_DIR})
	add_definitions(
		-DWIN32_LEAN_AND_MEAN
	)
	if (NOT DFPLEX_IXW)
		add_definitions(
			-D_WEBSOCKETPP_CPP11_FUNCTIONAL_
			-D_WEBSOCKETPP_CPP11_SYSTEM_ERROR_
			-D_WEBSOCKETPP_CPP11_RANDOM_DEVICE_
			-D_WEBSOCKETPP_CPP11_MEMORY_
		)
		set(BOOST_ROOT ${WFDIR}/boost)
		link_directories(${BOOST_ROOT}/lib) # Boost doesn't find everything
	endif()
endif(UNIX)

if (NOT DFPLEX_IXW)
	# Boost/Asio
	set(BOOST_ROOT "C:/Boost")
	set(Boost_INCLUDE_DIR "C:/Boost")
	set(Boost_FIND_REQUIRED TRUE)
	set(Boost_USE_STATIC_LIBS ON)
	set(Boost_USE_SHARED_LIBS OFF)
	set(Boost_USE_MULTITHREADED ON)
	set(WS_BOOST_LIBS system)
	find_package(Boost 1.38.0 REQUIRED COMPONENTS "${WS_BOOST_LIBS}")

	if(Boost_FOUND)
		include_directories(${Boost_INCLUDE_DIRS})
		list(APPEND PROJECT_LIBS ${Boost_LIBRARIES})
	endif()

	# websocketpp
	include_directories(websocketpp)
	
	add_definitions(-DDFPLEX_WEBSOCKETPP)
else()
	# ixwebsocket
	include_directories(IXWebSocket)
	add_subdirectory(IXWebSocket)
	list(APPEND PROJECT_LIBS
		ixwebsocket
	)
	add_definitions(-DDFPLEX_IXW)
endif()

# cpp-httplib
include_directories(cpp-httplib)

if (isPlugin)
	DFHACK_PLUGIN(dfplex ${SRC} LINK_LIBRARIES dfhack-tinythread ${PROJECT_LIBS} lua)
	target_link_libraries(dfplex ${Boost_LIBRARIES})
	INSTALL(DIRECTORY ../dist/shared/ DESTINATION .)
	INSTALL(DIRECTORY ../static/ DESTINATION ./hack/www)
else()
	if (NOT UNIX)
		message(FATAL_ERROR "Building dfplex out-of-tree not supported on windows.")
	endif()
	find_library(DFHACK_LIBRARY dfhack ${DH}/build/library)
	find_library(TINYTHREAD_LIBRARY dfhack-tinythread ${DH}/build/depends/tthread)
	list(APPEND PROJECT_LIBS
		${DFHACK_LIBRARY}
		${TINYTHREAD_LIBRARY}
	)

	include_directories(
		${DH}/library/include
		${DH}/library/proto
		${DH}/depends/protobuf
		${DH}/depends/tthread
	)

	add_library(dfplex.plug SHARED ${SRC})
	target_link_libraries(dfplex.plug ${PROJECT_LIBS} lua)
	# Installing
	install(TARGETS dfplex.plug
		RUNTIME DESTINATION ${DF_ROOT}/hack/plugins
		LIBRARY DESTINATION ${DF_ROOT}/hack/plugins
	)
endif()
