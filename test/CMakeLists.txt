cmake_minimum_required(VERSION 3.20)


add_executable(hiptest main.cpp)
target_link_libraries(hiptest hipengine)
