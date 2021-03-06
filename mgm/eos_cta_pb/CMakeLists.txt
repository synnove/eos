# ----------------------------------------------------------------------
# File: CMakeLists.txt
# Author: Jozsef Makai - CERN
# ----------------------------------------------------------------------

# ************************************************************************
# * EOS - the CERN Disk Storage System                                   *
# * Copyright (C) 2018 CERN/Switzerland                                  *
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

set(PROTOBUF3_INCLUDE_PATH ${EOS_CTA_PB_DIR})

PROTOBUF_GENERATE_CPP(CTA_ADMIN_SRCS CTA_ADMIN_HDRS ${PROTOBUF3_INCLUDE_PATH}/cta_admin.proto)
PROTOBUF_GENERATE_CPP(CTA_COMMON_SRCS CTA_COMMON_HDRS ${PROTOBUF3_INCLUDE_PATH}/cta_common.proto)
PROTOBUF_GENERATE_CPP(CTA_EOS_SRCS CTA_EOS_HDRS ${PROTOBUF3_INCLUDE_PATH}/cta_eos.proto)
PROTOBUF_GENERATE_CPP(CTA_FRONTEND_SRCS CTA_FRONTEND_HDRS ${PROTOBUF3_INCLUDE_PATH}/cta_frontend.proto)

set(EOS_CTA_PB_SRCS
  ${CTA_ADMIN_SRCS} ${CTA_COMMON_SRCS} ${CTA_EOS_SRCS} ${CTA_FRONTEND_SRCS})

set(EOS_CTA_PB_HDRS
  ${CTA_ADMIN_HDRS} ${CTA_COMMON_HDRS} ${CTA_EOS_HDRS} ${CTA_FRONTEND_HDRS})

set_source_files_properties(
  ${EOS_CTA_PB_SRCS} ${EOS_CTA_PB_HDRS}
  PROPERTIES GENERATED 1)

include_directories(${PROTOBUF3_INCLUDE_DIRS})
add_library (XrdSsiPbEosCta ${EOS_CTA_PB_SRCS})
set_target_properties(XrdSsiPbEosCta PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(XrdSsiPbEosCta PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
target_link_libraries(XrdSsiPbEosCta ${PROTOBUF3_LIBRARIES})
