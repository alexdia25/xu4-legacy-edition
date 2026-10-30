Flatpak
=======

`io.github.alexdia25.xu4_legacy_edition.yml` is the Flatpak manifest.  The
Release workflow builds it with `dist/build-flatpak.sh` and puts the bundle
(`xu4-legacy-<version>-x86_64.flatpak`) in each release.

Flatpak repository
------------------

After a full release (not a -pre, -rc or -beta), the Release workflow starts
the Pages workflow (`.github/workflows/pages.yml`), which also publishes the
web version of the game.  It runs `dist/flatpak-repo.sh` on the release's
bundle and publishes the signed repository on GitHub Pages:

 - https://alexdia25.github.io/xu4-legacy-edition/ (install page)
 - https://alexdia25.github.io/xu4-legacy-edition/flatpak/xu4-legacy.flatpakref

Installed copies then update with `flatpak update` or Discover.

### One-time setup

1. Make a signing key (no passphrase, as the workflow signs unattended),
   e.g. in Git Bash or on Linux:

       gpg --batch --passphrase '' --quick-gen-key "XU4 Legacy Edition Flatpak" ed25519 sign never
       gpg --list-keys --with-colons | grep '^fpr' | head -1 | cut -d: -f10
       gpg --armor --export-secret-keys <fingerprint> > flatpak-key.asc

2. In the repository's Settings > Secrets and variables > Actions, add
   `FLATPAK_GPG_KEY` (the whole contents of `flatpak-key.asc`) and
   `FLATPAK_GPG_KEY_ID` (the fingerprint).  Keep `flatpak-key.asc`
   somewhere safe and out of the repository: installed copies only accept
   updates signed with this key.

3. In Settings > Pages, set Source to GitHub Actions.

4. Run Actions > Pages > Run workflow once to publish the
   current release.
