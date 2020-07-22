#! /bin/bash
for game in "$@"
do
	echo "process $game"
	[ -f "roms/$game.h" ] || bin2c -o "roms/$game.h" -n embedrom "./gb/$game.gb"
	rm -f BUILD/zboy.o
	make -j4 rom="$game"
	cp BUILD/zboy.bin "./bin/$game.bin"
	cp BUILD/zboy.bin "/var/run/media/m/1465-94AF/gb/$game.bin"
done
sync && sudo umount /var/run/media/m/1465-94AF
