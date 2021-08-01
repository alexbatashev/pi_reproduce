// REQUIRES: opencl || level_zero || cuda || rocm
// RUN: %dpcpp_trace record --override -o %t_record %sycl_ls
// RUN: %dpcpp_trace print %t_record | FileCheck %s
//
// CHECK: ---> piPlatformsGet(
// CHECK: ---> piPlatformsGet(
// CHECK: ---> piDevicesGet(
// CHECK: ---> piDevicesGet(
// CHECK: ---> piDeviceGetInfo(
// CHECK: ---> piDeviceGetInfo(
// CHECK: ---> piPlatformGetInfo(
// CHECK: <pi_platform_info> : PI_PLATFORM_INFO_NAME
// CHECK: ---> piPlatformGetInfo(
// CHECK: <pi_platform_info> : PI_PLATFORM_INFO_NAME
// CHECK: ---> piTearDown(
