add_executable(brview brview/main.cpp)
target_link_libraries(brview PRIVATE BrReportWidgets)
install(TARGETS brview RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
