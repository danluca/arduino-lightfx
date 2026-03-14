#!/usr/bin/bash

file=$1
var=$2
ofs=$3
len=$4

out_file=fsi4_seed$var.txt
boardUri=http://192.168.0.139

# If output file already exists, skip regeneration to save time and keep previous file
if [ -f "$out_file" ]; then
	echo "Found existing $out_file — skipping generation"
else
	echo "Generating $out_file..."
	if ! make_audio_seed.sh "$file" "$var" "$ofs" "$len"; then
		echo "make_audio_seed.sh failed" >&2
		exit 2
	fi
fi

SHA256=$(sha256sum "$out_file" | awk '{print tolower($1)}')

echo "Uploading $out_file (sha256=$SHA256)"
if ! curl -s -X POST $boardUri/upload -H "X-Token: KlFpc1dAdFd0eDRXdkVSZg" -H "X-Path: fx/fxi4_seed$var.txt" -H "X-Check: $SHA256" --data-binary @"$out_file"; then
	echo "Upload failed" >&2
	exit 3
fi

curl -s $boardUri/files.json