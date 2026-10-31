configure_file(brreport_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/brreport_config.h)

set(BRREPORT_CORE_HEADERS
    brglobal.h
    brband.h
    brbarcode.h
    brcontrol.h
    brdatasource.h
    brdocument.h
    brexport.h
    brexpression.h
    brformat.h
    brgraphics.h
    brpager.h
    brqrcode.h
    brrendercontext.h
    brrenderer.h
    brreport.h
    brreportcore.h
    brtextcontrols.h
    brtextmetrics.h
    ${CMAKE_CURRENT_BINARY_DIR}/brreport_config.h
)

set(BRREPORT_CORE_SOURCES
    brband.cpp
    brbarcode.cpp
    brcontrol.cpp
    brdatasource.cpp
    brdocument.cpp
    brengine.cpp
    brengine_p.h
    brexport.cpp
    brexpression.cpp
    brformat.cpp
    brgraphics.cpp
    brpager.cpp
    brqrcode.cpp
    brrendercontext.cpp
    brrenderer.cpp
    brreport.cpp
    brtextcontrols.cpp
    brtextmetrics.cpp
)

if(BRREPORT_WITH_SQL)
    list(APPEND BRREPORT_CORE_HEADERS brsqldatasource.h)
    list(APPEND BRREPORT_CORE_SOURCES brsqldatasource.cpp)
endif()

add_library(BrReportCore ${BRREPORT_CORE_HEADERS} ${BRREPORT_CORE_SOURCES})
add_library(BrReport::Core ALIAS BrReportCore)

target_compile_definitions(BrReportCore PRIVATE BRREPORT_CORE_LIBRARY QT_NO_CAST_FROM_ASCII)
target_include_directories(BrReportCore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/BrReport>)
target_link_libraries(BrReportCore PUBLIC ${BRREPORT_QT}::Core ${BRREPORT_QT}::Gui)
if(BRREPORT_WITH_SQL)
    target_link_libraries(BrReportCore PUBLIC ${BRREPORT_QT}::Sql)
endif()

set_target_properties(BrReportCore PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME Core
    OUTPUT_NAME brreportcore)

install(TARGETS BrReportCore EXPORT BrReportTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${BRREPORT_CORE_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/BrReport)
