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
// author: Lukasz Janyst <ljanyst@cern.ch>
// desc:   Change log based FileMD service
//------------------------------------------------------------------------------

#include "ChangeLogFileMDSvc.hh"
#include "ChangeLogContainerMDSvc.hh"
#include "ChangeLogConstants.hh"
#include "namespace/utils/Locking.hh"

#include <algorithm>
#include <utility>
#include <set>

//------------------------------------------------------------------------------
// Follower
//------------------------------------------------------------------------------
namespace eos
{
  class FileMDFollower: public eos::ILogRecordScanner
  {
    public:
      FileMDFollower( eos::ChangeLogFileMDSvc *fileSvc ):
        pFileSvc( fileSvc )
      {
        pContSvc = pFileSvc->pContSvc;
      }

      //------------------------------------------------------------------------
      // Unpack new data and put it in the queue
      //------------------------------------------------------------------------
      virtual bool processRecord( uint64_t offset, char type,
                                  const eos::Buffer &buffer )
      {
        //----------------------------------------------------------------------
        // Update
        //----------------------------------------------------------------------
        if( type == UPDATE_RECORD_MAGIC )
        {
          FileMD *file = new FileMD( 0, 0 );
          file->deserialize( (Buffer&)buffer );
          FileMap::iterator it = pUpdated.find( file->getId() );
          if( it != pUpdated.end() )
          {
            delete it->second;
            it->second = file;
          }
          else
            pUpdated[file->getId()] = file;

          pDeleted.erase( file->getId() );
        }

        //----------------------------------------------------------------------
        // Deletion
        //----------------------------------------------------------------------
        else if( type == DELETE_RECORD_MAGIC )
        {
          FileMD::id_t id;
          buffer.grabData( 0, &id, sizeof( FileMD::id_t ) );
          FileMap::iterator it = pUpdated.find( id );
          if( it != pUpdated.end() )
          {
            delete it->second;
            pUpdated.erase( it );
          }
          pDeleted.insert( id );
        }
        return true;
      }

      //------------------------------------------------------------------------
      // Try to commit the data in the queue to the service
      //------------------------------------------------------------------------
      void commit()
      {
        pFileSvc->getSlaveLock()->writeLock();
        ChangeLogFileMDSvc::IdMap      *fileIdMap = &pFileSvc->pIdMap;
        ChangeLogContainerMDSvc::IdMap *contIdMap = &pContSvc->pIdMap;

        //----------------------------------------------------------------------
        // Handle deletions
        //----------------------------------------------------------------------
        std::set<FileMD::id_t>::iterator itD;
        for( itD = pDeleted.begin(); itD != pDeleted.end(); ++itD )
        {
          ChangeLogFileMDSvc::IdMap::iterator it;
          it = fileIdMap->find( *itD );
          if( it == fileIdMap->end() )
            continue;

          ChangeLogContainerMDSvc::IdMap::iterator itP;
          itP = contIdMap->find( it->second.ptr->getContainerId() );
          if( itP != contIdMap->end() )
          {
            // make sure it's the same pointer - cover name conflicts
            FileMD *file = itP->second.ptr->findFile( it->second.ptr->getName() );
            if( file == it->second.ptr )
              itP->second.ptr->removeFile( it->second.ptr->getName() );
          }
          delete it->second.ptr;
          fileIdMap->erase( it );
        }

        pDeleted.clear();

        //----------------------------------------------------------------------
        // Handle updates
        //----------------------------------------------------------------------
        FileMap::iterator itU;
        std::list<FileMD::id_t> processed;
        for( itU = pUpdated.begin(); itU != pUpdated.end(); ++itU )
        {
          ChangeLogFileMDSvc::IdMap::iterator it;
          ChangeLogContainerMDSvc::IdMap::iterator itP;
          FileMD *currentFile = itU->second;
          it = fileIdMap->find( currentFile->getId() );
          if( it == fileIdMap->end() )
          {
            itP = contIdMap->find( currentFile->getContainerId() );
            if( itP != contIdMap->end() )
            {
              itP->second.ptr->addFile( currentFile );
              (*fileIdMap)[currentFile->getId()] =
                ChangeLogFileMDSvc::DataInfo( 0, currentFile );

              processed.push_back( currentFile->getId() );
            }
          }
          else
          {
            (*it->second.ptr) = *currentFile;
            processed.push_back( currentFile->getId() );
            delete currentFile;
          }
        }

        std::list<FileMD::id_t>::iterator itPro;
        for( itPro = processed.begin(); itPro != processed.end(); ++itPro )
          pUpdated.erase( *itPro );
        pContSvc->getSlaveLock()->unLock();
      }

