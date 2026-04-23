add_subdirectory(common)
add_subdirectory(main)

target_link_libraries(back-tester-lib PUBLIC simdjson-static-lib)
add_dependencies(back-tester-lib simdjson-static-lib)