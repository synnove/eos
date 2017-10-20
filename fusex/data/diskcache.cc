//------------------------------------------------------------------------------
//! @file diskcache.cc
//! @author Andreas-Joachim Peters CERN
//! @brief data cache disk implementation
//------------------------------------------------------------------------------

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

#include "diskcache.hh"
#include "common/Logging.hh"
#include "common/Path.hh"
#include <unistd.h>
#include <sys/types.h>
#ifdef __APPLE__
#define EKEYEXPIRED 127
#include <sys/xattr.h>
#else
#include <attr/xattr.h>
#endif
std::string diskcache::sLocation;
bufferllmanager diskcache::sBufferManager;
off_t diskcache::sMaxSize = 2 * 1024 * 1024ll;
shared_ptr<dircleaner> diskcache::sDirCleaner;

/* -------------------------------------------------------------------------- */
int
/* -------------------------------------------------------------------------- */
diskcache::init()
/* -------------------------------------------------------------------------- */
{
  cachehandler::cacheconfig config = cachehandler::instance().get_config();

  if (::access(config.location.c_str(), W_OK)) {
    return errno;
  }

  sLocation = config.location;

  if (config.per_file_cache_max_size) {
    diskcache::sMaxSize = config.per_file_cache_max_size;
  }

  return 0;
}

/* -------------------------------------------------------------------------- */
int
/* -------------------------------------------------------------------------- */
diskcache::init_daemonized()
/* -------------------------------------------------------------------------- */
{
  cachehandler::cacheconfig config = cachehandler::instance().get_config();

  if (config.per_file_cache_max_size) {
    diskcache::sMaxSize = config.per_file_cache_max_size;
  }

  if (config.clean_on_startup) {
    eos_static_info("cleaning cache path=%s", config.location.c_str());
    sDirCleaner = std::make_shared<dircleaner>(config.location,
                  config.total_file_cache_size);

    if (sDirCleaner->cleanall()) {
      eos_static_err("cache cleanup failed");
      return -1;
    }
  }

  // start the disk cache leveling thread;
  return 0;
}

/* -------------------------------------------------------------------------- */
diskcache::diskcache() : ino(0), nattached(0), fd(-1)
  /* -------------------------------------------------------------------------- */
{
  return;
}

/* -------------------------------------------------------------------------- */
diskcache::diskcache(fuse_ino_t _ino) : ino(_ino), nattached(0), fd(-1)
  /* -------------------------------------------------------------------------- */
{
  return;
}

/* -------------------------------------------------------------------------- */
diskcache::~diskcache()
/* -------------------------------------------------------------------------- */
{
}

/* -------------------------------------------------------------------------- */
int
diskcache::location(std::string& path, bool mkpath)
/* -------------------------------------------------------------------------- */
{
  char cache_path[1024 + 20];
  snprintf(cache_path, sizeof(cache_path), "%s/%08lx/%08lX",
           sLocation.c_str(), ino / 10000, ino);

  if (mkpath) {
    eos::common::Path cPath(cache_path);

    if (!cPath.MakeParentPath(S_IRWXU)) {
      return errno;
    }
  }

  path = cache_path;
  return 0;
}

/* -------------------------------------------------------------------------- */
int
/* -------------------------------------------------------------------------- */
diskcache::attach(fuse_req_t req, std::string& acookie, int flag)
/* -------------------------------------------------------------------------- */
{
  XrdSysMutexHelper lLock(this);
  int rc = 0;

  if (nattached == 0) {
    std::string path;
    rc = location(path);

    if (rc) {
      return rc;
    }

    if (stat(path.c_str(), &attachstat)) {
      // a new file
      sDirCleaner->get_external_tree().change(0, 1);
    }

    // need to open the file
    fd = open(path.c_str(), O_CREAT | O_RDWR, S_IRWXU);

    if (fd < 0) {
      return -errno;
    }
  }

  std::string ccookie;

  if ((!nattached) && ((!cookie(ccookie) || (ccookie != "")))) {
    fstat(fd, &attachstat);

    // compare if the cookies are identical, otherwise we truncate to 0
    if (ccookie != acookie) {
      eos_static_debug("diskcache::attach truncating for cookie: %s <=> %s\n",
                       ccookie.c_str(), acookie.c_str());

      if (truncate(0)) {
        char msg[1024];
        snprintf(msg, sizeof(msg),
                 "failed to truncate to invalidate cache file - ino=%08lx", ino);
        throw std::runtime_error(msg);
      }

      set_cookie(acookie);
      rc = EKEYEXPIRED;
    }
  } else {
    set_cookie(acookie);
  }

  nattached++;
  return rc;
}

/* -------------------------------------------------------------------------- */
int
/* -------------------------------------------------------------------------- */
diskcache::detach(std::string& cookie)
/* -------------------------------------------------------------------------- */
{
  XrdSysMutexHelper lLock(this);
  nattached--;

  if (!nattached) {
    fstat(fd, &detachstat);
    sDirCleaner->get_external_tree().change(detachstat.st_size - attachstat.st_size,
                                            0);
    int rc = close(fd);

    if (rc) {
      return errno;
    }
  }

  return 0;
}

