# ----------------------------------------------------------------------
# File: CMakeLists.txt
# Author: Andreas-Joachim Peters - CERN
# ----------------------------------------------------------------------

# ************************************************************************
# * EOS - the CERN Disk Storage System                                   *
# * Copyright (C) 2016 CERN/Switzerland                                  *
# *                                                                      *
# * This program is free software: you can redistribute it and/or modify *
# * it under the terms of the GNU General Public License as published by *
# * the Free Software Foundation, either version 3 of the License, or    *
# * (at your option) any later version.                                  *
# *                                                                      *
# * This program is distributed in the hope that it will be useful,      *
# * but WITHOUT ANY WARRANTY; without even the implied warranty of       *
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
# * GNU General Public License for more details.                         *
# *                                                                      *
# * You should have received a copy of the GNU General Public License    *
# * along with this program.  If not, see <http://www.gnu.org/licenses/>.*
# ************************************************************************
include_directories(
  ${CMAKE_SOURCE_DIR}
  "${gtest_SOURCE_DIR}/include"
  "${gmock_SOURCE_DIR}/include")

#-------------------------------------------------------------------------------
# ProtoBuf representation tests
#-------------------------------------------------------------------------------
add_executable(
  eos_ns_quarkdb_tests
  ContainerMDSvcTest.cc
  FileMDSvcTest.cc
  FileSystemViewTest.cc
  HierarchicalViewTest.cc
  Main.cc
  MetadataTests.cc
  NextInodeProviderTest.cc
  OtherTests.cc
  TestUtils.cc
  VariousTests.cc
)

target_compile_definitions(eos_ns_quarkdb_tests PUBLIC -DGTEST_USE_OWN_TR1_TUPLE=0)

target_link_libraries(
  eos_ns_quarkdb_tests
  gtest
  gmock_main
  EosNsQuarkdb-Static
  ${CMAKE_THREAD_LIBS_INIT})

#-------------------------------------------------------------------------------
# eosnsbench executable
#-------------------------------------------------------------------------------
add_executable(eosnsbench EosNamespaceBenchmark.cc)

target_compile_options(
  eosnsbench
  PUBLIC -DFILE_OFFSET_BITS=64)

target_link_libraries(eosnsbench EosNsQuarkdb-Static eosCommon-Static)

install(
  TARGETS
  eosnsbench
  LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_FULL_BINDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR})
