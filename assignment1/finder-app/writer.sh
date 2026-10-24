#!/bin/sh

# Check that exactly two arguments were provided
if [ $# -ne 2 ]; then
    echo "Usage: $0 <writefile> <writestr>"
    exit 1
fi

writefile="$1"
writestr="$2"

# Extract directory path and create it if needed
dirpath=$(dirname "$writefile")
mkdir -p "$dirpath"

# Attempt to write the string to the file
echo "$writestr" > "$writefile"

# Check for write failure
if [ $? -ne 0 ]; then
    echo "Error: Could not write to $writefile"
    exit 1
fi

exit 0
