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
//! @brief Class representing the container metadata
//------------------------------------------------------------------------------

#ifndef __EOS_NS_CONTAINER_MD_HH__
#define __EOS_NS_CONTAINER_MD_HH__

#include "namespace/interface/IContainerMD.hh"
#include "namespace/interface/IFileMD.hh"
#include "namespace/ns_quarkdb/BackendClient.hh"
#include "namespace/ns_quarkdb/flusher/MetadataFlusher.hh"
#include "proto/ContainerMd.pb.h"
#include <sys/time.h>

EOSNSNAMESPACE_BEGIN

class IContainerMDSvc;
class IFileMDSvc;

//------------------------------------------------------------------------------
//! Class holding the metadata information concerning a single container
//------------------------------------------------------------------------------
class ContainerMD : public IContainerMD
{
public:
  //----------------------------------------------------------------------------
  //! Constructor
  //----------------------------------------------------------------------------
  ContainerMD(id_t id, IFileMDSvc* file_svc, IContainerMDSvc* cont_svc);

  //----------------------------------------------------------------------------
  //! Constructor used for testing and dump command
  //----------------------------------------------------------------------------
  ContainerMD(): pContSvc(nullptr), pFileSvc(nullptr), pFlusher(nullptr),
    pQcl(nullptr), mClock(1) {}

  //----------------------------------------------------------------------------
  //! Destructor
  //----------------------------------------------------------------------------
  virtual ~ContainerMD() {};

  //----------------------------------------------------------------------------
  //! Copy constructor
  //----------------------------------------------------------------------------
  ContainerMD(const ContainerMD& other);

  //----------------------------------------------------------------------------
  //! Virtual copy constructor
  //----------------------------------------------------------------------------
  virtual ContainerMD* clone() const override;

  //----------------------------------------------------------------------------
  //! Assignment operator
  //----------------------------------------------------------------------------
  ContainerMD& operator=(const ContainerMD& other);

  //----------------------------------------------------------------------------
  //! Set services
  //----------------------------------------------------------------------------
  void setServices(IFileMDSvc* file_svc, IContainerMDSvc* cont_svc);

  //----------------------------------------------------------------------------
  //! Add container
  //----------------------------------------------------------------------------
  void addContainer(IContainerMD* container) override;

  //----------------------------------------------------------------------------
  //! Remove container
  //----------------------------------------------------------------------------
  void removeContainer(const std::string& name) override;

  //----------------------------------------------------------------------------
  //! Find subcontainer
  //----------------------------------------------------------------------------
  std::shared_ptr<IContainerMD> findContainer(const std::string& name) override;

  //----------------------------------------------------------------------------
  //! Get number of containers
  //----------------------------------------------------------------------------
  size_t getNumContainers() override;

  //----------------------------------------------------------------------------
  //! Add file
  //----------------------------------------------------------------------------
  void addFile(IFileMD* file) override;

  //----------------------------------------------------------------------------
  //! Remove file
  //----------------------------------------------------------------------------
  void removeFile(const std::string& name) override;

  //----------------------------------------------------------------------------
  //! Find file
  //----------------------------------------------------------------------------
  std::shared_ptr<IFileMD> findFile(const std::string& name) override;

  //----------------------------------------------------------------------------
  //! Get number of files
  //----------------------------------------------------------------------------
  size_t getNumFiles() override;

  //----------------------------------------------------------------------------
  //! Get container id
  //----------------------------------------------------------------------------
  inline id_t
  getId() const override
  {
    return mCont.id();
  }

  //----------------------------------------------------------------------------
  //! Get parent id
  //----------------------------------------------------------------------------
  inline id_t
  getParentId() const override
  {
    return mCont.parent_id();
  }

  //----------------------------------------------------------------------------
  //! Set parent id
  //----------------------------------------------------------------------------
  void
  setParentId(id_t parentId) override
  {
    mCont.set_parent_id(parentId);
  }

  //----------------------------------------------------------------------------
  //! Get the flags
  //----------------------------------------------------------------------------
  inline uint16_t
  getFlags() const override
  {
    return mCont.flags();
  }

  //----------------------------------------------------------------------------
  //! Set flags
  //----------------------------------------------------------------------------
  virtual void setFlags(uint16_t flags) override
  {
    mCont.set_flags(0x00ff & flags);
  }

  //----------------------------------------------------------------------------
  //! Set creation time
  //----------------------------------------------------------------------------
  void setCTime(ctime_t ctime) override;

  //----------------------------------------------------------------------------
  //! Set creation time to now
  //----------------------------------------------------------------------------
  void setCTimeNow() override;

  //----------------------------------------------------------------------------
  //! Get creation time
  //----------------------------------------------------------------------------
  void getCTime(ctime_t& ctime) const override;

