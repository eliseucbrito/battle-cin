cmake_minimum_required(VERSION 3.15)
project(meu_projeto)

set(CMAKE_CXX_STANDARD 17)

find_package(raylib REQUIRED)
find_package(nlohmann_json REQUIRED)

# protocol.h lives at the project root — both targets need to find it
include_directories(${CMAKE_SOURCE_DIR})

# ── Client ────────────────────────────────────────────────────────────────────
add_executable(meu_projeto src/main.cpp)
target_link_libraries(meu_projeto raylib nlohmann_json::nlohmann_json)

# ── Server ────────────────────────────────────────────────────────────────────
add_executable(servidor server.cpp)

# ── Assets ────────────────────────────────────────────────────────────────────
add_custom_target(copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/assets
    COMMAND rsync -a --include="*.png" --include="*.jpg" --include="*.json" --exclude="*"
            ${CMAKE_SOURCE_DIR}/src/ ${CMAKE_BINARY_DIR}/assets/
    COMMENT "Copiando assets para build/assets/"
)
add_dependencies(meu_projeto copy_assets)
