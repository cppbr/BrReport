# Console example: needs only the core library.
add_executable(hello_world hello_world/main.cpp)
target_link_libraries(hello_world PRIVATE BrReportCore)

if(BRREPORT_BUILD_WIDGETS)
    foreach(example quick_start customer_list grouped_sales barcodes receipt dashboard)
        add_executable(${example} ${example}/main.cpp common/exampleutil.h)
        target_include_directories(${example} PRIVATE common)
        target_link_libraries(${example} PRIVATE BrReportWidgets)
    endforeach()

    add_executable(report_file report_file/main.cpp report_file/customers.brt common/exampleutil.h)
    target_include_directories(report_file PRIVATE common)
    target_compile_definitions(report_file PRIVATE BRREPORT_EXAMPLE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/report_file")
    target_link_libraries(report_file PRIVATE BrReportWidgets)

    if(BRREPORT_WITH_SQL)
        add_executable(master_detail_sql master_detail_sql/main.cpp common/exampleutil.h)
        target_include_directories(master_detail_sql PRIVATE common)
        target_link_libraries(master_detail_sql PRIVATE BrReportWidgets)
    endif()
endif()
