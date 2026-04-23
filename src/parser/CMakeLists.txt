foreach(app hard standard)
    set(TARGET back-tester-parser-${app})
    add_executable(${TARGET} ${app}.cpp)
    target_link_libraries(${TARGET} PUBLIC back-tester-common simdjson-static-lib)
    # both files trip -Wall -Wextra; relax until cleaned up
    target_compile_options(${TARGET} PRIVATE -Wno-error)
endforeach()