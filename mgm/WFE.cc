// ----------------------------------------------------------------------
// File: WFE.cc
// Author: Andreas-Joachim Peters - CERN
// ----------------------------------------------------------------------

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

#include "common/Logging.hh"
#include "common/LayoutId.hh"
#include "common/ShellCmd.hh"
#include "common/StringTokenizer.hh"
#include "mgm/Quota.hh"
#include "mgm/cta_interface/eos_cta/include/CtaFrontendApi.hpp"
#include "mgm/eos_cta_pb/EosCtaAlertHandler.hh"
#include "mgm/WFE.hh"
#include "mgm/Stat.hh"
#include "mgm/XrdMgmOfs.hh"
#include "mgm/XrdMgmOfsDirectory.hh"
#include "mgm/Master.hh"
#include "namespace/interface/IView.hh"
#include "Xrd/XrdScheduler.hh"
#include "XrdCl/XrdClDefaultEnv.hh"

#define EOS_WFE_BASH_PREFIX "/var/eos/wfe/bash/"

XrdSysMutex eos::mgm::WFE::gSchedulerMutex;
XrdScheduler* eos::mgm::WFE::gScheduler;

/*----------------------------------------------------------------------------*/
extern XrdSysError gMgmOfsEroute;
extern XrdOucTrace gMgmOfsTrace;

EOSMGMNAMESPACE_BEGIN

using namespace eos::common;

/*----------------------------------------------------------------------------*/
WFE::WFE()
/*----------------------------------------------------------------------------*/
/**
 * @brief Constructor of the work flow engine
 */
/*----------------------------------------------------------------------------*/
{
  mThread = 0;
  mMs = 0;
  mActiveJobs = 0;
  eos::common::Mapping::Root(mRootVid);
  XrdSysMutexHelper sLock(gSchedulerMutex);
  gScheduler = new XrdScheduler(&gMgmOfsEroute, &gMgmOfsTrace, 2, 128, 64);
  gScheduler->Start();
}

/*----------------------------------------------------------------------------*/
bool
WFE::Start()
/*----------------------------------------------------------------------------*/
/**
 * @brief asynchronous WFE thread startup function
 */
/*----------------------------------------------------------------------------*/
{
  // run an asynchronous WFE thread
  mThread = 0;
  XrdSysThread::Run(&mThread,
                    WFE::StartWFEThread,
                    static_cast<void*>(this),
                    XRDSYSTHREAD_HOLD,
                    "WFE engine Thread");
  return mThread != 0;
}

/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
void
WFE::Stop()
/*----------------------------------------------------------------------------*/
/**
 * @brief asynchronous WFE thread stop function
 */
/*----------------------------------------------------------------------------*/
{
  // cancel the asynchronous WFE thread
  if (mThread) {
    XrdSysThread::Cancel(mThread);
    XrdSysThread::Join(mThread, nullptr);
  }

  mThread = 0;
}

void*
WFE::StartWFEThread(void* arg)
{
  return reinterpret_cast<WFE*>(arg)->WFEr();
}

/*----------------------------------------------------------------------------*/
void*
WFE::WFEr()
/*----------------------------------------------------------------------------*/
/**
 * @brief WFE method doing the actual workflow
 *
 * This thread method loops in regular intervals over all workflow jobs in the
 * workflow directory /eos/<instance>/proc/workflow/
 */
/*----------------------------------------------------------------------------*/
{
  // ---------------------------------------------------------------------------
  // wait that the namespace is initialized
  // ---------------------------------------------------------------------------
  bool go = false;

  do {
    XrdSysThread::SetCancelOff();
    {
      XrdSysMutexHelper lock(gOFS->InitializationMutex);

      if (gOFS->Initialized == gOFS->kBooted) {
        go = true;
      }
    }
    XrdSysThread::SetCancelOn();
    XrdSysTimer sleeper;
    sleeper.Wait(1000);
  } while (!go);

  XrdSysTimer sleeper;
  sleeper.Snooze(10);
  //----------------------------------------------------------------------------
  // Eternal thread doing WFE scans
  //----------------------------------------------------------------------------
  time_t snoozetime = 10;
  size_t lWFEntx = 0;
  time_t cleanuptime = 0;
  eos_static_info("msg=\"async WFE thread started\"");

  while (1) {
    // -------------------------------------------------------------------------
    // every now and then we wake up
    // -------------------------------------------------------------------------
    XrdSysThread::SetCancelOff();
    bool IsEnabledWFE;
    time_t lWFEInterval;
    time_t lStartTime = time(NULL);
    time_t lStopTime;
    time_t lKeepTime = 7 * 86400;
    std::map<std::string, std::set<std::string> > wfedirs;
    XrdOucString stdErr;
    {
      eos::common::RWMutexReadLock lock(FsView::gFsView.ViewMutex);

      if (FsView::gFsView.mSpaceView.count("default") &&
          (FsView::gFsView.mSpaceView["default"]->GetConfigMember("wfe") == "paused")) {
        IsEnabledWFE = false;
      } else {
        IsEnabledWFE = true;
      }

      if (FsView::gFsView.mSpaceView.count("default")) {
        lWFEInterval =
          atoi(FsView::gFsView.mSpaceView["default"]->GetConfigMember("wfe.interval").c_str());
        lWFEntx =
          atoi(FsView::gFsView.mSpaceView["default"]->GetConfigMember("wfe.ntx").c_str());
        lKeepTime = atoi(
                      FsView::gFsView.mSpaceView["default"]->GetConfigMember("wfe.keepTIME").c_str());

        if (!lKeepTime) {
          lKeepTime = 7 * 86400;
        }
      } else {
        lWFEInterval = 0;
        lWFEntx = 0;
      }
    }

    // only a master needs to run WFE
    if (gOFS->MgmMaster.IsMaster() && IsEnabledWFE) {
      // -------------------------------------------------------------------------
      // do a find
      // -------------------------------------------------------------------------
      eos_static_debug("msg=\"start WFE scan\"");
      // -------------------------------------------------------------------------
      // find all directories defining an WFE policy
      // -------------------------------------------------------------------------
      gOFS->MgmStats.Add("WFEFind", 0, 0, 1);
      EXEC_TIMING_BEGIN("WFEFind");
      // prepare four queries today, yestereday for queued and error jobs
      std::string queries[4];

      for (size_t i = 0; i < 4; ++i) {
        queries[i] = gOFS->MgmProcWorkflowPath.c_str();
        queries[i] += "/";
      }

      {
        // today
        time_t when = time(NULL);
        std::string day = eos::common::Timing::UnixTimstamp_to_Day(when);
        queries[0] += day;
        queries[0] += "/q/";
        queries[1] += day;
        queries[1] += "/e/";
        //yesterday
        when -= (24 * 3600);
        day = eos::common::Timing::UnixTimstamp_to_Day(when);
        queries[2] += day;
        queries[2] += "/q/";
        queries[3] += day;
        queries[3] += "/e/";
      }

      for (size_t i = 0; i < 4; ++i) {
        eos_static_debug("query-path=%s", queries[i].c_str());
        gOFS->_find(queries[i].c_str(),
                    mError,
                    stdErr,
                    mRootVid,
                    wfedirs,
                    0,
                    0,
                    false,
                    0,
                    false,
                    0
                   );
      }

      {
        eos_static_debug("msg=\"finished WFE find\" WFE-dirs=%llu %s",
                         wfedirs.size(), stdErr.c_str()
                        );
        time_t now = time(NULL);

        for (auto it = wfedirs.begin(); it != wfedirs.end(); it++) {
          // -------------------------------------------------------------------
          // get workflows
          // -------------------------------------------------------------------
          if (it->second.size()) {
            for (auto wit = it->second.begin(); wit != it->second.end(); ++wit) {
              eos_static_debug("wfe-dir=\"%s\" wfe-job=\"%s\"", it->first.c_str(),
                               wit->c_str());
              std::string f = it->first;
              f += *wit;
              Job* job = new Job();

              if (!job || job->Load(f)) {
                eos_static_err("msg=\"cannot load workflow entry\" value=\"%s\"", f.c_str());

                if (job) {
                  delete job;
                }
              } else {
                // don't schedule jobs for the future
                if ((!job->mActions.size()) || (now < job->mActions[0].mTime)) {
                  delete job;
                  continue;
                }

                // stop scheduling if there are too many jobs running
                if ((lWFEntx - GetActiveJobs()) <= 0) {
                  if (lWFEntx > 0) {
                    mDoneSignal.Wait(10);

                    if ((lWFEntx - GetActiveJobs()) <= 0) {
                      delete job;
                      break;
                    }
                  }
                }

                if (!job->IsSync()) {
                  // use the shared scheduler for asynchronous jobs
                  XrdSysMutexHelper sLock(gSchedulerMutex);
                  time_t storetime = 0;
                  // move job into the scheduled queue
                  job->Move("q", "s", storetime);
                  job->mActions[0].mQueue = "s";
                  job->mActions[0].mTime = storetime;
                  XrdOucString tst;
                  job->mActions[0].mWhen = eos::common::StringConversion::GetSizeString(tst,
                                           (unsigned long long) storetime);
                  gScheduler->Schedule((XrdJob*) job);
                  IncActiveJobs();
                  eos_static_info("msg=\"scheduled workflow\" job=\"%s\"",
                                  job->mDescription.c_str());
                } else {
                  delete job;
                }
              }
            }
          }
        }
      }

      EXEC_TIMING_END("WFEFind");
      eos_static_debug("msg=\"finished WFE application\" WFE-dirs=%llu",
                       wfedirs.size()
                      );
    }

    lStopTime = time(NULL);

    if ((lStopTime - lStartTime) < lWFEInterval) {
      snoozetime = lWFEInterval - (lStopTime - lStartTime);
    }

    eos_static_debug("snooze-time=%llu enabled=%d", snoozetime, IsEnabledWFE);
    XrdSysThread::SetCancelOn();
    XrdSysTimer sleeper;
    time_t snoozeinterval = 1;
    size_t snoozeloop = snoozetime / 1;

    for (size_t i = 0; i < snoozeloop; i++) {
      sleeper.Snooze(snoozeinterval);
      {
        // check if the setting changes
        eos::common::RWMutexReadLock lock(FsView::gFsView.ViewMutex);

        if (FsView::gFsView.mSpaceView.count("default") &&
            (FsView::gFsView.mSpaceView["default"]->GetConfigMember("wfe") == "on")) {
          if (!IsEnabledWFE) {
            break;
          }
        } else {
          if (IsEnabledWFE) {
            break;
          }
        }
      }
    }

    if (gOFS->MgmMaster.IsMaster() && (!cleanuptime ||
                                       (cleanuptime < time(NULL)))) {
      time_t now = time(NULL);
      eos_static_info("msg=\"clean old workflows\"");
      XrdMgmOfsDirectory dir;

      if (dir.open(gOFS->MgmProcWorkflowPath.c_str(), mRootVid, "") != SFS_OK) {
        eos_static_err("msg=\"failed to open proc workflow directory\"");
        continue;
      }

      const char* entry;

      while ((entry = dir.nextEntry())) {
        std::string when = entry;

        if ((when == ".") ||
            (when == "..")) {
          continue;
        }

        time_t tst = eos::common::Timing::Day_to_UnixTimestamp(when);

        if (!tst || (tst < (now - lKeepTime))) {
          eos_static_info("msg=\"cleaning\" dir=\"%s\"", entry);
          ProcCommand Cmd;
          XrdOucString info;
          XrdOucString out;
          XrdOucString err;
          info = "mgm.cmd=rm&eos.ruid=0&eos.rgid=0&mgm.deletion=deep&mgm.option=r&mgm.path=";
          info += gOFS->MgmProcWorkflowPath;
          info += "/";
          info += entry;
          Cmd.open("/proc/user", info.c_str(), mRootVid, &mError);
          Cmd.AddOutput(out, err);

          if (err.length()) {
            eos_static_err("msg=\"cleaning failed\" errmsg=\"%s\"", err.c_str());
          } else {
            eos_static_info("msg=\"cleaned\" dri=\"%s\"");
          }

          Cmd.close();
        }
      }

      cleanuptime = now + 3600;
    }
  }

  return 0;
}