    private:
      typedef std::map<eos::FileMD::id_t, eos::FileMD*> FileMap;
      FileMap                       pUpdated;
      std::set<eos::FileMD::id_t>   pDeleted;
      eos::ChangeLogFileMDSvc      *pFileSvc;
      eos::ChangeLogContainerMDSvc *pContSvc;
  };
}

extern "C"
{
  //----------------------------------------------------------------------------
  // Follow the change log
  //----------------------------------------------------------------------------
  static void *followerThread( void *data )
  {
    eos::ChangeLogFileMDSvc *fileSvc = reinterpret_cast<eos::ChangeLogFileMDSvc*>( data );
    uint64_t                 offset  = fileSvc->getFollowOffset();
    eos::ChangeLogFile      *file    = fileSvc->getChangeLog();
    uint32_t                 pollInt = fileSvc->getFollowPollInterval();

    eos::FileMDFollower f( fileSvc );
    pthread_setcanceltype( PTHREAD_CANCEL_ASYNCHRONOUS, 0 );
    while( 1 )
    {
      pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, 0 );
      offset = file->follow( &f, offset );
      f.commit();
      pthread_setcancelstate( PTHREAD_CANCEL_ENABLE, 0 );
      usleep( pollInt );
    }
    return 0;
  }
}

//------------------------------------------------------------------------------
// Helper structures for online compacting
//------------------------------------------------------------------------------
namespace
{
  //----------------------------------------------------------------------------
  // Store info about old and new offset for a given file id
  //----------------------------------------------------------------------------
  struct RecordData
  {
    RecordData(): offset(0), newOffset(0), fileId(0) {}
    RecordData( uint64_t o, eos::FileMD::id_t i, uint64_t no = 0 ):
      offset(o), newOffset(no), fileId(i) {}
    uint64_t          offset;
    uint64_t          newOffset;
    eos::FileMD::id_t fileId;
  };

  //----------------------------------------------------------------------------
  // Carry the data between compacting stages
  //----------------------------------------------------------------------------
  struct CompactingData
  {
    CompactingData():
      newLog( new eos::ChangeLogFile() ),
      originalLog(0),
      newRecord(0)
    {}
    ~CompactingData() { delete newLog; }
    std::string              logFileName;
    eos::ChangeLogFile      *newLog;
    eos::ChangeLogFile      *originalLog;
    std::vector<RecordData>  records;
    uint64_t                 newRecord;
  };

  //----------------------------------------------------------------------------
  // Compare record data objects in order to sort them
  //----------------------------------------------------------------------------
  struct OffsetComparator
  {
    bool operator () ( const RecordData &a, const RecordData &b )
    {
      return a.offset < b.offset;
    }
  };

  //----------------------------------------------------------------------------
  // Process the records being scanned and copy them to the new log
  //----------------------------------------------------------------------------
  class UpdateHandler: public eos::ILogRecordScanner
  {
    public:

      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      UpdateHandler( std::map<eos::FileMD::id_t, RecordData> &updates,
                     eos::ChangeLogFile                      *newLog ):
        pUpdates( updates ), pNewLog( newLog ) {}

      //------------------------------------------------------------------------
      // Process the records
      //------------------------------------------------------------------------
      virtual bool processRecord( uint64_t           offset,
                                  char               type,
                                  const eos::Buffer &buffer )
      {
        //----------------------------------------------------------------------
        // Write to the new change log - we need to cast - nasty, but safe in
        // this case
        //----------------------------------------------------------------------
        uint64_t newOffset = pNewLog->storeRecord( type, (eos::Buffer&)buffer );

        //----------------------------------------------------------------------
        // Put the right stuff in the updates map
        //----------------------------------------------------------------------
        eos::FileMD::id_t id;
        buffer.grabData( 0, &id, sizeof( eos::FileMD::id_t ) );
        if( type == eos::UPDATE_RECORD_MAGIC )
          pUpdates[id] = RecordData( offset, id, newOffset );
        else if( type == eos::DELETE_RECORD_MAGIC )
          pUpdates.erase( id );

        return true;
      }

      //------------------------------------------------------------------------
      // Private
      //------------------------------------------------------------------------
      private:
        std::map<eos::FileMD::id_t, RecordData> &pUpdates;
        eos::ChangeLogFile                      *pNewLog;
        uint64_t                                 pCounter;
    };
}

