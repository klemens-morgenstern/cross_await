project(cross_await)
cmake_minimum_required(VERSION 3.31)

find_package(Boost REQUIRED cobalt corosio capy)

set(CMAKE_CXX_STANDARD 20)

add_executable(cobalt_capy cobalt_capy.cpp)
target_link_libraries(cobalt_capy PUBLIC Boost::cobalt Boost::capy Boost::corosio)

add_executable(corosio_cobalt corosio_cobalt.cpp)
target_link_libraries(corosio_cobalt PUBLIC Boost::cobalt Boost::capy Boost::corosio)


add_executable(tmc_capy tmc_capy.cpp)
target_link_libraries(tmc_capy PUBLIC Boost::capy Boost::corosio)
target_include_directories(tmc_capy PUBLIC third-party/tmc/include)


add_executable(corosio_tmc corosio_tmc.cpp)
target_link_libraries(corosio_tmc PUBLIC Boost::capy Boost::corosio)
target_include_directories(corosio_tmc PUBLIC third-party/tmc/include)
