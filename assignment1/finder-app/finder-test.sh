#!/bin/sh
set -e
set -u

WRITESTR=${1:-AELD_IS_FUN}
NUMFILES=${2:-10}
WRITEDIR=/tmp/aeld-data

echo "Writing ${NUMFILES} files containing string ${WRITESTR} to ${WRITEDIR}"

rm -rf "${WRITEDIR}"
mkdir -p "${WRITEDIR}"

for i in $( seq 1 $NUMFILES)
do
    ./writer.sh "$WRITEDIR/file$i.txt" "$WRITESTR"
done

OUTPUTSTRING=$(./finder.sh "$WRITEDIR" "$WRITESTR")

MATCHSTR="The number of files are ${NUMFILES} and the number of matching lines are ${NUMFILES}"

set +e
echo ${OUTPUTSTRING} | grep "${MATCHSTR}"
if [ $? -eq 0 ]; then
    echo "success"
    exit 0
else
    echo "failed: expected ${MATCHSTR} in ${OUTPUTSTRING} but instead found"
    exit 1
fi
