#!/bin/bash
# Build xu4 as a Flatpak and export it as a single-file bundle
# (build-flatpak/xu4-legacy-<version>-x86_64.flatpak), which users install with
#   flatpak install --user xu4-legacy-<version>-x86_64.flatpak
#
# Usage: dist/build-flatpak.sh [version]
#
# Needs: flatpak, flatpak-builder & git.  The Freedesktop runtime & SDK are
# installed from Flathub for the current user if missing.

set -e

VERSION=${1:-$(git describe --tags --always | sed 's/^legacy-//')}
APP=io.github.alexdia25.xu4_legacy_edition
MANIFEST=dist/flatpak/${APP}.yml
RUNTIME=25.08
WORK=$(pwd)/build-flatpak

flatpak remote-add --user --if-not-exists flathub \
    https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak install --user -y --noninteractive flathub \
    org.freedesktop.Platform//${RUNTIME} org.freedesktop.Sdk//${RUNTIME}

# The manifest takes the version from git describe, so tag the build
# locally when it is not tagged yet.
if [ "$(git describe --tags --always | sed 's/^legacy-//')" != "${VERSION}" ] &&
   ! git rev-parse -q --verify "refs/tags/legacy-${VERSION}" >/dev/null
then
    git tag "legacy-${VERSION}" HEAD
    trap 'git tag -d "legacy-${VERSION}" >/dev/null' EXIT
fi

mkdir -p ${WORK}
flatpak-builder --user --force-clean --disable-rofiles-fuse \
    --state-dir=${WORK}/state --repo=${WORK}/repo \
    ${WORK}/app ${MANIFEST}

flatpak build-bundle --runtime-repo=https://dl.flathub.org/repo/flathub.flatpakrepo \
    ${WORK}/repo ${WORK}/xu4-legacy-${VERSION}-x86_64.flatpak ${APP}

ls ${WORK}/xu4-legacy-${VERSION}-x86_64.flatpak
