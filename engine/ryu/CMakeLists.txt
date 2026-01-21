cmake_minimum_required(VERSION 3.10)

project(Ryu
	LANGUAGES C
)

add_library(ryu STATIC
	src/ryu.c
)
target_include_directories(ryu
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
set_target_properties(ryu PROPERTIES
	C_STANDARD 11
	C_STANDARD_REQUIRED YES
	C_EXTENSIONS NO
)
target_compile_options(ryu PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -g>
)

enable_testing()
add_subdirectory(tests)
