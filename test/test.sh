#!/bin/bash

# GPLv3 or later
# Copyright (c) 2015 Genome Research Limited

set -eu -o pipefail

# Echo to stderr
function stderr {
  >&2 echo "$@"
}

REPODIR=$(git rev-parse --show-toplevel)
TESTDIR=$REPODIR/test

# Find 13amp binary
CRAMP=$(find $REPODIR -name 13amp -type f -perm -u=x)
if [ -z "$CRAMP" ]; then
  stderr "13amp binary not found"
  exit 1
fi

# Check samtools exists
# n.b., Can be overridden from command line: SAMTOOLS=foo ./test.sh
: "${SAMTOOLS:=samtools}"
if ! command -v $SAMTOOLS &>/dev/null; then
  stderr "samtools binary not found: $SAMTOOLS"
  exit 1
fi

# ...likewise for htsfile
: "${HTSFILE:=htsfile}"
if ! command -v $HTSFILE &>/dev/null; then
  stderr "htsfile binary not found: $HTSFILE"
  exit 1
fi

# Warn about no reference cache setting
if [ -z "${REF_CACHE:-}" ]; then
  stderr "No reference cache is set; your home directory will be used."
fi

# Working directories
SRCDIR=$TESTDIR/source
MNTDIR=$TESTDIR/mount
CHKDIR=$TESTDIR/check

if [ ! -d "$SRCDIR" ]; then
  stderr "Source directory not found: $SRCDIR"
  exit 1
fi

if [ ! -d "$MNTDIR" ]; then
  echo "Creating mount directory: $MNTDIR"
  mkdir -p $MNTDIR
fi

if [ -e "$CHKDIR" ]; then
  rm -rf $CHKDIR
fi
echo "Creating check directory: $CHKDIR"
cp -R $SRCDIR $CHKDIR

# FIXME Files with spaces might be a problem...
CRAMS=$(find -L $CHKDIR -name "*.cram" -type f \
       | xargs $HTSFILE \
       | grep "\tCRAM" \
       | sed "s/:.*$//")

for CRAM in $CRAMS; do
  BAM=$(sed "s/\.cram$/.bam/" <<< $CRAM)
  if [ ! -e "$BAM" ]; then
    echo "Creating $BAM"
    $SAMTOOLS view -b -o $BAM $CRAM
  fi
done

# Mount directory
echo "Mounting virtual filesystem"
$CRAMP $MNTDIR -S $SRCDIR

# FIXME Wait for mount
sleep 1

# Unmount and clean up on exit
function cleanup {
  echo "Unmounting and cleaning up"
  umount $MNTDIR
  rm -rf $MNTDIR $CHKDIR
}
trap cleanup EXIT

# Check the two directories contain the same files
echo "Checking directory structure"

function contents {
  find $1 | sed "s|^$1||;/^$/d" | sort
}

DIR_DIFF=$(diff <(contents $CHKDIR) <(contents $MNTDIR) || true)
if [ -n "$DIR_DIFF" ]; then
  stderr "Directory contents differ:"
  stderr "$DIR_DIFF"
  exit 1
fi

# Check file contents are the same
# n.b., Up to (and including) the specimen file size
echo "Checking file contents"
for FILE in $(find -L $MNTDIR -type f); do
  CHECK=$(sed "s+^$MNTDIR+$CHKDIR+" <<< $FILE)
  LIMIT=$(wc -c < "$CHECK")
  FILE_DIFF=$(cmp -n $LIMIT $FILE $CHECK || true)
  if [ -n "$FILE_DIFF" ]; then
    stderr "$FILE_DIFF"
    exit 1
  fi
done

# We're good :)
TICK="\xe2\x9c\x93"
ANSI="\033["
GREEN="${ANSI}0;32m"
RESET="${ANSI}0m"
echo -e "All tests passed $GREEN$TICK$RESET"
