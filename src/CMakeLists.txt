find_package(Boost REQUIRED)

find_package(Boost)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
endif()

include_directories(${Boost_INCLUDE_DIRS})
include_directories(../include)

SET(CMAKE_CXX_FLAGS "-std=c++14 -Wno-attributes")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

add_library(gif
	parser.cpp
	bit_stream.cpp
)
