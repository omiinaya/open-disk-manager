# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/root/.openclaw/agents/zero-3/workspace/repos/open-disk-manager/build/_deps/googletest-src"
  "/root/.openclaw/agents/zero-3/workspace/repos/open-disk-manager/build/_deps/googletest-build"
  "/root/.openclaw/agents/zero-3/workspace/repos/open-disk-manager/build/_deps/googletest-subbuild/googletest-populate-prefix"
  "/root/.openclaw/agents/zero-3/workspace/repos/open-disk-manager/build/_deps/googletest-subbuild/googletest-populate-prefix/tmp"
  "/root/.openclaw/agents/zero-3/workspace/repos/open-disk-manager/build/_deps/googletest-subbuild/googletest-populate-prefix/src/googletest-populate-stamp"
  "/root/.openclaw/agents/zero-3/workspace/repos/open-disk-manager/build/_deps/googletest-subbuild/googletest-populate-prefix/src"
  "/root/.openclaw/agents/zero-3/workspace/repos/open-disk-manager/build/_deps/googletest-subbuild/googletest-populate-prefix/src/googletest-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/root/.openclaw/agents/zero-3/workspace/repos/open-disk-manager/build/_deps/googletest-subbuild/googletest-populate-prefix/src/googletest-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/root/.openclaw/agents/zero-3/workspace/repos/open-disk-manager/build/_deps/googletest-subbuild/googletest-populate-prefix/src/googletest-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
