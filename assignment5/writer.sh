#!/bin/bash

# Check arguments
if [ $# -ne 2 ]
then
    echo "Usage: $0 <file> <string>"
    exit 1
fi

writefile=$1
writestr=$2

# Ensure directory exists
mkdir -p "$(dirname "$writefile")"

# Write the string
echo "$writestr" > "$writefile"

# Check success
if [ $? -ne 0 ]
then
    echo "Error writing to $writefile"
    exit 1
fi

exit 0