namespace eos
{
  //------------------------------------------------------------------------
  // Initizlize the file service
  //------------------------------------------------------------------------
  void ChangeLogFileMDSvc::initialize() throw( MDException )
  {
    if( !pContSvc )
    {
      MDException e( EINVAL );
      e.getMessage() << "FileMDSvc: container service not set";
      throw e;
    }

    //--------------------------------------------------------------------------
    // Decide on how to open the change log
    //--------------------------------------------------------------------------
    int logOpenFlags = 0;
    if( pSlaveMode )
    {
      if( !pSlaveLock )
      {
        MDException e( EINVAL );
        e.getMessage() << "FileMDSvc: slave lock not set";
        throw e;
      }
      logOpenFlags = ChangeLogFile::ReadOnly;
    }
    else
      logOpenFlags = ChangeLogFile::Create | ChangeLogFile::Append;

    //--------------------------------------------------------------------------
    // Rescan the change log if needed
    //
    // In the master mode we go throug the entire file
    // In the slave mode up untill the compaction mark or not at all
    // if the compaction mark is not present
    //--------------------------------------------------------------------------
    pChangeLog->open( pChangeLogPath, logOpenFlags, FILE_LOG_MAGIC );
    bool logIsCompacted = (pChangeLog->getUserFlags() & LOG_FLAG_COMPACTED);
    pFollowStart = pChangeLog->getFirstOffset();


    if( !pSlaveMode || logIsCompacted )
    {
      FileMDScanner scanner( pIdMap, pSlaveMode );
      pFollowStart = pChangeLog->scanAllRecords( &scanner );
      pFirstFreeId = scanner.getLargestId()+1;

      //------------------------------------------------------------------------
      // Recreate the files
      //------------------------------------------------------------------------
      IdMap::iterator it;
      for( it = pIdMap.begin(); it != pIdMap.end(); ++it )
      {
        //----------------------------------------------------------------------
        // Unpack the serialized buffers
        //----------------------------------------------------------------------
        FileMD *file = new FileMD( 0, this );
        file->deserialize( *it->second.buffer );
        it->second.ptr = file;
        delete it->second.buffer;
        it->second.buffer = 0;
        ListenerList::iterator it;
        for( it = pListeners.begin(); it != pListeners.end(); ++it )
          (*it)->fileMDRead( file );

        //----------------------------------------------------------------------
        // Attach to the hierarchy
        //----------------------------------------------------------------------
        if( file->getContainerId() == 0 )
          continue;

        ContainerMD *cont = 0;
        try { cont = pContSvc->getContainerMD( file->getContainerId() ); }
        catch( MDException &e ) {}

        if( !cont )
        {
          if( !pSlaveMode )
            attachBroken( "orphans", file );
          continue;
        }

        if( cont->findFile( file->getName() ) )
        {
          if( !pSlaveMode )
            attachBroken( "name_conflicts", file );
          continue;
        }
        else
          cont->addFile( file );
      }
    }
  }

  //------------------------------------------------------------------------
  // Configure the file service
  //------------------------------------------------------------------------
  void ChangeLogFileMDSvc::configure(
                             std::map<std::string, std::string> &config )
    throw( MDException )
  {
    //--------------------------------------------------------------------------
    // Configure the changelog
    //--------------------------------------------------------------------------
    std::map<std::string, std::string>::iterator it;
    it = config.find( "changelog_path" );
    if( it == config.end() )
    {
      MDException e( EINVAL );
      e.getMessage() << "changelog_path not specified" ;
      throw e;
    }
    pChangeLogPath = it->second;

    //--------------------------------------------------------------------------
    // Check whether we should run in the slave mode
    //--------------------------------------------------------------------------
    it = config.find( "slave_mode" );
    if( it != config.end() && it->second == "true" )
    {
      pSlaveMode = true;
      int32_t pollInterval = 1000;
      it = config.find( "poll_interval_us" );
      if( it != config.end() )
      {
        pollInterval = strtol( it->second.c_str(), 0, 0 );
        if( pollInterval == 0 ) pollInterval = 1000;
      }
    }
  }

  //------------------------------------------------------------------------
  // Finalize the file service
  //------------------------------------------------------------------------
  void ChangeLogFileMDSvc::finalize() throw( MDException )
  {
    pChangeLog->close();
    IdMap::iterator it;
    for( it = pIdMap.begin(); it != pIdMap.end(); ++it )
      delete it->second.ptr;
    pIdMap.clear();
  }

