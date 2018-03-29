
include_directories(../include)

SET(CMAKE_CXX_FLAGS "-std=c++14")

add_executable(giftest
	main.cpp
)

target_link_libraries(giftest gif)
