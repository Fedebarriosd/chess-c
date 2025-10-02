cmake_minimum_required(VERSION 3.16)
project(chess-c C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Copiar carpeta assets/ al directorio de build
file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})

# Optimiza en Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Incluye headers
include_directories(${CMAKE_SOURCE_DIR}/src)

# Fuentes
set(SOURCES
        src/main.c
        src/board.c
)

add_executable(chess ${SOURCES})

# Encontrar raylib (primero intenta paquete del sistema)
find_package(raylib QUIET)

if(raylib_FOUND)
    target_link_libraries(chess raylib)
else()
    # Fallback manual por plataforma
    if(WIN32)
        # Asume MSYS2 / MinGW con raylib instalado vía pacman
        # Si usás Visual Studio + vcpkg, reemplaza por la ruta de raylib o usa find_package con toolchain de vcpkg.
        target_link_libraries(chess
                raylib
                winmm gdi32 user32 shell32 ole32 opengl32
        )
    else()
        # Linux: raylib de sistema
        target_link_libraries(chess raylib m)
    endif()
endif()

# En GCC/Clang agrega algunas warnings útiles
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(chess PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()