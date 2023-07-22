cmake_minimum_required(VERSION 3.12)
set(PROJECT_NAME "pixel_aa")
project(${PROJECT_NAME})

option(BUILD_FOR_MM "Enable build for Miyoo Mini" OFF)
option(USE_OPENMP "Enable OpenMP multithreading" OFF)

if (BUILD_FOR_MM)
    message(STATUS "Building for MM, cross compile var is $ENV{CROSS_COMPILE}") 
    set(CMAKE_C_COMPILER "$ENV{CROSS_COMPILE}gcc")
    set(CMAKE_CXX_COMPILER "$ENV{CROSS_COMPILE}g++")
endif()

if (USE_OPENMP)
    find_package(OpenMP)
endif()

include_directories(
    "deps/stb"
)

set(TINYCC_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/tinycc)
if (BUILD_FOR_MM)
    set(TINYCC_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib_tcc/arm)
else()
    set(TINYCC_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib_tcc/x86_64)
endif()
add_library(tinycc INTERFACE)
target_link_libraries(tinycc INTERFACE ${TINYCC_LIB_DIR}/libtcc.a ${TINYCC_LIB_DIR}/libtcc1.a)
target_include_directories(tinycc INTERFACE ${TINYCC_SOURCE_DIR})
target_link_libraries(tinycc INTERFACE pthread dl)

add_executable(${PROJECT_NAME}
    "src/pixel_aa.c"
)
set_target_properties(${PROJECT_NAME} PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
target_link_libraries(${PROJECT_NAME} PRIVATE
    m
    tinycc
)
if (USE_OPENMP)
    target_compile_definitions(${PROJECT_NAME}
        PRIVATE
        "USE_OPENMP"
    )
endif()
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
        "-fopenmp"
    )
    if (USE_OPENMP)
        target_link_options(${PROJECT_NAME}
            PRIVATE
            "-fopenmp"
        )
    endif()
    target_compile_definitions(${PROJECT_NAME}
        PRIVATE
        "BUILD_FOR_MM"
        "FIXED_POINT"
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
    if(USE_OPENMP AND OpenMP_CXX_FOUND)
        target_link_libraries(${PROJECT_NAME} PUBLIC OpenMP::OpenMP_CXX)
    endif()
endif()

add_executable(tcc_jit
    "src/tcc_jit.c"
)
target_link_libraries(tcc_jit PRIVATE
    m
    tinycc
)
if (BUILD_FOR_MM)
    target_compile_options(tcc_jit
        PRIVATE
        "-Wall"
        "-pedantic"
        "-O3"
        "-finline-functions"
        "-funroll-loops"
        "-marm" "-march=armv7ve+simd" "-mtune=cortex-a7" "-mfpu=neon-vfpv4" "-mfloat-abi=hard"
        "-ffunction-sections" "-fdata-sections" "-Wl,--gc-sections" "-Wl,-s"
    )
    target_compile_definitions(tcc_jit
        PRIVATE
        "BUILD_FOR_MM"
    )
else()
    target_compile_options(tcc_jit
        PRIVATE
        "-Wall"
        "-pedantic"
        "-O3"
        "-finline-functions"
        "-funroll-loops"
        "-march=native"
    )
endif()

find_package(ZLIB REQUIRED)

set(LLVM_DIR "/root/workspace/stage/lib/cmake/llvm")
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

add_executable(llvm_jit
    "src/llvm_jit.c"
)
# target_link_libraries(llvm_jit PRIVATE

# )
if (BUILD_FOR_MM)
    target_compile_options(llvm_jit
        PRIVATE
        "-Wall"
        "-pedantic"
        "-O3"
        "-finline-functions"
        "-funroll-loops"
        "-marm" "-march=armv7ve+simd" "-mtune=cortex-a7" "-mfpu=neon-vfpv4" "-mfloat-abi=hard"
        "-ffunction-sections" "-fdata-sections" "-Wl,--gc-sections" "-Wl,-s"
    )
    target_compile_definitions(llvm_jit
        PRIVATE
        "BUILD_FOR_MM"
    )
else()
    target_compile_options(llvm_jit
        PRIVATE
        "-Wall"
        "-pedantic"
        "-O3"
        "-finline-functions"
        "-funroll-loops"
        "-march=native"
    )
endif()

target_include_directories(llvm_jit PRIVATE ${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
target_compile_definitions(llvm_jit PRIVATE ${LLVM_DEFINITIONS_LIST})
llvm_map_components_to_libnames(llvm_libs support core irreader)
target_link_libraries(llvm_jit ${llvm_libs})