/*----------------------------------------------------------------------------*/
/**
 * @brief store a workflow jobs in the workflow queue
 * @return SFS_OK if success
 */
/*----------------------------------------------------------------------------*/
int
WFE::Job::Save(std::string queue, time_t& when, int action, int retry)
{
  if (mActions.size() != 1) {
    return -1;
  }

  std::string workflowdir = gOFS->MgmProcWorkflowPath.c_str();
  workflowdir += "/";
  workflowdir += mActions[action].mDay;
  workflowdir += "/";
  workflowdir += queue;
  workflowdir += "/";
  workflowdir += mActions[action].mWorkflow;
  workflowdir += "/";
  std::string entry;
  XrdOucString hexfid;
  eos::common::FileId::Fid2Hex(mFid, hexfid);
  entry = hexfid.c_str();
  eos_static_info("workflowdir=\"%s\" retry=%d when=%u job-time=%s",
                  workflowdir.c_str(),
                  retry, when, mActions[action].mWhen.c_str());
  XrdOucErrInfo lError;
  eos::common::Mapping::VirtualIdentity rootvid;
  eos::common::Mapping::Root(rootvid);
  // check that the workflow directory exists
  struct stat buf;

  if (gOFS->_stat(workflowdir.c_str(),
                  &buf,
                  lError,
                  rootvid,
                  "")) {
    // create the workflow sub directory
    if (gOFS->_mkdir(workflowdir.c_str(),
                     S_IRWXU | SFS_O_MKPTH,
                     lError,
                     rootvid,
                     "")) {
      eos_static_err("msg=\"failed to create workflow directory\" path=\"%s\"",
                     workflowdir.c_str());
      return -1;
    }
  }

  // write a workflow file
  std::string workflowpath = workflowdir;

  // evt. store with the current time

  if (!when) {
    when = time(NULL);
  }

  XrdOucString tst;
  workflowpath += eos::common::StringConversion::GetSizeString(tst,
                  (unsigned long long) when);
  workflowpath += ":";
  workflowpath += entry;
  workflowpath += ":";
  workflowpath += mActions[action].mEvent;
  mWorkflowPath = workflowpath;

  if (gOFS->_touch(workflowpath.c_str(), lError, rootvid, 0)) {
    eos_static_err("msg=\"failed to create workflow entry\" path=\"%s\"",
                   workflowpath.c_str());
    return -1;
  }

  if (gOFS->_attr_set(workflowpath.c_str(),
                      lError,
                      rootvid,
                      0,
                      "sys.action",
                      mActions[0].mAction.c_str())) {
    eos_static_err("msg=\"failed to store workflow action\" path=\"%s\" action=\"%s\"",
                   workflowpath.c_str(),
                   mActions[0].mAction.c_str());
    return -1;
  }

  std::string vids = eos::common::Mapping::VidToString(mVid);

  if (gOFS->_attr_set(workflowpath.c_str(),
                      lError,
                      rootvid,
                      0,
                      "sys.vid",
                      vids.c_str())) {
    eos_static_err("msg=\"failed to store workflow vid\" path=\"%s\" vid=\"%s\"",
                   workflowpath.c_str(),
                   vids.c_str());
    return -1;
  }

  XrdOucString sretry;
  sretry += (int) retry;

  if (gOFS->_attr_set(workflowpath.c_str(),
                      lError,
                      rootvid,
                      0,
                      "sys.wfe.retry",
                      sretry.c_str())) {
    eos_static_err("msg=\"failed to store workflow retry count\" path=\"%s\" retry=\"%d\"",
                   workflowpath.c_str(),
                   retry);
    return -1;
  }

  mRetry = retry;
  return SFS_OK;
}

