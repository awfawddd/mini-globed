cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(MiniGlobed VERSION 1.0.0)

# CRITICO: Previene il conflitto winsock.h vs winsock2.h
# Deve essere impostato PRIMA di qualsiasi add_subdirectory
add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)

# Geode SDK setup
add_library(${PROJECT_NAME} SHARED
    src/main.cpp
    src/network.cpp
    src/player_node.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE src)

# Link ws2_32 per le funzioni di rete
if(WIN32)
    target_link_libraries(${PROJECT_NAME} ws2_32)
endif()

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "GEODE_SDK non trovato! Assicurati di aver impostato la variabile d'ambiente.")
else()
    message(STATUS "GEODE_SDK trovato: $ENV{GEODE_SDK}")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

setup_geode_mod(${PROJECT_NAME})
