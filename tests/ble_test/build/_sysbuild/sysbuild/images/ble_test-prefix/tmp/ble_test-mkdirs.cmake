# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Users/tomcajot/Desktop/bap/grompack-firmware/tests/ble_test")
  file(MAKE_DIRECTORY "/Users/tomcajot/Desktop/bap/grompack-firmware/tests/ble_test")
endif()
file(MAKE_DIRECTORY
  "/Users/tomcajot/Desktop/bap/grompack-firmware/tests/ble_test/build/ble_test"
  "/Users/tomcajot/Desktop/bap/grompack-firmware/tests/ble_test/build/_sysbuild/sysbuild/images/ble_test-prefix"
  "/Users/tomcajot/Desktop/bap/grompack-firmware/tests/ble_test/build/_sysbuild/sysbuild/images/ble_test-prefix/tmp"
  "/Users/tomcajot/Desktop/bap/grompack-firmware/tests/ble_test/build/_sysbuild/sysbuild/images/ble_test-prefix/src/ble_test-stamp"
  "/Users/tomcajot/Desktop/bap/grompack-firmware/tests/ble_test/build/_sysbuild/sysbuild/images/ble_test-prefix/src"
  "/Users/tomcajot/Desktop/bap/grompack-firmware/tests/ble_test/build/_sysbuild/sysbuild/images/ble_test-prefix/src/ble_test-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/tomcajot/Desktop/bap/grompack-firmware/tests/ble_test/build/_sysbuild/sysbuild/images/ble_test-prefix/src/ble_test-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/tomcajot/Desktop/bap/grompack-firmware/tests/ble_test/build/_sysbuild/sysbuild/images/ble_test-prefix/src/ble_test-stamp${cfgdir}") # cfgdir has leading slash
endif()
