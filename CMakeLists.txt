cmake_minimum_required(VERSION 3.15)
project(battle_cin)

set(CMAKE_CXX_STANDARD 17)

find_package(raylib REQUIRED CONFIG)
find_package(nlohmann_json REQUIRED CONFIG)

include_directories(include)

# ── Shared Source ─────────────────────────────────────────────────────────────
set(SHARED_SOURCES
    src/heroes/hero.cpp
    src/heroes/hero_tank.cpp
    src/heroes/hero_fighter.cpp
    src/heroes/hero_mage.cpp
    src/heroes/hero_assassin.cpp
    src/heroes/hero_support.cpp
    src/hero_factory.cpp
    src/trainer.cpp
)

# ── Client ────────────────────────────────────────────────────────────────────
add_executable(meu_projeto 
    src/client/main.cpp 
    src/client/renderer.cpp
    ${SHARED_SOURCES}
)
target_link_libraries(meu_projeto raylib nlohmann_json::nlohmann_json)

# ── Server ────────────────────────────────────────────────────────────────────
add_executable(servidor 
    src/server/main.cpp 
    src/server/game.cpp
    ${SHARED_SOURCES}
)
target_link_libraries(servidor nlohmann_json::nlohmann_json)

# ── Assets ────────────────────────────────────────────────────────────────────
add_custom_target(copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets ${CMAKE_BINARY_DIR}/assets
    COMMENT "Copiando assets para build/assets/"
)
add_dependencies(meu_projeto copy_assets)
add_dependencies(servidor copy_assets)
