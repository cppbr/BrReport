# Console example: needs only the core library.
add_executable(hello_world hello_world/main.cpp)
target_link_libraries(hello_world PRIVATE BrReportCore)

if(BRREPORT_BUILD_WIDGETS)
    foreach(example quick_start customer_list grouped_sales barcodes)
        add_executable(${example} ${example}/main.cpp common/exampleutil.h)
        target_include_directories(${example} PRIVATE common)
        target_link_libraries(${example} PRIVATE BrReportWidgets)
    endforeach()

    if(BRREPORT_WITH_SQL)
        add_executable(master_detail_sql master_detail_sql/main.cpp common/exampleutil.h)
        target_include_directories(master_detail_sql PRIVATE common)
        target_link_libraries(master_detail_sql PRIVATE BrReportWidgets)
    endif()
endif()
