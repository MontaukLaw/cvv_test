cmake_minimum_required(VERSION 3.10)
project(cvv_test)
find_package(OpenCV REQUIRED)

if(OpenCV_FOUND)
  message(STATUS "OpenCV_INCLUDE_DIRS=${OpenCV_INCLUDE_DIRS}")
  message(STATUS "OpenCV_LIBS=${OpenCV_LIBS}")
else()
  message(FATAL_ERROR "OpenCV not found")
endif()  

# rknn api
set(RKNN_API_PATH ${CMAKE_SOURCE_DIR}/dep/rknn)
set(RKNN_RT_LIB ${RKNN_API_PATH}/lib/librknnrt.so)
include_directories(${RKNN_API_PATH}/include)

# rga
set(RGA_PATH ${CMAKE_SOURCE_DIR}/dep/rga)
set(RGA_LIB ${RGA_PATH}/lib/librga.so)
include_directories(${RGA_PATH}/include)

include_directories(${CMAKE_SOURCE_DIR}/include)

include_directories(${OpenCV_INCLUDE_DIRS})
add_executable(cvv_test src/main.cpp src/postprocess.cpp)
target_link_libraries(cvv_test ${OpenCV_LIBS} ${RKNN_RT_LIB} ${RGA_LIB})