// ----------------------------------------------------------------------
// File: MimeTypes.hh
// Author: Andreas-Joachim Peters CERN
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

/**
 * @file   MimeTypes.hh
 *
 * @brief  class deriving mime types from suffixes
 */

#ifndef __EOSCOMMON_MIMETYPES__HH__
#define __EOSCOMMON_MIMETYPES__HH__

/*----------------------------------------------------------------------------*/
#include "common/Namespace.hh"
#include "common/StringConversion.hh"
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
#include <map>
#include <string>

/*----------------------------------------------------------------------------*/

EOSCOMMONNAMESPACE_BEGIN

class MimeTypes {
private:
  std::map<std::string, std::string> mTypes;

public:

  std::string
  MimeType (const char* suffix)
  {
    if (mTypes.count(suffix))
      return mTypes[suffix];
    else
      return "application/octet-stream";
  }

  std::string
  Match (const std::string path)
  {
    std::string suffix = path;
    if (path.rfind(".") != std::string::npos)
      suffix.erase(0,path.rfind("."));
    std::string lower_case_suffix = LC_STRING(suffix);
    return MimeType(lower_case_suffix.c_str());
  }

  MimeTypes ()
  {
    mTypes[".3dm"] = "x-world/x-3dmf";
    mTypes[".3dmf"] = "x-world/x-3dmf";
    mTypes[".a"] = "application/octet-stream";
    mTypes[".aab"] = "application/x-authorware-bin";
    mTypes[".aam"] = "application/x-authorware-map";
    mTypes[".aas"] = "application/x-authorware-seg";
    mTypes[".abc"] = "text/vnd.abc";
    mTypes[".acgi"] = "text/html";
    mTypes[".afl"] = "video/animaflex";
    mTypes[".ai"] = "application/postscript";
    mTypes[".aif"] = "audio/aiff";
    mTypes[".aif"] = "audio/x-aiff";
    mTypes[".aifc"] = "audio/aiff";
    mTypes[".aiff"] = "audio/aiff";
    mTypes[".aim"] = "application/x-aim";
    mTypes[".aip"] = "text/x-audiosoft-intra";
    mTypes[".ani"] = "application/x-navi-animation";
    mTypes[".aos"] = "application/x-nokia-9000-communicator-add-on-software";
    mTypes[".aps"] = "application/mime";
    mTypes[".arc"] = "application/octet-stream";
    mTypes[".arj"] = "application/arj";
    mTypes[".art"] = "image/x-jg";
    mTypes[".asf"] = "video/x-ms-asf";
    mTypes[".asm"] = "text/x-asm";
    mTypes[".asp"] = "text/asp";
    mTypes[".asx"] = "application/x-mplayer2";
    mTypes[".au"] = "audio/basic";
    mTypes[".avi"] = "video/avi";
    mTypes[".bcpio"] = "application/x-bcpio";
    mTypes[".bin"] = "application/octet-stream";
    mTypes[".bm"] = "image/bmp";
    mTypes[".bmp"] = "image/bmp";
    mTypes[".boo"] = "application/book";
    mTypes[".book"] = "application/book";
    mTypes[".boz"] = "application/x-bzip2";
    mTypes[".bsh"] = "application/x-bsh";
    mTypes[".bz"] = "application/x-bzip";
    mTypes[".bz2"] = "application/x-bzip2";
    mTypes[".c"] = "text/plain";
    mTypes[".c++"] = "text/plain";
    mTypes[".cat"] = "application/vnd.ms-pki.seccat";
    mTypes[".cc"] = "text/plain";
    mTypes[".ccad"] = "application/clariscad";
    mTypes[".cco"] = "application/x-cocoa";
    mTypes[".cdf"] = "application/cdf";
    mTypes[".cer"] = "application/pkix-cert";
    mTypes[".cha"] = "application/x-chat";
    mTypes[".chat"] = "application/x-chat";
    mTypes[".class"] = "application/java";
    mTypes[".com"] = "application/octet-stream";
    mTypes[".conf"] = "text/plain";
    mTypes[".cpio"] = "application/x-cpio";
    mTypes[".cpp"] = "text/x-c";
    mTypes[".cpt"] = "application/x-cpt";
    mTypes[".crl"] = "application/pkcs-crl";
    mTypes[".crt"] = "application/pkix-cert";
    mTypes[".csh"] = "application/x-csh";
    mTypes[".css"] = "text/css";
    mTypes[".cxx"] = "text/plain";
    mTypes[".dcr"] = "application/x-director";
    mTypes[".deepv"] = "application/x-deepv";
    mTypes[".def"] = "text/plain";
    mTypes[".dif"] = "video/x-dv";
    mTypes[".dir"] = "application/x-director";
    mTypes[".dl"] = "video/dl";
    mTypes[".doc"] = "application/msword";
    mTypes[".docm"] = "application/vnd.ms-word.document.macroEnabled.12";
    mTypes[".docx"] = "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    mTypes[".dotm"] = "application/vnd.ms-word.templatet.macroEnabled.12";
    mTypes[".dotx"] = "application/vnd.openxmlformats-officedocument.wordprocessingml.template";
    mTypes[".dot"] = "application/msword";
    mTypes[".dp"] = "application/commonground";
    mTypes[".drw"] = "application/drafting";
    mTypes[".dump"] = "application/octet-stream";
    mTypes[".dv"] = "video/x-dv";
    mTypes[".dvi"] = "application/x-dvi";
    mTypes[".dwf"] = "drawing/x-dwf";
    mTypes[".dwf"] = "model/vnd.dwf";
    mTypes[".dwg"] = "application/acad";
    mTypes[".dxf"] = "application/dxf";
    mTypes[".dxr"] = "application/x-director";
    mTypes[".el"] = "text/x-script.elisp";
    mTypes[".elc"] = "application/x-bytecode.elisp";
    mTypes[".env"] = "application/x-envoy";
    mTypes[".eps"] = "application/postscript";
    mTypes[".es"] = "application/x-esrehber";
    mTypes[".etx"] = "text/x-setext";
    mTypes[".evy"] = "application/envoy";
    mTypes[".evy"] = "application/x-envoy";
    mTypes[".exe"] = "application/octet-stream";
    mTypes[".f"] = "text/plain";
    mTypes[".f77"] = "text/x-fortran";
    mTypes[".f90"] = "text/plain";
    mTypes[".fdf"] = "application/vnd.fdf";
    mTypes[".fif"] = "image/fif";
    mTypes[".fli"] = "video/fli";
    mTypes[".fli"] = "video/x-fli";
    mTypes[".flo"] = "image/florian";
    mTypes[".flx"] = "text/vnd.fmi.flexstor";
    mTypes[".fmf"] = "video/x-atomic3d-feature";
    mTypes[".for"] = "text/plain";
    mTypes[".fpx"] = "image/vnd.fpx";
    mTypes[".frl"] = "application/freeloader";
    mTypes[".funk"] = "audio/make";
    mTypes[".g"] = "text/plain";
    mTypes[".g3"] = "image/g3fax";
    mTypes[".gif"] = "image/gif";
    mTypes[".gl"] = "video/gl";
    mTypes[".gsd"] = "audio/x-gsm";
    mTypes[".gsm"] = "audio/x-gsm";
    mTypes[".gsp"] = "application/x-gsp";
    mTypes[".gss"] = "application/x-gss";
    mTypes[".gtar"] = "application/x-gtar";
    mTypes[".gz"] = "application/x-gzip";
    mTypes[".gzip"] = "application/x-gzip";
    mTypes[".h"] = "text/plain";
    mTypes[".hdf"] = "application/x-hdf";
    mTypes[".help"] = "application/x-helpfile";
    mTypes[".hgl"] = "application/vnd.hp-hpgl";
    mTypes[".hh"] = "text/plain";
    mTypes[".hlb"] = "text/x-script";
    mTypes[".hlp"] = "application/hlp";
    mTypes[".hpg"] = "application/vnd.hp-hpgl";
    mTypes[".hpgl"] = "application/vnd.hp-hpgl";
    mTypes[".hqx"] = "application/binhex";
    mTypes[".hta"] = "application/hta";
    mTypes[".htc"] = "text/x-component";
    mTypes[".htm"] = "text/html";
    mTypes[".html"] = "text/html";
    mTypes[".htmls"] = "text/html";
    mTypes[".htt"] = "text/webviewhtml";
    mTypes[".htx"] = "text/html";
    mTypes[".ice"] = "x-conference/x-cooltalk";
    mTypes[".ico"] = "image/x-icon";
    mTypes[".idc"] = "text/plain";
    mTypes[".ief"] = "image/ief";
    mTypes[".iefs"] = "image/ief";
    mTypes[".iges"] = "application/iges";
    mTypes[".igs"] = "application/iges";
    mTypes[".ima"] = "application/x-ima";
    mTypes[".imap"] = "application/x-httpd-imap";
    mTypes[".inf"] = "application/inf";
    mTypes[".ins"] = "application/x-internett-signup";
    mTypes[".ip"] = "application/x-ip2";
    mTypes[".isu"] = "video/x-isvideo";
    mTypes[".it"] = "audio/it";
    mTypes[".iv"] = "application/x-inventor";
    mTypes[".ivr"] = "i-world/i-vrml";
    mTypes[".ivy"] = "application/x-livescreen";
    mTypes[".jam"] = "audio/x-jam";
    mTypes[".jar"] = "application/java-archive";
    mTypes[".jav"] = "text/plain";
    mTypes[".jav"] = "text/x-java-source";
    mTypes[".java"] = "text/plain";
    mTypes[".jcm"] = "application/x-java-commerce";
    mTypes[".jfif"] = "image/jpeg";
    mTypes[".jfif-tbnl"] = "image/jpeg";
    mTypes[".jpe"] = "image/jpeg";
    mTypes[".jpeg"] = "image/jpeg";
    mTypes[".jpg"] = "image/jpeg";
    mTypes[".js"] = "application/x-javascript";
    mTypes[".js"] = "text/javascript";
    mTypes[".jut"] = "image/jutvision";
    mTypes[".kar"] = "audio/midi";
    mTypes[".ksh"] = "application/x-ksh";
    mTypes[".la"] = "audio/nspaudio";
    mTypes[".lam"] = "audio/x-liveaudio";
    mTypes[".latex"] = "application/x-latex";
    mTypes[".lha"] = "application/lha";
    mTypes[".lhx"] = "application/octet-stream";
    mTypes[".list"] = "text/plain";
    mTypes[".lma"] = "audio/nspaudio";
    mTypes[".log"] = "text/plain";
    mTypes[".lsp"] = "application/x-lisp";
    mTypes[".lst"] = "text/plain";
    mTypes[".lsx"] = "text/x-la-asf";
    mTypes[".ltx"] = "application/x-latex";
    mTypes[".lzh"] = "application/octet-stream";
    mTypes[".lzx"] = "application/lzx";
    mTypes[".m"] = "text/plain";
    mTypes[".m1v"] = "video/mpeg";
    mTypes[".m2a"] = "audio/mpeg";
    mTypes[".m2v"] = "video/mpeg";
    mTypes[".m3u"] = "audio/x-mpequrl";
    mTypes[".man"] = "application/x-troff-man";
    mTypes[".map"] = "application/x-navimap";
    mTypes[".mar"] = "text/plain";
    mTypes[".mbd"] = "application/mbedlet";
    mTypes[".mc$"] = "application/x-magic-cap-package-1.0";
    mTypes[".mcd"] = "application/mcad";
    mTypes[".mcf"] = "text/mcf";
    mTypes[".mcp"] = "application/netmc";
    mTypes[".me"] = "application/x-troff-me";
    mTypes[".mht"] = "message/rfc822";
    mTypes[".mhtml"] = "message/rfc822";
    mTypes[".mid"] = "audio/midi";
    mTypes[".midi"] = "audio/midi";
    mTypes[".mif"] = "application/x-mif";
    mTypes[".mime"] = "www/mime";
    mTypes[".mjf"] = "audio/x-vnd.audioexplosion.mjuicemediafile";
    mTypes[".mjpg"] = "video/x-motion-jpeg";
    mTypes[".mm"] = "application/base64";
    mTypes[".mme"] = "application/base64";
    mTypes[".mod"] = "audio/mod";
    mTypes[".moov"] = "video/quicktime";
    mTypes[".mov"] = "video/quicktime";
    mTypes[".movie"] = "video/x-sgi-movie";
    mTypes[".mp2"] = "audio/mpeg";
    mTypes[".mp3"] = "audio/mpeg";
    mTypes[".mp4"] = "video/mp4";
    mTypes[".m4a"] = "audio/m4a";
    mTypes[".mpa"] = "audio/mpeg";
    mTypes[".mpc"] = "application/x-project";
    mTypes[".mpe"] = "video/mpeg";
    mTypes[".mpeg"] = "video/mpeg";
    mTypes[".mpg"] = "audio/mpeg";
    mTypes[".mpga"] = "audio/mpeg";
    mTypes[".mpp"] = "application/vnd.ms-project";
    mTypes[".mpt"] = "application/x-project";
    mTypes[".mpv"] = "application/x-project";
    mTypes[".mpx"] = "application/x-project";
    mTypes[".mrc"] = "application/marc";
    mTypes[".ms"] = "application/x-troff-ms";
    mTypes[".mv"] = "video/x-sgi-movie";
    mTypes[".my"] = "audio/make";
    mTypes[".mzz"] = "application/x-vnd.audioexplosion.mzz";
    mTypes[".nap"] = "image/naplps";
    mTypes[".naplps"] = "image/naplps";
    mTypes[".nc"] = "application/x-netcdf";
    mTypes[".ncm"] = "application/vnd.nokia.configuration-message";
    mTypes[".nif"] = "image/x-niff";
    mTypes[".niff"] = "image/x-niff";
    mTypes[".nix"] = "application/x-mix-transfer";
    mTypes[".nsc"] = "application/x-conference";
    mTypes[".nvd"] = "application/x-navidoc";
    mTypes[".o"] = "application/octet-stream";
    mTypes[".oda"] = "application/oda";
    mTypes[".ogg"] = "application/ogg";
    mTypes[".omc"] = "application/x-omc";
    mTypes[".omcd"] = "application/x-omcdatamaker";
    mTypes[".omcr"] = "application/x-omcregerator";
    mTypes[".p"] = "text/x-pascal";
    mTypes[".p10"] = "application/pkcs10";
    mTypes[".p12"] = "application/pkcs-12";
    mTypes[".p7a"] = "application/x-pkcs7-signature";
    mTypes[".p7c"] = "application/pkcs7-mime";
    mTypes[".p7m"] = "application/pkcs7-mime";
    mTypes[".p7r"] = "application/x-pkcs7-certreqresp";
    mTypes[".p7s"] = "application/pkcs7-signature";
    mTypes[".part"] = "application/pro_eng";
    mTypes[".pas"] = "text/pascal";
    mTypes[".pbm"] = "image/x-portable-bitmap";
    mTypes[".pcl"] = "application/vnd.hp-pcl";
    mTypes[".pct"] = "image/x-pict";
    mTypes[".pcx"] = "image/x-pcx";
    mTypes[".pdb"] = "chemical/x-pdb";
    mTypes[".pdf"] = "application/pdf";
    mTypes[".pfunk"] = "audio/make";
    mTypes[".pgm"] = "image/x-portable-graymap";
    mTypes[".pic"] = "image/pict";
    mTypes[".pict"] = "image/pict";
    mTypes[".pkg"] = "application/x-newton-compatible-pkg";
    mTypes[".pko"] = "application/vnd.ms-pki.pko";
    mTypes[".pl"] = "text/plain";
    mTypes[".plx"] = "application/x-pixclscript";
    mTypes[".pm"] = "image/x-xpixmap";
    mTypes[".pm4"] = "application/x-pagemaker";
    mTypes[".pm5"] = "application/x-pagemaker";
    mTypes[".png"] = "image/png";
    mTypes[".pnm"] = "application/x-portable-anymap";
    mTypes[".pot"] = "application/vnd.ms-powerpoint";
    mTypes[".potm"] = "application/vnd.ms-powerpoint.template.macroEnabled.12";
    mTypes[".potx"] = "application/vnd.openxmlformats-officedocument.presentationml.template";
    mTypes[".pov"] = "model/x-pov";
    mTypes[".ppa"] = "application/vnd.ms-powerpoint";
    mTypes[".ppam"] = "application/vnd.ms-powerpoint.addin.macroEnabled.12";
    mTypes[".ppm"] = "image/x-portable-pixmap";
    mTypes[".pps"] = "application/vnd.ms-powerpoint";
    mTypes[".ppsm"] = "application/vnd.ms-powerpoint.slideshow.macroEnabled.12";
    mTypes[".ppsx"] = "application/vnd.openxmlformats-officedocument.presentationml.slideshow";
    mTypes[".ppt"] = "application/vnd.ms-powerpoint";
    mTypes[".pptm"] = "application/vnd.ms-powerpoint.presentation.macroEnabled.12";
    mTypes[".pptx"] = "application/vnd.openxmlformats-officedocument.presentationml.presentation";
    mTypes[".ppz"] = "application/mspowerpoint";
    mTypes[".pre"] = "application/x-freelance";
    mTypes[".prt"] = "application/pro_eng";
    mTypes[".ps"] = "application/postscript";
    mTypes[".psd"] = "application/octet-stream";
    mTypes[".pvu"] = "paleovu/x-pv";
    mTypes[".pwz"] = "application/vnd.ms-powerpoint";
    mTypes[".py"] = "text/x-script.phyton";
    mTypes[".pyc"] = "application/x-bytecode.python";
    mTypes[".qcp"] = "audio/vnd.qcelp";
    mTypes[".qd3"] = "x-world/x-3dmf";
    mTypes[".qd3d"] = "x-world/x-3dmf";
    mTypes[".qif"] = "image/x-quicktime";
    mTypes[".qt"] = "video/quicktime";
    mTypes[".qtc"] = "video/x-qtc";
    mTypes[".qti"] = "image/x-quicktime";
    mTypes[".qtif"] = "image/x-quicktime";
    mTypes[".ra"] = "audio/x-realaudio";
    mTypes[".ram"] = "audio/x-pn-realaudio";
    mTypes[".ras"] = "application/x-cmu-raster";
    mTypes[".ras"] = "image/cmu-raster";
    mTypes[".rast"] = "image/cmu-raster";
    mTypes[".rexx"] = "text/x-script.rexx";
    mTypes[".rf"] = "image/vnd.rn-realflash";
    mTypes[".rgb"] = "image/x-rgb";
    mTypes[".rm"] = "application/vnd.rn-realmedia";
    mTypes[".rmi"] = "audio/mid";
    mTypes[".rmm"] = "audio/x-pn-realaudio";
    mTypes[".rmp"] = "audio/x-pn-realaudio";
    mTypes[".rng"] = "application/ringing-tones";
    mTypes[".rnx"] = "application/vnd.rn-realplayer";
    mTypes[".roff"] = "application/x-troff";
    mTypes[".rp"] = "image/vnd.rn-realpix";
    mTypes[".rpm"] = "audio/x-pn-realaudio-plugin";
    mTypes[".rt"] = "text/richtext";
    mTypes[".rtf"] = "application/rtf";
    mTypes[".rtx"] = "application/rtf";
    mTypes[".rv"] = "video/vnd.rn-realvideo";
    mTypes[".s"] = "text/x-asm";
    mTypes[".s3m"] = "audio/s3m";
    mTypes[".saveme"] = "application/octet-stream";
    mTypes[".sbk"] = "application/x-tbook";
    mTypes[".scm"] = "video/x-scm";
    mTypes[".sdml"] = "text/plain";
    mTypes[".sdp"] = "application/sdp";
    mTypes[".sdr"] = "application/sounder";
    mTypes[".sea"] = "application/sea";
    mTypes[".set"] = "application/set";
    mTypes[".sgm"] = "text/sgml";
    mTypes[".sgml"] = "text/sgml";
    mTypes[".sh"] = "application/x-sh";
    mTypes[".shar"] = "application/x-bsh";
    mTypes[".shar"] = "application/x-shar";
    mTypes[".shtml"] = "text/html";
    mTypes[".sid"] = "audio/x-psid";
    mTypes[".sit"] = "application/x-sit";
    mTypes[".skd"] = "application/x-koan";
    mTypes[".skm"] = "application/x-koan";
    mTypes[".skp"] = "application/x-koan";
    mTypes[".skt"] = "application/x-koan";
    mTypes[".sl"] = "application/x-seelogo";
    mTypes[".sldx"] = "application/vnd.openxmlformats-officedocument.presentationml.slide";
    mTypes[".smi"] = "application/smil";
    mTypes[".smil"] = "application/smil";
    mTypes[".snd"] = "audio/basic";
    mTypes[".sol"] = "application/solids";
    mTypes[".spc"] = "text/x-speech";
    mTypes[".spl"] = "application/futuresplash";
    mTypes[".spr"] = "application/x-sprite";
    mTypes[".sprite"] = "application/x-sprite";
    mTypes[".src"] = "application/x-wais-source";
    mTypes[".ssi"] = "text/x-server-parsed-html";
    mTypes[".ssm"] = "application/streamingmedia";
    mTypes[".sst"] = "application/vnd.ms-pki.certstore";
    mTypes[".step"] = "application/step";
    mTypes[".stl"] = "application/sla";
    mTypes[".stp"] = "application/step";
    mTypes[".sv4cpio"] = "application/x-sv4cpio";
    mTypes[".sv4crc"] = "application/x-sv4crc";
    mTypes[".svf"] = "image/vnd.dwg";
    mTypes[".svg"] = "image/svg+xml";
    mTypes[".svr"] = "application/x-world";
    mTypes[".swf"] = "application/x-shockwave-flash";
    mTypes[".t"] = "application/x-troff";
    mTypes[".talk"] = "text/x-speech";
    mTypes[".tar"] = "application/x-tar";
    mTypes[".tbk"] = "application/toolbook";
    mTypes[".tcl"] = "application/x-tcl";
    mTypes[".tcsh"] = "text/x-script.tcsh";
    mTypes[".tex"] = "application/x-tex";
    mTypes[".texi"] = "application/x-texinfo";
    mTypes[".texinfo"] = "application/x-texinfo";
    mTypes[".text"] = "text/plain";
    mTypes[".tgz"] = "application/gnutar";
    mTypes[".tif"] = "image/tiff";
    mTypes[".tiff"] = "image/tiff";
    mTypes[".tr"] = "application/x-troff";
    mTypes[".tsi"] = "audio/tsp-audio";
    mTypes[".tsp"] = "application/dsptype";
    mTypes[".tsv"] = "text/tab-separated-values";
    mTypes[".turbot"] = "image/florian";
    mTypes[".txt"] = "text/plain";
    mTypes[".uil"] = "text/x-uil";
    mTypes[".uni"] = "text/uri-list";
    mTypes[".unis"] = "text/uri-list";
    mTypes[".unv"] = "application/i-deas";
    mTypes[".uri"] = "text/uri-list";
    mTypes[".uris"] = "text/uri-list";
    mTypes[".ustar"] = "application/x-ustar";
    mTypes[".uu"] = "application/octet-stream";
    mTypes[".uue"] = "text/x-uuencode";
    mTypes[".vcd"] = "application/x-cdlink";
    mTypes[".vcs"] = "text/x-vcalendar";
    mTypes[".vda"] = "application/vda";
    mTypes[".vdo"] = "video/vdo";
    mTypes[".vew"] = "application/groupwise";
    mTypes[".viv"] = "video/vivo";
    mTypes[".vivo"] = "video/vivo";
    mTypes[".vmd"] = "application/vocaltec-media-desc";
    mTypes[".vmf"] = "application/vocaltec-media-file";
    mTypes[".voc"] = "audio/voc";
    mTypes[".vos"] = "video/vosaic";
    mTypes[".vox"] = "audio/voxware";
    mTypes[".vqe"] = "audio/x-twinvq-plugin";
    mTypes[".vqf"] = "audio/x-twinvq";
    mTypes[".vql"] = "audio/x-twinvq-plugin";
    mTypes[".vrml"] = "application/x-vrml";
    mTypes[".vrt"] = "x-world/x-vrt";
    mTypes[".vsd"] = "application/x-visio";
    mTypes[".vst"] = "application/x-visio";
    mTypes[".vsw"] = "application/x-visio";
    mTypes[".w60"] = "application/wordperfect6.0";
    mTypes[".w61"] = "application/wordperfect6.1";
    mTypes[".w6w"] = "application/msword";
    mTypes[".wav"] = "audio/wav";
    mTypes[".wb1"] = "application/x-qpro";
    mTypes[".wbmp"] = "image/vnd.wap.wbmp";
    mTypes[".web"] = "application/vnd.xara";
    mTypes[".wiz"] = "application/msword";
    mTypes[".wk1"] = "application/x-123";
    mTypes[".wmf"] = "windows/metafile";
    mTypes[".wml"] = "text/vnd.wap.wml";
    mTypes[".wmlc"] = "application/vnd.wap.wmlc";
    mTypes[".wmls"] = "text/vnd.wap.wmlscript";
    mTypes[".wmlsc"] = "application/vnd.wap.wmlscriptc";
    mTypes[".word"] = "application/msword";
    mTypes[".wp"] = "application/wordperfect";
    mTypes[".wp5"] = "application/wordperfect";
    mTypes[".wp6"] = "application/wordperfect";
    mTypes[".wpd"] = "application/wordperfect";
    mTypes[".wq1"] = "application/x-lotus";
    mTypes[".wri"] = "application/mswrite";
    mTypes[".wri"] = "application/x-wri";
    mTypes[".wrl"] = "model/vrml";
    mTypes[".wrz"] = "x-world/x-vrml";
    mTypes[".wsc"] = "text/scriplet";
    mTypes[".wsrc"] = "application/x-wais-source";
    mTypes[".wtk"] = "application/x-wintalk";
    mTypes[".xbm"] = "image/xbm";
    mTypes[".xdr"] = "video/x-amt-demorun";
    mTypes[".xgz"] = "xgl/drawing";
    mTypes[".xif"] = "image/vnd.xiff";
    mTypes[".xl"] = "application/vnd-ms.excel";
    mTypes[".xla"] = "application/vnd-ms.excel";
    mTypes[".xlam"] = "application/vnd.ms-excel.addin.macroEnabled.12";
    mTypes[".xlb"] = "application/x-excel";
    mTypes[".xlc"] = "application/vnd-ms.excel";
    mTypes[".xlc"] = "application/x-excel";
    mTypes[".xld"] = "application/vnd.ms-excel";
    mTypes[".xlk"] = "application/vnd.ms-excel";
    mTypes[".xll"] = "application/vnd.ms-excel";
    mTypes[".xll"] = "application/x-excel";
    mTypes[".xlm"] = "application/vnd-ms.excel";
    mTypes[".xls"] = "application/vnd.ms.excel";
    mTypes[".xlsb"] = "application/vnd.ms-excel.sheet.binary.macroEnabled.12";
    mTypes[".xlsx"] = "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
    mTypes[".xltx"] = "application/vnd.openxmlformats-officedocument.spreadsheetml.template";
    mTypes[".xlt"] = "application/vnd.ms-excel";
    mTypes[".xlv"] = "application/vnd.ms-excel";
    mTypes[".xlw"] = "application/vnd.ms-excel";
    mTypes[".xlw"] = "application/x-msexcel";
    mTypes[".xm"] = "audio/xm";
    mTypes[".xml"] = "text/xml";
    mTypes[".xmz"] = "xgl/movie";
    mTypes[".xpix"] = "application/x-vnd.ls-xpix";
    mTypes[".xpm"] = "image/xpm";
    mTypes[".x-png"] = "image/png";
    mTypes[".xsr"] = "video/x-amt-showrun";
    mTypes[".xwd"] = "image/x-xwd";
    mTypes[".xyz"] = "chemical/x-pdb";
    mTypes[".z"] = "application/x-compress";
    mTypes[".zip"] = "application/zip";
    mTypes[".zoo"] = "application/octet-stream";
    mTypes[".zsh"] = "text/x-script.zsh";
  }
};

/*----------------------------------------------------------------------------*/

EOSCOMMONNAMESPACE_END

/*----------------------------------------------------------------------------*/

#endif