/* -------------------------------------------------------------------------- */
int
/* -------------------------------------------------------------------------- */
diskcache::unlink()
/* -------------------------------------------------------------------------- */
{
  std::string path;
  int rc = location(path);

  if (!rc) {
    struct stat buf;
    rc = stat(path.c_str(), &buf);

    if (!rc) {
      rc = ::unlink(path.c_str());

      if (!rc) {
        // a deleted file
        sDirCleaner->get_external_tree().change(-buf.st_size, -1);
      }
    }
  }

  return rc;
}

/* -------------------------------------------------------------------------- */
ssize_t
/* -------------------------------------------------------------------------- */
diskcache::pread(void* buf, size_t count, off_t offset)
/* -------------------------------------------------------------------------- */
{
  eos_static_debug("diskcache::pread %lu %lu\n", count, offset);

  // restrict to our local max size cache size
  if (offset >= sMaxSize) {
    return 0;
  }

  if ((off_t)(offset + count) > sMaxSize) {
    count = sMaxSize - offset;
  }

  return ::pread(fd, buf, count, offset);
}

/* -------------------------------------------------------------------------- */
ssize_t
/* -------------------------------------------------------------------------- */
diskcache::peek_read(char*& buf, size_t count, off_t offset)
/* -------------------------------------------------------------------------- */
{
  this->Lock();
  buffer = sBufferManager.get_buffer();

  // restrict to our local max size cache size
  if ((off_t) offset >= sMaxSize) {
    return 0;
  }

  if ((off_t)(offset + count) > sMaxSize) {
    count = sMaxSize - offset;
  }

  if (count > buffer->capacity()) {
    buffer->reserve(count);
  }

  buf = buffer->ptr();
  return ::pread(fd, buf,  count, offset);
}

/* -------------------------------------------------------------------------- */
void
/* -------------------------------------------------------------------------- */
diskcache::release_read()
/* -------------------------------------------------------------------------- */
{
  sBufferManager.put_buffer(buffer);
  buffer.reset();
  this->UnLock();
  return;
}

/* -------------------------------------------------------------------------- */
ssize_t
diskcache::pwrite(const void* buf, size_t count, off_t offset)
/* -------------------------------------------------------------------------- */
{
  eos_static_debug("diskcache::pwrite %lu %lu\n", count, offset);

  if ((off_t) offset >= sMaxSize) {
    return 0;
  }

  if ((off_t)(offset + count) > sMaxSize) {
    count = sMaxSize - offset;
  }

  return ::pwrite(fd, buf, count, offset);
}

/* -------------------------------------------------------------------------- */
int
/* -------------------------------------------------------------------------- */
diskcache::truncate(off_t offset)
/* -------------------------------------------------------------------------- */
{
  eos_static_debug("diskcache::truncate %lu\n", offset);
  fstat(fd, &detachstat);
  int rc = 0;

  if (offset >= sMaxSize) {
    offset = sMaxSize;
  }

  rc = ::ftruncate(fd, offset);

  if (!rc) {
    sDirCleaner->get_external_tree().change(detachstat.st_size - attachstat.st_size,
                                            0);
    attachstat.st_size = offset;
  }

  return rc;
}

/* -------------------------------------------------------------------------- */
int
/* -------------------------------------------------------------------------- */
diskcache::sync()
/* -------------------------------------------------------------------------- */
{
  return ::fdatasync(fd);
}

/* -------------------------------------------------------------------------- */
size_t
/* -------------------------------------------------------------------------- */
diskcache::size()
/* -------------------------------------------------------------------------- */
{
  struct stat buf;
  buf.st_size = 0;

  if (::fstat(fd, &buf)) {
    throw std::runtime_error("diskcache stat failure");
  }

  return buf.st_size;
}

/* -------------------------------------------------------------------------- */
int
/* -------------------------------------------------------------------------- */
diskcache::set_attr(std::string& key, std::string& value)
/* -------------------------------------------------------------------------- */
{
  if (fd > 0) {
    int rc = 0;
#ifdef __APPLE__
    rc = fsetxattr(fd, key.c_str(), value.c_str(), value.size(), 0, 0);
#else
    rc = fsetxattr(fd, key.c_str(), value.c_str(), value.size(), 0);
#endif

    if (rc && errno == ENOTSUP) {
      throw std::runtime_error("diskcache has no xattr support");
    }
  }

  eos_static_debug("set_attr key=%s val=%s fd=%d\n", key.c_str(), value.c_str(),
                   fd);
  return -1;
}

/* -------------------------------------------------------------------------- */
int
/* -------------------------------------------------------------------------- */
diskcache::attr(std::string key, std::string& value)
/* -------------------------------------------------------------------------- */
{
  if (fd > 0) {
    value.resize(4096);
    ssize_t n = 0;
#ifdef __APPLE__
    n = fgetxattr(fd, key.c_str(), (void*) value.c_str(), value.size(), 0 , 0);
#else
    n = fgetxattr(fd, key.c_str(), (void*) value.c_str(), value.size());
#endif

    if (n >= 0) {
      value.resize(n);
      return 0;
    } else {
      value.resize(0);
      return -1;
    }
  }

  return -1;
}

/* -------------------------------------------------------------------------- */
int
/* -------------------------------------------------------------------------- */
diskcache::rescue(std::string& rescue_location)
/* -------------------------------------------------------------------------- */
{
  std::string path;
  int rc = location(path);

  if (!rescue_location.length()) {
    rescue_location = path;
    rescue_location += ".recover";
  }

  if (!rc) {
    return ::rename(path.c_str(), rescue_location.c_str());
  } else {
    return rc;
  }
}