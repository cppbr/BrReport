# Overlay port: vcpkg install brreport --overlay-ports=packaging/vcpkg
# Fill SHA512 once the tag v${VERSION} is pushed (vcpkg prints the right value on the first install).
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO "cppbr/BrReport"
    REF "v${VERSION}"
    SHA512 0
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
