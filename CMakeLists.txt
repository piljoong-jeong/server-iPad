# Author: PilJoong Jeong, 20/02/11

cmake_minimum_required(VERSION 3.12)
set(PROJECT_NAME server)
project(${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-O3")

find_package(OpenCV 3 REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(Threads) # for pthreads; Ubuntu confirmed, MSVC may not be compatible

add_executable(${PROJECT_NAME} src/main.cpp src/Network.cpp src/getimages.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OpenCV_INCLUDE_DIRS}
    ${EIGEN3_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME} 
    ${CMAKE_THREAD_LIBS_INIT}
    ${OpenCV_LIBS}
)