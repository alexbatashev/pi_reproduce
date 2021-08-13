import lit.formats
from lit.llvm import llvm_config
import subprocess

import os

config.name = "dpcpp_trace tests"
config.test_format = lit.formats.ShTest(True)
config.test_source_root = os.path.dirname(__file__)
config.test_exec_root = config.dpcpp_trace_tests_obj_dir

llvm_config.with_system_environment('LD_LIBRARY_PATH')
llvm_config.with_environment('LD_LIBRARY_PATH', config.dpcpp_trace_lib_dir, append_path=True)
llvm_config.with_environment('LD_LIBRARY_PATH', os.path.join(config.intel_llvm_bin_root, "lib"), append_path=True)

config.available_features.add('linux')

devices_res = subprocess.run(config.intel_llvm_bin_root + "/bin/sycl-ls", shell=True, stdout=subprocess.PIPE)

if "opencl:" in str(devices_res.stdout):
    config.available_features.add('opencl')
if "level_zero:" in str(devices_res.stdout):
    config.available_features.add('level_zero')
if "cuda:" in str(devices_res.stdout):
    config.available_features.add('cuda')
if "rocm:" in str(devices_res.stdout):
    config.available_features.add('rocm')

config.available_features.add('DISABLED')

config.substitutions.append(('%dpcpp_trace', config.dpcpp_trace_bin_dir + "/dpcpp_trace"))
config.substitutions.append(('%clangxx', config.intel_llvm_bin_root + "/bin/clang++"))
config.substitutions.append(('%sycl_ls', config.intel_llvm_bin_root + "/bin/sycl-ls"))
config.substitutions.append(('%FileCheck', config.filecheck_path))

config.suffixes = ['.cpp']
