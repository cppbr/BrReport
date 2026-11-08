cmake_minimum_required(VERSION 3.16)

project(BrReport
    VERSION 0.1.0
    DESCRIPTION "Band-oriented report generator for C++/Qt"
    HOMEPAGE_URL "https://github.com/cppbr/BrReport"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

option(BUILD_SHARED_LIBS        "Build shared libraries"                         ON)
option(BRREPORT_BUILD_WIDGETS   "Build the preview/print library (QtWidgets)"    ON)
option(BRREPORT_BUILD_DESIGNER  "Build the visual report designer"               ON)
option(BRREPORT_WITH_SQL        "Build BrSqlDataSource (QtSql)"                  ON)
option(BRREPORT_WITH_SVG        "SVG images kept as vectors (QtSvg, if found)"   ON)
option(BRREPORT_BUILD_EXAMPLES  "Build the examples"                             ON)
option(BRREPORT_BUILD_TOOLS     "Build brview and brdesigner"                    ON)
option(BRREPORT_BUILD_TESTS     "Build the unit tests"                           ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
set(BRREPORT_QT_COMPONENTS Core Gui)
if(BRREPORT_WITH_SQL)
    list(APPEND BRREPORT_QT_COMPONENTS Sql)
endif()
if(BRREPORT_BUILD_WIDGETS)
    list(APPEND BRREPORT_QT_COMPONENTS Widgets PrintSupport)
endif()
if(BRREPORT_BUILD_TESTS)
    list(APPEND BRREPORT_QT_COMPONENTS Test)
endif()
# The minimum is asked per major version: some generated configs (e.g. Conan's) reject 5.15 for Qt 6.
if(QT_VERSION_MAJOR EQUAL 5)
    set(BRREPORT_QT_MIN_VERSION 5.15)
else()
    set(BRREPORT_QT_MIN_VERSION ${QT_VERSION_MAJOR}.0)
endif()
find_package(Qt${QT_VERSION_MAJOR} ${BRREPORT_QT_MIN_VERSION} REQUIRED COMPONENTS ${BRREPORT_QT_COMPONENTS})
set(BRREPORT_QT Qt${QT_VERSION_MAJOR})
if(BRREPORT_WITH_SVG)
    find_package(Qt${QT_VERSION_MAJOR} QUIET OPTIONAL_COMPONENTS Svg)
endif()
if(TARGET ${BRREPORT_QT}::Svg)
    set(BRREPORT_HAS_SVG ON)
endif()
message(STATUS "BrReport: using Qt ${${BRREPORT_QT}_VERSION}")

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Windows has no rpath: put the DLLs and the executables in one folder so that the
# tests, tools and examples run from the build tree.
if(WIN32 AND CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR AND NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
endif()

if(NOT BUILD_SHARED_LIBS)
    set(BRREPORT_STATIC ON)
endif()
if(BRREPORT_WITH_SQL)
    set(BRREPORT_HAS_SQL ON)
endif()

add_subdirectory(src/core)
if(BRREPORT_BUILD_WIDGETS)
    add_subdirectory(src/widgets)
    if(BRREPORT_BUILD_DESIGNER)
        add_subdirectory(src/designer)
    endif()
endif()
if(BRREPORT_BUILD_WIDGETS AND BRREPORT_BUILD_TOOLS)
    add_subdirectory(tools)
endif()
# Maintainers: `cmake --build build --target update_translations` refreshes the .ts files.
if(TARGET ${BRREPORT_QT}::lupdate)
    file(GLOB BRREPORT_ALL_TS ${CMAKE_CURRENT_SOURCE_DIR}/translations/*.ts)
    add_custom_target(update_translations
        COMMAND ${BRREPORT_QT}::lupdate -locations none -no-obsolete src/widgets src/designer tools
                -ts ${BRREPORT_ALL_TS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Updating translation sources"
        VERBATIM)
endif()

# API reference: `cmake --build build --target docs`, in build/docs/html.
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    configure_file(docs/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        COMMENT "Generating the API reference"
        VERBATIM)
endif()

if(BRREPORT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
if(BRREPORT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# --- Package config (find_package(BrReport)) ---
configure_package_config_file(
    cmake/BrReportConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/BrReportConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/BrReport)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/BrReportConfigVersion.cmake
    COMPATIBILITY SameMinorVersion)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/BrReportConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/BrReportConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/BrReport)
install(EXPORT BrReportTargets
    NAMESPACE BrReport::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/BrReport)
