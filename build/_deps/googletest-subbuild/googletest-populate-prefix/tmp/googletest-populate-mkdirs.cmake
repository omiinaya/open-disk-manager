# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/root/projects/opm/build/_deps/googletest-src"
  "/root/projects/opm/build/_deps/googletest-build"
  "/root/projects/opm/build/_deps/googletest-subbuild/googletest-populate-prefix"
  "/root/projects/opm/build/_deps/googletest-subbuild/googletest-populate-prefix/tmp"
  "/root/projects/opm/build/_deps/googletest-subbuild/googletest-populate-prefix/src/googletest-populate-stamp"
  "/root/projects/opm/build/_deps/googletest-subbuild/googletest-populate-prefix/src"
  "/root/projects/opm/build/_deps/googletest-subbuild/googletest-populate-prefix/src/googletest-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/root/projects/opm/build/_deps/googletest-subbuild/googletest-populate-prefix/src/googletest-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/root/projects/opm/build/_deps/googletest-subbuild/googletest-populate-prefix/src/googletest-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
