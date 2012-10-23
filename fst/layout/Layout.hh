//------------------------------------------------------------------------------
// File: Layout.hh
// Author: Andreas-Joachim Peters - CERN
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

//------------------------------------------------------------------------------
//! @file Layout.hh
//! @author Andreas-Joachim Peters - CERN
//! @brief Abstraction of the physical layout of a file
//------------------------------------------------------------------------------

#ifndef __EOSFST_LAYOUT_HH__
#define __EOSFST_LAYOUT_HH__

/*----------------------------------------------------------------------------*/
#include <sys/types.h>
/*----------------------------------------------------------------------------*/
#include "common/LayoutId.hh"
#include "common/Logging.hh"
#include "fst/Namespace.hh"
#include "fst/layout/FileIoPlugin.hh"
/*----------------------------------------------------------------------------*/
#include "XrdOuc/XrdOucString.hh"
#include "XrdSfs/XrdSfsInterface.hh"
/*----------------------------------------------------------------------------*/

EOSFSTNAMESPACE_BEGIN


class XrdFstOfsFile;

//------------------------------------------------------------------------------
//! Class which abstracts the physical layout of the file
//------------------------------------------------------------------------------
class Layout: public eos::common::LogId
{
  public:

    //--------------------------------------------------------------------------
    //! Constructor
    //!
    //! @param file file handler
    //!
    //--------------------------------------------------------------------------
    Layout( XrdFstOfsFile* file );


    //--------------------------------------------------------------------------
    //! Constructor
    //!
    //! @param file handler to current file
    //! @param name name of the layout
    //! @param lid layout id
    //! @param client security information
    //! @param outError error information
    //!
    //--------------------------------------------------------------------------
    Layout( XrdFstOfsFile*      file,
            int                 lid,
            const XrdSecEntity* client,
            XrdOucErrInfo*      outError );


    //--------------------------------------------------------------------------
    //! Destructor
    //--------------------------------------------------------------------------
    virtual ~Layout(); 


    //--------------------------------------------------------------------------
    //! Get the name of the layout
    //--------------------------------------------------------------------------
    const char* GetName() {
      return mName.c_str();
    }


    //--------------------------------------------------------------------------
    //! Get path to the local replica
    //--------------------------------------------------------------------------
    const char* GetLocalReplicaPath() {
      return mLocalPath.c_str();
    }


    //--------------------------------------------------------------------------
    //! Get layout id
    //--------------------------------------------------------------------------
    unsigned int GetLayOutId() {
      return mLayoutId;
    }


    //--------------------------------------------------------------------------
    //! Test if we are at the entry server
    //--------------------------------------------------------------------------
    virtual bool IsEntryServer() {
      return mIsEntryServer;
    }


    //--------------------------------------------------------------------------
    //! Open a file of the current layout type
    //!
    //! @param path file path
    //! @param flags open flags
    //! @param mode open mode
    //! @param opaque opaque information
    //!
    //! @return 0 if successful, -1 otherwise and error code is set
    //!
    //--------------------------------------------------------------------------
    virtual int Open( const std::string&  path,
                      XrdSfsFileOpenMode  flags,
                      mode_t              mode,
                      const char*         opaque ) = 0;


    //--------------------------------------------------------------------------
    //! Read from file
    //!
    //! @param offset offset
    //! @param buffer place to hold the read data
    //! @param length length
    //!
    //! @return number of bytes read or -1 if error
    //!
    //--------------------------------------------------------------------------
    virtual int64_t Read( XrdSfsFileOffset offset,
                          char*            buffer,
                          XrdSfsXferSize   length ) = 0;


    //--------------------------------------------------------------------------
    //! Write to file
    //!
    //! @param offset offset
    //! @paramm buffer data to be written
    //! @param length length
    //!
    //! @return number of bytes written or -1 if error
    //!
    //--------------------------------------------------------------------------
    virtual int64_t Write( XrdSfsFileOffset offset,
                           char*            buffer,
                           XrdSfsXferSize   length ) = 0;


    //--------------------------------------------------------------------------
    //! Truncate
    //!
    //! @param offset truncate file to this value
    //!
    //! @return 0 if successful, -1 otherwise and error code is set
    //!
    //--------------------------------------------------------------------------
    virtual int Truncate( XrdSfsFileOffset offset ) = 0;


    //--------------------------------------------------------------------------
    //! Allocate file space
    //!
    //! @param length space to be allocated
    //!
    //! @return 0 if successful, -1 otherwise and error code is set
    //!
    //--------------------------------------------------------------------------
    virtual int Fallocate( XrdSfsFileOffset lenght ) {
      return 0;
    }


    //--------------------------------------------------------------------------
    //! Deallocate file space
    //!
    //! @param fromOffset offset start
    //! @param toOffset offset end
    //!
    //! @return 0 if successful, -1 otherwise and error code is set
    //!
    //--------------------------------------------------------------------------
    virtual int Fdeallocate( XrdSfsFileOffset fromOffset,
                             XrdSfsFileOffset toOffset ) {
      return 0;
    }


    //--------------------------------------------------------------------------
    //! Remove file
    //!
    //! @return 0 if successful, -1 otherwise and error code is set
    //!
    //--------------------------------------------------------------------------
    virtual int Remove() {
      return 0;
    }


    //--------------------------------------------------------------------------
    //! Sync file to disk
    //!
    //! @return 0 if successful, -1 otherwise and error code is set
    //!
    //--------------------------------------------------------------------------
    virtual int Sync() = 0;


    //--------------------------------------------------------------------------
    //! Close file
    //!
    //! @return 0 if successful, -1 otherwise and error code is set
    //!
    //--------------------------------------------------------------------------
    virtual int Close() = 0;


    //--------------------------------------------------------------------------
    //! Get stats about the file
    //!
    //! @param buf stat buffer
    //!
    //! @return 0 if successful, -1 otherwise and error code is set
    //!
    //--------------------------------------------------------------------------
    virtual int Stat( struct stat* buf ) = 0;

  protected:

    bool           mIsEntryServer;      ///< mark entry server
    unsigned int   mLayoutId;           ///< layout id
    XrdOucString   mName;               ///< layout name
    XrdFstOfsFile* mOfsFile;            ///< handler to logical file
    std::string    mLocalPath;          ///< path to local file
    XrdOucErrInfo* mError;              ///< error information for current file
    XrdSecEntity*  mSecEntity;          ///< security information

};

EOSFSTNAMESPACE_END

#endif
