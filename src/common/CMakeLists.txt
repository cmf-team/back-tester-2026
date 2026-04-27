set(TARGET back-tester-common)
file(GLOB SRC CONFIGURE_DEPENDS *.cpp)

add_library(${TARGET} STATIC ${SRC})
add_dependencies(${TARGET} GenerateVersion)

# AsyncQueue uses std::thread / mutex / condition_variable.
find_package(Threads REQUIRED)

target_link_libraries(${TARGET} PUBLIC
    Threads::Threads
)
