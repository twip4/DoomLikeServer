cmake_minimum_required(VERSION 3.25)
project(untitled)

set(CMAKE_CXX_STANDARD 17)

include_directories(/opt/homebrew/include/)

link_directories(/opt/homebrew/lib)

add_executable(DoomLikeServer main.cpp
        constante.h
)
