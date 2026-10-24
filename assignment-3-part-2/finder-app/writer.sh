#!/bin/bash
# TODO: Implement writer script wrapper for writer utility

if [ $# -ne 2 ]
then
    echo "Usage: $0 <writefile> <writestr>"
    exit 1
fi

writefile=$1
writestr=$2

mkdir -p "$(dirname "$writefile")"

./writer "$writefile" "$writestr"
