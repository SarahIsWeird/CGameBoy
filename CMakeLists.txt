cmake_minimum_required(VERSION 3.19)
project(CGameBoy C)

set(CMAKE_C_STANDARD 99)

add_executable(CGameBoy src/main.c src/components/cpu.h src/components/cpu.c)