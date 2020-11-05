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

import os
from VideoEncoder import VideoEncode
from VideoDecoder import VideoDecode
from VideoScaler import UpScaling, GetDownScaledOutFile, GetUpScaledOutFile
from Config import SUFFIX, LoggerName
from Utils import GetShortContentName, Clip
import logging

subloggername = "EncDecUpscale"
loggername = LoggerName + '.' + '%s' % subloggername
logger = logging.getLogger(loggername)

################################################################################
##################### Internal Helper Functions ################################
def GetBitstreamFile(method, codec, test_cfg, preset, yuvfile, qp, outpath):
    bs_suffix = SUFFIX[codec]
    Prefix_EncodeCfg = '_%s_%s_%s_Preset_%s' % (method, codec, test_cfg, preset)
    filename = GetShortContentName(yuvfile, False) + Prefix_EncodeCfg + "_QP_"\
               + str(qp) + bs_suffix
    filename = os.path.join(outpath, filename)
    return filename

def GetDecodedFile(bsfile, outpath):
    filename = GetShortContentName(bsfile, False) + '_Decoded.y4m'
    decodedfile = os.path.join(outpath, filename)
    return decodedfile

################################################################################
##################### Major Functions ##########################################
def Encode(method, codec, preset, clip, test_cfg, qp, num, path,
           LogCmdOnly=False):
    bsfile = GetBitstreamFile(method, codec, test_cfg, preset, clip.file_path,
                              qp, path)
    # call VideoEncoder to do the encoding
    VideoEncode(method, codec, clip, test_cfg, qp, num, bsfile, preset,
                LogCmdOnly)
    return bsfile

def Decode(codec, bsfile, path, LogCmdOnly=False):
    decodedfile = GetDecodedFile(bsfile, path)
    #call VideoDecoder to do the decoding
    VideoDecode(codec, bsfile, decodedfile, LogCmdOnly)
    return decodedfile

def Run_EncDec_Upscale(method, codec, preset, clip, test_cfg, QP, num, outw,
                       outh, path_bs, path_decoded, path_upscaled, path_cfg,
                       upscale_algo, LogCmdOnly = False):
    logger.info("%s %s start encode file %s with QP = %d" %
                (method, codec, clip.file_name, QP))
    bsFile = Encode(method, codec, preset, clip, test_cfg, QP, num, path_bs,
                    LogCmdOnly)
    logger.info("start decode file %s" % os.path.basename(bsFile))
    decodedYUV = Decode(codec, bsFile, path_decoded, LogCmdOnly)
    logger.info("start upscale file %s" % os.path.basename(decodedYUV))
    dec_clip = Clip(GetShortContentName(decodedYUV, False) + '.y4m',
                    decodedYUV, clip.file_class, clip.width, clip.height,
                    clip.fmt, clip.fps_num, clip.fps_denom, clip.bit_depth)
    upscaledYUV = UpScaling(dec_clip, num, outw, outh, path_upscaled, path_cfg,
                            upscale_algo, LogCmdOnly)
    logger.info("finish Run Encode, Decode and Upscale")
    return upscaledYUV


def GetBsReconFileName(encmethod, codecname, test_cfg, preset, clip, dw, dh,
                       dnScAlgo, upScAlgo, qp, path_bs):
    dsyuv_name = GetDownScaledOutFile(clip, dw, dh, path_bs, dnScAlgo)
    # return bitstream file with absolute path
    bs = GetBitstreamFile(encmethod, codecname, test_cfg, preset, dsyuv_name,
                          qp, path_bs)
    decoded = GetDecodedFile(bs, path_bs)
    ds_clip = Clip(GetShortContentName(decoded, False) + '.y4m',
                   decoded, clip.file_class, dw, dh, clip.fmt, clip.fps_num,
                   clip.fps_denom, clip.bit_depth)
    reconfilename = GetUpScaledOutFile(ds_clip, clip.width, clip.height,
                                       upScAlgo, path_bs)
    # return only Recon yuv file name w/o path
    reconfilename = GetShortContentName(reconfilename, False) + '.y4m'
    return bs, reconfilename
