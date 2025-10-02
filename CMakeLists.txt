cmake_minimum_required(VERSION 3.16)
project(chess C)

set(CMAKE_C_STANDARD 17)
find_package(raylib REQUIRED)

add_executable(chess
        src/main.c
        src/board.c
)

target_link_libraries(chess PRIVATE raylib m)

# Copiar carpeta assets/ al directorio de build
file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})