/*----------------------------------------------------------------------------*/
int
WFE::Job::Load(std::string path2entry)
/*----------------------------------------------------------------------------*/
/**
 * @brief load a workflow job from the given path
 * @return SFS_OK if success
 */
/*----------------------------------------------------------------------------*/
{
  XrdOucErrInfo lError;
  eos::common::Mapping::VirtualIdentity rootvid;
  eos::common::Mapping::Root(rootvid);
  std::string f = path2entry;
  f.erase(0, path2entry.rfind('/') + 1);
  std::string workflow = path2entry;
  workflow.erase(path2entry.rfind('/'));
  workflow.erase(0, workflow.rfind('/') + 1);
  std::string q = path2entry;
  q.erase(q.rfind('/'));
  q.erase(q.rfind('/'));
  q.erase(0, q.rfind('/') + 1);
  std::string when;
  std::string idevent;
  std::string id;
  std::string event;
  bool s1 = eos::common::StringConversion::SplitKeyValue(f, when, idevent, ":");
  bool s2 = eos::common::StringConversion::SplitKeyValue(idevent, id, event, ":");
  mWorkflowPath = path2entry;

  if (s1 && s2) {
    mFid = eos::common::FileId::Hex2Fid(id.c_str());
    eos_static_info("workflow=\"%s\" fid=%lx", workflow.c_str(), mFid);
    XrdOucString action;

    if (!gOFS->_attr_get(path2entry.c_str(), lError, rootvid, 0,
                         "sys.action", action)) {
      time_t t_when = strtoull(when.c_str(), 0, 10);
      AddAction(action.c_str(), event, t_when, workflow, q);
    } else {
      eos_static_err("msg=\"no action stored\" path=\"%s\"", f.c_str());
    }

    XrdOucString vidstring;

    if (!gOFS->_attr_get(path2entry.c_str(), lError, rootvid, 0,
                         "sys.vid", vidstring)) {
      if (!eos::common::Mapping::VidFromString(mVid, vidstring.c_str())) {
        eos_static_crit("parsing of %s failed - setting nobody\n", vidstring.c_str());
        eos::common::Mapping::Nobody(mVid);
      }
    } else {
      eos::common::Mapping::Nobody(mVid);
      eos_static_err("msg=\"no vid stored\" path=\"%s\"", f.c_str());
    }

    XrdOucString sretry;

    if (!gOFS->_attr_get(path2entry.c_str(), lError, rootvid, 0,
                         "sys.wfe.retry", sretry)) {
      mRetry = (int)strtoul(sretry.c_str(), 0, 10);
    } else {
      eos_static_err("msg=\"no retry stored\" path=\"%s\"", f.c_str());
    }
  } else {
    eos_static_err("msg=\"illegal workflow entry\" key=\"%s\"", f.c_str());
    return SFS_ERROR;
  }

  return SFS_OK;
}

/*----------------------------------------------------------------------------*/
int
WFE::Job::Move(std::string from_queue, std::string to_queue, time_t& when,
               int retry)
/*----------------------------------------------------------------------------*/
/**
 * @brief move workflow jobs between queues
 * @return SFS_OK if success
 */
/*----------------------------------------------------------------------------*/
{
  if (Save(to_queue, when, 0, retry) == SFS_OK) {
    if ((from_queue != to_queue) && (Delete(from_queue) == SFS_ERROR)) {
      eos_static_err("msg=\"failed to remove for move from queue\"%s\" to  queue=\"%s\"",
                     from_queue.c_str(), to_queue.c_str());
    }
  } else {
    eos_static_err("msg=\"failed to save for move to queue\" queue=\"%s\"",
                   to_queue.c_str());
    return SFS_ERROR;
  }

  return SFS_OK;
}

/*----------------------------------------------------------------------------*/
int
/*----------------------------------------------------------------------------*/
WFE::Job::Results(std::string queue, int retc, XrdOucString log, time_t when)
/*----------------------------------------------------------------------------*/
{
  std::string workflowdir = gOFS->MgmProcWorkflowPath.c_str();
  workflowdir += "/";
  workflowdir += mActions[0].mDay;
  workflowdir += "/";
  workflowdir += queue;
  workflowdir += "/";
  workflowdir += mActions[0].mWorkflow;
  workflowdir += "/";
  std::string entry;
  XrdOucString hexfid;
  eos::common::FileId::Fid2Hex(mFid, hexfid);
  entry = hexfid.c_str();
  eos_static_info("workflowdir=\"%s\" entry=%s", workflowdir.c_str(),
                  entry.c_str());
  XrdOucErrInfo lError;
  eos::common::Mapping::VirtualIdentity rootvid;
  eos::common::Mapping::Root(rootvid);
  // check that the workflow directory exists
  struct stat buf;

  if (gOFS->_stat(workflowdir.c_str(),
                  &buf,
                  lError,
                  rootvid,
                  "")) {
    eos_static_err("msg=\"failed to find the workflow dir\" path=\"%s\"",
                   workflowdir.c_str());
    return -1;
  }

  // write a workflow file
  std::string workflowpath = workflowdir;
  XrdOucString tst;
  workflowpath += eos::common::StringConversion::GetSizeString(tst,
                  (unsigned long long) when);
  workflowpath += ":";
  workflowpath += entry;
  workflowpath += ":";
  workflowpath += mActions[0].mEvent;
  mWorkflowPath = workflowpath;
  XrdOucString sretc;
  sretc += retc;

  if (gOFS->_attr_set(workflowpath.c_str(),
                      lError,
                      rootvid,
                      0,
                      "sys.wfe.retc",
                      sretc.c_str())) {
    eos_static_err("msg=\"failed to store workflow return code\" path=\"%s\" retc=\"%s\"",
                   workflowpath.c_str(),
                   sretc.c_str());
    return -1;
  }

  if (gOFS->_attr_set(workflowpath.c_str(),
                      lError,
                      rootvid,
                      0,
                      "sys.wfe.log",
                      log.c_str())) {
    eos_static_err("msg=\"failed to store workflow log\" path=\"%s\" log=\"%s\"",
                   workflowpath.c_str(),
                   log.c_str());
    return -1;
  }

  return SFS_OK;
}



/*----------------------------------------------------------------------------*/
int
WFE::Job::Delete(std::string queue)
/*----------------------------------------------------------------------------*/
/**
 * @brief delete a workflow job from a queue
 * @return SFS_OK if success
 */
/*----------------------------------------------------------------------------*/
{
  if (mActions.size() != 1) {
    return SFS_ERROR;
  }

  std::string workflowdir = gOFS->MgmProcWorkflowPath.c_str();
  workflowdir += "/";
  workflowdir += mActions[0].mDay;
  workflowdir += "/";
  workflowdir += queue;
  workflowdir += "/";
  workflowdir += mActions[0].mWorkflow;
  workflowdir += "/";
  std::string entry;
  XrdOucString hexfid;
  eos::common::FileId::Fid2Hex(mFid, hexfid);
  entry = hexfid.c_str();
  eos_static_info("workflowdir=\"%s\"", workflowdir.c_str());
  XrdOucErrInfo lError;
  eos::common::Mapping::VirtualIdentity rootvid;
  eos::common::Mapping::Root(rootvid);
  // write a workflow file
  std::string workflowpath = workflowdir;
  workflowpath += mActions[0].mWhen;
  workflowpath += ":";
  workflowpath += entry;
  workflowpath += ":";
  workflowpath += mActions[0].mEvent;

  if (!gOFS->_rem(workflowpath.c_str(),
                  lError,
                  rootvid,
                  "",
                  false,
                  false,
                  true)) {
    return SFS_OK;
  } else {
    eos_static_err("msg=\"failed to delete job\" job=\"%s\"", mDescription.c_str());
    return SFS_ERROR;
  }
}

/*----------------------------------------------------------------------------*/
int
WFE::Job::DoIt(bool issync)
/*----------------------------------------------------------------------------*/
/**
 * @brief execute a workflow
 * @return
 *  */
