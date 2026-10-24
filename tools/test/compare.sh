#!/bin/bash
# Compare the results of two run.sh runs (e.g. before & after a change).
#
# Usage: tools/test/compare.sh <before> <after> [diffdir]
#
# Every screenshot & saved game file must match exactly.  When ImageMagick
# is installed, differing screenshots are written to diffdir (default
# <after>/diff) as before | after | difference images.

A=$1
B=$2
DIFF=${3:-$2/diff}
STATUS=0

rm -rf ${DIFF}
for f in $(cd ${A} && find . -type f ! -name log.txt | sort); do
    if [ ! -f ${B}/$f ]; then
        echo "MISSING in ${B}: $f"
        STATUS=1
    elif ! cmp -s ${A}/$f ${B}/$f; then
        echo "DIFFERENT: $f"
        STATUS=1
        if [[ $f == *.ppm ]] && command -v montage >/dev/null; then
            name=$(echo ${f#./} | tr / -)
            mkdir -p ${DIFF}
            compare ${A}/$f ${B}/$f ${DIFF}/tmp.png 2>/dev/null
            montage ${A}/$f ${B}/$f ${DIFF}/tmp.png -tile 3x1 -geometry +4+4 \
                ${DIFF}/${name%.ppm}.png
            rm -f ${DIFF}/tmp.png
        fi
    fi
done
for f in $(cd ${B} && find . -type f ! -name log.txt ! -path "./diff/*" | sort); do
    [ -f ${A}/$f ] || { echo "EXTRA in ${B}: $f"; STATUS=1; }
done

if [ ${STATUS} -eq 0 ]; then
    echo "All $(cd ${A} && find . -type f ! -name log.txt | wc -l) files match."
fi
exit ${STATUS}
