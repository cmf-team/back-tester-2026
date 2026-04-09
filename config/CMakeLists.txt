file(GLOB CFG CONFIGURE_DEPENDS *.cfg *.cfg.in *.txt *.txt.in)

foreach(SRC ${CFG})
    get_filename_component(BASE ${SRC} NAME)
    string(REGEX REPLACE "\\.in$" "" TGT ${BASE})
    configure_file(${SRC} ${CMAKE_BINARY_DIR}/cfg/${TGT})
endforeach()
