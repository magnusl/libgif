find_package(Boost REQUIRED)

add_library(gif
	parser.cpp
	bit_stream.cpp
)

target_compile_features(gif PRIVATE cxx_std_17)
target_include_directories(gif PRIVATE ${Boost_INCLUDE_DIRS})