  //----------------------------------------------------------------------------
  // Get the file metadata information for the given file ID
  //----------------------------------------------------------------------------
  FileMD *ChangeLogFileMDSvc::getFileMD( FileMD::id_t id ) throw( MDException )
  {
    IdMap::iterator it = pIdMap.find( id );
    if( it == pIdMap.end() )
    {
      MDException e( ENOENT );
      e.getMessage() << "File #" << id << " not found";
      throw e;
    }
    return it->second.ptr;
  }

  //----------------------------------------------------------------------------
  // Create new file metadata object
  //----------------------------------------------------------------------------
  FileMD *ChangeLogFileMDSvc::createFile() throw( MDException )
  {
    FileMD *file = new FileMD( pFirstFreeId++, this );
    pIdMap.insert( std::make_pair( file->getId(), DataInfo( 0, file ) ) );
    IFileMDChangeListener::Event e( file, IFileMDChangeListener::Created );
    notifyListeners( &e );
    return file;
  }

  //----------------------------------------------------------------------------
  // Update the file metadata
  //----------------------------------------------------------------------------
  void ChangeLogFileMDSvc::updateStore( FileMD *obj ) throw( MDException )
  {
    //--------------------------------------------------------------------------
    // Find the object in the map
    //--------------------------------------------------------------------------
    IdMap::iterator it = pIdMap.find( obj->getId() );
    if( it == pIdMap.end() )
    {
      MDException e( ENOENT );
      e.getMessage() << "File #" << obj->getId() << " not found. ";
      e.getMessage() << "The object was not created in this store!";
      throw e;
    }

    //--------------------------------------------------------------------------
    // Store the file in the changelog and notify the listener
    //--------------------------------------------------------------------------
    eos::Buffer buffer;
    obj->serialize( buffer );
    it->second.logOffset = pChangeLog->storeRecord( eos::UPDATE_RECORD_MAGIC,
                                                    buffer );
    IFileMDChangeListener::Event e( obj, IFileMDChangeListener::Updated );
    notifyListeners( &e );
  }

  //----------------------------------------------------------------------------
  // Remove object from the store
  //----------------------------------------------------------------------------
  void ChangeLogFileMDSvc::removeFile( FileMD *obj ) throw( MDException )
  {
    removeFile( obj->getId() );
  }

  //----------------------------------------------------------------------------
  // Remove object from the store
  //----------------------------------------------------------------------------
  void ChangeLogFileMDSvc::removeFile( FileMD::id_t fileId )
    throw( MDException )
  {
    //--------------------------------------------------------------------------
    // Find the object in the map
    //--------------------------------------------------------------------------
    IdMap::iterator it = pIdMap.find( fileId );
    if( it == pIdMap.end() )
    {
      MDException e( ENOENT );
      e.getMessage() << "File #" << fileId << " not found. ";
      e.getMessage() << "The object was not created in this store!";
      throw e;
    }

    //--------------------------------------------------------------------------
    // Store the file in the changelog and notify the listener
    //--------------------------------------------------------------------------
    eos::Buffer buffer;
    buffer.putData( &fileId, sizeof( FileMD::id_t ) );
    pChangeLog->storeRecord( eos::DELETE_RECORD_MAGIC, buffer );
    IFileMDChangeListener::Event e( fileId, IFileMDChangeListener::Deleted );
    notifyListeners( &e );
    delete it->second.ptr;
    pIdMap.erase( it );
  }

  //----------------------------------------------------------------------------
  // Add file listener
  //----------------------------------------------------------------------------
  void ChangeLogFileMDSvc::addChangeListener( IFileMDChangeListener *listener )
  {
    pListeners.push_back( listener );
  }

  //----------------------------------------------------------------------------
  // Visit all the files
  //----------------------------------------------------------------------------
  void ChangeLogFileMDSvc::visit( IFileVisitor *visitor )
  {
    IdMap::iterator it;
    for( it = pIdMap.begin(); it != pIdMap.end(); ++it )
      visitor->visitFile( it->second.ptr );
  }

