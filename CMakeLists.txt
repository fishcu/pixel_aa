cmake_minimum_required(VERSION 3.12)
set(PROJECT_NAME "pixel_aa")
project(${PROJECT_NAME})

option(BUILD_FOR_MM "Enable build for Miyoo Mini" OFF)

if (BUILD_FOR_MM)
    set(CMAKE_C_COMPILER "$(CROSS_COMPILE)gcc")
    set(CMAKE_CXX_COMPILER "$(CROSS_COMPILE)g++")
endif()

find_package(OpenMP)

add_executable(${PROJECT_NAME}
    "src/main.cpp"
)
include_directories(
    "deps/stb"
)
set_target_properties(${EXECUTABLE_NAME} PROPERTIES
#   CXX_STANDARD 17
#   CXX_STANDARD_REQUIRED YES
    POSITION_INDEPENDENT_CODE ON
)
if (BUILD_FOR_MM)
    target_compile_options(${PROJECT_NAME}
        PRIVATE
        "-Wall"
        "-pedantic"
        "-O3"
        "-finline-functions"
        "-funroll-loops"
        "-marm" "-march=armv7ve+simd" "-mtune=cortex-a7" "-mfpu=neon-vfpv4" "-mfloat-abi=hard"
        "-ffunction-sections" "-fdata-sections" "-Wl,--gc-sections" "-Wl,-s"
    )
else()
    target_compile_options(${PROJECT_NAME}
        PRIVATE
        "-Wall"
        "-pedantic"
        "-O3"
        "-march=native"
        "-finline-functions"
        "-funroll-loops"
    )
endif()
if(OpenMP_CXX_FOUND)
    target_link_libraries(${PROJECT_NAME} PUBLIC OpenMP::OpenMP_CXX)
endif()
