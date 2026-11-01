set(BRREPORT_WIDGETS_HEADERS
    brwidgetsglobal.h
    brpreviewdialog.h
    brpreviewwidget.h
    brprinting.h
    brreportwidgets.h
    brtranslations.h
)

set(BRREPORT_WIDGETS_SOURCES
    brpreviewdialog.cpp
    brpreviewwidget.cpp
    brprinting.cpp
    brtranslations.cpp
)

add_library(BrReportWidgets ${BRREPORT_WIDGETS_HEADERS} ${BRREPORT_WIDGETS_SOURCES})
add_library(BrReport::Widgets ALIAS BrReportWidgets)

target_compile_definitions(BrReportWidgets PRIVATE BRREPORT_WIDGETS_LIBRARY QT_NO_CAST_FROM_ASCII)
target_include_directories(BrReportWidgets PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/BrReport>)
target_link_libraries(BrReportWidgets PUBLIC
    BrReportCore ${BRREPORT_QT}::Widgets ${BRREPORT_QT}::PrintSupport)

# --- Translations: .ts -> .qm, embedded as resources (:/brreport/translations)
file(GLOB BRREPORT_TS_FILES ${PROJECT_SOURCE_DIR}/translations/*.ts)
find_package(${BRREPORT_QT} QUIET COMPONENTS LinguistTools)
if(TARGET ${BRREPORT_QT}::lrelease AND QT_VERSION_MAJOR GREATER_EQUAL 6)
    set(BRREPORT_QM_FILES)
    foreach(ts ${BRREPORT_TS_FILES})
        get_filename_component(base ${ts} NAME_WE)
        set(qm ${CMAKE_CURRENT_BINARY_DIR}/translations/${base}.qm)
        add_custom_command(OUTPUT ${qm}
            COMMAND ${BRREPORT_QT}::lrelease ${ts} -qm ${qm} -silent
            DEPENDS ${ts} VERBATIM)
        list(APPEND BRREPORT_QM_FILES ${qm})
    endforeach()
    qt_add_resources(BrReportWidgets brreport_translations
        PREFIX /brreport/translations
        BASE ${CMAKE_CURRENT_BINARY_DIR}/translations
        FILES ${BRREPORT_QM_FILES})
    target_compile_definitions(BrReportWidgets PRIVATE BRREPORT_HAS_TRANSLATIONS)
else()
    message(WARNING "BrReport: Qt LinguistTools (Qt 6) not found, the user interface will not be translated.")
endif()

set_target_properties(BrReportWidgets PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME Widgets
    OUTPUT_NAME brreportwidgets)

install(TARGETS BrReportWidgets EXPORT BrReportTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${BRREPORT_WIDGETS_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/BrReport)
