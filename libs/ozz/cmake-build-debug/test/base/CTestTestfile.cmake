# CMake generated Testfile for 
# Source directory: C:/Users/ftp/Desktop/GameEngine/libs/ozz/test/base
# Build directory: C:/Users/ftp/Desktop/GameEngine/libs/ozz/cmake-build-debug/test/base
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[test_endianness]=] "C:/Users/ftp/Desktop/GameEngine/libs/ozz/cmake-build-debug/test/base/test_endianness.exe")
set_tests_properties([=[test_endianness]=] PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/ftp/Desktop/GameEngine/libs/ozz/test/base/CMakeLists.txt;11;add_test;C:/Users/ftp/Desktop/GameEngine/libs/ozz/test/base/CMakeLists.txt;0;")
add_test([=[test_log]=] "C:/Users/ftp/Desktop/GameEngine/libs/ozz/cmake-build-debug/test/base/test_log.exe")
set_tests_properties([=[test_log]=] PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/ftp/Desktop/GameEngine/libs/ozz/test/base/CMakeLists.txt;19;add_test;C:/Users/ftp/Desktop/GameEngine/libs/ozz/test/base/CMakeLists.txt;0;")
add_test([=[test_platform]=] "C:/Users/ftp/Desktop/GameEngine/libs/ozz/cmake-build-debug/test/base/test_platform.exe")
set_tests_properties([=[test_platform]=] PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/ftp/Desktop/GameEngine/libs/ozz/test/base/CMakeLists.txt;29;add_test;C:/Users/ftp/Desktop/GameEngine/libs/ozz/test/base/CMakeLists.txt;0;")
add_test([=[test_fuse_base]=] "C:/Users/ftp/Desktop/GameEngine/libs/ozz/cmake-build-debug/test/base/test_fuse_base.exe")
set_tests_properties([=[test_fuse_base]=] PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/ftp/Desktop/GameEngine/libs/ozz/test/base/CMakeLists.txt;44;add_test;C:/Users/ftp/Desktop/GameEngine/libs/ozz/test/base/CMakeLists.txt;0;")
subdirs("containers")
subdirs("io")
subdirs("maths")
subdirs("memory")
