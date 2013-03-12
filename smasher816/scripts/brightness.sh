#!/bin/bash

WD=/home/andy/.mediakeys
BIN=/home/andy/bin/backlight
FILE=ips
DELTA=10

mkdir -p $WD
cd $WD

if [ "$1" == "i" ]; then
	if [ -f $FILE ]; then
		rm $FILE
		sudo $BIN -s $(sudo $BIN -i)
	else
		touch $FILE
		sudo $BIN -i -s $(sudo $BIN)
	fi
fi


IPS=""
if [ -f $FILE ]; then
	IPS="-i"
fi

if [ "$1" == "i" ]; then
	echo -n "" #Already handled
elif [ "$1" == "+" ]; then
	sudo $BIN --inc $DELTA $INC
elif [ "$1" == "-" ]; then
	sudo $BIN --dec $DELTA $INC
else #Change to the saved value
	sudo $BIN -s $(cat brightness) $IPS
fi

#Write the new value down
sudo $BIN $IPS > brightness
