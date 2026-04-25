set(TARGET back-tester-pipeline)
file(GLOB SRC CONFIGURE_DEPENDS *.cpp)

add_library(${TARGET} STATIC ${SRC})

target_link_libraries(${TARGET} PUBLIC
    back-tester-common
    back-tester-parser
    back-tester-lob
)

# Pipeline uses std::thread; ensure pthread is linked even with old toolchains.
find_package(Threads REQUIRED)
target_link_libraries(${TARGET} PUBLIC Threads::Threads)
