# CMake generated Testfile for 
# Source directory: C:/Users/ftp/Desktop/GameEngine/libs/ozz/howtos
# Build directory: C:/Users/ftp/Desktop/GameEngine/libs/ozz/cmake-build-debug/howtos
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[load_from_file]=] "C:/Users/ftp/Desktop/GameEngine/libs/ozz/cmake-build-debug/howtos/load_from_file.exe" "C:/Users/ftp/Desktop/GameEngine/libs/ozz/media/bin/pab_skeleton.ozz")
set_tests_properties([=[load_from_file]=] PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/ftp/Desktop/GameEngine/libs/ozz/howtos/CMakeLists.txt;10;add_test;C:/Users/ftp/Desktop/GameEngine/libs/ozz/howtos/CMakeLists.txt;0;")
add_test([=[load_from_file_no_arg]=] "C:/Users/ftp/Desktop/GameEngine/libs/ozz/cmake-build-debug/howtos/load_from_file.exe")
set_tests_properties([=[load_from_file_no_arg]=] PROPERTIES  WILL_FAIL "true" _BACKTRACE_TRIPLES "C:/Users/ftp/Desktop/GameEngine/libs/ozz/howtos/CMakeLists.txt;11;add_test;C:/Users/ftp/Desktop/GameEngine/libs/ozz/howtos/CMakeLists.txt;0;")
add_test([=[load_from_file_bad_arg]=] "C:/Users/ftp/Desktop/GameEngine/libs/ozz/cmake-build-debug/howtos/load_from_file.exe" "C:/Users/ftp/Desktop/GameEngine/libs/ozz/media/bin/doesn_t_exist.ozz")
set_tests_properties([=[load_from_file_bad_arg]=] PROPERTIES  WILL_FAIL "true" _BACKTRACE_TRIPLES "C:/Users/ftp/Desktop/GameEngine/libs/ozz/howtos/CMakeLists.txt;13;add_test;C:/Users/ftp/Desktop/GameEngine/libs/ozz/howtos/CMakeLists.txt;0;")
add_test([=[custom_skeleton_importer]=] "C:/Users/ftp/Desktop/GameEngine/libs/ozz/cmake-build-debug/howtos/custom_skeleton_importer.exe")
set_tests_properties([=[custom_skeleton_importer]=] PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/ftp/Desktop/GameEngine/libs/ozz/howtos/CMakeLists.txt;25;add_test;C:/Users/ftp/Desktop/GameEngine/libs/ozz/howtos/CMakeLists.txt;0;")
add_test([=[custom_animation_importer]=] "C:/Users/ftp/Desktop/GameEngine/libs/ozz/cmake-build-debug/howtos/custom_animation_importer.exe")
set_tests_properties([=[custom_animation_importer]=] PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/ftp/Desktop/GameEngine/libs/ozz/howtos/CMakeLists.txt;36;add_test;C:/Users/ftp/Desktop/GameEngine/libs/ozz/howtos/CMakeLists.txt;0;")
