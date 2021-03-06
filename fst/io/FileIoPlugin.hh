//------------------------------------------------------------------------------
//! @file FileIoPlugin.hh
//! @author Elvin-Alin Sindrilaru <esindril@cern.ch>
//! @brief Class generating an IO plugin object
//------------------------------------------------------------------------------

/************************************************************************
 * EOS - the CERN Disk Storage System                                   *
 * Copyright (C) 2011 CERN/Switzerland                                  *
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

#ifndef __EOS_FST_FILEIOPLUGIN_HH__
#define __EOS_FST_FILEIOPLUGIN_HH__

#include "fst/io/xrd/XrdIo.hh"
#include "fst/io/rados/RadosIo.hh"
#include "common/LayoutId.hh"

EOSFSTNAMESPACE_BEGIN

using eos::common::LayoutId;

//! Forward declaration
class XrdFstOfsFile;
class FileIo;

//------------------------------------------------------------------------------
//! Class used to obtain a IO plugin object
//------------------------------------------------------------------------------
class FileIoPlugin
{
public:
  //----------------------------------------------------------------------------
  //! Constructor
  //----------------------------------------------------------------------------
  FileIoPlugin() {}

  //----------------------------------------------------------------------------
  //! Destructor
  //----------------------------------------------------------------------------
  ~FileIoPlugin() {}

  //----------------------------------------------------------------------------
  //! Get IO object
  //!
  //! @param file file handler
  //! @param layoutId layout id type
  //! @param error error information
  //!
  //! @return requested layout type object
  //----------------------------------------------------------------------------
  static FileIo*
  GetIoObject(std::string path,
              XrdFstOfsFile* file = 0,
              const XrdSecEntity* client = 0);
};

EOSFSTNAMESPACE_END

#endif // __EOS_FST_FILEIOPLUGIN_HH__