/*----------------------------------------------------------------------------*/
{
  std::string method;
  std::string args;
  eos::common::Mapping::VirtualIdentity lRootVid;
  XrdOucErrInfo lError;
  eos::common::Mapping::Root(lRootVid);
  eos_static_info("queue=\"%s\"", mActions[0].mQueue.c_str());
  int retc = 0;
  time_t storetime = 0;

  if (mActions[0].mQueue == "s" || mActions[0].mQueue == "e") {
    if (eos::common::StringConversion::SplitKeyValue(mActions[0].mAction, method,
        args, ":")) {
      if (method == "mail") {
        std::string recipient;
        std::string freetext;

        if (!eos::common::StringConversion::SplitKeyValue(args, recipient, freetext,
            ":")) {
          recipient = args;
          freetext = "EOS workflow notification";
        }

        std::string topic = gOFS->MgmOfsInstanceName.c_str();
        topic += " ( ";
        topic += gOFS->HostName;
        topic += " ) ";
        topic += " ";
        topic += " event=";
        topic += mActions[0].mEvent;
        topic += " fxid=";
        XrdOucString hexid;
        eos::common::FileId::Fid2Hex(mFid, hexid);
        topic += hexid.c_str();
        std::string do_mail = "echo ";
        do_mail += "\"";
        do_mail += freetext;
        do_mail += "\"";
        do_mail += "| mail -s \"";
        do_mail += topic;
        do_mail += "\" ";
        do_mail += recipient;
        eos_static_info("shell-cmd=\"%s\"", do_mail.c_str());
        eos::common::ShellCmd cmd(do_mail);
        eos::common::cmd_status rc = cmd.wait(5);

        if (rc.exit_code) {
          eos_static_err("msg=\"failed to send workflow notification mail\" job=\"%s\"",
                         mDescription.c_str());
          storetime = 0;
          Move("s", "f", storetime);
          XrdOucString log = "failed to send workflow notification mail";
          Results("f", -1, log, storetime);
        } else {
          eos_static_info("msg=\"done notification\" job=\"%s\"",
                          mDescription.c_str());
          storetime = 0;
          Move("s", "d", storetime);
          XrdOucString log = "notified by email";
          Results("d", 0, log, storetime);
        }
      } else if (method == "bash") {
        std::string executable;
        std::string executableargs;

        if (!eos::common::StringConversion::SplitKeyValue(args, executable,
            executableargs, ":")) {
          executable = args;
          executableargs = "";
        }

        XrdOucString execargs = executableargs.c_str();
        std::string fullpath;
        bool format_error = false;

        if (executable.find('/') == std::string::npos) {
          std::shared_ptr<eos::IFileMD> fmd ;
          std::shared_ptr<eos::IContainerMD> cmd ;
          // do meta replacement
          gOFS->eosViewRWMutex.LockRead();

          try {
            fmd = gOFS->eosFileService->getFileMD(mFid);
            cmd = gOFS->eosDirectoryService->getContainerMD(fmd->getContainerId());
            fullpath = gOFS->eosView->getUri(fmd.get());
          } catch (eos::MDException& e) {
            eos_static_debug("caught exception %d %s\n", e.getErrno(),
                             e.getMessage().str().c_str());
          }

          if (fmd.get() && cmd.get()) {
            std::shared_ptr<eos::IFileMD> cfmd  = fmd;
            std::shared_ptr<eos::IContainerMD> ccmd = cmd;
            gOFS->eosViewRWMutex.UnLockRead();
            std::string cv;
            eos::IFileMD::ctime_t ctime;
            eos::IFileMD::ctime_t mtime;
            cfmd->getCTime(ctime);
            cfmd->getMTime(mtime);
            std::string checksum;
            size_t cxlen = eos::common::LayoutId::GetChecksumLen(cfmd->getLayoutId());

            for (unsigned int i = 0; i < cxlen; i++) {
              char hb[3];
              sprintf(hb, "%02x", (i < cxlen) ? (unsigned char)(
                        cfmd->getChecksum().getDataPadded(i)) : 0);
              checksum += hb;
            }

            // translate uid/gid to username/groupname
            std::string user_name;
            std::string group_name;
            int errc;
            errc = 0;
            user_name  = Mapping::UidToUserName(cfmd->getCUid(), errc);

            if (errc) {
              user_name = "nobody";
            }

            errc = 0;
            group_name = Mapping::GidToGroupName(cfmd->getCGid(), errc);

            if (errc) {
              group_name = "nobody";
            }

            XrdOucString unbase64;
            XrdOucString base64;
            unbase64 = fullpath.c_str();
            eos::common::SymKey::Base64(unbase64, base64);
            int cnt = 0;

            while (execargs.replace("<eos::wfe::path>", unbase64.c_str())) {
              if (++cnt > 16) {
                break;
              }
            }

            cnt = 0;

            while (execargs.replace("<eos::wfe::base64:path>", base64.c_str())) {
              if (++cnt > 16) {
                break;
              }
            }

            cnt = 0;

            while (execargs.replace("<eos::wfe::uid>",
                                    eos::common::StringConversion::GetSizeString(cv,
                                        (unsigned long long) cfmd->getCUid()))) {
              if (++cnt > 16) {
                break;
              }
            }

            cnt = 0;

            while (execargs.replace("<eos::wfe::gid>",
                                    eos::common::StringConversion::GetSizeString(cv,
                                        (unsigned long long) cfmd->getCGid()))) {
              if (++cnt > 16) {
                break;
              }
            }

            cnt = 0;

            while (execargs.replace("<eos::wfe::ruid>",
                                    eos::common::StringConversion::GetSizeString(cv,
                                        (unsigned long long) mVid.uid))) {
              if (++cnt > 16) {
                break;
              }
            }

            cnt = 0;

            while (execargs.replace("<eos::wfe::rgid>",
                                    eos::common::StringConversion::GetSizeString(cv,
                                        (unsigned long long) mVid.gid))) {
              if (++cnt > 16) {
                break;
              }
            }

            cnt = 0;

            while (execargs.replace("<eos::wfe::username>",
                                    user_name.c_str())) {
              if (++cnt > 16) {
                break;
              }
            }

            cnt = 0;

            while (execargs.replace("<eos::wfe::groupname>",
                                    group_name.c_str())) {
              if (++cnt > 16) {
                break;
              }
            }

            cnt = 0;

            while (execargs.replace("<eos::wfe::rusername>",
                                    mVid.uid_string.c_str())) {
              if (++cnt > 16) {
                break;
              }
            }

            cnt = 0;

            while (execargs.replace("<eos::wfe::rgroupname>",
                                    mVid.gid_string.c_str())) {
              if (++cnt > 16) {
                break;
              }
            }

            cnt = 0;

            while (execargs.replace("<eos::wfe::host>",
                                    mVid.host.c_str())) {
              if (++cnt > 16) {
                break;
              }
            }

            cnt = 0;

            while (execargs.replace("<eos::wfe::sec.app>",
                                    mVid.app.c_str())) {
              if (++cnt > 16) {
                break;
              }
            }

            cnt = 0;

            while (execargs.replace("<eos::wfe::sec.name>",
                                    mVid.name.c_str())) {
              if (++cnt > 16) {
                break;
              }
            }

            cnt = 0;

            while (execargs.replace("<eos::wfe::sec.prot>",
                                    mVid.prot.c_str())) {
              if (++cnt > 16) {
                break;
              }
            }

            cnt = 0;

            while (execargs.replace("<eos::wfe::sec.grps>",
                                    mVid.grps.c_str())) {
              if (++cnt > 16) {
                break;
              }
            }

            cnt = 0;

            while (execargs.replace("<eos::wfe::instance>",
                                    gOFS->MgmOfsInstanceName)) {
              if (++cnt > 16) {
                break;
              }
            }

            cnt = 0;

            while (execargs.replace("<eos::wfe::ctime.s>",
                                    eos::common::StringConversion::GetSizeString(cv,
                                        (unsigned long long) ctime.tv_sec))) {
              if (++cnt > 16) {
                break;
              }
            }

            cnt = 0;

            while (execargs.replace("<eos::wfe::mtime.s>",
                                    eos::common::StringConversion::GetSizeString(cv,
                                        (unsigned long long) ctime.tv_sec))) {
              if (++cnt > 16) {
                break;
              }
            }

            cnt = 0;

            while (execargs.replace("<eos::wfe::ctime.ns>",
                                    eos::common::StringConversion::GetSizeString(cv,
                                        (unsigned long long) ctime.tv_nsec))) {
              if (++cnt > 16) {
                break;
              }
            }

            cnt = 0;

            while (execargs.replace("<eos::wfe::mtime.ns>",
                                    eos::common::StringConversion::GetSizeString(cv,
                                        (unsigned long long) ctime.tv_nsec))) {
              if (++cnt > 16) {
                break;
              }
            }

            cnt = 0;

            while (execargs.replace("<eos::wfe::ctime>",
                                    eos::common::StringConversion::GetSizeString(cv,
                                        (unsigned long long) ctime.tv_sec))) {
              if (++cnt > 16) {
                break;
              }
            }

            cnt = 0;

            while (execargs.replace("<eos::wfe::mtime>",
                                    eos::common::StringConversion::GetSizeString(cv,
                                        (unsigned long long) ctime.tv_sec))) {
              if (++cnt > 16) {
                break;
              }
            }

            cnt = 0;

            while (execargs.replace("<eos::wfe::size>",
                                    eos::common::StringConversion::GetSizeString(cv,
                                        (unsigned long long) cfmd->getSize()))) {
              if (++cnt > 16) {
                break;
              }
            }

            cnt = 0;

            while (execargs.replace("<eos::wfe::cid>",
                                    eos::common::StringConversion::GetSizeString(cv,
                                        (unsigned long long) cfmd->getContainerId()))) {
              if (++cnt > 16) {
                break;
              }
            }

            cnt = 0;

            while (execargs.replace("<eos::wfe::fid>",
                                    eos::common::StringConversion::GetSizeString(cv, (unsigned long long) mFid))) {
              if (++cnt > 16) {
                break;
              }
            }

            XrdOucString hexfid;
            eos::common::FileId::Fid2Hex(mFid, hexfid);
            cnt = 0;

            while (execargs.replace("<eos::wfe::fxid>",
                                    hexfid)) {
              if (++cnt > 16) {
                break;
              }
            }

            std::string turl = "root://";
            turl += gOFS->MgmOfsAlias.c_str();
            turl += "/";
            turl += fullpath;
            turl += "?eos.lfn=fxid:";
            turl += hexfid.c_str();
            cnt = 0;

            while (execargs.replace("<eos::wfe::turl>",
                                    turl.c_str())) {
              if (++cnt > 16) {
                break;
              }
            }

            unbase64 = cfmd->getName().c_str();
            eos::common::SymKey::Base64(unbase64, base64);
            cnt = 0;

            while (execargs.replace("<eos::wfe::name>", unbase64.c_str())) {
              if (++cnt > 16) {
                break;
              }
            }

            cnt = 0;

            while (execargs.replace("<eos::wfe::base64:name>", base64.c_str())) {
              if (++cnt > 16) {
                break;
              }
            }

            unbase64 = cfmd->getLink().c_str();
            eos::common::SymKey::Base64(unbase64, base64);
            cnt = 0;

            while (execargs.replace("<eos::wfe::link>", unbase64.c_str())) {
              if (++cnt > 16) {
                break;
              }
            }

            cnt = 0;

            while (execargs.replace("<eos::wfe::base64:link>", base64.c_str())) {
              if (++cnt > 16) {
                break;
              }
            }

            cnt = 0;

            while (execargs.replace("<eos::wfe::checksum>", checksum.c_str())) {
              if (++cnt > 16) {
                break;
              }
            }

            cnt = 0;

            while (execargs.replace("<eos::wfe::checksumtype>",
                                    eos::common::LayoutId::GetChecksumString(cfmd->getLayoutId()))) {
              if (++cnt > 16) {
                break;
              }
            }

            cnt = 0;

            while (execargs.replace("<eos::wfe::event>", mActions[0].mEvent.c_str())) {
              if (++cnt > 16) {
                break;
              }
            }

            cnt = 0;

            while (execargs.replace("<eos::wfe::queue>", mActions[0].mQueue.c_str())) {
              if (++cnt > 16) {
                break;
              }
            }

            cnt = 0;

            while (execargs.replace("<eos::wfe::workflow>",
                                    mActions[0].mWorkflow.c_str())) {
              if (++cnt > 16) {
                break;
              }
            }

            cnt = 0;

            while (execargs.replace("<eos::wfe::vpath>", mWorkflowPath.c_str())) {
              if (++cnt > 16) {
                break;
              }
            }

            time_t now = time(NULL);
            cnt = 0;

            while (execargs.replace("<eos::wfe::now>",
                                    eos::common::StringConversion::GetSizeString(cv, (unsigned long long) now))) {
              if (++cnt > 16) {
                break;
              }
            }

            int xstart = 0;
            cnt = 0;

            while ((xstart = execargs.find("<eos::wfe::fxattr:")) != STR_NPOS) {
              if (++cnt > 256) {
                break;
              }

              int xend = execargs.find(">", xstart);

              if (xend == STR_NPOS) {
                format_error = true;
                break;
              } else {
                bool b64encode = false;
                std::string key;
                std::string value;
                key.assign(execargs.c_str() + xstart + 18, xend - xstart - 18);
                execargs.erase(xstart, xend + 1 - xstart);
                XrdOucString skey = key.c_str();

                if (skey.beginswith("base64:")) {
                  key.erase(0, 7);
                  b64encode = true;
                }

                if (gOFS->_attr_get(cfmd->getId(), key, value)) {
                  if (b64encode) {
                    unbase64 = value.c_str();
                    eos::common::SymKey::Base64(unbase64, base64);
                    value = base64.c_str();
                  }

                  if (xstart == execargs.length()) {
                    execargs += value.c_str();
                  } else {
                    execargs.insert(value.c_str(), xstart);
                  }
                } else {
                  execargs.insert("UNDEF", xstart);
                }
              }
            }

            xstart = 0;
            cnt = 0;

            while ((xstart = execargs.find("<eos::wfe::cxattr:")) != STR_NPOS) {
              if (++cnt > 256) {
                break;
              }

              int xend = execargs.find(">", xstart);

              if (xend == STR_NPOS) {
                format_error = true;
                break;
              } else {
                bool b64encode = false;
                std::string key;
                std::string value;
                key.assign(execargs.c_str() + xstart + 18, xend - xstart - 18);
                execargs.erase(xstart, xend + 1 - xstart);
                XrdOucString skey = key.c_str();

                if (skey.beginswith("base64:")) {
                  key.erase(0, 7);
                  b64encode = true;
                }

                if (gOFS->_attr_get(ccmd->getId(), key, value)) {
                  if (b64encode) {
                    unbase64 = value.c_str();
                    eos::common::SymKey::Base64(unbase64, base64);
                    value = base64.c_str();
                  }

                  if (xstart == execargs.length()) {
                    execargs += value.c_str();
                  } else {
                    execargs.insert(value.c_str(), xstart);
                  }
                } else {
                  execargs.insert("UNDEF", xstart);
                }
              }
            }

            if (execargs.find("<eos::wfe::base64:metadata>") != STR_NPOS) {
              XrdOucString out = "";
              XrdOucString err = "";
              // ---------------------------------------------------------------------------------
              // run file info to get file md
              // ---------------------------------------------------------------------------------
              XrdOucString file_metadata;
              ProcCommand Cmd;
              XrdOucString info;
              info = "mgm.cmd=fileinfo&mgm.path=fid:";
              info += eos::common::StringConversion::GetSizeString(cv,
                      (unsigned long long) mFid);
              info += "&mgm.file.info.option=-m";
              Cmd.open("/proc/user", info.c_str(), lRootVid, &lError);
              Cmd.AddOutput(out, err);
              Cmd.close();
              file_metadata = out;

              if (err.length()) {
                eos_static_err("msg=\"file info returned error\" err=\"%s\"", err.c_str());
              }

              cnt = 0;

              while (file_metadata.replace("\"", "'")) {
                if (++cnt > 16) {
                  break;
                }
              }

              out = err = "";
              // ---------------------------------------------------------------------------------
              // run container info to get container md
              // ---------------------------------------------------------------------------------
              XrdOucString container_metadata;
              info = "mgm.cmd=fileinfo&mgm.path=pid:";
              info += eos::common::StringConversion::GetSizeString(cv,
                      (unsigned long long) cfmd->getContainerId());
              info += "&mgm.file.info.option=-m";
              Cmd.open("/proc/user", info.c_str(), lRootVid, &lError);
              Cmd.AddOutput(out, err);
              Cmd.close();
              container_metadata = out;

              if (err.length()) {
                eos_static_err("msg=\"container info returned error\" err=\"%s\"", err.c_str());
              }

              cnt = 0;

              while (container_metadata.replace("\"", "'")) {
                if (++cnt > 16) {
                  break;
                }
              }

              std::string metadata = "\"fmd={ ";
              metadata += file_metadata.c_str();
              metadata += "} dmd={ ";
              metadata += container_metadata.c_str();
              metadata += "}\"";
              unbase64 = metadata.c_str();
              eos::common::SymKey::Base64(unbase64, base64);
              execargs.replace("<eos::wfe::base64:metadata>", base64.c_str());
            }

            execargs.replace("<eos::wfe::action>", mActions[0].mAction.c_str());
            std::string bashcmd = EOS_WFE_BASH_PREFIX + executable + " " + execargs.c_str();

            if (!format_error) {
              eos::common::ShellCmd cmd(bashcmd);
              storetime = (time_t) mActions[0].mTime;
              Move(mActions[0].mQueue, "r", storetime, mRetry);
              eos_static_info("shell-cmd=\"%s\"", bashcmd.c_str());
              eos::common::cmd_status rc = cmd.wait(1800);
              // retrieve the stderr of this command
              XrdOucString outerr;
              char buff[65536];
              int end;
              memset(buff, 0, sizeof(buff));

              while ((end = ::read(cmd.errfd, buff, sizeof(buff))) > 0) {
                outerr += buff;
                memset(buff, 0, sizeof(buff));
              }

              eos_static_info("shell-cmd-stderr=%s", outerr.c_str());
              // scan for result tags referencing the trigger path
              xstart = 0;
              cnt = 0;

              while ((xstart = outerr.find("<eos::wfe::path::fxattr:", xstart)) != STR_NPOS) {
                if (++cnt > 256) {
                  break;
                }

                int xend = outerr.find(">", xstart);

                if (xend == STR_NPOS) {
                  eos_static_err("malformed shell stderr tag");
                  break;
                } else {
                  std::string key;
                  std::string value;
                  key.assign(outerr.c_str() + xstart + 24, xend - xstart - 24);
                  int vend = outerr.find(" ", xend + 1);

                  if (vend > 0) {
                    value.assign(outerr.c_str(), xend + 1, vend - (xend + 1));
                  } else {
                    value.assign(outerr.c_str(), xend + 1, string::npos);
                  }

                  // remove a possible line feed from the value
                  while (value.length() && (value[value.length() - 1] == '\n')) {
                    value.erase(value.length() - 1);
                  }

                  eos::common::RWMutexWriteLock nsLock(gOFS->eosViewRWMutex);

                  try {
                    fmd = gOFS->eosFileService->getFileMD(mFid);
                    base64 = value.c_str();
                    eos::common::SymKey::DeBase64(base64, unbase64);
                    fmd->setAttribute(key, unbase64.c_str());
                    fmd->setMTimeNow();
                    gOFS->eosView->updateFileStore(fmd.get());
                    errno = 0;
                    eos_static_info("msg=\"stored extended attribute\" key=%s value=%s",
                                    key.c_str(), value.c_str());
                  } catch (eos::MDException& e) {
                    eos_static_err("msg=\"failed set extended attribute\" key=%s value=%s",
                                   key.c_str(), value.c_str());
                  }
                }

                xstart++;
              }

              retc = rc.exit_code;

              if (rc.exit_code) {
                eos_static_err("msg=\"failed to run bash workflow\" job=\"%s\" retc=%d",
                               mDescription.c_str(), rc.exit_code);
                int retry = 0;
                time_t delay = 0;

                if (rc.exit_code == EAGAIN) {
                  try {
                    std::string retryattr = "sys.workflow." + mActions[0].mEvent + "." +
                                            mActions[0].mWorkflow + ".retry.max";
                    std::string delayattr = "sys.workflow." + mActions[0].mEvent + "." +
                                            mActions[0].mWorkflow + ".retry.delay";
                    eos_static_info("%s %s", retryattr.c_str(), delayattr.c_str());
                    std::string value = ccmd->getAttribute(retryattr);
                    retry = (int)strtoul(value.c_str(), 0, 10);
                    value = ccmd->getAttribute(delayattr);
                    delay = (int)strtoul(value.c_str(), 0, 10);
                  } catch (eos::MDException& e) {
                    execargs.insert("UNDEF", xstart);
                  }

                  if (!IsSync() && (mRetry < retry)) {
                    storetime = (time_t) mActions[0].mTime + delay;
                    // can retry
                    Move("r", "e", storetime, ++mRetry);
                    XrdOucString log = "scheduled for retry";
                    Results("e", EAGAIN , log, storetime);
                  } else {
                    storetime = (time_t) mActions[0].mTime;
                    // can not retry
                    Move("r", "f", storetime, mRetry);
                    XrdOucString log = "workflow failed without possibility to retry";
                    Results("f", rc.exit_code , log, storetime);
                  }
                } else {
                  storetime = 0;
                  // can not retry
                  Move("r", "f", storetime);
                  XrdOucString log = "workflow failed without possibility to retry";
                  Results("f", rc.exit_code , log, storetime);
                }
              } else {
                eos_static_info("msg=\"done bash workflow\" job=\"%s\"",
                                mDescription.c_str());
                storetime = 0;
                Move("r", "d", storetime);
                XrdOucString log = "workflow succeeded";
                Results("d", rc.exit_code , log, storetime);
              }

              // scan for result tags referencing the workflow path
              xstart = 0;
              cnt = 0;

              while ((xstart = outerr.find("<eos::wfe::vpath::fxattr:",
                                           xstart)) != STR_NPOS) {
                if (++cnt > 256) {
                  break;
                }

                int xend = outerr.find(">", xstart);

                if (xend == STR_NPOS) {
                  eos_static_err("malformed shell stderr tag");
                  break;
                } else {
                  std::string key;
                  std::string value;
                  key.assign(outerr.c_str() + xstart + 25, xend - xstart - 25);
                  int vend = outerr.find(" ", xend + 1);

                  if (vend > 0) {
                    value.assign(outerr.c_str(), xend + 1, vend - (xend + 1));
                  } else {
                    value.assign(outerr.c_str(), xend + 1, string::npos);
                  }

                  eos::common::RWMutexWriteLock nsLock(gOFS->eosViewRWMutex);

                  try {
                    fmd = gOFS->eosView->getFile(mWorkflowPath);
                    base64 = value.c_str();
                    eos::common::SymKey::DeBase64(base64, unbase64);
                    fmd->setAttribute(key, unbase64.c_str());
                    fmd->setMTimeNow();
                    gOFS->eosView->updateFileStore(fmd.get());
                    errno = 0;
                    eos_static_info("msg=\"stored extended attribute on vpath\" vpath=%s key=%s value=%s",
                                    mWorkflowPath.c_str(), key.c_str(), value.c_str());
                  } catch (eos::MDException& e) {
                    eos_static_err("msg=\"failed set extended attribute\" key=%s value=%s",
                                   key.c_str(), value.c_str());
                  }
                }

                xstart++;
              }
            } else {
              retc = EINVAL;
              storetime = 0;
              // cannot retry
              Move(mActions[0].mQueue, "f", storetime);
              XrdOucString log = "workflow failed to invalid arguments";
              Results("f", retc , log, storetime);
            }
          } else {
            storetime = 0;
            retc = EINVAL;
            gOFS->eosViewRWMutex.UnLockRead();
            eos_static_err("msg=\"failed to run bash workflow - file gone\" job=\"%s\"",
                           mDescription.c_str());
            Move(mActions[0].mQueue, "g", storetime);
            XrdOucString log = "workflow failed to invalid arguments - file is gone";
            Results("g", retc , log, storetime);
          }
        } else {
          storetime = 0;
          retc = EINVAL;
          eos_static_err("msg=\"failed to run bash workflow - executable name modifies path\" job=\"%s\"",
                         mDescription.c_str());
          Move(mActions[0].mQueue, "g", storetime);
        }
      } else if (method == "proto") {
        auto event = mActions[0].mEvent;
        auto params = eos::common::StringTokenizer::split<std::vector<std::string>>
                      (args, ' ');
        auto hostPort = params[0];
        auto endPoint = std::string{"/"} + params[1];

        storetime = (time_t) mActions[0].mTime;
        Move(mActions[0].mQueue, "r", storetime, mRetry);

        std::shared_ptr<eos::IFileMD> fmd;
        std::shared_ptr<eos::IContainerMD> cmd;
        std::string fullPath;
        {
          eos::common::RWMutexReadLock rlock(gOFS->eosViewRWMutex);

          try {
            fmd = gOFS->eosFileService->getFileMD(mFid);
            fullPath = gOFS->eosView->getUri(fmd.get());
            cmd = gOFS->eosDirectoryService->getContainerMD(fmd->getContainerId());
          } catch (eos::MDException& e) {
            eos_static_err("Could not get metadata for file %u. Reason: %s", mFid,
                           e.getMessage().str().c_str());
            MoveWithResults(ENOENT);
            return ENOENT;
          }
        }
        auto eventUpperCase = event;
        std::transform(eventUpperCase.begin(), eventUpperCase.end(),
                       eventUpperCase.begin(),
                       [](unsigned char c) {
                         return std::toupper(c);
                       }
        );
        eos_static_info("%s %s %s", eventUpperCase.c_str(), fullPath.c_str(),
                        hostPort.c_str());
        cta::xrd::Request request;
        auto notification = request.mutable_notification();
        int errc = 0;
        auto user_name  = Mapping::UidToUserName(mVid.uid, errc);

        if (errc) {
          user_name = "nobody";
        }

        errc = 0;
        auto group_name = Mapping::GidToGroupName(mVid.gid, errc);

        if (errc) {
          group_name = "nobody";
        }

        auto collectAttributes = [&notification, &fmd, &cmd] {
          for (const auto& fileAttrPair : fmd->getAttributes())
          {
            if (fileAttrPair.first.find("sys.") == 0 ||
            fileAttrPair.first.find("user.") == 0) {
              continue;
            }

            google::protobuf::MapPair<std::string, std::string> attr(fileAttrPair.first,
                fileAttrPair.second);
            notification->mutable_file()->mutable_xattr()->insert(attr);
          }

          for (const auto& dirAttrPair : cmd->getAttributes())
          {
            if (dirAttrPair.first.find("sys.") == 0 ||
                dirAttrPair.first.find("user.") == 0) {
              continue;
            }

            google::protobuf::MapPair<std::string, std::string> attr(dirAttrPair.first,
                dirAttrPair.second);
            notification->mutable_file()->mutable_xattr()->insert(attr);
          }
        };

        notification->mutable_cli()->mutable_user()->set_username(user_name);
        notification->mutable_cli()->mutable_user()->set_groupname(group_name);

        if (event == "sync::delete") {
          collectAttributes();
          notification->mutable_wf()->set_event(cta::eos::Workflow::DELETE);
        } else if (event == "sync::prepare") {
          struct stat buf;
          XrdOucErrInfo errInfo;

          // Check if we have a disk replica and if not, whether it's on tape
          if (gOFS->_stat(fullPath.c_str(), &buf, errInfo, mVid, nullptr, nullptr,
                          false) == 0) {
            auto onDisk = ((buf.st_mode & EOS_TAPE_MODE_T) ? buf.st_nlink - 1 :
                           buf.st_nlink) > 0;
            auto onTape = buf.st_mode & EOS_TAPE_MODE_T;

            if (onDisk) {
              eos_static_info("File %s is already on disk, nothing to prepare.",
                              fullPath.c_str());
              MoveWithResults(SFS_OK);
              return SFS_OK;
            } else if (!onTape) {
              eos_static_err("File %s is not on disk nor on tape, cannot prepare it.",
                             fullPath.c_str());
              MoveWithResults(EINVAL);
              return EINVAL;
            } else {
              collectAttributes();
              notification->mutable_wf()->set_event(cta::eos::Workflow::PREPARE);
              notification->mutable_file()->set_lpath(fullPath);
              notification->mutable_file()->mutable_owner()->set_uid(fmd->getCUid());
              notification->mutable_file()->mutable_owner()->set_gid(fmd->getCGid());
              notification->mutable_file()->mutable_owner()->set_username(user_name);
              notification->mutable_file()->mutable_owner()->set_groupname(group_name);
              char buffer[1024];
              StringConversion::FastUnsignedToAsciiHex(mFid, buffer);
              std::ostringstream destStream;
              destStream << "root://" << gOFS->HostName << "/" << fullPath << "?eos.lfn=fxid:"
                         << std::string{buffer};
              destStream <<
                         "&eos.ruid=0&eos.rgid=0&eos.injection=1&eos.workflow=CTA_retrieve";
              notification->mutable_transport()->set_dst_url(destStream.str());
            }
          } else {
            eos_static_err("Cannot determine file and disk replicas, not doing the prepare. Reason: %s",
                           errInfo.getErrText());
            return EAGAIN;
          }
        }
        else if (event == "sync::openw") {
          notification->mutable_wf()->set_event(cta::eos::Workflow::OPENW);
          notification->mutable_wf()->mutable_instance()->set_name(gOFS->MgmOfsInstanceName.c_str());
          notification->mutable_file()->set_lpath(fullPath);
          notification->mutable_file()->mutable_owner()->set_uid(fmd->getCUid());
          notification->mutable_file()->mutable_owner()->set_gid(fmd->getCGid());
          notification->mutable_file()->mutable_owner()->set_username(user_name);
          notification->mutable_file()->mutable_owner()->set_groupname(group_name);
          notification->mutable_file()->set_fid(mFid);

          collectAttributes();
        } else if (event == "closew") {
          notification->mutable_wf()->set_event(cta::eos::Workflow::CLOSEW);
          notification->mutable_wf()->mutable_instance()->set_name(
            gOFS->MgmOfsInstanceName.c_str());
          notification->mutable_file()->set_lpath(fullPath);
          notification->mutable_file()->mutable_owner()->set_uid(fmd->getCUid());
          notification->mutable_file()->mutable_owner()->set_gid(fmd->getCGid());
          notification->mutable_file()->mutable_owner()->set_username(user_name);
          notification->mutable_file()->mutable_owner()->set_groupname(group_name);
          notification->mutable_file()->mutable_cks()->set_type(
            eos::common::LayoutId::GetChecksumString(fmd->getLayoutId()));
          std::ostringstream checksum;

          for (auto i = 0u; i < eos::common::LayoutId::GetChecksumLen(fmd->getLayoutId());
               i++) {
            char hb[4];
            sprintf(hb, "%02x", (unsigned char)(fmd->getChecksum().getDataPadded(i)));
            checksum << hb;
          }

          notification->mutable_file()->mutable_cks()->set_value(checksum.str());
          notification->mutable_file()->set_size(fmd->getSize());
          notification->mutable_file()->set_fid(mFid);
          auto fxidString = StringConversion::FastUnsignedToAsciiHex(mFid);
          std::ostringstream srcStream;
          srcStream << "root://" << gOFS->HostName << "/" << fullPath << "?eos.lfn=fxid:"
                    << fxidString;
          notification->mutable_wf()->mutable_instance()->set_url(srcStream.str());
          std::ostringstream reportStream;
          reportStream << "eosQuery://" << gOFS->HostName <<
                       "//eos/wfe/passwd?mgm.pcmd=event&mgm.fid=" << fxidString;
          reportStream <<
                       "&mgm.logid=cta&mgm.event=archived&mgm.workflow=default&mgm.path=/eos/wfe/passwd&mgm.ruid=0&mgm.rgid=0";
          notification->mutable_transport()->set_report_url(reportStream.str());
          collectAttributes();
        } else if (event == "archived") {
          static constexpr auto tapeFsid = 65535u;

          if (fmd->hasLocation(tapeFsid) && fmd->getLocations().size() == 1) {
            eos_static_info("File %s already has a tape copy. Ignoring request.",
                            fullPath.c_str());
          } else {
            XrdOucErrInfo errInfo;
            eos::common::Mapping::VirtualIdentity root_vid;
            eos::common::Mapping::Root(root_vid);

            bool dropFailed = false;
            for (auto location : fmd->getLocations()) {
              if (location != tapeFsid) {
                errInfo.clear();

                if (gOFS->_dropstripe(fullPath.c_str(), errInfo, root_vid, location,
                                      true) != 0) {
                  eos_static_err("Could not delete file replica %s on filesystem %u. Reason: %s",
                                 fullPath.c_str(), location, errInfo.getErrText());
                  dropFailed = true;
                }
              }
            }

            if (!fmd->hasLocation(tapeFsid)) {
              eos::common::RWMutexWriteLock lock(gOFS->eosViewRWMutex);
              fmd->addLocation(tapeFsid);
              gOFS->eosView->updateFileStore(fmd.get());
            }

            if (dropFailed) {
              MoveToRetry(cmd);
              return EAGAIN;
            }
          }

          MoveWithResults(SFS_OK);
          return SFS_OK;
        }

        static XrdCl::Env* xrootEnv = nullptr;
        xrootEnv = xrootEnv == nullptr ? XrdCl::DefaultEnv::GetEnv() : xrootEnv;
        static int requestTimeout = 0;

        if (xrootEnv != nullptr) {
          xrootEnv->GetInt("RequestTimeout", requestTimeout);
        }

        static int timeoutResolution = 0;

        if (xrootEnv != nullptr) {
          xrootEnv->GetInt("TimeoutResolution", timeoutResolution);
        }

        static int streamTimeout = 0;

        if (xrootEnv != nullptr) {
          xrootEnv->GetInt("StreamTimeout", streamTimeout);
        }

        eos_static_debug("XRD_TIMEOUTRESOLUTION=%d XRD_REQUESTTIMEOUT=%d XRD_STREAMTIMEOUT=%d",
                         timeoutResolution, requestTimeout, streamTimeout);
        eos_static_debug("Request sent to outside service:\n%s",
                         notification->DebugString().c_str());
        XrdSsiPbServiceType service(hostPort, endPoint);
        cta::xrd::Response response;

        try {
          auto future = service.Send(request, response);
          future.get();
        } catch (std::runtime_error& error) {
          eos_static_err("Could not send request to outside service. Reason: %s",
                         error.what());
          MoveWithResults(SFS_ERROR);
          return SFS_ERROR;
        }

        switch (response.type()) {
        case cta::xrd::Response::RSP_SUCCESS: {
          retc = SFS_OK;
          eos_static_debug("Response received from outside service:\n%s",
                           response.DebugString().c_str());
          // Set all attributes for file from response
          eos::common::Mapping::VirtualIdentity rootvid;
          eos::common::Mapping::Root(rootvid);
          XrdOucErrInfo errInfo;

          for (const auto& attrPair : response.xattr()) {
            errInfo.clear();

            if (gOFS->_attr_set(fullPath.c_str(), errInfo, rootvid,
                                nullptr, attrPair.first.c_str(), attrPair.second.c_str()) != 0) {
              eos_static_err("Could not set attribute %s with value %s for file %s. Reason: %s",
                             attrPair.first.c_str(), attrPair.second.c_str(), fullPath.c_str(),
                             errInfo.getErrText());
            }
          }

          break;
        }

        case cta::xrd::Response::RSP_ERR_CTA:
          retc = EINVAL;
          eos_static_err("RSP_ERR_CTA %s", response.message_txt().c_str());
          break;

        case cta::xrd::Response::RSP_ERR_USER:
          retc = EINVAL;
          eos_static_err("RSP_ERR_USER %s", response.message_txt().c_str());
          break;

        case cta::xrd::Response::RSP_ERR_PROTOBUF:
          retc = EINVAL;
          eos_static_err("RSP_ERR_PROTOBUF %s", response.message_txt().c_str());
          break;

        case cta::xrd::Response::RSP_INVALID:
          retc = EINVAL;
          eos_static_err("RSP_INVALID %s", response.message_txt().c_str());
          break;

        default:
          retc = EINVAL;
          eos_static_err("Response:\n%s", response.DebugString().c_str());
        }

        MoveWithResults(retc);
      } else {
        storetime = 0;
        eos_static_err("msg=\"moving unknown workflow\" job=\"%s\"",
                       mDescription.c_str());
        Move(mActions[0].mQueue, "g", storetime);
        XrdOucString log = "workflow is not known";
        Results("g", EINVAL , log, storetime);
      }
    } else {
      storetime = 0;
      retc = EINVAL;
      eos_static_err("msg=\"moving illegal workflow\" job=\"%s\"",
                     mDescription.c_str());
      Move(mActions[0].mQueue, "g", storetime);
      XrdOucString log = "workflow illegal";
      Results("g", retc , log, storetime);
    }
  } else {
    //Delete(mActions[0].mQueue);
  }

  if (!IsSync()) {
    gOFS->WFEd.GetSignal()->Signal();
    gOFS->WFEd.DecActiveJobs();
  }

  return retc;
}

