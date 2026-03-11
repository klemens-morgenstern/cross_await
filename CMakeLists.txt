project(cross_await)
cmake_minimum_required(VERSION 3.31)

find_package(Boost REQUIRED cobalt corosio capy)

set(CMAKE_CXX_STANDARD 20)

add_executable(cobalt_capy cobalt_capy.cpp)
target_link_libraries(cobalt_capy PUBLIC Boost::cobalt Boost::capy Boost::corosio)

add_executable(corosio_cobalt corosio_cobalt.cpp)
target_link_libraries(corosio_cobalt PUBLIC Boost::cobalt Boost::capy Boost::corosio)

