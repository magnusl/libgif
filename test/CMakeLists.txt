find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

add_executable(giftest
	main.cpp
)

target_include_directories(giftest PRIVATE ${SDL2_INCLUDE_DIRS})
target_compile_features(giftest PRIVATE cxx_std_17)
target_link_libraries(giftest gif)
target_link_libraries(giftest ${SDL2_LIBRARIES})
