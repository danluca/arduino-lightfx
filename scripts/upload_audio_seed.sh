#!/usr/bin/bash

file=$1
var=$2
ofs=$3
len=$4

out_file=fsi4_seed$var.txt

make_audio_seed.sh $file $var $ofs $len

SHA256=$(sha256sum $out_file | awk '{print tolower($1)}')

curl -X POST http://192.168.0.10/upload -H "X-Token: KlFpc1dAdFd0eDRXdkVSZg" -H "X-Path: fx/fxi4_seed$var.txt" -H "X-Check: $SHA256" --data-binary @$out_file

# curl -s http://192.168.0.10/files.json