cmake_minimum_required( VERSION 3.2 )

project(fixtr VERSION 0.1.0)
set(SPEC_INSTALL_DIR share/fixtr)

################################
# fixtr
################################

# ------
# SOURCE
# ------

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(INCLUDE_FILES
   fixcore.h
    )

set(FIXCORE_SOURCE_FILES
    fixcore.cpp
    )

set(FIXTR_SOURCE_FILES
    fixtr.cpp
    )

set(FIXSPEC_SOURCE_FILES
    fixspec.cpp
    )

find_package(LibXml2)

# -------
# fixcore
# -------

add_library(fixcore STATIC ${FIXCORE_SOURCE_FILES} ${INCLUDE_FILES})
target_include_directories(fixcore PUBLIC ${LIBXML2_INCLUDE_DIR})
target_link_libraries(fixcore PUBLIC ${LIBXML2_LIBRARIES})
target_compile_definitions(fixcore PUBLIC FIXTR_SPEC_DIR=${CMAKE_INSTALL_PREFIX}/${SPEC_INSTALL_DIR})

# -----
# fixtr
# -----

add_executable(fixtr ${FIXTR_SOURCE_FILES})
target_link_libraries(fixtr PUBLIC fixcore)

# -------
# fixspec
# -------

add_executable(fixspec ${FIXSPEC_SOURCE_FILES})
target_link_libraries(fixspec PUBLIC fixcore)


# -------
# INSTALL
# -------

install(
    DIRECTORY spec/
    DESTINATION ${SPEC_INSTALL_DIR}
    )
install (
    TARGETS fixtr fixspec
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    )

