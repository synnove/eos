//------------------------------------------------------------------------------
// File: XrdFstOfsFile.cc
// Author: Elvin-Alin Sindrilaru / Andreas-Joachim Peters - CERN
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

/*----------------------------------------------------------------------------*/
#include "fst/XrdFstOfsFile.hh"
#include "fst/XrdFstOfs.hh"
#include "fst/layout/LayoutPlugin.hh"
#include "fst/checksum/ChecksumPlugins.hh"
#include "XrdOss/XrdOssApi.hh"
#include "common/Path.hh"
/*----------------------------------------------------------------------------*/

extern XrdOssSys  *XrdOfsOss;

EOSFSTNAMESPACE_BEGIN

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
XrdFstOfsFile::XrdFstOfsFile( const char* user, int MonID ) :
  XrdOfsFile( user, MonID )
{
  openOpaque = 0;
  capOpaque = 0;
  fstPath = "";
  fstBlockXS = 0;
  fstBlockSize = 0;
  eos::common::LogId();
  closed = false;
  opened = false;
  haswrite = false;
  fMd = 0;
  checkSum = 0;
  layOut = 0;
  isRW = 0;
  isCreation = 0;
  rBytes = wBytes = srBytes = swBytes = rOffset = wOffset = 0;
  rTime.tv_sec = wTime.tv_sec = lrTime.tv_sec = lwTime.tv_sec = cTime.tv_sec = 0 ;
  rTime.tv_usec = wTime.tv_usec = lrTime.tv_usec = lwTime.tv_usec = cTime.tv_usec = 0;
  fileid = 0;
  fsid = 0;
  lid = 0;
  cid = 0;
  rCalls = wCalls = 0;
  localPrefix = "";
  maxOffsetWritten = 0;
  openSize = 0;
  closeSize = 0;
  isReplication = false;
  deleteOnClose = false;
  closeTime.tv_sec = closeTime.tv_usec = 0;
  openTime.tv_sec = openTime.tv_usec = 0;
  tz.tz_dsttime = tz.tz_minuteswest = 0;
  viaDelete = false;
}


