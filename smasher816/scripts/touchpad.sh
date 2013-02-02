#!/bin/bash

WD=/home/andy/.mediakeys
BIN=/home/andy/bin/touchpad
FILE=touchpad

mkdir -p $WD
cd $WD

if [ "$1" == "on" ]; then
	touch $FILE
elif [ "$1" == "off" ]; then
	rm $FILE
elif [ "$1" == "toggle" ]; then
	if [ -f $FILE ]; then
		rm $FILE
	else
		touch $FILE
	fi
fi

if [ -f $FILE ]; then
	sudo $BIN -e
else
	sudo $BIN -d
fi
