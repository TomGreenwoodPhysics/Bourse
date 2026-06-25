from pybind11.setup_helpers import Pybind11Extension, build_ext
from setuptools import setup

ext_modules = [
    Pybind11Extension(
        "bourse._bourse",
        ["src/order_book.cpp", "bindings/pybind_module.cpp"],
        include_dirs=["include"],
        cxx_std=20,
    ),
]

setup(ext_modules=ext_modules, cmdclass={"build_ext": build_ext})