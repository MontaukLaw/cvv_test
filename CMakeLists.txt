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

add_executable(cv_rknn_drm 
                src/cv_rknn_drm.cpp 
                src/postprocess.cpp 
                src/display_demo.cpp
                dep/rkdrm/bo
                dep/rkdrm/dev.c
                dep/rkdrm/modeset.c  
)
target_link_libraries(cv_rknn_drm ${OpenCV_LIBS} ${RKNN_RT_LIB} ${RGA_LIB} rockchip_mpp drm)
target_include_directories(cv_rknn_drm PUBLIC "${CMAKE_SOURCE_DIR}/dep/rkdrm" 
                                               "/usr/include/rockchip/"
                                               "/usr/include/drm/")   

add_executable(hdmi_test 
               src/hdmi_test.cpp 
               dep/rkdrm/bo
               dep/rkdrm/dev.c
               dep/rkdrm/modeset.c  
)
target_link_libraries(hdmi_test ${OpenCV_LIBS} ${RKNN_RT_LIB} ${RGA_LIB} rockchip_mpp drm)
target_include_directories(hdmi_test PUBLIC "${CMAKE_SOURCE_DIR}/dep/rkdrm" 
                                               "/usr/include/rockchip/"
                                               "/usr/include/drm/")   

add_executable(display_output_test 
               src/display_output_test.cpp 
               src/display_demo.cpp
               dep/rkdrm/bo
               dep/rkdrm/dev.c
               dep/rkdrm/modeset.c  
)

set(GFLAG_LIB /home/orangepi/gflags/build/lib/libgflags.a)
target_link_libraries(display_output_test ${OpenCV_LIBS} ${RKNN_RT_LIB} ${RGA_LIB} rockchip_mpp drm ${GFLAG_LIB})
target_include_directories(display_output_test PUBLIC "${CMAKE_SOURCE_DIR}/dep/rkdrm" 
                                               "/usr/include/rockchip/"
                                               "/usr/include/drm/"
                                               "/home/orangepi/gflags/build/include"
                                               )   

add_executable(app 
               src/app.cpp 
               src/monitor_init.cpp
               dep/rkdrm/bo
               dep/rkdrm/dev.c
               dep/rkdrm/modeset.c  
               src/model_utils.cpp
               src/postprocess.cpp
)

target_link_libraries(app ${OpenCV_LIBS} ${RKNN_RT_LIB} ${RGA_LIB} rockchip_mpp drm ${GFLAG_LIB})
target_include_directories(app PUBLIC "${CMAKE_SOURCE_DIR}/dep/rkdrm" 
                                               "/usr/include/rockchip/"
                                               "/usr/include/drm/"
                                               "/home/orangepi/gflags/build/include"
                                               )            