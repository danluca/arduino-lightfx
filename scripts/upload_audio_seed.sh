#!/usr/bin/bash

file=$1
var=$2
ofs=$3
len=$4

out_file=fsi4_seed$var.txt
boardUri=http://192.168.0.75   # Dev board - see scripts/boards.ps1

# upload auth token - LIGHTFX_AUTH_TOKEN environment variable, or FW_AUTH_TOKEN from the git-ignored include/secrets.h
token=$LIGHTFX_AUTH_TOKEN
if [ -z "$token" ]; then
	secrets_file="$(dirname "$(readlink -f "$0")")/../include/secrets.h"
	[ -f "$secrets_file" ] && token=$(sed -nE 's/^\s*#define\s+FW_AUTH_TOKEN\s+"([^"]+)".*/\1/p' "$secrets_file" | head -n 1)
fi
if [ -z "$token" ]; then
	echo "Upload auth token not found - set LIGHTFX_AUTH_TOKEN or define FW_AUTH_TOKEN in include/secrets.h" >&2
	exit 1
fi

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
if ! curl -s -X POST $boardUri/upload -H "X-Token: $token" -H "X-Path: fx/fxi4_seed$var.txt" -H "X-Check: $SHA256" --data-binary @"$out_file"; then
	echo "Upload failed" >&2
	exit 3
fi

curl -s $boardUri/files.json