  //----------------------------------------------------------------------------
  // Scan the changelog and put the appropriate data in the lookup table
  //----------------------------------------------------------------------------
  bool ChangeLogFileMDSvc::FileMDScanner::processRecord( uint64_t      offset,
                                                         char          type,
                                                         const Buffer &buffer )
  {
    //--------------------------------------------------------------------------
    // Update
    //--------------------------------------------------------------------------
    if( type == UPDATE_RECORD_MAGIC )
    {
      FileMD::id_t id;
      buffer.grabData( 0, &id, sizeof( FileMD::id_t ) );
      DataInfo &d = pIdMap[id];
      d.logOffset = offset;
      if( !d.buffer )
        d.buffer = new Buffer();
      (*d.buffer) = buffer;
      if( pLargestId < id ) pLargestId = id;
    }

    //--------------------------------------------------------------------------
    // Deletion
    //--------------------------------------------------------------------------
    else if( type == DELETE_RECORD_MAGIC )
    {
      FileMD::id_t id;
      buffer.grabData( 0, &id, sizeof( FileMD::id_t ) );
      IdMap::iterator it = pIdMap.find( id );
      if( it != pIdMap.end() )
      {
        delete it->second.buffer;
        pIdMap.erase( it );
      }
      if( pLargestId < id ) pLargestId = id;
    }
    //--------------------------------------------------------------------------
    // Compaction mark - we stop scanning here
    //--------------------------------------------------------------------------
    else if( type == COMPACT_STAMP_RECORD_MAGIC )
    {
      if( pSlaveMode )
        return false;
    }

    return true;
  }

  //----------------------------------------------------------------------------
  // Prepare for online compacting.
  //----------------------------------------------------------------------------
  void *ChangeLogFileMDSvc::compactPrepare( const std::string &newLogFileName ) const
   throw( MDException )
  {
    //--------------------------------------------------------------------------
    // Try to open a new log file for writing
    //--------------------------------------------------------------------------
    ::CompactingData *data = new ::CompactingData();
    try
    {
      data->newLog->open( newLogFileName, ChangeLogFile::Create,
                           FILE_LOG_MAGIC );
      data->logFileName = newLogFileName;
      data->originalLog = pChangeLog;
      data->newRecord   = pChangeLog->getNextOffset();
    }
    catch( MDException &e )
    {
      delete data;
      throw;
    }

    //--------------------------------------------------------------------------
    // Get the list of records
    //--------------------------------------------------------------------------
    IdMap::const_iterator it;
    for( it = pIdMap.begin(); it != pIdMap.end(); ++it )
      data->records.push_back( ::RecordData( it->second.logOffset, it->first ) );
    return data;
  }

  //----------------------------------------------------------------------------
  // Do the compacting.
  //----------------------------------------------------------------------------
  void ChangeLogFileMDSvc::compact( void *&compactingData ) throw( MDException )
  {
    //--------------------------------------------------------------------------
    // Sort the records to avoid random seeks
    //--------------------------------------------------------------------------
    ::CompactingData *data = (::CompactingData*)compactingData;
    if( !data )
    {
      MDException e( EINVAL );
      e.getMessage() << "Compacting data incorrect" ;
      throw e;
    }
    std::sort( data->records.begin(), data->records.end(),
               ::OffsetComparator() );

    //--------------------------------------------------------------------------
    // Copy the records to the new file
    //--------------------------------------------------------------------------
    try
    {
      std::vector<RecordData>::iterator it;
      for( it = data->records.begin(); it != data->records.end(); ++it )
      {
        Buffer  buff;
        uint8_t type;
        type = data->originalLog->readRecord( it->offset, buff );
        it->newOffset = data->newLog->storeRecord( type, buff );
      }
    }
    catch( MDException &e )
    {
      data->newLog->close();
      delete data;
      compactingData = 0;
      throw;
    }
  }

