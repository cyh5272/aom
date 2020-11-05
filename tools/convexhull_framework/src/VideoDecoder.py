#!/usr/bin/env python
## Copyright (c) 2019, Alliance for Open Media. All rights reserved
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##
__author__ = "maggie.sun@intel.com, ryan.lei@intel.com"

import Utils
from Config import AOMDEC
from Utils import ExecuteCmd

def DecodeWithAOM(infile, outfile, LogCmdOnly=False):
    args = " --codec=av1 --summary -o %s %s" % (outfile, infile)
    cmd = AOMDEC + args
    ExecuteCmd(cmd, LogCmdOnly)

def VideoDecode(codec, infile, outfile, LogCmdOnly=False):
    Utils.CmdLogger.write("::Decode\n")
    if codec == 'av1':
        DecodeWithAOM(infile, outfile, LogCmdOnly)
    else:
        raise ValueError("invalid parameter for decode.")
