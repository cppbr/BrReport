# Overlay port: vcpkg install brreport --overlay-ports=packaging/vcpkg
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO "cppbr/BrReport"
    REF "v${VERSION}"
    SHA512 172655e8c979ada87f1e56cbc1463106c30a2250d8d6c56561006145d97ac33f41848471755162d726e4075a79cf7e2b7bd25a915ac0b7c67a8649a20c29574b
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
