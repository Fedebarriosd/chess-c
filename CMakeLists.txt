cmake_minimum_required(VERSION 3.16)
project(chess-c LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Perfil por defecto: Release (si el generador es single-config)
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Fuentes
set(SOURCES
        src/main.c
        src/board.c
)

add_executable(chess ${SOURCES})
target_include_directories(chess PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Buscar raylib (paquete del sistema o MSYS2)
find_package(raylib QUIET)
if(raylib_FOUND)
    target_link_libraries(chess PRIVATE raylib)
else()
    if(WIN32)
        # MSYS2 / MinGW: raylib viene como -lraylib
        target_link_libraries(chess PRIVATE
                raylib
                winmm gdi32 user32 shell32 ole32 opengl32
        )
    else()
        # Linux
        target_link_libraries(chess PRIVATE raylib m)
    endif()
endif()

# Warnings útiles
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(chess PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# --- Copiar assets junto al binario final (funciona en VS/MSYS2/Unix) ---
add_custom_command(
        TARGET chess POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:chess>/assets
        COMMENT "Copiando assets/ a la carpeta del ejecutable"
)

# --- (Opcional) reglas de instalación: cmake --install build --prefix out ---
install(TARGETS chess RUNTIME DESTINATION .)
install(DIRECTORY assets DESTINATION .)

# (Opcional) salida en build/bin para generadores single-config
# set_target_properties(chess PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
