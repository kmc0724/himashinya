cmake_minimum_required(VERSION 3.21)

project(himashinya VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

qt_add_executable(himashinya
    src/main.cpp
)

target_link_libraries(himashinya PRIVATE Qt6::Widgets)

set_target_properties(himashinya PROPERTIES
    WIN32_EXECUTABLE TRUE
)