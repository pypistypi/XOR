cmake_minimum_required(VERSION 3.16)
project(XorFileModifier LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(XorFileModifier
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    worker.cpp
    worker.h
)

target_link_libraries(XorFileModifier PRIVATE Qt6::Widgets)
