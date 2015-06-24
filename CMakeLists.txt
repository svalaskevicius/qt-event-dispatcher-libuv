project(qt-event-dispatcher-libuv)
cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

set(CMAKE_MODULE_PATH
  ${CMAKE_MODULE_PATH}
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules"
)

set(PUBLIC
  src/eventdispatcherlibuv.h
)
set(SOURCES
  src/eventdispatcherlibuv_p.h
  src/eventdispatcherlibuv.cpp
  src/eventdispatcherlibuv/async_channel.cpp
  src/eventdispatcherlibuv/timer_notifier.cpp
  src/eventdispatcherlibuv/time_tracker.cpp
  src/eventdispatcherlibuv/libuv_api.cpp
  src/eventdispatcherlibuv/socket_notifier.cpp
)

if(CMAKE_COMPILER_IS_GNUCXX OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

if(NOT TARGET LibUV::LibUV)
  find_package(LibUV REQUIRED)
endif()

find_package(Qt5 5.2.0 REQUIRED COMPONENTS Core Gui)
include_directories(${Qt5Gui_PRIVATE_INCLUDE_DIRS})

add_library(qt-event-dispatcher-libuv STATIC ${PUBLIC} ${SOURCES})

target_link_libraries(qt-event-dispatcher-libuv
  LibUV::LibUV
  Qt5::Core
  Qt5::Gui
)
set_target_properties(qt-event-dispatcher-libuv PROPERTIES AUTOMOC TRUE)