  //----------------------------------------------------------------------------
  // Commit the compacting infomrmation.
  //----------------------------------------------------------------------------
  void ChangeLogFileMDSvc::compactCommit( void *compactingData )
    throw( MDException )
  {
    ::CompactingData *data = (::CompactingData*)compactingData;
    if( !data )
    {
      MDException e( EINVAL );
      e.getMessage() << "Compacting data incorrect" ;
      throw e;
    }

    //--------------------------------------------------------------------------
    // Copy the part of the old log that has been appended after we
    // prepared
    //--------------------------------------------------------------------------
    std::map<eos::FileMD::id_t, RecordData> updates;
    try
    {
      ::UpdateHandler updateHandler( updates, data->newLog );
      data->originalLog->scanAllRecordsAtOffset( &updateHandler,
                                                  data->newRecord );
    }
    catch( MDException &e )
    {
      data->newLog->close();
      delete data;
      throw;
    }

    //--------------------------------------------------------------------------
    // Looks like we're all good and we won't be throwing any exceptions any
    // more so we may get to updating the in-memory structures.
    //
    // We start with the originally copied records
    //--------------------------------------------------------------------------
    uint64_t fileCounter = 0;
    IdMap::iterator it;
    std::vector<RecordData>::iterator itO;
    for( itO = data->records.begin(); itO != data->records.end(); ++itO )
    {
      //------------------------------------------------------------------------
      // Check if we still have the file, if not, it must have been deleted
      // so we don't care
      //------------------------------------------------------------------------
      it = pIdMap.find( itO->fileId );
      if( it == pIdMap.end() )
        continue;

      //------------------------------------------------------------------------
      // If the original offset does not match it means that we must have
      // be updated later, if not we've messed up so we die in order not
      // to lose data
      //------------------------------------------------------------------------
      assert( it->second.logOffset >= itO->offset );
      if( it->second.logOffset == itO->offset )
      {
        it->second.logOffset = itO->newOffset;
        ++fileCounter;
      }
    }

    //--------------------------------------------------------------------------
    // Now we handle updates, if we don't have the file, we're messed up,
    // if the original offsets don't match we're messed up too
    //--------------------------------------------------------------------------
    std::map<FileMD::id_t, RecordData>::iterator itU;
    for( itU = updates.begin(); itU != updates.end(); ++itU )
    {
      it = pIdMap.find( itU->second.fileId );
      assert( it != pIdMap.end() );
      assert( it->second.logOffset == itU->second.offset );

      it->second.logOffset = itO->newOffset;
      ++fileCounter;
    }

    assert( fileCounter == pIdMap.size() );

    //--------------------------------------------------------------------------
    // Replace the logs
    //--------------------------------------------------------------------------
    pChangeLog = data->newLog;
    pChangeLogPath = data->logFileName;
    data->newLog = 0;
    data->originalLog->close();
    delete data;
  }

  //----------------------------------------------------------------------------
  // Start the slave
  //----------------------------------------------------------------------------
  void ChangeLogFileMDSvc::startSlave() throw( MDException )
  {
    if( !pSlaveMode )
    {
      MDException e( errno );
      e.getMessage() << "ContainerMDSvc: not in slave mode";
      throw e;
    }

    if( pthread_create( &pFollowerThread, 0, followerThread, this ) != 0 )
    {
      MDException e( errno );
      e.getMessage() << "ContainerMDSvc: unable to start the slave follower: ";
      e.getMessage() << strerror( errno );
      throw e;
    }
    pSlaveStarted = true;
  }

  //----------------------------------------------------------------------------
  // Stop the slave mode
  //----------------------------------------------------------------------------
  void ChangeLogFileMDSvc::stopSlave() throw( MDException )
  {
    if( !pSlaveMode )
    {
      MDException e( errno );
      e.getMessage() << "ContainerMDSvc: not in slave mode";
      throw e;
    }

    if( !pSlaveStarted )
    {
      MDException e( errno );
      e.getMessage() << "ContainerMDSvc: the slave follower is not started";
      throw e;
    }

    if( pthread_cancel( pFollowerThread ) != 0 )
    {
      MDException e( errno );
      e.getMessage() << "ContainerMDSvc: unable to cancel the slave follower: ";
      e.getMessage() << strerror( errno );
      throw e;
    }

    if( pthread_join( pFollowerThread, 0 ) != 0 )
    {
      MDException e( errno );
      e.getMessage() << "ContainerMDSvc: unable to join the slave follower: ";
      e.getMessage() << strerror( errno );
      throw e;
    }
    pSlaveStarted = false;
  }

  //----------------------------------------------------------------------------
  // Attach a broken file to lost+found
  //----------------------------------------------------------------------------
  void ChangeLogFileMDSvc::attachBroken( const std::string &parent,
                                         FileMD            *file )
  {
    std::ostringstream s1, s2;
    ContainerMD *parentCont = pContSvc->getLostFoundContainer( parent );

    s1 << file->getContainerId();
    ContainerMD *cont = parentCont->findContainer( s1.str() );
    if( !cont )
      cont = pContSvc->createInParent( s1.str(), parentCont );

    s2 << file->getName() << "." << file->getId();
    file->setName( s2.str() );
    cont->addFile( file );
  }
}

