cmake_minimum_required(VERSION 3.22.1)
project(thtml LANGUAGES VERSION 1.0.1 CXX C)

set(TARGET thtml)
set(CMAKE_C_STANDARD   11)
set(CMAKE_C_STANDARD_REQUIRED true)
set(THREADS_PREFER_PTHREAD_FLAG ON)

#set(CMAKE_VERBOSE_MAKEFILE ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(TCL 8.6.13 REQUIRED)  # TCL_INCLUDE_PATH TCL_LIBRARY

message(STATUS "TCL_INCLUDE_PATH: ${TCL_INCLUDE_PATH}")
message(STATUS "TCL_LIBRARY: ${TCL_LIBRARY}")

add_compile_options(-Wall -Wextra -Wpedantic)
add_compile_definitions(TCL_THREADS PROJECT_VERSION=${PROJECT_VERSION})

if ("${ADDRESS_SANITIZER}" STREQUAL "ON")
    add_compile_options(-fPIC -g -fsanitize=undefined -fsanitize=address)
    add_link_options(-fsanitize=undefined -fsanitize=address)
endif ()

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(CMAKE_C_FLAGS "-g -DDEBUG ${CMAKE_C_FLAGS}")
else()
    # NDEBUG to not generate code for assert
    add_compile_definitions(NDEBUG)
endif ()


add_library(${PROJECT_NAME} SHARED src/library.c src/compiler_tcl.c src/compiler_c.c src/md5.c
        src/common.c)
set_target_properties(${PROJECT_NAME}
        PROPERTIES POSITION_INDEPENDENT_CODE ON
        INSTALL_RPATH_USE_LINK_PATH ON
)

include_directories(${TCL_INCLUDE_PATH})
target_link_directories(${PROJECT_NAME} PRIVATE ${TCL_LIBRARY_PATH})
target_link_libraries(${PROJECT_NAME} PRIVATE ${TCL_LIBRARY})
get_filename_component(TCL_LIBRARY_PATH "${TCL_LIBRARY}" PATH)

install(TARGETS ${TARGET}
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${TARGET}${PROJECT_VERSION}
)

configure_file(pkgIndex.tcl.in pkgIndex.tcl @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${TARGET}${PROJECT_VERSION}
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/tcl
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${TARGET}${PROJECT_VERSION}
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/cmake
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${TARGET}${PROJECT_VERSION}
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include
        DESTINATION ${CMAKE_INSTALL_PREFIX}
)