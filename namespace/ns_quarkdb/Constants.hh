/************************************************************************
 * EOS - the CERN Disk Storage System                                   *
 * Copyright (C) 2016 CERN/Switzerland                                  *
 *                                                                      *
 * This program is free software: you can redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the Free Software Foundation, either version 3 of the License, or    *
 * (at your option) any later version.                                  *
 *                                                                      *
 * This program is distributed in the hope that it will be useful,      *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have received a copy of the GNU General Public License    *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.*
 ************************************************************************/

//------------------------------------------------------------------------------
//! @author Elvin-Alin Sindrilaru <esindril@cern.ch>
//! @brief Contants used for the namespace implementation on top of quarkdb
//------------------------------------------------------------------------------

#ifndef __EOS_NS_REDIS_CONSTANTS_HH__
#define __EOS_NS_REDIS_CONSTANTS_HH__

#include "namespace/Namespace.hh"
#include <string>

EOSNSNAMESPACE_BEGIN

//! Variables associated with the HierarchcalView
namespace constants
{
//! Suffix for container metadata in Redis
static const std::string sContKeySuffix{":c_bucket"};
//! Sufix for file metadata in Redis
static const std::string sFileKeySuffix{":f_bucket"};
//! Suffix for set of subcontainers in a container
static const std::string sMapDirsSuffix{":map_conts"};
//! Suffix for set of files in a container
static const std::string sMapFilesSuffix{":map_files"};
//! Key for map containing meta info
static const std::string sMapMetaInfoKey{"meta_map"};
//! Field last used file id in meta info map
static const std::string sLastUsedFid{"last_used_fid"};
//! Field last used container id in meta info map
static const std::string sLastUsedCid{"last_used_cid"};
//! Set of files that need to be rechecked
static const std::string sSetCheckFiles{"files_check_set"};
}

//! Variable associated with the QuotaView
namespace quota
{
// QuotaStats
//! Prefix for all quota related maps
static const std::string sPrefix = "quota:";
//! Quota hmap of uids suffix
static const std::string sUidsSuffix = "map_uid";
//! Quta hmap of gids suffix
static const std::string sGidsSuffix = "map_gid";

// QuotaNode
//! Tag for space
static const std::string sLogicalSize = ":logical_size";
//! Tag for physical space
static const std::string sPhysicalSize = ":physical_size";
//! Tag for number of files
static const std::string sNumFiles = ":files";
}

// Variable associated with the FileSystemView
namespace fsview
{
//! Prefix for sets storing file ids
static const std::string sPrefix = "fsview:";
//! Set suffix for file ids on a fs
static const std::string sFilesSuffix = "files";
//! Set suffix for unlinked file ids on a fs
static const std::string sUnlinkedSuffix = "unlinked";
//! Set suffix for file ids with no replicas
static const std::string sNoReplicaPrefix = "fsview_noreplicas";
}

EOSNSNAMESPACE_END

#endif // __EOS_NS_REDIS_CONSTANTS_HH__