//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------
XrdFstOfsFile::~XrdFstOfsFile()
{
  viaDelete = true;

  if ( !closed ) {
    close();
  }

  if ( openOpaque ) {
    delete openOpaque;
    openOpaque = 0;
  }

  if ( capOpaque )  {
    delete capOpaque;
    capOpaque = 0;
  }

  //............................................................................
  // Unmap the MD record
  //............................................................................
  if ( fMd ) {
    delete fMd;
    fMd = 0;
  }

  if ( checkSum ) {
    delete checkSum;
    checkSum = 0;
  }

  if ( layOut ) {
    delete layOut;
    layOut = 0;
  }
}


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
void
XrdFstOfsFile:: AddReadTime()
{
  unsigned long mus = ( ( lrTime.tv_sec - cTime.tv_sec ) * 1000000 ) +
                      lrTime.tv_usec - cTime.tv_usec;
  rTime.tv_sec  += ( mus / 1000000 );
  rTime.tv_usec += ( mus % 1000000 );
}


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
void
XrdFstOfsFile::AddWriteTime()
{
  unsigned long mus = ( ( lwTime.tv_sec - cTime.tv_sec ) * 1000000 ) +
                      lwTime.tv_usec - cTime.tv_usec;
  wTime.tv_sec  += ( mus / 1000000 );
  wTime.tv_usec += ( mus % 1000000 );
}


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
void
XrdFstOfsFile::MakeReportEnv( XrdOucString& reportString )
{
  char report[16384];
  sprintf( report, "log=%s&path=%s&ruid=%u&rgid=%u&td=%s&host=%s&lid=%lu&"
           "fid=%llu&fsid=%lu&ots=%lu&otms=%lu&cts=%lu&ctms=%lu&rb=%llu&"
           "wb=%llu&srb=%llu&swb=%llu&nrc=%lu&nwc=%lu&rt=%.02f&wt=%.02f&"
           "osize=%llu&csize=%llu",
           this->logId, Path.c_str(),
           this->vid.uid,
           this->vid.gid,
           tIdent.c_str(),
           hostName.c_str(),
           lid,
           fileid,
           fsid,
           openTime.tv_sec,
           static_cast<unsigned long>( openTime.tv_usec / 1000 ),
           closeTime.tv_sec,
           static_cast<unsigned long>( closeTime.tv_usec / 1000 ),
           rBytes,
           wBytes,
           srBytes,
           swBytes,
           rCalls,
           wCalls,
           ( ( rTime.tv_sec * 1000.0 ) + ( rTime.tv_usec / 1000.0 ) ),
           ( ( wTime.tv_sec * 1000.0 ) + ( wTime.tv_usec / 1000.0 ) ),
           static_cast< unsigned long long>( openSize ),
           static_cast< unsigned long long>( closeSize ) );
  reportString = report;
}


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
int
XrdFstOfsFile::openofs( const char*         path,
                        XrdSfsFileOpenMode  open_mode,
                        mode_t              create_mode,
                        const XrdSecEntity* client,
                        const char*         opaque,
                        bool                openBlockXS,
                        unsigned long       lid )
{
  bool isRead = true;

  eos_debug( "Open mod = %x", open_mode );
  if ( ( open_mode & ( SFS_O_RDONLY | SFS_O_WRONLY | SFS_O_RDWR |
                       SFS_O_CREAT  | SFS_O_TRUNC ) ) != 0 ) {
    eos_debug(" Set isRead = false" );
    isRead = false;
  } else {
    //..........................................................................
    // TODO: drop this ....
    // Anyway we need to open for writing for the recovery case
    //..........................................................................
    open_mode |= SFS_O_RDWR;
  }

  if ( openBlockXS && isRead ) {
    fstBlockXS = ChecksumPlugins::GetChecksumObject( lid, true );
    fstBlockSize = eos::common::LayoutId::GetBlocksize( lid );
    XrdOucString fstXSPath = fstBlockXS->MakeBlockXSPath( path );
    struct stat buf;

    if ( !XrdOfsOss->Stat( path, &buf ) ) {
      if ( !fstBlockXS->OpenMap( fstXSPath.c_str(), buf.st_size, fstBlockSize, false ) ) {
        eos_err( "unable to create block checksum file" );
        return gOFS.Emsg( "XrdFstOfsFile", error, EIO,
                          "open - cannot get block checksum file", fstXSPath.c_str() );
      }
    }
  }

  return XrdOfsFile::open( path, open_mode, create_mode, client, opaque );
}


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
int
XrdFstOfsFile::open( const char*                path,
                     XrdSfsFileOpenMode    open_mode,
                     mode_t              create_mode,
                     const XrdSecEntity*      client,
                     const char*              opaque )
{
  EPNAME( "open" );
  const char* tident = error.getErrUser();
  tIdent = error.getErrUser();
  char* val = 0;
  isRW = false;
  int   retc = SFS_OK;
  Path = path;
  hostName = gOFS.HostName;
  gettimeofday( &openTime, &tz );
  XrdOucString stringOpaque = opaque;
  XrdOucString opaqueBlockCheckSum = "";
  XrdOucString opaqueCheckSum = "";

  while ( stringOpaque.replace( "?", "&" ) ) {}

  while ( stringOpaque.replace( "&&", "&" ) ) {}

  stringOpaque += "&mgm.path=";
  stringOpaque += path;
  openOpaque  = new XrdOucEnv( stringOpaque.c_str() );

  if ( ( val = openOpaque->Get( "mgm.logid" ) ) ) {
    SetLogId( val, tident );
  }

  if ( ( val = openOpaque->Get( "mgm.blockchecksum" ) ) ) {
    opaqueBlockCheckSum = val;
  }

  if ( ( val = openOpaque->Get( "mgm.checksum" ) ) ) {
    opaqueCheckSum = val;
  }

  int caprc = 0;

  if ( ( caprc = gCapabilityEngine.Extract( openOpaque, capOpaque ) ) ) {
    if ( caprc == ENOKEY ) {
      //........................................................................
      // If we just miss the key, better stall the client
      //........................................................................
      return gOFS.Stall( error, 10, "FST still misses the required capability key" );
    }

    //............................................................................
    // No capability - go away!
    //............................................................................
    return gOFS.Emsg( epname, error, caprc, "open - capability illegal", path );
  }

  int envlen;
  //ZTRACE(open,"capability contains: " << capOpaque->Env(envlen));
  eos_info( "path=%s info=%s capability=%s", path, opaque, capOpaque->Env( envlen ) );
  const char* hexfid = 0;
  const char* sfsid = 0;
  const char* slid = 0;
  const char* scid = 0;
  const char* smanager = 0;
  const char* sbookingsize = 0;
  const char* stargetsize = 0;
  bookingsize = 0;
  targetsize = 0;
  fileid = 0;
  fsid = 0;
  lid = 0;
  cid = 0;

  if ( !( hexfid = capOpaque->Get( "mgm.fid" ) ) ) {
    return gOFS.Emsg( epname, error, EINVAL, "open - no file id in capability", path );
  }

  if ( !( sfsid = capOpaque->Get( "mgm.fsid" ) ) ) {
    return gOFS.Emsg( epname, error, EINVAL, "open - no file system id in capability", path );
  }

  //............................................................................
  // If we open a replica we have to take the right filesystem id and filesystem
  // prefix for that replica
  //............................................................................
  if ( openOpaque->Get( "mgm.replicaindex" ) ) {
    XrdOucString replicafsidtag = "mgm.fsid";
    replicafsidtag += ( int ) atoi( openOpaque->Get( "mgm.replicaindex" ) );

    if ( capOpaque->Get( replicafsidtag.c_str() ) )
      sfsid = capOpaque->Get( replicafsidtag.c_str() );
  }

  //............................................................................
  // Extract the local path prefix from the broadcasted configuration!
  //............................................................................
  eos::common::RWMutexReadLock lock( gOFS.Storage->fsMutex );
  fsid = atoi( sfsid ? sfsid : "0" );

  if ( fsid && gOFS.Storage->fileSystemsMap.count( fsid ) ) {
    localPrefix = gOFS.Storage->fileSystemsMap[fsid]->GetPath().c_str();
  }

  //............................................................................
  // Attention: the localprefix implementation does not work for gateway machines
  // - this needs some modifications
  //............................................................................

  if ( !localPrefix.length() ) {
    return gOFS.Emsg( epname, error, EINVAL,
                      "open - cannot determine the prefix path to use for the given filesystem id", path );
  }

  if ( !( slid = capOpaque->Get( "mgm.lid" ) ) ) {
    return gOFS.Emsg( epname, error, EINVAL, "open - no layout id in capability", path );
  }

  if ( !( scid = capOpaque->Get( "mgm.cid" ) ) ) {
    return gOFS.Emsg( epname, error, EINVAL, "open - no container id in capability", path );
  }

  if ( !( smanager = capOpaque->Get( "mgm.manager" ) ) ) {
    return gOFS.Emsg( epname, error, EINVAL, "open - no manager name in capability", path );
  }

  RedirectManager = smanager;
  int dpos = RedirectManager.find( ":" );

  if ( dpos != STR_NPOS )
    RedirectManager.erase( dpos );

  eos::common::FileId::FidPrefix2FullPath( hexfid, localPrefix.c_str(), fstPath );
  fileid = eos::common::FileId::Hex2Fid( hexfid );
  fsid   = atoi( sfsid );
  lid = atoi( slid );
  cid = strtoull( scid, 0, 10 );
  //............................................................................
  // Extract blocksize from the layout
  //............................................................................
  fstBlockSize = eos::common::LayoutId::GetBlocksize( lid );
  eos_info( "blocksize=%llu lid=%x", fstBlockSize, lid );

  //............................................................................
  // Check if this is an open for replication
  //............................................................................
  if ( Path.beginswith( "/replicate:" ) ) {
    bool isopenforwrite = false;
    gOFS.OpenFidMutex.Lock();

    if ( gOFS.WOpenFid[fsid].count( fileid ) ) {
      if ( gOFS.WOpenFid[fsid][fileid] > 0 ) {
        isopenforwrite = true;
      }
    }

    gOFS.OpenFidMutex.UnLock();

    if ( isopenforwrite ) {
      eos_err( "forbid to open replica - file %s is opened in RW mode", Path.c_str() );
      return gOFS.Emsg( epname, error, ENOENT,
                        "open - cannot replicate: file is opened in RW mode", path );
    }

    isReplication = true;
  }

  open_mode |= SFS_O_MKPTH;
  create_mode |= SFS_O_MKPTH;

  if ( ( open_mode & ( SFS_O_RDONLY | SFS_O_WRONLY | SFS_O_RDWR |
                       SFS_O_CREAT  | SFS_O_TRUNC ) ) != 0 ) {
    isRW = true;
  }

  struct stat statinfo;

  if ( ( retc = XrdOfsOss->Stat( fstPath.c_str(), &statinfo ) ) ) {
    //..........................................................................
    // File does not exist, keep the create lfag
    //..........................................................................
    isCreation = true;
    openSize = 0;
    //..........................................................................
    // Used to indicate if a file was written in the meanwhile by someone else
    //..........................................................................
    statinfo.st_mtime = 0;
  } else {
    if ( open_mode & SFS_O_CREAT )
      open_mode -= SFS_O_CREAT;
  }

  //............................................................................
  // Bookingsize is only needed for file creation
  //............................................................................
  if ( isRW && isCreation ) {
    if ( !( sbookingsize = capOpaque->Get( "mgm.bookingsize" ) ) ) {
      return gOFS.Emsg( epname, error, EINVAL, "open - no booking size in capability", path );
    } else {
      bookingsize = strtoull( capOpaque->Get( "mgm.bookingsize" ), 0, 10 );
    }

    if ( ( stargetsize = capOpaque->Get( "mgm.targetsize" ) ) ) {
      targetsize = strtoull( capOpaque->Get( "mgm.targetsize" ), 0, 10 );
    }
  }

  //............................................................................
  // Code dealing with block checksums
  //............................................................................
  eos_info( "blocksize=%llu layoutid=%x oxs=<%s>",
            fstBlockSize, lid, opaqueBlockCheckSum.c_str() );

  //............................................................................
  // Create a block checksum object if blocksize is defined and the feature is
  // not explicitly disabled by the client
  //............................................................................
  if ( ( opaqueBlockCheckSum != "ignore" ) )
    fstBlockXS = ChecksumPlugins::GetChecksumObject( lid, true );
  else
    fstBlockXS = 0;

  if ( fstBlockXS ) {
    eos_info( "info=\"created/got blocklevel checksum\"" );
    XrdOucString fstXSPath = fstBlockXS->MakeBlockXSPath( fstPath.c_str() );

    if ( !fstBlockXS->OpenMap( fstXSPath.c_str(),
                               isCreation ? bookingsize : statinfo.st_size,
                               fstBlockSize,
                               isRW ) ) {
      eos_err( "unable to create block checksum file" );

      if ( eos::common::LayoutId::GetLayoutType( lid ) == eos::common::LayoutId::kReplica ) {
        //..........................................................................
        // There was a blockchecksum open error
        //..........................................................................
        if ( !isRW ) {
          int ecode = 1094;
          eos_warning( "rebouncing client since we failed to open the block checksum file back to MGM %s:%d",
                       RedirectManager.c_str(), ecode );
          return gOFS.Redirect( error, RedirectManager.c_str(), ecode );
        }
      } else {
        return gOFS.Emsg( epname, error, EIO, "open - cannot create/get block checksum file", fstXSPath.c_str() );
      }
    }
  }

  //............................................................................
  // Get the identity
  //............................................................................
  eos::common::Mapping::VirtualIdentity vid;
  eos::common::Mapping::Nobody( vid );

  if ( ( val = capOpaque->Get( "mgm.ruid" ) ) ) {
    vid.uid = atoi( val );
  } else {
    return gOFS.Emsg( epname, error, EINVAL, "open - sec ruid missing", path );
  }

  if ( ( val = capOpaque->Get( "mgm.rgid" ) ) ) {
    vid.gid = atoi( val );
  } else {
    return gOFS.Emsg( epname, error, EINVAL, "open - sec rgid missing", path );
  }

  if ( ( val = capOpaque->Get( "mgm.uid" ) ) ) {
    vid.uid_list.clear();
    vid.uid_list.push_back( atoi( val ) );
  } else {
    return gOFS.Emsg( epname, error, EINVAL, "open - sec uid missing", path );
  }

  if ( ( val = capOpaque->Get( "mgm.gid" ) ) ) {
    vid.gid_list.clear();
    vid.gid_list.push_back( atoi( val ) );
  } else {
    return gOFS.Emsg( epname, error, EINVAL, "open - sec gid missing", path );
  }

  if ( ( val = capOpaque->Get( "mgm.logid" ) ) ) {
    snprintf( logId, sizeof( logId ) - 1, "%s", val );
  }

  SetLogId( logId, vid, tident );
  eos_info( "fstpath=%s", fstPath.c_str() );
  //............................................................................
  // Attach meta data
  //............................................................................
  fMd = gFmdSqliteHandler.GetFmd( fileid, fsid, vid.uid, vid.gid, lid, isRW );

  if ( !fMd ) {
    eos_crit( "no fmd for fileid %llu on filesystem %lu", fileid, fsid );
    int ecode = 1094;
    eos_warning( "rebouncing client since we failed to get the FMD record back to MGM %s:%d",
                 RedirectManager.c_str(), ecode );
    return gOFS.Redirect( error, RedirectManager.c_str(), ecode );
  }

  //............................................................................
  // Call the checksum factory function with the selected layout
  //............................................................................
  layOut = eos::fst::LayoutPlugin::GetLayoutObject( this, lid, client, &error );

  if ( !layOut ) {
    int envlen;
    eos_err( "unable to handle layout for %s", capOpaque->Env( envlen ) );
    delete fMd;
    return gOFS.Emsg( epname, error, EINVAL, "open - illegal layout specified ",
                      capOpaque->Env( envlen ) );
  }

  layOut->SetLogId( logId, vid, tident );

  if ( isRW ||
       ( ( opaqueCheckSum != "ignore" ) &&
         ( ( eos::common::LayoutId::GetLayoutType( lid ) == eos::common::LayoutId::kReplica ) ||
           ( eos::common::LayoutId::GetLayoutType( lid ) == eos::common::LayoutId::kPlain ) ) ) ) {
    if ( ( ( eos::common::LayoutId::GetLayoutType( lid ) == eos::common::LayoutId::kRaidDP ) ||
           ( eos::common::LayoutId::GetLayoutType( lid ) == eos::common::LayoutId::kReedS ) ) &&
         ( !layOut->IsEntryServer() ) ) {
      //........................................................................
      // This case we need to exclude!
      //........................................................................
    } else {
      //........................................................................
      // We always do checksums for writes and reads for kPlain & kReplica if
      // it was not explicitly switched off
      //........................................................................
      checkSum = eos::fst::ChecksumPlugins::GetChecksumObject( lid );
      eos_debug( "checksum requested %d %u", checkSum, lid );
    }
  }

  //........................................................................
  // Get layout implementation
  //........................................................................
  int rc = layOut->Open( fstPath.c_str(),
                         open_mode,
                         create_mode,
                         stringOpaque.c_str() );

  if ( ( !rc ) && isCreation && bookingsize ) {
    rc = layOut->Fallocate( bookingsize );

    if ( rc ) {
      eos_crit( "file allocation gave return code %d errno=%d for allocation of size=%llu",
                rc, errno, bookingsize );
      layOut->Remove();
      int ecode = 1094;
      eos_warning( "rebouncing client since we don't have enough space back to MGM %s:%d",
                   RedirectManager.c_str(), ecode );
      return gOFS.Redirect( error, RedirectManager.c_str(), ecode );
    }
  }

  if ( !isCreation ) {
    //..........................................................................
    // Get the real size of the file, not the local stripe size!
    //..........................................................................
    eos_debug( "Get the real size of the file, not the local stripe size!" );
    if ( ( retc = layOut->Stat( &statinfo ) ) ) {
      return gOFS.Emsg( epname, error, EIO, "open - cannot stat layout to determine file size", Path.c_str() );
    }

    //........................................................................
    // We feed the layout size, not the physical on disk!
    //........................................................................
    eos_info( "The layout size is: %zu, and the value stored in db is: %llu.",
              statinfo.st_size, fMd->fMd.size );

    if ( statinfo.st_size < 0 ) {
      // in a RAID0like layout if the header is corruptede there is no way to know
      // the size of the initial file, therefore we take the value from the DB
      openSize = fMd->fMd.size;
    }
    else {
      openSize = statinfo.st_size;
    }

    if ( checkSum && isRW ) {
      //........................................................................
      // Preset with the last known checksum
      //........................................................................
      checkSum->ResetInit( 0, openSize, fMd->fMd.checksum.c_str() );
    }
  }

  std::string filecxerror = "0";

  if ( !rc ) {
    //........................................................................
    // Set the eos lfn as extended attribute
    //........................................................................
    eos::common::Attr* attr = eos::common::Attr::OpenAttr( layOut->GetLocalReplicaPath() );

    if ( attr && isRW ) {
      if ( Path.beginswith( "/replicate:" ) ) {
        if ( capOpaque->Get( "mgm.path" ) ) {
          if ( !attr->Set( std::string( "user.eos.lfn" ), std::string( capOpaque->Get( "mgm.path" ) ) ) ) {
            eos_err( "unable to set extended attribute <eos.lfn> errno=%d", errno );
          }
        } else {
          eos_err( "no lfn in replication capability" );
        }
      } else {
        if ( !attr->Set( std::string( "user.eos.lfn" ), std::string( path ) ) ) {
          eos_err( "unable to set extended attribute <eos.lfn> errno=%d", errno );
        }
      }
    }

    //........................................................................
    // Try to get error if the file has a scan error
    //........................................................................
    if ( attr ) {
      filecxerror = attr->Get( "user.filecxerror" );
      delete attr;
    }
  }

  if ( ( !isRW ) && ( filecxerror == "1" ) ) {
    //..........................................................................
    // If we have a replica layout
    //..........................................................................
    if ( eos::common::LayoutId::GetLayoutType( lid ) == eos::common::LayoutId::kReplica ) {
      //........................................................................
      // There was a checksum error during the last scan
      //........................................................................
      if ( layOut->IsEntryServer() ) {
        int ecode = 1094;
        eos_warning( "rebouncing client since our replica has a wrong checksum back to MGM %s:%d",
                     RedirectManager.c_str(), ecode );
        return gOFS.Redirect( error, RedirectManager.c_str(), ecode );
      }
    }
  }

  if ( !rc ) {
    opened = true;
    gOFS.OpenFidMutex.Lock();

    if ( isRW ) {
      gOFS.WOpenFid[fsid][fileid]++;
    } else {
      gOFS.ROpenFid[fsid][fileid]++;
    }

    gOFS.OpenFidMutex.UnLock();
  } else {
    //..........................................................................
    // If we have local errors in open we might disable ourselfs
    //..........................................................................
    if ( error.getErrInfo() != EREMOTEIO ) {
      eos::common::RWMutexReadLock( gOFS.Storage->fsMutex );
      std::vector <eos::fst::FileSystem*>::const_iterator it;

      for ( unsigned int i = 0; i < gOFS.Storage->fileSystemsVector.size(); i++ ) {
        //........................................................................
        // Check if the local prefix matches a filesystem path ...
        //........................................................................
        if ( ( errno != ENOENT ) && ( fstPath.beginswith( gOFS.Storage->fileSystemsVector[i]->GetPath().c_str() ) ) ) {
          //........................................................................
          // Broadcast error for this FS
          //........................................................................
          eos_crit( "disabling filesystem %u after IO error on path %s",
                    gOFS.Storage->fileSystemsVector[i]->GetId(),
                    gOFS.Storage->fileSystemsVector[i]->GetPath().c_str() );
          
          XrdOucString s = "local IO error";
          gOFS.Storage->fileSystemsVector[i]->BroadcastError( EIO, s.c_str() );
          // gOFS.Storage->fileSystemsVector[i]->BroadcastError(error.getErrInfo(), "local IO error");
          break;
        }
      }
    }

    //..........................................................................
    // In any case we just redirect back to the manager if we are the 1st entry
    // point of the client
    //..........................................................................
    if ( layOut->IsEntryServer() ) {
      int ecode = 1094;
      rc = SFS_REDIRECT;
      eos_warning( "rebouncing client after open error back to MGM %s:%d", RedirectManager.c_str(), ecode );
      return gOFS.Redirect( error, RedirectManager.c_str(), ecode );
    }
  }

  if ( rc == SFS_OK ) {
    //..........................................................................
    // Tag this transaction as open
    //..........................................................................
    if ( isRW ) {
      if ( !gOFS.Storage->OpenTransaction( fsid, fileid ) ) {
        eos_crit( "cannot open transaction for fsid=%u fid=%llu", fsid, fileid );
      }
    }
  }

  eos_debug( "OPEN FINISHED!\n" );
  return rc;
}


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
int
XrdFstOfsFile::closeofs()
{
  int rc = 0;

  //............................................................................
  // Code dealing with block checksums
  //............................................................................
  if ( fstBlockXS ) {
    struct stat statinfo;
    
    if ( ( XrdOfsOss->Stat( fstPath.c_str(), &statinfo ) ) ) {
      eos_warning( "close - cannot stat closed file %s- probably already unlinked!", Path.c_str() );
      return XrdOfsFile::close();
    }

    if ( !rc ) {
      //........................................................................
      // check if there is more than one writer in this moment or a reader ,
      // if yes, we don't recompute wholes in the checksum and we don't truncate
      // the checksum map, the last single writer will do that
      //........................................................................
      eos_info( "%s wopen=%d ropen=%d fsid=%ld fid=%lld",
                fstPath.c_str(),
                ( gOFS.WOpenFid[fsid].count( fileid ) ) ? gOFS.WOpenFid[fsid][fileid] : 0 ,
                ( gOFS.ROpenFid[fsid].count( fileid ) ) ? gOFS.ROpenFid[fsid][fileid] : 0,
                fsid,
                fileid );
      // ---->
      gOFS.OpenFidMutex.Lock();

      if ( ( gOFS.WOpenFid[fsid].count( fileid ) && ( gOFS.WOpenFid[fsid][fileid] == 1 ) &&
             ( ( !gOFS.ROpenFid[fsid].count( fileid ) ) || ( gOFS.ROpenFid[fsid][fileid] == 0 ) ) ) ) {
        if ( isRW ) {
          if ( !fstBlockXS->ChangeMap( statinfo.st_size, true ) ) {
            eos_err( "unable to change block checksum map" );
            rc = SFS_ERROR;
          } else {
            eos_info( "info=\"adjusting block XS map to %llu\"", statinfo.st_size );
          }
        }

        XrdOucErrInfo error;

        if ( !fctl( SFS_FCTL_GETFD, 0, error ) ) {
          int fd = error.getErrInfo();

          if ( !fstBlockXS->AddBlockSumHoles( fd ) ) {
            eos_err( "unable to fill holes of block checksum map" );
          }
        }
      } else {
        eos_info( "info=\"block-xs skipping hole check and changemap\" nwriter=%d nreader=%d",
                  ( gOFS.WOpenFid[fsid].count( fileid ) ) ? gOFS.WOpenFid[fsid][fileid] : 0 ,
                  ( gOFS.ROpenFid[fsid].count( fileid ) ) ? gOFS.ROpenFid[fsid][fileid] : 0 );
      }

      gOFS.OpenFidMutex.UnLock();
      // -----|
      eos_info( "block-xs wblocks=%llu rblocks=%llu holes=%llu",
                fstBlockXS->GetXSBlocksWritten(),
                fstBlockXS->GetXSBlocksChecked(),
                fstBlockXS->GetXSBlocksWrittenHoles() );

      if ( !fstBlockXS->CloseMap() ) {
        eos_err( "unable to close block checksum map" );
        rc = SFS_ERROR;
      }
    }
  }

  rc |= XrdOfsFile::close();
  return rc;
}


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
bool
XrdFstOfsFile::verifychecksum()
{
  bool checksumerror = false;
  int checksumlen = 0;

  //............................................................................
  // Deal with checksums
  //............................................................................
  if ( checkSum ) {
    if ( !isRW ) {
      //............................................................................
      // For read's we don't scan the whole file if the file was not read upto
      // the end - we skip checksumming for large files > 64M
      //............................................................................
      if ( ( checkSum->GetMaxOffset() != openSize ) &&
           ( openSize > ( 1024 * 1024 * 64 ) ) ) {
        eos_info( "info=\"skipping checksum (re-scan) for files > 64M ...\"" );
        //............................................................................
        // Remove the checksum object
        //............................................................................
        delete checkSum;
        checkSum = 0;
        return false;
      }
    }

    checkSum->Finalize();

    if ( checkSum->NeedsRecalculation() && ( !isRW ) && fstBlockXS ) {
      //............................................................................
      // If we didn't have streaming IO, but we have block checksumming, we don't
      // need to rescan the file
      //............................................................................
      delete checkSum;
      checkSum = 0;
      return false;
    }

    if ( checkSum->NeedsRecalculation() ) {
      unsigned long long scansize = 0;
      float scantime = 0; // is ms

      if ( !fctl( SFS_FCTL_GETFD, 0, error ) ) {
        int fd = error.getErrInfo();

        if ( checkSum->ScanFile( fd, scansize, scantime ) ) {
          XrdOucString sizestring;
          eos_info( "info=\"rescanned checksum\" size=%s time=%.02f ms rate=%.02f MB/s %x",
                    eos::common::StringConversion::GetReadableSizeString( sizestring, scansize, "B" ),
                    scantime, 1.0 * scansize / 1000 / ( scantime ? scantime : 99999999999999LL ),
                    checkSum->GetHexChecksum() );
        } else {
          eos_err( "Rescanning of checksum failed" );
        }
      } else {
        eos_err( "Couldn't get file descriptor" );
      }
    } else {
      //........................................................................
      // This was prefect streaming I/O
      //........................................................................
      if ( ( !isRW ) && ( checkSum->GetMaxOffset() != openSize ) ) {
        eos_info( "info=\"skipping checksum (re-scan) since file was not read completely %llu %llu...\"",
                  checkSum->GetMaxOffset(), openSize );
        //......................................................................
        // Remove the checksum object
        //......................................................................
        delete checkSum;
        checkSum = 0;
        return false;
      }
    }

    if ( isRW ) {
      eos_info( "(write) checksum type: %s checksum hex: %s requested-checksum hex: %s",
                checkSum->GetName(),
                checkSum->GetHexChecksum(),
                openOpaque->Get( "mgm.checksum" ) ? openOpaque->Get( "mgm.checksum" ) : "-none-" );

      //........................................................................
      // Check if the check sum for the file was given at upload time
      //........................................................................
      if ( openOpaque->Get( "mgm.checksum" ) ) {
        XrdOucString opaqueChecksum = openOpaque->Get( "mgm.checksum" );
        XrdOucString hexChecksum = checkSum->GetHexChecksum();

        if ( opaqueChecksum != hexChecksum ) {
          eos_err( "requested checksum %s does not match checksum %s of uploaded file" );
          delete checkSum;
          checkSum = 0;
          return true;
        }
      }

      checkSum->GetBinChecksum( checksumlen );
      //............................................................................
      // Copy checksum into meta data
      //............................................................................
      fMd->fMd.checksum = checkSum->GetHexChecksum();

      if ( haswrite ) {
        //............................................................................
        // If we have no write, we don't set this attributes (xrd3cp!)
        // set the eos checksum extended attributes
        //............................................................................
        eos::common::Attr* attr = eos::common::Attr::OpenAttr( fstPath.c_str() );

        if ( attr ) {
          if ( !attr->Set( std::string( "user.eos.checksumtype" ), std::string( checkSum->GetName() ) ) ) {
            eos_err( "unable to set extended attribute <eos.checksumtype> errno=%d", errno );
          }

          if ( !attr->Set( "user.eos.checksum", checkSum->GetBinChecksum( checksumlen ), checksumlen ) ) {
            eos_err( "unable to set extended attribute <eos.checksum> errno=%d", errno );
          }

          //............................................................................
          // Reset any tagged error
          //............................................................................
          if ( !attr->Set( "user.eos.filecxerror", "0" ) ) {
            eos_err( "unable to set extended attribute <eos.filecxerror> errno=%d", errno );
          }

          if ( !attr->Set( "user.eos.blockcxerror", "0" ) ) {
            eos_err( "unable to set extended attribute <eos.blockcxerror> errno=%d", errno );
          }

          delete attr;
        }
      }
    } else {
      //............................................................................
      // This is a read with checksum check, compare with fMD
      //............................................................................
      eos_info( "(read)  checksum type: %s checksum hex: %s fmd-checksum: %s",
                checkSum->GetName(),
                checkSum->GetHexChecksum(),
                fMd->fMd.checksum.c_str() );
      std::string calculatedchecksum = checkSum->GetHexChecksum();

      if ( calculatedchecksum != fMd->fMd.checksum.c_str() ) {
        checksumerror = true;
      }
    }
  }

  return checksumerror;
}


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
int
XrdFstOfsFile::close()
{
  EPNAME( "close" );
  int rc = 0;
  bool checksumerror = false;
  bool targetsizeerror = false;

  //............................................................................
  // We enter the close logic only once since there can be an explicit close or
  // a close via the destructor
  //............................................................................
  if ( opened && ( !closed ) && fMd ) {
    eos_info( "" );
    //..........................................................................
    // Check if the file close comes from a client disconnect e.g. the destructor
    //..........................................................................
    XrdOucString hexstring = "";
    eos::common::FileId::Fid2Hex( fMd->fMd.fid, hexstring );
    XrdOucErrInfo error;
    XrdOucString capOpaqueString = "/?mgm.pcmd=drop";
    XrdOucString OpaqueString = "";
    OpaqueString += "&mgm.fsid=";
    OpaqueString += ( int )fMd->fMd.fsid;
    OpaqueString += "&mgm.fid=";
    OpaqueString += hexstring;
    XrdOucEnv Opaque( OpaqueString.c_str() );
    capOpaqueString += OpaqueString;

    if ( viaDelete && isCreation ) {
      //........................................................................
      // It is closed by the constructor e.g. no proper close
      // or the specified checksum does not match the computed one
      //........................................................................
      eos_static_debug( "(unpersist): deleting File Id=%llu on Fs=%u",
                        fMd->fMd.fsid, fMd->fMd.fid );
      //........................................................................
      // Delete the file - set the file to be deleted
      //........................................................................
      deleteOnClose = true;
      layOut->Remove();

      if ( fstBlockXS ) {
        fstBlockXS->CloseMap();
        //......................................................................
        // Delete also the block checksum file
        //......................................................................
        fstBlockXS->UnlinkXSPath();
        delete fstBlockXS;
        fstBlockXS = 0;
      }

      //........................................................................
      // Delete the replica in the MGM
      //........................................................................
      int rc = gOFS.CallManager( &error, capOpaque->Get( "mgm.path" ),
                                 capOpaque->Get( "mgm.manager" ), capOpaqueString );

      if ( rc ) {
        eos_warning( "(unpersist): unable to drop file id %s fsid %u at manager %s",
                     hexstring.c_str(), fMd->fMd.fid, capOpaque->Get( "mgm.manager" ) );
      }
    } else {
      //........................................................................
      // Check if this was a newly created file
      //........................................................................
      if ( isCreation ) {
        //......................................................................
        // If we had space allocation we have to truncate the allocated space to
        // the real size of the file
        //......................................................................
        if ( ( strcmp( layOut->GetName(), "raidDP" ) == 0 ) ||
             ( strcmp( layOut->GetName(), "reedS" ) == 0 ) ) {
          if ( layOut->IsEntryServer() )
            layOut->Truncate( maxOffsetWritten );
        } else {
          if ( ( long long )maxOffsetWritten > ( long long )openSize ) {
            //..................................................................
            // Check if we have to deallocate something for this file transaction
            //..................................................................
            if ( ( bookingsize ) && ( bookingsize > ( long long ) maxOffsetWritten ) ) {
              eos_info( "deallocationg %llu bytes", bookingsize - maxOffsetWritten );
              layOut->Truncate( maxOffsetWritten );
              //................................................................
              // We have evt. to deallocate blocks which have not been written
              //................................................................
              layOut->Fdeallocate( maxOffsetWritten, bookingsize );
            }
          }
        }
      }

      eos_info( "calling verifychecksum" );
      //........................................................................
      // Call checksum verification
      //........................................................................
      checksumerror = verifychecksum();
      targetsizeerror = ( targetsize ) ? ( targetsize != ( off_t )maxOffsetWritten ) : false;
      
      if ( ( strcmp( layOut->GetName(), "raidDP" ) == 0 ) ||
           !( strcmp( layOut->GetName(), "reedS" ) == 0 ) ) {
        //......................................................................
        // For RAID-like layouts don't do this check
        //......................................................................
        targetsizeerror = false;
      }

      eos_debug( "checksumerror = %i, targetsizerror= %i,"
                 "maxOffsetWritten = %zu, targetsize = %lli",
                 checksumerror, targetsizeerror, maxOffsetWritten, targetsize );
      
      if ( isCreation && ( checksumerror || targetsizeerror ) ) {
        //......................................................................
        // We have a checksum error if the checksum was preset and does not match!
        // We have a target size error, if the target size was preset and does not match!
        //......................................................................
        // Set the file to be deleted
        //......................................................................
        deleteOnClose = true;
        layOut->Remove();

        if ( fstBlockXS ) {
          //....................................................................
          // Delete also the block checksum file
          //....................................................................
          fstBlockXS->UnlinkXSPath();
          delete fstBlockXS;
          fstBlockXS = 0;
        }

        //......................................................................
        // Delete the replica in the MGM
        //......................................................................
        int rc = gOFS.CallManager( &error,
                                   capOpaque->Get( "mgm.path" ),
                                   capOpaque->Get( "mgm.manager" ),
                                   capOpaqueString );

        if ( rc ) {
          eos_warning( "(unpersist): unable to drop file id %s fsid %u at manager %s",
                       hexstring.c_str(), fMd->fMd.fid, capOpaque->Get( "mgm.manager" ) );
        }
      }

      //........................................................................
      // Store the entry server information before closing the layout
      //........................................................................
      bool isEntryServer = false;

      if ( layOut->IsEntryServer() ) {
        isEntryServer = true;
      }

      //........................................................................
      // First we assume that, if we have writes, we update it
      //........................................................................
      closeSize = openSize;

      if ( ( !checksumerror ) && ( haswrite || isCreation ) ) {
        //......................................................................
        // Commit meta data
        //......................................................................
        struct stat statinfo;

        if ( ( rc = layOut->Stat( &statinfo ) ) ) {
          rc = gOFS.Emsg( epname, error, EIO,
                          "close - cannot stat closed layout to determine file size",
                          Path.c_str() );
        }

        eos_debug (" XOXOXOXOX statinfo.st_size = %zu", statinfo.st_size );        
        if ( !rc ) {
          if ( ( statinfo.st_size == 0 ) || haswrite ) {
            //..................................................................
            // Update size
            //..................................................................
            closeSize = statinfo.st_size;
            fMd->fMd.size     = statinfo.st_size;
            fMd->fMd.disksize = statinfo.st_size;
            fMd->fMd.mgmsize  = 0xfffffff1ULL;    // now again undefined
            fMd->fMd.mgmchecksum = "";            // now again empty
            fMd->fMd.layouterror = 0;             // reset layout errors
            fMd->fMd.locations   = "";            // reset locations
            fMd->fMd.filecxerror = 0;
            fMd->fMd.blockcxerror = 0;
            fMd->fMd.mtime    = statinfo.st_mtime;
#ifdef __APPLE__
            fMd->fMd.mtime_ns = 0;
#else
            fMd->fMd.mtime_ns = statinfo.st_mtim.tv_nsec;
#endif
            //..................................................................
            // Set the container id
            //..................................................................
            fMd->fMd.cid = cid;

            //..................................................................
            // For replicat's set the original uid/gid/lid values
            //..................................................................
            if ( capOpaque->Get( "mgm.source.lid" ) ) {
              fMd->fMd.lid = strtoul( capOpaque->Get( "mgm.source.lid" ), 0, 10 );
            }

            if ( capOpaque->Get( "mgm.source.ruid" ) ) {
              fMd->fMd.uid = atoi( capOpaque->Get( "mgm.source.ruid" ) );
            }

            if ( capOpaque->Get( "mgm.source.rgid" ) ) {
              fMd->fMd.uid = atoi( capOpaque->Get( "mgm.source.rgid" ) );
            }


            //..................................................................
            // Commit local
            //..................................................................
            if ( !gFmdSqliteHandler.Commit( fMd ) )
              rc = gOFS.Emsg( epname, error, EIO,
                              "close - unable to commit meta data",
                              Path.c_str() );

            //..................................................................
            // Commit to central mgm cache
            //..................................................................
            int envlen = 0;
            XrdOucString capOpaqueFile = "";
            XrdOucString mTimeString = "";
            capOpaqueFile += "/?";
            capOpaqueFile += capOpaque->Env( envlen );
            capOpaqueFile += "&mgm.pcmd=commit";
            capOpaqueFile += "&mgm.size=";
            char filesize[1024];
            sprintf( filesize, "%llu", fMd->fMd.size );
            capOpaqueFile += filesize;

            if ( checkSum ) {
              capOpaqueFile += "&mgm.checksum=";
              capOpaqueFile += checkSum->GetHexChecksum();
            }

            capOpaqueFile += "&mgm.mtime=";
            capOpaqueFile += eos::common::StringConversion::GetSizeString( mTimeString, ( unsigned long long )fMd->fMd.mtime );
            capOpaqueFile += "&mgm.mtime_ns=";
            capOpaqueFile += eos::common::StringConversion::GetSizeString( mTimeString, ( unsigned long long )fMd->fMd.mtime_ns );
            capOpaqueFile += "&mgm.add.fsid=";
            capOpaqueFile += ( int )fMd->fMd.fsid;

            //..................................................................
            // If <drainfsid> is set, we can issue a drop replica
            //..................................................................
            if ( capOpaque->Get( "mgm.drainfsid" ) ) {
              capOpaqueFile += "&mgm.drop.fsid=";
              capOpaqueFile += capOpaque->Get( "mgm.drainfsid" );
            }

            if ( isEntryServer && !isReplication ) {
              //................................................................
              // The entry server commits size and checksum
              //................................................................
              capOpaqueFile += "&mgm.commit.size=1&mgm.commit.checksum=1";
            } else {
              capOpaqueFile += "&mgm.replication=1";
            }

            //..................................................................
            // The log ID to the commit
            //..................................................................
            capOpaqueFile += "&mgm.logid=";
            capOpaqueFile += logId;
            rc = gOFS.CallManager( &error, capOpaque->Get( "mgm.path" ),
                                   capOpaque->Get( "mgm.manager" ), capOpaqueFile );

            if ( ( rc == -EIDRM ) || ( rc == -EBADE ) || ( rc == -EBADR ) ) {
              if ( !gOFS.Storage->CloseTransaction( fsid, fileid ) ) {
                eos_crit( "cannot close transaction for fsid=%u fid=%llu", fsid, fileid );
              }

              if ( rc == -EIDRM ) {
                //..............................................................
                // This file has been deleted in the meanwhile ... we can
                // unlink that immedeatly
                //..............................................................
                eos_info( "info=\"unlinking fid=%08x path=%s - "
                          "file has been already unlinked from the namespace\"",
                          fMd->fMd.fid, Path.c_str() );
              }

              if ( rc == -EBADE ) {
                eos_err( "info=\"unlinking fid=%08x path=%s - "
                         "file size of replica does not match reference\"",
                         fMd->fMd.fid, Path.c_str() );
              }

              if ( rc == -EBADR ) {
                eos_err( "info=\"unlinking fid=%08x path=%s - "
                         "checksum of replica does not match reference\"",
                         fMd->fMd.fid, Path.c_str() );
              }

              int retc =  gOFS._rem( Path.c_str(), error, 0, capOpaque,
                                     fstPath.c_str(), fileid, fsid );

              if ( !retc ) {
                eos_debug( "<rem> returned retc=%d", retc );
              }

              deleteOnClose = true;
            }
          }
        }
      }
    }

    if ( isRW ) {
      if ( rc == SFS_OK ) {
        gOFS.Storage->CloseTransaction( fsid, fileid );
      }
    }

    if ( layOut ) {
      rc = layOut->Close();
    } else {
      rc = closeofs();
    }

    closed = true;
    
    if ( rc ) {
      deleteOnClose = true;
    }

    gOFS.OpenFidMutex.Lock();

    if ( isRW )
      gOFS.WOpenFid[fMd->fMd.fsid][fMd->fMd.fid]--;
    else
      gOFS.ROpenFid[fMd->fMd.fsid][fMd->fMd.fid]--;

    if ( gOFS.WOpenFid[fMd->fMd.fsid][fMd->fMd.fid] <= 0 ) {
      //........................................................................
      // If this was a write of the last writer we had the lock and we release it
      //........................................................................
      gOFS.WOpenFid[fMd->fMd.fsid].erase( fMd->fMd.fid );
      gOFS.WOpenFid[fMd->fMd.fsid].resize( 0 );
    }

    if ( gOFS.ROpenFid[fMd->fMd.fsid][fMd->fMd.fid] <= 0 ) {
      gOFS.ROpenFid[fMd->fMd.fsid].erase( fMd->fMd.fid );
      gOFS.ROpenFid[fMd->fMd.fsid].resize( 0 );
    }

    gOFS.OpenFidMutex.UnLock();
    gettimeofday( &closeTime, &tz );

    if ( !deleteOnClose ) {
      //........................................................................
      // Prepare a report and add to the report queue
      //........................................................................
      XrdOucString reportString = "";
      MakeReportEnv( reportString );
      gOFS.ReportQueueMutex.Lock();
      gOFS.ReportQueue.push( reportString );
      gOFS.ReportQueueMutex.UnLock();

      if ( isRW ) {
        //......................................................................
        // Store in the WrittenFilesQueue
        //......................................................................
        gOFS.WrittenFilesQueueMutex.Lock();
        gOFS.WrittenFilesQueue.push( fMd->fMd );
        gOFS.WrittenFilesQueueMutex.UnLock();
      }
    }
  }

  if ( deleteOnClose ) {
    eos_info( "info=\"deleting on close\" fn=%s fstpath=%s\n",
              capOpaque->Get( "mgm.path" ), fstPath.c_str() );
    int retc =  gOFS._rem( Path.c_str(), error, 0, capOpaque, fstPath.c_str(),
                           fileid, fsid, true );

    if ( retc ) {
      eos_debug( "<rem> returned retc=%d", retc );
    }

    rc = SFS_ERROR;

    if ( fstBlockXS ) {
      //........................................................................
      // Delete also the block checksum file
      //........................................................................
      fstBlockXS->UnlinkXSPath();
    }
  } else {
    if ( checksumerror ) {
      rc = SFS_ERROR;
      gOFS.Emsg( epname, error, EIO, "verify checksum - checksum error for file fn=",
                 capOpaque->Get( "mgm.path" ) );
      int envlen = 0;
      eos_crit( "file-xs error file=%s", capOpaque->Env( envlen ) );
    }
  }

  if ( fstBlockXS ) {
    delete fstBlockXS;
    fstBlockXS = 0;
  }

  return rc;
}


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
XrdSfsXferSize
XrdFstOfsFile::readofs( XrdSfsFileOffset   fileOffset,
                        char*              buffer,
                        XrdSfsXferSize     buffer_size )
{
  int retc = XrdOfsFile::read( fileOffset, buffer, buffer_size );
  eos_debug( "read %llu %lli %lli", this,
             static_cast<int64_t>( fileOffset ),
             static_cast<int64_t>( buffer_size ) );

  if ( fstBlockXS ) {
    XrdSysMutexHelper cLock( BlockXsMutex );

    if ( ( retc > 0 ) && ( !fstBlockXS->CheckBlockSum( fileOffset, buffer, retc ) ) ) {
      int envlen = 0;
      eos_crit( "block-xs error offset=%llu len=%llu file=%s",
                ( unsigned long long )fileOffset, ( unsigned long long )buffer_size, FName(),
                capOpaque ? capOpaque->Env( envlen ) : FName() );
      BlockXsMutex.UnLock();
      return gOFS.Emsg( "readofs", error, EIO, "read file - wrong block checksum fn=",
                        capOpaque ? ( capOpaque->Get( "mgm.path" ) ? capOpaque->Get( "mgm.path" ) : FName() ) : FName() );
    } else {
      eos_debug( "blockchecksum is correct\n" );
    }
  }

  eos_debug( "return value=%i", retc );
  return retc;
}


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
int
XrdFstOfsFile::read( XrdSfsFileOffset   fileOffset,
                     XrdSfsXferSize     amount )
{
  //  EPNAME("read");
  int rc = XrdOfsFile::read( fileOffset, amount );
  eos_debug( "rc=%d offset=%lu size=%llu", rc, fileOffset, amount );
  return rc;
}


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
XrdSfsXferSize
XrdFstOfsFile::read( XrdSfsFileOffset   fileOffset,
                     char*              buffer,
                     XrdSfsXferSize     buffer_size )
{
  //  EPNAME("read");
  gettimeofday( &cTime, &tz );
  rCalls++;
  eos_debug( "XrdFstOfsFile: read - fileOffset: %lli, buffer_size: %i\n",
             fileOffset, buffer_size );
  int rc = layOut->Read( fileOffset, buffer, buffer_size );

  if ( ( rc > 0 ) && ( checkSum ) ) {
    XrdSysMutexHelper cLock( ChecksumMutex );
    checkSum->Add( buffer, rc, fileOffset );
  }

  if ( rOffset != static_cast<unsigned long long>( fileOffset ) ) {
    srBytes += llabs( rOffset - fileOffset );
  }

  if ( rc > 0 ) {
    rBytes += rc;
    rOffset += rc;
  }

  gettimeofday( &lrTime, &tz );
  AddReadTime();

  if ( rc < 0 ) {
    // here we might take some other action
    int envlen = 0;
    eos_crit( "block-read error=%d offset=%llu len=%llu file=%s",
              error.getErrInfo(),
              static_cast<unsigned long long>( fileOffset ),
              static_cast< unsigned long long>( buffer_size ),
              FName(),
              capOpaque ? capOpaque->Env( envlen ) : FName() );
  }

  eos_debug( "rc=%d offset=%lu size=%llu", rc, fileOffset,
             static_cast<unsigned long long>( buffer_size ) );

  if ( ( fileOffset + buffer_size ) >= openSize ) {
    if ( checkSum ) {
      checkSum->Finalize();

      if ( !checkSum->NeedsRecalculation() ) {
        // if this is the last read of sequential reading, we can verify the checksum now
        if ( verifychecksum() )
          return gOFS.Emsg( "read", error, EIO, "read file - wrong file checksum fn=", FName() );
      }
    }
  }

  return rc;
}


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
int
XrdFstOfsFile::read( XrdSfsAio* aioparm )
{
  //  EPNAME("read");
  return SFS_ERROR;
  //  eos_debug("aio");
  //  int rc = XrdOfsFile::read(aioparm);
  //  return rc;
}


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
XrdSfsXferSize
XrdFstOfsFile::writeofs( XrdSfsFileOffset   fileOffset,
                         const char*        buffer,
                         XrdSfsXferSize     buffer_size )
{
  if ( fsid ) {
    // check if the file system is full
    XrdSysMutexHelper( gOFS.Storage->fileSystemFullMapMutex );

    if ( gOFS.Storage->fileSystemFullMap[fsid] ) {
      return gOFS.Emsg( "writeofs", error, ENOSPC, "write file - disk space (headroom) exceeded fn=", FName() );
    }
  }

  if ( fstBlockXS ) {
    XrdSysMutexHelper cLock( BlockXsMutex );
    fstBlockXS->AddBlockSum( fileOffset, buffer, buffer_size );
  }

  return XrdOfsFile::write( fileOffset, buffer, buffer_size );
}


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
XrdSfsXferSize
XrdFstOfsFile::write( XrdSfsFileOffset   fileOffset,
                      const char*        buffer,
                      XrdSfsXferSize     buffer_size )
{
  //  EPNAME("write");
  gettimeofday( &cTime, &tz );
  wCalls++;
  int rc = layOut->Write( fileOffset, const_cast<char*>( buffer ), buffer_size );

  // evt. add checksum
  if ( ( rc > 0 ) && ( checkSum ) ) {
    XrdSysMutexHelper cLock( ChecksumMutex );
    checkSum->Add( buffer,
                   static_cast<size_t>( rc ),
                   static_cast<off_t>( fileOffset ) );
  }

  if ( wOffset != static_cast<unsigned long long>( fileOffset ) ) {
    swBytes += llabs( wOffset - fileOffset );
  }

  if ( rc > 0 ) {
    wBytes += rc;
    wOffset += rc;

    if ( static_cast<unsigned long long>( fileOffset + buffer_size ) >
         static_cast<unsigned long long>( maxOffsetWritten ) )
      maxOffsetWritten = ( fileOffset + buffer_size );
  }

  gettimeofday( &lwTime, &tz );
  AddWriteTime();
  haswrite = true;
  eos_debug( "rc=%d offset=%lu size=%lu", rc, fileOffset,
             static_cast<unsigned long>( buffer_size ) );

  if ( rc < 0 ) {
    int envlen = 0;
    eos_crit( "block-write error=%d offset=%llu len=%llu file=%s",
              error.getErrInfo(),
              static_cast<unsigned long long>( fileOffset ),
              static_cast<unsigned long long>( buffer_size ),
              FName(),
              capOpaque ? capOpaque->Env( envlen ) : FName() );
  }

  return rc;
}


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
int
XrdFstOfsFile::write( XrdSfsAio* aioparm )
{
  //  EPNAME("write");
  // this is not supported
  return SFS_ERROR;
}


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
int
XrdFstOfsFile::syncofs()
{
  return XrdOfsFile::sync();
}


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
int
XrdFstOfsFile::sync()
{
  return layOut->Sync();
}


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
int
XrdFstOfsFile::sync( XrdSfsAio* aiop )
{
  return layOut->Sync();
}



