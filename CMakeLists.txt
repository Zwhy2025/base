cmake_minimum_required(VERSION 3.10)
project(base)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_VERBOSE_MAKEFILE 1)  # 开启调试信息

# 查找GTest库
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# 设置输出路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)

aux_source_directory(${PROJECT_SOURCE_DIR}test DIR_TEST)

# 创建可执行文件
add_executable(base main.cpp ${DIR_TEST})

# 设置头文件路径
target_include_directories(base PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 链接GTest库
target_link_libraries(base ${GTEST_BOTH_LIBRARIES})

