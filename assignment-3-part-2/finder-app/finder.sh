#!/bin/bash
# Script for assignment 2 and 3
# TODO: Accept two arguments: filesdir and searchstr

if [ $# -ne 2 ]
then
    echo "Usage: $0 <filesdir> <searchstr>"
    exit 1
fi

filesdir=$1
searchstr=$2

if [ ! -d "$filesdir" ]
then
    echo "Error: $filesdir is not a directory"
    exit 1
fi

# TODO: Count the number of files and matching lines
file_count=$(find "$filesdir" -type f | wc -l)
match_count=$(grep -R "$searchstr" "$filesdir" 2>/dev/null | wc -l)

echo "The number of files are $file_count and the number of matching lines are $match_count"
