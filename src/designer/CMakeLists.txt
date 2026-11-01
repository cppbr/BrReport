set(BRREPORT_DESIGNER_HEADERS
    brdesignerglobal.h
    brdesignerwidget.h
)

set(BRREPORT_DESIGNER_SOURCES
    brdesignerwidget.cpp
    brdesignerlabels.cpp
    brdesignerlabels_p.h
    brdesignscene.cpp
    brdesignscene_p.h
    brpropertyeditor.cpp
    brpropertyeditor_p.h
)

add_library(BrReportDesigner ${BRREPORT_DESIGNER_HEADERS} ${BRREPORT_DESIGNER_SOURCES})
add_library(BrReport::Designer ALIAS BrReportDesigner)

target_compile_definitions(BrReportDesigner PRIVATE BRREPORT_DESIGNER_LIBRARY QT_NO_CAST_FROM_ASCII)
target_include_directories(BrReportDesigner PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/BrReport>)
target_link_libraries(BrReportDesigner PUBLIC BrReportWidgets)

set_target_properties(BrReportDesigner PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME Designer
    OUTPUT_NAME brreportdesigner)

install(TARGETS BrReportDesigner EXPORT BrReportTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${BRREPORT_DESIGNER_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/BrReport)
