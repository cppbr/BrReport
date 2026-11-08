set(BRREPORT_TEST_ENV "QT_QPA_PLATFORM=offscreen")
if(WIN32)
    # On Windows the offscreen platform does not look for the system fonts by itself.
    file(TO_CMAKE_PATH "$ENV{WINDIR}/Fonts" BRREPORT_WINDOWS_FONTS)
    list(APPEND BRREPORT_TEST_ENV "QT_QPA_FONTDIR=${BRREPORT_WINDOWS_FONTS}")
endif()

function(brreport_test_properties name)
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "${BRREPORT_TEST_ENV}")
    if(WIN32 AND NOT CMAKE_VERSION VERSION_LESS 3.22)
        # Find the Qt DLLs without adding Qt to the PATH by hand.
        set_tests_properties(${name} PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:${BRREPORT_QT}::Core>")
    endif()
endfunction()

function(brreport_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE BrReportCore ${BRREPORT_QT}::Test)
    add_test(NAME ${name} COMMAND ${name})
    brreport_test_properties(${name})
endfunction()

brreport_add_test(tst_expression)
brreport_add_test(tst_format)
brreport_add_test(tst_barcode)
brreport_add_test(tst_engine)
brreport_add_test(tst_document)
brreport_add_test(tst_features)
brreport_add_test(tst_reportfile)

if(TARGET BrReportDesigner)
    add_executable(tst_designer tst_designer.cpp)
    target_link_libraries(tst_designer PRIVATE BrReportDesigner ${BRREPORT_QT}::Test)
    target_include_directories(tst_designer PRIVATE ${PROJECT_SOURCE_DIR}/src/designer)
    add_test(NAME tst_designer COMMAND tst_designer)
    brreport_test_properties(tst_designer)
endif()
