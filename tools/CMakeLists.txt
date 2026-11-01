add_executable(brview brview/main.cpp)
target_link_libraries(brview PRIVATE BrReportWidgets)
install(TARGETS brview RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(BRREPORT_BUILD_DESIGNER)
    add_executable(brdesigner brdesigner/main.cpp)
    target_link_libraries(brdesigner PRIVATE BrReportDesigner)
    install(TARGETS brdesigner RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
