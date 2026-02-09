# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/cachy/workspaces/libs/cels/modules/cels-clay/build/_deps/clay-src")
  file(MAKE_DIRECTORY "/home/cachy/workspaces/libs/cels/modules/cels-clay/build/_deps/clay-src")
endif()
file(MAKE_DIRECTORY
  "/home/cachy/workspaces/libs/cels/modules/cels-clay/build/_deps/clay-build"
  "/home/cachy/workspaces/libs/cels/modules/cels-clay/build/_deps/clay-subbuild/clay-populate-prefix"
  "/home/cachy/workspaces/libs/cels/modules/cels-clay/build/_deps/clay-subbuild/clay-populate-prefix/tmp"
  "/home/cachy/workspaces/libs/cels/modules/cels-clay/build/_deps/clay-subbuild/clay-populate-prefix/src/clay-populate-stamp"
  "/home/cachy/workspaces/libs/cels/modules/cels-clay/build/_deps/clay-subbuild/clay-populate-prefix/src"
  "/home/cachy/workspaces/libs/cels/modules/cels-clay/build/_deps/clay-subbuild/clay-populate-prefix/src/clay-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/cachy/workspaces/libs/cels/modules/cels-clay/build/_deps/clay-subbuild/clay-populate-prefix/src/clay-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/cachy/workspaces/libs/cels/modules/cels-clay/build/_deps/clay-subbuild/clay-populate-prefix/src/clay-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