void
WFE::Job::MoveToRetry(std::shared_ptr<eos::IContainerMD>& ccmd) {
  int retry = 0, delay = 0;
  try {
    std::string retryattr = "sys.workflow." + mActions[0].mEvent + "." +
                            mActions[0].mWorkflow + ".retry.max";
    std::string delayattr = "sys.workflow." + mActions[0].mEvent + "." +
                            mActions[0].mWorkflow + ".retry.delay";
    eos_static_info("%s %s", retryattr.c_str(), delayattr.c_str());
    retry = std::stoi(ccmd->getAttribute(retryattr));
    delay = std::stoi(ccmd->getAttribute(delayattr));
  } catch (eos::MDException& e) {
    MoveWithResults(SFS_ERROR);
  }

  if (!IsSync() && (mRetry < retry)) {
    time_t storetime = (time_t) mActions[0].mTime + delay;
    // can retry
    Move("r", "e", storetime, ++mRetry);
    Results("e", EAGAIN , "scheduled for retry", storetime);
  }
  else {
    MoveWithResults(SFS_ERROR);
  }
}

void
WFE::Job::MoveWithResults(int rcode) {
  time_t storetime = 0;
  if (rcode == 0) {
    Move("r", "d", storetime);
    Results("d", rcode , "moved to done", storetime);
  }
  else {
    Move("r", "f", storetime);
    Results("f", rcode , "moved to failed", storetime);
  }
}

/*----------------------------------------------------------------------------*/
void
WFE::PublishActiveJobs()
/*----------------------------------------------------------------------------*/
/**
 * @brief publish the active job number in the space view
 *
 */
/*----------------------------------------------------------------------------*/
{
  eos::common::RWMutexReadLock lock(FsView::gFsView.ViewMutex);
  char sactive[256];
  snprintf(sactive, sizeof(sactive) - 1, "%lu", GetActiveJobs());
  FsView::gFsView.mSpaceView["default"]->SetConfigMember
  ("stat.wfe.active",
   sactive,
   true,
   "/eos/*/mgm",
   true);
}

EOSMGMNAMESPACE_END