  //----------------------------------------------------------------------------
  //! Set creation time
  //----------------------------------------------------------------------------
  void setMTime(mtime_t mtime) override;

  //----------------------------------------------------------------------------
  //! Set creation time to now
  //----------------------------------------------------------------------------
  void setMTimeNow() override;

  //----------------------------------------------------------------------------
  //! Get modification time
  //----------------------------------------------------------------------------
  void getMTime(mtime_t& mtime) const override;

  //----------------------------------------------------------------------------
  //! Set propagated modification time (if newer)
  //----------------------------------------------------------------------------
  bool setTMTime(tmtime_t tmtime) override;

  //----------------------------------------------------------------------------
  //! Set propagated modification time to now
  //----------------------------------------------------------------------------
  void setTMTimeNow() override;

  //----------------------------------------------------------------------------
  //! Get propagated modification time
  //----------------------------------------------------------------------------
  void getTMTime(tmtime_t& tmtime) override;

  //----------------------------------------------------------------------------
  //! Trigger an mtime change event
  //----------------------------------------------------------------------------
  void notifyMTimeChange(IContainerMDSvc* containerMDSvc) override;

  //----------------------------------------------------------------------------
  //! Get tree size
  //----------------------------------------------------------------------------
  inline uint64_t
  getTreeSize() const override
  {
    return mCont.tree_size();
  }

  //----------------------------------------------------------------------------
  //! Set tree size
  //----------------------------------------------------------------------------
  inline void
  setTreeSize(uint64_t treesize) override
  {
    mCont.set_tree_size(treesize);
  }

  //----------------------------------------------------------------------------
  //! Update to tree size
  //----------------------------------------------------------------------------
  uint64_t updateTreeSize(int64_t delta) override;

  //----------------------------------------------------------------------------
  //! Get name
  //----------------------------------------------------------------------------
  inline const std::string&
  getName() const override
  {
    return mCont.name();
  }

  //----------------------------------------------------------------------------
  //! Set name
  //----------------------------------------------------------------------------
  void setName(const std::string& name) override;

  //----------------------------------------------------------------------------
  //! Get uid
  //----------------------------------------------------------------------------
  inline uid_t
  getCUid() const override
  {
    return mCont.uid();
  }

  //----------------------------------------------------------------------------
  //! Set uid
  //----------------------------------------------------------------------------
  inline void
  setCUid(uid_t uid) override
  {
    mCont.set_uid(uid);
  }

  //----------------------------------------------------------------------------
  //! Get gid
  //----------------------------------------------------------------------------
  inline gid_t
  getCGid() const override
  {
    return mCont.gid();
  }

  //----------------------------------------------------------------------------
  //! Set gid
  //----------------------------------------------------------------------------
  inline void
  setCGid(gid_t gid) override
  {
    mCont.set_gid(gid);
  }

  //----------------------------------------------------------------------------
  //! Get mode
  //----------------------------------------------------------------------------
  inline mode_t
  getMode() const override
  {
    return mCont.mode();
  }

  //----------------------------------------------------------------------------
  //! Set mode
  //----------------------------------------------------------------------------
  inline void
  setMode(mode_t mode) override
  {
    mCont.set_mode(mode);
  }

  //----------------------------------------------------------------------------
  //! Add extended attribute
  //----------------------------------------------------------------------------
  void
  setAttribute(const std::string& name, const std::string& value) override
  {
    (*mCont.mutable_xattrs())[name] = value;
  }

  //----------------------------------------------------------------------------
  //! Remove attribute
  //----------------------------------------------------------------------------
  void removeAttribute(const std::string& name) override;

  //----------------------------------------------------------------------------
  //! Check if the attribute exist
  //----------------------------------------------------------------------------
  bool
  hasAttribute(const std::string& name) const override
  {
    return (mCont.xattrs().find(name) != mCont.xattrs().end());
  }

  //----------------------------------------------------------------------------
  //! Return number of attributes
  //----------------------------------------------------------------------------
  size_t
  numAttributes() const override
  {
    return mCont.xattrs().size();
  }

  //----------------------------------------------------------------------------
  // Get the attribute
  //----------------------------------------------------------------------------
  std::string getAttribute(const std::string& name) const override;

  //----------------------------------------------------------------------------
  //! Get map copy of the extended attributes
  //!
  //! @return std::map containing all the extended attributes
  //----------------------------------------------------------------------------
  XAttrMap getAttributes() const override;

  //------------------------------------------------------------------------------
  //! Check the access permissions
  //!
  //! @return true if all the requested rights are granted, false otherwise
  //------------------------------------------------------------------------------
  bool access(uid_t uid, gid_t gid, int flags = 0) override;

