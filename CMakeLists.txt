cmake_minimum_required (VERSION 3.10)
project (libgif)

# path for additional modules
find_package(Boost REQUIRED)

add_subdirectory(src)
add_subdirectory(test)
target_include_directories(gif PUBLIC include)
