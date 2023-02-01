#!/bin/bash

dirlist=$(find $1 -mindepth 1 -maxdepth 1 -type d)

for dir in $dirlist
do
  echo "packing cheat file for $dir"
  test -f "$dir.zip" && { echo "previous file found, erasing..."; rm "$dir.zip"; }
  zip -q -9 "$dir.zip" "$dir/$dir.TXT"
  
done
