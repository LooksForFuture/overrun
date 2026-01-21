cmake_minimum_required(VERSION 3.10)

project(overrun
	LANGUAGES C
)

add_subdirectory(engine)

add_executable(game
	src/main.c
)
target_include_directories(game
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
set_target_properties(game PROPERTIES
	C_STANDARD 11
	C_STANDARD_REQUIRED YES
	C_EXTENSIONS NO
)
target_compile_options(game PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)
target_link_libraries(game ryu)
