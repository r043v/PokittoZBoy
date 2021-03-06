#! /bin/bash
for game in ./gb/*.gb
do
  game=$(basename "$game")
  game=${game%.*}
  echo "process $game"
  [ -f "roms/$game.h" ] || bin2c -o "roms/$game.h" -n embedrom "./gb/$game.gb"
  rm -f BUILD/zboy.o
  make -j4 rom="$game" > /dev/null 2>&1
  cp BUILD/zboy.bin "./bin/$game.bin"
  cp BUILD/zboy.bin "/var/run/media/m/1465-94AF/gb/$game.bin"
done

sync && sudo umount /var/run/media/m/1465-94AF
