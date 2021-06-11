cmake_minimum_required(VERSION 3.13.0) # 3.13 minimum for CMP0079
project(allovisor VERSION 0.2.0)

add_subdirectory (deps/allonet)

set(SOURCES
    src/main.c
)
include_directories(src deps/allonet/include deps/allonet/lib)
include_directories(SYSTEM
    "${PROJECT_SOURCE_DIR}/lib/enet/include"
    "${PROJECT_SOURCE_DIR}/lib/tinycthread/source"
    "deps/allonet/lib"
)

add_executable(alloplace2 ${SOURCES})
target_link_libraries(alloplace2 allonet)