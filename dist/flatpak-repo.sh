#!/bin/bash
# Make the Flatpak repository that GitHub Pages serves, from the Flatpak
# bundle of the latest release: an OSTree repository signed with our GPG key,
# plus the .flatpakref (one-click install from a browser or Discover, e.g. on
# a Steam Deck) & .flatpakrepo files.  Installed copies then update with every
# other Flatpak (Discover, or flatpak update).
#
# Usage: dist/flatpak-repo.sh <output directory> [bundle.flatpak]
#
# Without a bundle the latest release's .flatpak is downloaded with gh.
# Needs: flatpak, ostree, gpg (& gh), and in the environment
#   FLATPAK_GPG_KEY     the ASCII armoured private signing key
#   FLATPAK_GPG_KEY_ID  its key ID or fingerprint
#   SITE                the site address, e.g. https://alexdia25.github.io/xu4-legacy-edition

set -e

OUT=${1:?usage: flatpak-repo.sh <output directory> [bundle.flatpak]}
BUNDLE=$2
: "${FLATPAK_GPG_KEY:?FLATPAK_GPG_KEY is not set (see dist/flatpak/README.md)}"
: "${FLATPAK_GPG_KEY_ID:?FLATPAK_GPG_KEY_ID is not set}"
: "${SITE:?SITE is not set}"
REPO=${GITHUB_REPOSITORY:-alexdia25/xu4-legacy-edition}
NAME=xu4-legacy
TITLE="XU4 Legacy Edition"
WORK=$(mktemp -d)
trap 'rm -rf ${WORK}' EXIT

export GNUPGHOME=${WORK}/gnupg
mkdir -m 700 ${GNUPGHOME}
gpg --batch --quiet --import <<<"${FLATPAK_GPG_KEY}"

if [ -z "${BUNDLE}" ]; then
    gh release download --repo ${REPO} --pattern '*.flatpak' --dir ${WORK}/dl
    BUNDLE=$(ls ${WORK}/dl/*.flatpak | head -1)
fi
[ -f "${BUNDLE}" ] || { echo "No Flatpak bundle found" >&2; exit 1; }

ostree init --repo=${WORK}/repo --mode=archive-z2
flatpak build-import-bundle --gpg-homedir=${GNUPGHOME} \
    --gpg-sign=${FLATPAK_GPG_KEY_ID} ${WORK}/repo "${BUNDLE}"
flatpak build-update-repo --gpg-homedir=${GNUPGHOME} \
    --gpg-sign=${FLATPAK_GPG_KEY_ID} --generate-static-deltas --prune \
    --title="${TITLE}" ${WORK}/repo

REF=$(ostree refs --repo=${WORK}/repo | grep '^app/' | head -1)  # app/<id>/<arch>/<branch>
APP=$(echo ${REF} | cut -d/ -f2)
BRANCH=$(echo ${REF} | cut -d/ -f4)
KEY=$(gpg --export ${FLATPAK_GPG_KEY_ID} | base64 | tr -d '\n')

mkdir -p "${OUT}"
cp -R ${WORK}/repo/. "${OUT}/"
cat > "${OUT}/${NAME}.flatpakrepo" <<EOF
[Flatpak Repo]
Title=${TITLE}
Url=${SITE}/flatpak/
Homepage=https://github.com/${REPO}
Comment=Ultima IV recreated, with gamepad and console style menus
GPGKey=${KEY}
EOF
cat > "${OUT}/${NAME}.flatpakref" <<EOF
[Flatpak Ref]
Title=${TITLE}
Name=${APP}
Branch=${BRANCH}
Url=${SITE}/flatpak/
SuggestRemoteName=${NAME}
Homepage=https://github.com/${REPO}
IsRuntime=false
RuntimeRepo=https://dl.flathub.org/repo/flathub.flatpakrepo
GPGKey=${KEY}
EOF
echo "Flatpak repository: ${REF} from $(basename "${BUNDLE}") in ${OUT}"
