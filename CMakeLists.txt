
cmake_minimum_required(VERSION 3.21)

project(himashinya VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(libssh CONFIG REQUIRED)

qt_standard_project_setup()

# libvterm
set(LIBVTERM_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/external/libvterm"
)

add_library(vterm STATIC
    ${LIBVTERM_DIR}/src/encoding.c
    ${LIBVTERM_DIR}/src/keyboard.c
    ${LIBVTERM_DIR}/src/mouse.c
    ${LIBVTERM_DIR}/src/parser.c
    ${LIBVTERM_DIR}/src/pen.c
    ${LIBVTERM_DIR}/src/screen.c
    ${LIBVTERM_DIR}/src/state.c
    ${LIBVTERM_DIR}/src/unicode.c
    ${LIBVTERM_DIR}/src/vterm.c
)

target_include_directories(vterm
    PUBLIC
        ${LIBVTERM_DIR}/include
    PRIVATE
        ${LIBVTERM_DIR}/src
)

if(MSVC)
    target_compile_definitions(vterm PRIVATE
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# Himashinya
qt_add_executable(himashinya
    src/main.cpp
    src/main_window.cpp
    src/main_window.h
    src/server_profile.h
    src/server_repository.cpp
    src/server_repository.h
    src/ssh_client.cpp
    src/ssh_client.h
    src/ssh_worker.cpp
    src/ssh_worker.h
    src/ssh_connection.cpp
    src/ssh_connection.h
    src/terminal_widget.cpp
    src/terminal_widget.h
)

target_link_libraries(himashinya PRIVATE
    Qt6::Widgets
    ssh
    vterm
)

set_target_properties(himashinya PROPERTIES
    WIN32_EXECUTABLE TRUE
)
