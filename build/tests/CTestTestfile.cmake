# CMake generated Testfile for 
# Source directory: /root/.openclaw/agents/zero-3/workspace/repos/open-disk-manager/tests
# Build directory: /root/.openclaw/agents/zero-3/workspace/repos/open-disk-manager/build/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(opm_unit_tests "/root/.openclaw/agents/zero-3/workspace/repos/open-disk-manager/build/tests/opm_tests")
set_tests_properties(opm_unit_tests PROPERTIES  _BACKTRACE_TRIPLES "/root/.openclaw/agents/zero-3/workspace/repos/open-disk-manager/tests/CMakeLists.txt;42;add_test;/root/.openclaw/agents/zero-3/workspace/repos/open-disk-manager/tests/CMakeLists.txt;0;")
subdirs("../_deps/googletest-build")
subdirs("integration")
