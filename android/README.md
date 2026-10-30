Android
=======

The Android app is the same SDL3 build of XU4 as on the desktop, packed in
an APK by Gradle.  `dist/build-android.sh [version]` downloads the SDL3 and
Boron sources, builds the modules and runs Gradle; the Release workflow's
`android` job does the same and adds the APK to the release.

What is Android specific is in `src/android.cpp`:

 - The game data, disk images, ROMs, settings and saves are kept in the
   app's files folder, `Android/data/io.github.alexdia25.xu4_legacy_edition/files`,
   which a computer can reach over USB.
 - `render.pak` and the modules come in the APK and are copied to the
   app's internal storage when a new version first starts.
 - When `ultima4.zip` is missing, the app asks for it with the Android file
   picker and copies what is chosen into the files folder.

Signing key
-----------

An APK installs over an older one only when both are signed with the same
key.  Without a key the Release workflow signs with a throwaway debug key,
so each such build has to be uninstalled (losing the saves) before the next
one can be installed.  To give every release the same key, once:

1. Make a keystore (Java's keytool, e.g. in Git Bash or on Linux):

       keytool -genkeypair -keystore xu4.keystore -alias xu4 -keyalg RSA \
           -keysize 4096 -validity 36500 -dname "CN=XU4 Legacy Edition"

2. In the repository's Settings > Secrets and variables > Actions, add
   `ANDROID_KEYSTORE` (the keystore in base64: `base64 -w0 xu4.keystore`)
   and `ANDROID_KEYSTORE_PASSWORD` (the password chosen in step 1).

Keep `xu4.keystore` and its password safe and out of the repository.
