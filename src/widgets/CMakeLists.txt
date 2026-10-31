set(BRREPORT_WIDGETS_HEADERS
    brwidgetsglobal.h
    brpreviewdialog.h
    brpreviewwidget.h
    brprinting.h
    brreportwidgets.h
)

set(BRREPORT_WIDGETS_SOURCES
    brpreviewdialog.cpp
    brpreviewwidget.cpp
    brprinting.cpp
)

add_library(BrReportWidgets ${BRREPORT_WIDGETS_HEADERS} ${BRREPORT_WIDGETS_SOURCES})
add_library(BrReport::Widgets ALIAS BrReportWidgets)

target_compile_definitions(BrReportWidgets PRIVATE BRREPORT_WIDGETS_LIBRARY QT_NO_CAST_FROM_ASCII)
target_include_directories(BrReportWidgets PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/BrReport>)
target_link_libraries(BrReportWidgets PUBLIC
    BrReportCore ${BRREPORT_QT}::Widgets ${BRREPORT_QT}::PrintSupport)

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
