from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy
import os


class BrReportConan(ConanFile):
    """Run from the root of the repository: conan create packaging/conan --build=missing"""

    name = "brreport"
    version = "0.1.0"
    license = "Apache-2.0"
    description = "Band-oriented report generator for C++/Qt"
    topics = ("qt", "reports", "pdf", "barcode", "report-designer")
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "widgets": [True, False], "designer": [True, False], "sql": [True, False]}
    default_options = {"shared": True, "widgets": True, "designer": True, "sql": True}
    package_type = "library"

    def export_sources(self):
        root = os.path.join(self.recipe_folder, "..", "..")
        for pattern in ("CMakeLists.txt", "LICENSE", "cmake/*", "docs/Doxyfile.in", "src/*", "translations/*", "tests/*",
                        "examples/*", "tools/*"):
            copy(self, pattern, root, self.export_sources_folder)

    def requirements(self):
        self.requires("qt/[>=6.5 <7]")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        toolchain = CMakeToolchain(self)
        toolchain.variables["BRREPORT_BUILD_WIDGETS"] = self.options.widgets
        toolchain.variables["BRREPORT_BUILD_DESIGNER"] = self.options.designer
        toolchain.variables["BRREPORT_WITH_SQL"] = self.options.sql
        toolchain.variables["BRREPORT_BUILD_TESTS"] = False
        toolchain.variables["BRREPORT_BUILD_EXAMPLES"] = False
        toolchain.variables["BRREPORT_BUILD_TOOLS"] = False
        toolchain.generate()
        CMakeDeps(self).generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", self.source_folder, os.path.join(self.package_folder, "licenses"))
        CMake(self).install()

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "BrReport")
        self.cpp_info.builddirs.append(os.path.join("lib", "cmake", "BrReport"))
        self.cpp_info.set_property("cmake_find_mode", "none")
