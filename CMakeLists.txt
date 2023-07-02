cmake_minimum_required(VERSION 3.12)
set(PROJECT_NAME "pixel_aa")
project(${PROJECT_NAME})

add_executable(${PROJECT_NAME}
    "src/main.cpp"
)
include_directories(
    "deps/stb"
    "deps/glm"
)
set_target_properties(${EXECUTABLE_NAME} PROPERTIES
#   CXX_STANDARD 17
#   CXX_STANDARD_REQUIRED YES
    POSITION_INDEPENDENT_CODE ON
)
target_compile_options(${PROJECT_NAME}
    PRIVATE "-Wall" "-pedantic" "-Os"
)