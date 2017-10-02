// ----------------------------------------------------------------------
// File: com_fusex.cc
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

/*----------------------------------------------------------------------------*/
#include "console/ConsoleMain.hh"
/*----------------------------------------------------------------------------*/

/* fusex Clients -  Interface */
int
com_fusex(char* arg1)
{
  eos::common::StringTokenizer subtokenizer(arg1);
  subtokenizer.GetLine();
  XrdOucString option = "";
  XrdOucString options = "";
  XrdOucString in = "";
  XrdOucString subcmd = subtokenizer.GetToken();
  XrdOucString filter;

  if (wants_help(arg1)) {
    goto com_fusex_usage;
  }

  in = "mgm.cmd=fusex";

  if (subcmd == "ls") {
    in += "&mgm.subcmd=ls";
  } else if (subcmd == "evict") {
    XrdOucString uuid = subtokenizer.GetToken();
    XrdOucString reason = subtokenizer.GetToken();

    if (!uuid.length()) {
      goto com_fusex_usage;
    }

    in += "&mgm.subcmd=evict";
    in += "&mgm.fusex.uuid=";
    in += uuid;

    if (reason.length()) {
      XrdOucString b64;
      eos::common::SymKey::Base64(reason, b64);
      in += "&mgm.fusex.reason=";
      in += b64;
    }
  } else if (subcmd == "caps") {
    option = subtokenizer.GetToken();
    filter = subtokenizer.GetToken();

    while (option.replace("-", "")) {
    };

    in += "&mgm.subcmd=caps";

    in += "&mgm.option=";

    in += option;

    while (auto val = subtokenizer.GetToken()) {
      filter += " ";
      filter += val;
    }

    if (filter.length()) {
      in += "&mgm.filter=";
      in += eos::common::StringConversion::curl_escaped(filter.c_str()).c_str();
    }
  } else if (subcmd == "dropcaps") {
    XrdOucString uuid = subtokenizer.GetToken();

    if (!uuid.length()) {
      goto com_fusex_usage;
    }

    in += "&mgm.subcmd=dropcaps";
    in += "&mgm.fusex.uuid=";
    in += uuid;
  } else if (subcmd == "droplocks") {
    XrdOucString inode = subtokenizer.GetToken();
    XrdOucString pid = subtokenizer.GetToken();

    if (!inode.length() || !pid.length()) {
      goto com_fusex_usage;
    }

    in += "&mgm.subcmd=droplocks";
    in += "&mgm.inode=";
    in += inode;
    in += "&mgm.fusex.pid=";
    in += pid;
  } else {
    goto com_fusex_usage;
  }

  do {
    option = subtokenizer.GetToken();

    if (!option.length()) {
      break;
    }

    if (option == "-c") {
      options += "c";
    } else {
      if (option == "-a") {
        options += "a";
      } else {
        if (option == "-m") {
          options += "m";
        } else {
          if (option == "-s") {
            options += "s";
          } else {
            if (option == "-f") {
              options += "f";
            } else {
              goto com_fusex_usage;
            }
          }
        }
      }
    }
  } while (1);

  if (options.length()) {
    in += "&mgm.option=";
    in += options;
  }

  global_retc = output_result(client_command(in, true));
  return (0);
com_fusex_usage:
  fprintf(stdout,
          "usage: fusex ls [-c] [-f]                         :  print statistics about eosxd fuse clients\n");
  fprintf(stdout,
          "                -c                                                   -  break down by client host\n");
  fprintf(stdout,
          "                -f                                                   -  show ongoing flush locks\n");
  fprintf(stdout, "\n");
  fprintf(stdout,
          "       fuxex evict <uuid> [<reason>]                                 :  evict a fuse client\n");
  fprintf(stdout,
          "                                                              <uuid> -  uuid of the client to evict\n");
  fprintf(stdout,
          "                                                            <reason> -  optional text shown to the client why he has been evicted\n");
  fprintf(stdout, "\n");
  fprintf(stdout,
          "       fusex dropcaps <uuid>                                         :  advice a client to drop all caps\n");
  fprintf(stdout, "\n");
  fprintf(stdout,
          "       fusex droplocks <inode> <pid>                                 :  advice a client to drop for a given (hexadecimal) inode and process id\n");
  fprintf(stdout, "\n");
  fprintf(stdout,
          "       fusex caps [-t | -i | -p [<regexp>] ]                         :  print caps\n");
  fprintf(stdout,
          "                -t                                                   -  sort by expiration time\n");
  fprintf(stdout,
          "                -i                                                   -  sort by inode\n");
  fprintf(stdout,
          "                -p                                                   -  display by path\n");
  fprintf(stdout,
          "                -t|i|p <regexp>>                                     -  display entries matching <regexp> for the used filter type");
  fprintf(stdout, "\n");
  fprintf(stdout, "examples:\n");
  fprintf(stdout,
          "           fusex caps -i ^0000abcd$                                  :  show caps for inode 0000abcd\n");
  fprintf(stdout,
          "           fusex caps -p ^/eos/$                                     :  show caps for path /eos\n");
  fprintf(stdout,
          "           fusex caps -p ^/eos/caps/                                 :  show all caps in subtree /eos/caps\n");
  return (0);
}