#-------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
#--------------------------------------------------------------------------

from setuptools import setup, find_packages
from os import path, getcwd
import platform
import sys

package_name = 'onnxruntime'
if '--use_cuda' in sys.argv:
    package_name = 'onnxruntime-gpu'
    sys.argv.remove('--use_cuda')
          
try:
    from wheel.bdist_wheel import bdist_wheel as _bdist_wheel
    class bdist_wheel(_bdist_wheel):
        def finalize_options(self):
            _bdist_wheel.finalize_options(self)
            self.root_is_pure = False
except ImportError:
    bdist_wheel = None

# Additional binaries
if platform.system() == 'Linux':
  libs = ['onnxruntime_pybind11_state.so', 'libmkldnn.so.0', 'libmklml_intel.so', 'libiomp5.so']
else:
  libs = ['onnxruntime_pybind11_state.pyd', 'mkldnn.dll', 'mklml.dll', 'libiomp5md.dll']

data = [path.join('capi', x) for x in libs if path.isfile(path.join('onnxruntime', 'capi', x))]

# Additional examples
examples_names = ["mul_1.pb", "logreg_iris.onnx", "sigmoid.onnx"]
examples = [path.join('datasets', x) for x in examples_names]

# Extra files such as EULA and ThirdPartyNotices
extra = ["ONNXRuntime_EndUserLicenseAgreement.docx", "ThirdPartyNotices.txt"]

# Description
README = path.join(getcwd(), "README.rst")
if not path.exists(README):
    this = path.dirname(__file__)
    README = path.join(this, "README.rst")
if not path.exists(README):
    raise FileNotFoundError("Unable to find 'README.rst'")
with open(README) as f:
    long_description = f.read()


# Setup
setup(
    name=package_name,
    version='0.1.3',
    description='ONNX Runtime Runtime Python bindings',
    long_description=long_description,
    author='Microsoft Corporation',
    author_email='onnx@microsoft.com',
    cmdclass={'bdist_wheel': bdist_wheel},
    license="Microsoft Software License Terms",
    packages=['onnxruntime',
              'onnxruntime.backend',
              'onnxruntime.capi',
              'onnxruntime.datasets',
              'onnxruntime.tools',
              ],
    package_data={
        'onnxruntime': data + examples + extra,
    },
    extras_require={
        'backend': ['onnx>=1.2.3'],
    },
    entry_points= {
        'console_scripts': [
            'onnxruntime_test = onnxruntime.tools.onnxruntime_test:main',
        ]
    },
    classifiers=[
        'Development Status :: 4 - Beta',
        'Environment :: Console',
        'Intended Audience :: Developers',
        'Operating System :: POSIX :: Linux',
        'Programming Language :: Python',
        'Programming Language :: Python :: 3 :: Only',
        'Programming Language :: Python :: 3.5',
        'Programming Language :: Python :: 3.6',
        'Programming Language :: Python :: 3.7'],
    )    

