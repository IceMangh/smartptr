cmake_minimum_required(VERSION 4.0)
project(smartptr)

set(CMAKE_CXX_STANDARD 20)


add_executable(smartptr main.cpp
        UnqPtr.h
        ShrdPtr.h
        )

add_executable(pointer_tests tests.cpp
        Stack.h
)

target_compile_options(pointer_tests PRIVATE
        -g
        -UNDEBUG
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
        -fno-sanitize-recover=all
)

target_link_options(pointer_tests PRIVATE
        -fsanitize=address,undefined
)
set_target_properties(pointer_tests PROPERTIES CXX_STANDARD 20)


# Отдельная сборка тех же тестов без санитайзеров.
add_executable(pointer_tests_leaks tests.cpp)

target_compile_options(pointer_tests_leaks PRIVATE
        -g
        -O0
        -UNDEBUG
)

add_custom_target(check_leaks
        COMMAND ${CMAKE_COMMAND} -E env MallocStackLogging=1
        /usr/bin/leaks --atExit --
        $<TARGET_FILE:pointer_tests_leaks>
        DEPENDS pointer_tests_leaks
        USES_TERMINAL
)
