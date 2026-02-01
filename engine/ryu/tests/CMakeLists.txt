add_executable(test_basic test_basic.c)
target_link_libraries(test_basic ryu)
target_compile_options(test_basic PRIVATE -g -Wall -Wpedantic -Wextra)
add_test(NAME BasicTest COMMAND test_basic)

add_executable(test_random test_random.c)
target_link_libraries(test_random ryu)
target_compile_options(test_random PRIVATE -g -Wall -Wpedantic -Wextra)
add_test(NAME RandomTest COMMAND test_random)
