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
//! @brief File MD service based on quarkdb
//------------------------------------------------------------------------------

#ifndef __EOS_NS_FILE_MD_SVC_HH__
#define __EOS_NS_FILE_MD_SVC_HH__

#include "namespace/interface/IFileMDSvc.hh"
#include "namespace/ns_quarkdb/persistency/NextInodeProvider.hh"
#include "namespace/ns_quarkdb/LRU.hh"

EOSNSNAMESPACE_BEGIN

class IQuotaStats;
class MetadataFlusher;

//------------------------------------------------------------------------------
//! FileMDSvc based on Redis
//------------------------------------------------------------------------------
class FileMDSvc : public IFileMDSvc
{
public:
  //----------------------------------------------------------------------------
  //! Get file bucket which is computed as the id of the container modulo the
  //! number of file buckets (1M).
  //!
  //! @param id file id
  //!
  //! @return file bucket key
  //----------------------------------------------------------------------------
  static std::string getBucketKey(IContainerMD::id_t id);

  //----------------------------------------------------------------------------
  //! Constructor
  //----------------------------------------------------------------------------
  FileMDSvc();

  //----------------------------------------------------------------------------
  //! Destructor
  //----------------------------------------------------------------------------
  virtual ~FileMDSvc();

  //----------------------------------------------------------------------------
  //! Initizlize the file service
  //----------------------------------------------------------------------------
  virtual void initialize() override;

  //----------------------------------------------------------------------------
  //! Configure the file service
  //!
  //! @param config map holding configuration parameters
  //----------------------------------------------------------------------------
  virtual void configure(const std::map<std::string, std::string>& config)
  override;

  //----------------------------------------------------------------------------
  //! Finalize the file service
  //----------------------------------------------------------------------------
  virtual void finalize() override {};

  //----------------------------------------------------------------------------
  //! Get the file metadata information for the given file ID
  //!
  //! @param id file id
  //----------------------------------------------------------------------------
  virtual std::shared_ptr<IFileMD> getFileMD(IFileMD::id_t id) override
  {
    return getFileMD(id, 0);
  }

  //------------------------------------------------------------------------
  //! Get the file metadata information for the given file ID and clock
  //! value
  //------------------------------------------------------------------------
  virtual std::shared_ptr<IFileMD> getFileMD(IFileMD::id_t id,
      uint64_t* clock) override;

  //----------------------------------------------------------------------------
  //! Create new file metadata object with an assigned id
  //----------------------------------------------------------------------------
  virtual std::shared_ptr<IFileMD> createFile() override;

  //----------------------------------------------------------------------------
  //! Update the file metadata in the backing store after the FileMD object
  //! has been changed
  //----------------------------------------------------------------------------
  virtual void updateStore(IFileMD* obj) override;

  //----------------------------------------------------------------------------
  //! Remove object from the store
  //----------------------------------------------------------------------------
  virtual void removeFile(IFileMD* obj) override;

  //----------------------------------------------------------------------------
  //! Get number of files
  //----------------------------------------------------------------------------
  virtual uint64_t getNumFiles() override;

  //----------------------------------------------------------------------------
  //! Add file listener that will be notified about all of the changes in
  //! the store
  //----------------------------------------------------------------------------
  virtual void addChangeListener(IFileMDChangeListener* listener) override;

  //----------------------------------------------------------------------------
  //! Notify the listeners about the change
  //----------------------------------------------------------------------------
  virtual void notifyListeners(IFileMDChangeListener::Event* event) override;

  //----------------------------------------------------------------------------
  //! Set container service
  //!
  //! @param cont_svc container service
  //----------------------------------------------------------------------------
  void setContMDService(IContainerMDSvc* cont_svc) override;

  //----------------------------------------------------------------------------
  //! Set the QuotaStats object for the follower
  //!
  //! @param quota_stats object implementing the IQuotaStats interface
  //----------------------------------------------------------------------------
  void setQuotaStats(IQuotaStats* quota_stats) override
  {
    pQuotaStats = quota_stats;
  }

  //----------------------------------------------------------------------------
  //! Visit all the files
  //----------------------------------------------------------------------------
  void visit(IFileVisitor* visitor) override {};

  //----------------------------------------------------------------------------
  //! Check files that had errors - these are stored in a separate set in the
  //! KV store.
  //!
  //! @param oss output string stream
  //!
  //! @return true if all files are consistent, otherwise false
  //----------------------------------------------------------------------------
  bool checkFiles(std::ostringstream& oss);

  //----------------------------------------------------------------------------
  //! Get first free file id
  //----------------------------------------------------------------------------
  IFileMD::id_t getFirstFreeId() override;

  //----------------------------------------------------------------------------
  //! Override number of buckets
  //----------------------------------------------------------------------------
  static void OverrideNumberOfBuckets(uint64_t buckets = 1024 * 1024);

private:
  typedef std::list<IFileMDChangeListener*> ListenerList;
  static std::uint64_t sNumFileBuckets; ///< Number of buckets power of 2
  //! Interval for backend flush of consistent file ids
  static std::chrono::seconds sFlushInterval;

  //----------------------------------------------------------------------------
  //! Check file object consistency
  //!
  //! @param fid file id
  //!
  //! @return true if file info is consistent, otherwise false
  //----------------------------------------------------------------------------
  bool checkFile(std::uint64_t fid);

  //----------------------------------------------------------------------------
  //! Attach a broken file to lost+found
  //----------------------------------------------------------------------------
  void attachBroken(const std::string& parent, IFileMD* file);

  //----------------------------------------------------------------------------
  //! Safety check to make sure there are nofile entries in the backend with
  //! ids bigger than the max file id. If there is any problem this will throw
  //! an eos::MDException.
  //----------------------------------------------------------------------------
  void SafetyCheck();

  //----------------------------------------------------------------------------
  //! Compute the number of files from the backend
  //----------------------------------------------------------------------------
  void ComputeNumberOfFiles();

  ListenerList pListeners; ///< List of listeners to notify of changes
  IQuotaStats* pQuotaStats; ///< Quota view
  IContainerMDSvc* pContSvc; ///< Container metadata service
  MetadataFlusher* pFlusher; ///< Metadata flusher object
  qclient::QClient* pQcl; ///< QClient object
  qclient::QHash mMetaMap ; ///< Map holding metainfo about the namespace
  NextInodeProvider mInodeProvider; ///< Provides next free inode
  qclient::QSet mDirtyFidBackend; ///< Set of "dirty" files
  LRU<IFileMD::id_t, IFileMD> mFileCache; ///< Local cache of file objects
  std::atomic<uint64_t> mNumFiles; ///< Total number of fileso
};

EOSNSNAMESPACE_END

#endif // __EOS_NS_FILE_MD_SVC_HH__
