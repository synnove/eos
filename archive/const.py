#-------------------------------------------------------------------------------
# File: const.py
# Author: Elvin-Alin Sindrilaru <esindril@cern.ch>
#-------------------------------------------------------------------------------
#
#*******************************************************************************
# EOS - the CERN Disk Storage System
# Copyright (C) 2014 CERN/Switzerland
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#*******************************************************************************


class _const:
  """ Class holding the constant values used throughout the project"""
  class ConstError(TypeError): pass

  def __setattr__(self,name,value):
    if self.__dict__.has_key(name):
      raise self.ConstError("Can't rebind const({0})".format(name))
    self.__dict__[name]=value

from sys import modules
modules[__name__]=_const()
