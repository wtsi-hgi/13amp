#!/bin/bash

# GPLv3 or later
# Copyright (c) 2015 Genome Research Limited

set -eu -o pipefail

# Get script directory
pushd $(dirname $0) &>/dev/null
ROOTDIR=$(pwd)
popd &>/dev/null

# Working directories
SRCDIR=$ROOTDIR/source
MNTDIR=$ROOTDIR/mount
CHKDIR=$ROOTDIR/check

if [ ! -d "$SRCDIR" ]; then
  >&2 echo "Source directory not found: $SRCDIR"
  exit 1
fi

if [ ! -d "$MNTDIR" ]; then
  mkdir -p $MNTDIR
fi

if [ ! -d "$CHKDIR" ]; then
  mkdir -p $CHKDIR
fi

BINDIR=$ROOTDIR/../bin

# Binaries
CRAMP=$BINDIR/13amp
SAMTOOLS=/usr/local/bin/samtools

if ! command -v $CRAMP; then
  >&2 echo "13 Amp binary not found: $CRAMP"
  exit 1
fi

if ! command -v $SAMTOOLS; then
  >&2 echo "Samtools binary not found: $SAMTOOLS"
  # TODO Check version?
fi

# Reference cache
if [ -z "${REF_CACHE:-}" ]; then
  export REF_CACHE=$ROOTDIR/.cache
  >&2 echo "Reference cache set to: $REF_CACHE"
fi

# Mount testing directory
$CRAMP $MNTDIR $SRCDIR && echo "VFS mounted successfully"

# TODO
# Check $MNTDIR is the BAM'd version of $SRCDIR, wrt readdir
# Transcoding check:
#   Convert $SRCDIR/*.cram into $CHKDIR/*.bam using samtools view -b
#   Check $MNTDIR/*.bam against $CHKDIR/*.bam, wrt MD5 sums