//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
int
XrdFstOfsFile::truncateofs( XrdSfsFileOffset fileOffset )
{
  // truncation moves the max offset written
  maxOffsetWritten = fileOffset;
  return XrdOfsFile::truncate( fileOffset );
}



//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
int
XrdFstOfsFile::truncate( XrdSfsFileOffset   fileOffset )
{
  if ( fileOffset == EOS_FST_DELETE_FLAG_VIA_TRUNCATE_LEN ) {
    eos_warning( "Deletion flag for file %s indicated", fstPath.c_str() );
    // this truncate offset indicates to delete the file during the close operation
    viaDelete = true;
    return SFS_OK;
  }

  //  fprintf(stderr,"truncate called %llu\n", fileOffset);
  eos_info( "subcmd=truncate openSize=%llu fileOffset=%llu", openSize, fileOffset );

  if ( fileOffset != openSize ) {
    haswrite = true;

    if ( checkSum ) {
      if ( fileOffset != checkSum->GetMaxOffset() ) {
        checkSum->Reset();
        checkSum->SetDirty();
      }
    }
  }

  return layOut->Truncate( fileOffset );
}


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
int
XrdFstOfsFile::stat( struct stat* buf )
{
  EPNAME( "stat" );
  int rc = SFS_OK ;

  if ( layOut ) {
    if ( ( rc = layOut->Stat( buf ) ) ) {
      rc = gOFS.Emsg( epname, error, EIO, "stat - cannot stat layout to determine file size ", Path.c_str() );
    }
  } else {
    rc = gOFS.Emsg( epname, error, ENXIO, "stat - no layout to determine file size ", Path.c_str() );
  }

  return rc;
}


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
std::string
XrdFstOfsFile::GetFstPath()
{
  std::string ret = fstPath.c_str();
  return ret;
}


EOSFSTNAMESPACE_END

