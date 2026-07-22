"""Builds the `nanoback` pybind11 module by shelling out to the existing
CMake config (bindings/nanoback_bindings.cpp), targeting only the `nanoback`
CMake target so Backtester and the test suite aren't built during `pip install .`.
"""
import os
import subprocess
import sys
from pathlib import Path

from setuptools import Extension, setup
from setuptools.command.build_ext import build_ext


class CMakeExtension(Extension):
    def __init__(self, name, sourcedir=""):
        super().__init__(name, sources=[])
        self.sourcedir = os.fspath(Path(sourcedir).resolve())


class CMakeBuild(build_ext):
    def build_extension(self, ext):
        # Imported here, not at module level: this file is executed once (with
        # only setuptools present) to report `setup_requires` back to pip's build
        # isolation, then again after pip has installed pybind11 into that env.
        import pybind11

        ext_fullpath = Path.cwd() / self.get_ext_fullpath(ext.name)
        extdir = ext_fullpath.parent.resolve()

        cmake_args = [
            f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={extdir}{os.sep}",
            f"-DPython_EXECUTABLE={sys.executable}",
            f"-Dpybind11_DIR={pybind11.get_cmake_dir()}",
            "-DCMAKE_BUILD_TYPE=Release",
            # Legacy FindPythonLibsNew.cmake (pybind11's default) looks for a
            # standalone libpython.so, which isn't reliably discoverable inside
            # a venv. FindPython's Development.Module component only needs the
            # headers, which is all an extension module actually requires.
            "-DPYBIND11_FINDPYTHON=ON",
        ]
        build_args = ["--target", "nanoback"]

        build_temp = Path(self.build_temp) / ext.name
        build_temp.mkdir(parents=True, exist_ok=True)

        subprocess.run(
            ["cmake", ext.sourcedir, *cmake_args], cwd=build_temp, check=True
        )
        subprocess.run(
            ["cmake", "--build", ".", *build_args], cwd=build_temp, check=True
        )


setup(
    name="nanoback",
    version="0.1.0",
    setup_requires=["pybind11>=2.11"],
    ext_modules=[CMakeExtension("nanoback")],
    cmdclass={"build_ext": CMakeBuild},
)
