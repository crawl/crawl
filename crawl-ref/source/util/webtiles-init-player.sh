#!/bin/sh

RCDIR=../../crawldata/rcs/
INPROGRESSDIR=../../crawldata/rcs/running
TTYRECDIR=../../crawldata/rcs/ttyrecs/$1
DEFAULT_RC=../settings/init.txt
PLAYERNAME=$1

mkdir -p $RCDIR
mkdir -p $INPROGRESSDIR
mkdir -p $TTYRECDIR

if [ ! -f ${RCDIR}/${PLAYERNAME}.rc ]; then
    cp ${DEFAULT_RC} ${RCDIR}/${PLAYERNAME}.rc
fi
