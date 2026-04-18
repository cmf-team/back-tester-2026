add_subdirectory(common)

file(GLOB_RECURSE BACK_TESTER_LIB_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
)
list(FILTER BACK_TESTER_LIB_SOURCES EXCLUDE REGEX "/common/")
list(REMOVE_ITEM BACK_TESTER_LIB_SOURCES
  "${CMAKE_CURRENT_LIST_DIR}/main/main.cpp"
)

add_executable(back-tester
  "${CMAKE_CURRENT_LIST_DIR}/main/main.cpp"
  ${BACK_TESTER_LIB_SOURCES}
)
target_link_libraries(back-tester PUBLIC back-tester-common)
