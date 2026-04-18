include(CTest)

if (EXISTS ${CMAKE_SOURCE_DIR}/3rdparty/Catch2/extras/Catch.cmake)
    include(${CMAKE_SOURCE_DIR}/3rdparty/Catch2/extras/Catch.cmake)
else()
    function(catch_discover_tests TARGET)
    # Dummy macro to satisfy CMake until it fetches Catch2 repo.
    endfunction()
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/test)

file(GLOB TEST_SRC CONFIGURE_DEPENDS *.cpp)

# executable
set(TARGET back-tester-tests)
add_executable(${TARGET} ${TEST_SRC})

# Link against the necessary libraries
target_link_libraries(${TARGET} PRIVATE
    Catch2-static-lib
    back-tester-lib
)

# Register the tests with CTest
catch_discover_tests(${TARGET})
