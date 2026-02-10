# CMake generated Testfile for 
# Source directory: /root/projects/opm/tests
# Build directory: /root/projects/opm/build/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(opm_unit_tests "/root/projects/opm/build/tests/opm_tests")
set_tests_properties(opm_unit_tests PROPERTIES  _BACKTRACE_TRIPLES "/root/projects/opm/tests/CMakeLists.txt;39;add_test;/root/projects/opm/tests/CMakeLists.txt;0;")
subdirs("../_deps/googletest-build")
subdirs("integration")
