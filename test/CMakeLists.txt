
include_directories(../include)

SET(CMAKE_CXX_FLAGS "-std=c++17")

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

add_executable(giftest
	main.cpp
)

target_link_libraries(giftest gif)
target_link_libraries(giftest ${SDL2_LIBRARIES})
