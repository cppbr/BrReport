# Overlay port: vcpkg install brreport --overlay-ports=packaging/vcpkg
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO "cppbr/BrReport"
    REF "v${VERSION}"
    SHA512 ad9513da000d2663114ea5e9d087ca49724f95b78a86756707fd9dca4ffc953a3ae93c6d4763e2486c3d474007e853c30dba7ac1c47057c26aa388af3b40b1be
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBRREPORT_BUILD_EXAMPLES=OFF
        -DBRREPORT_BUILD_TOOLS=OFF
        -DBRREPORT_BUILD_TESTS=OFF
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME BrReport CONFIG_PATH lib/cmake/BrReport)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