  //----------------------------------------------------------------------------
  //! Clean up the entire contents for the container. Delete files and
  //! containers recurssively
  //----------------------------------------------------------------------------
  void cleanUp() override;

  //----------------------------------------------------------------------------
  //! Serialize the object to a buffer
  //----------------------------------------------------------------------------
  void serialize(Buffer& buffer) override;

  //----------------------------------------------------------------------------
  //! Load children of container
  //----------------------------------------------------------------------------
  void loadChildren();

  //----------------------------------------------------------------------------
  //! Deserialize the class to a buffer and load its children
  //----------------------------------------------------------------------------
  void deserialize(Buffer& buffer) override;

  //----------------------------------------------------------------------------
  //! Initialize, and load children
  //----------------------------------------------------------------------------
  void initialize(eos::ns::ContainerMdProto &&proto);

  //----------------------------------------------------------------------------
  //! Get value tracking changes to the metadata object
  //----------------------------------------------------------------------------
  virtual uint64_t getClock() const override
  {
    return mClock;
  }

  //----------------------------------------------------------------------------
  //! Get env representation of the container object
  //!
  //! @param env string where representation is stored
  //! @param escapeAnd if true escape & with #AND# ...
  //----------------------------------------------------------------------------
  void getEnv(std::string& env, bool escapeAnd = false) override;

  //----------------------------------------------------------------------------
  //! Get iterator to the begining of the subcontainers map
  //----------------------------------------------------------------------------
  eos::IContainerMD::ContainerMap::const_iterator
  subcontainersBegin() override {
    waitOnContainerMap();
    return mSubcontainers.begin();
  }

  //----------------------------------------------------------------------------
  //! Get iterator to the end of the subcontainers map
  //----------------------------------------------------------------------------
  virtual eos::IContainerMD::ContainerMap::const_iterator
  subcontainersEnd() override {
    waitOnContainerMap();
    return mSubcontainers.end();
  }

  //----------------------------------------------------------------------------
  //! Get iterator to the begining of the files map
  //----------------------------------------------------------------------------
  virtual eos::IContainerMD::FileMap::const_iterator
  filesBegin() override {
    waitOnFileMap();
    return mFiles.begin();
  }

  //----------------------------------------------------------------------------
  //! Get iterator to the end of the files map
  //----------------------------------------------------------------------------
  virtual eos::IContainerMD::FileMap::const_iterator
  filesEnd() override {
    waitOnFileMap();
    return mFiles.end();
  }

protected:
  ContainerMap mSubcontainers; //! Directory name to id map
  FileMap mFiles; ///< File name to id map

private:
  //----------------------------------------------------------------------------
  //! Load FileMap
  //----------------------------------------------------------------------------
  void waitOnFileMap() {
    std::lock_guard<std::mutex> lock(mFilesMtx);
    if(!pFilesLoaded) {
      // Attention, there's a trap here: pFilesLoaded must be set to true
      // before getting the future - if the future throws, we don't want
      // to call .get() on it again later.

      pFilesLoaded = true;
      mFiles = mFilesFuture.get();
    }
  }

  //----------------------------------------------------------------------------
  //! Load ContainerMap
  //----------------------------------------------------------------------------
  void waitOnContainerMap() {
    std::lock_guard<std::mutex> lock(mSubcontainersMtx);
    if(!pSubContainersLoaded) {
      // Attention, there's a trap here: pSubContainersLoaded must be set to true
      // before getting the future - if the future throws, we don't want
      // to call .get() on it again later.

      pSubContainersLoaded = true;
      mSubcontainers = mSubcontainersFuture.get();
    }
  }

  eos::ns::ContainerMdProto mCont;      ///< Protobuf container representation
  IContainerMDSvc* pContSvc = nullptr;  ///< Container metadata service
  IFileMDSvc* pFileSvc = nullptr;       ///< File metadata service
  MetadataFlusher* pFlusher;            ///< Metadata flusher object
  qclient::QClient* pQcl;               ///< QClient object
  std::string pFilesKey;                ///< Map files key
  std::string pDirsKey;                 ///< Map dir key
  qclient::QHash pFilesMap;             ///< Map holding info about files
  qclient::QHash pDirsMap;              ///< Map holding info about subcontainers
  uint64_t mClock;                      ///< Value tracking changes

  std::future<ContainerMap> mSubcontainersFuture;
  std::future<FileMap> mFilesFuture;

  std::mutex mSubcontainersMtx;
  std::mutex mFilesMtx;

  bool pSubContainersLoaded = true;
  bool pFilesLoaded = true;

};

EOSNSNAMESPACE_END

#endif // __EOS_NS_CONTAINER_MD_HH__
