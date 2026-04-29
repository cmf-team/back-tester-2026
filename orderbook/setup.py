import os
from setuptools import setup, Extension, find_packages
import numpy as np

extra_compile_args = [
    "-g0",
    "-DNPY_NO_DEPRECATED_API=NPY_1_7_API_VERSION",
    "-Wno-unused-function",
    "-fvisibility=hidden",
    "-ffunction-sections", "-fdata-sections",
    "-O3",

    "-march=native",
    "-mtune=native",
    "-mavx2",
    "-mfma",
    "-funroll-loops",
    "-finline-functions",
]

extra_link_args = [
    "-Wl,--strip-all",
    "-flto",
    "-Wl,--gc-sections",
]

orderbook_module = Extension('orderbook',
    sources=['orderbook.pyx'],
    include_dirs=[np.get_include()],
    language='c++',
    extra_compile_args=extra_compile_args,
    extra_link_args=extra_link_args
)

__location__ = os.path.realpath(os.path.join(os.getcwd(), os.path.dirname(__file__)))

setup(
    name='orderbook',
    description='simple L3 orderbook builder',
    url='https://github.com/cmf-team/back-tester-2026/orderbook',
    author='landy0',
    version='0.1',
    long_description_content_type='text/markdown',
    packages=find_packages(),
    python_requires='>=3.9',
    ext_modules=[orderbook_module]
)
