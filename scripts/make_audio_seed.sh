#!/usr/bin/bash

file=$1
variant=$2
ofs=$3
len=$4

out_file=fsi4_seed$variant.txt


python create_audio_seed.py $file $out_file 4 30 --stretch_p 1 99 --contrast 1.8 --transient 0.4 --max_minutes $len --offset_minutes $ofs

# Get the file size in bytes
FILE_SIZE=$(stat -c%s "$out_file")
if [ $FILE_SIZE -gt 22528 ]; then
    echo "Warning: Audio seed file size $FILE_SIZE is larger than 22k limit, applying sifting..."
    python every_nth_line.py $out_file -o "$out_file.short" -n 3
    mv "$out_file.short" $out_file
    #check again and error out if still large file
    FILE_SIZE=$(stat -c%s "$out_file")
    if [ $FILE_SIZE -gt 22528 ]; then
      echo "ERROR: File size $FILE_SIZE (after sifting) is still larger than 22k limit. Please adjust audio spectral analysis params and try again."
      exit 1
    fi
fi

ls -alp $out_file
