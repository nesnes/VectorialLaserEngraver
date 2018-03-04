SET(execName LaserPrinterSVG)
CMAKE_MINIMUM_REQUIRED(VERSION 2.4)
PROJECT(${execName})

SET(EXECUTABLE_OUTPUT_PATH ".")

find_package(OpenCV)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
if(OpenCV_FOUND)
    include_directories(${OPENCV_INCLUDE_DIRS})
    link_directories(${OpenCV_LIBRARY_DIRS})
	add_definitions(-DWITH_OPENCV)
endif()

FILE(GLOB_RECURSE SRC_FILES src/*.cpp)

ADD_EXECUTABLE(${execName} ${SRC_FILES})

add_definitions(-std=c++11 -g -O3)

if(OpenCV_FOUND)
    TARGET_LINK_LIBRARIES(${execName} ${OpenCV_LIBRARIES} )
endif()