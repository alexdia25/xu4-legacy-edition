package io.github.alexdia25.xu4_legacy_edition;

import android.app.AlertDialog;
import android.content.Intent;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.Uri;
import android.provider.DocumentsContract;
import android.os.Bundle;
import android.provider.OpenableColumns;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import org.json.JSONObject;
import org.libsdl.app.SDLActivity;

/**
 * XU4 runs as an SDL3 app; SDL, Boron & the game are all in libmain.so.
 * The helpers here are for importing the game files (src/android.cpp),
 * and the activity tells the player when a newer release is out.
 */
public class XU4Activity extends SDLActivity {
    private static final int PICK_FOLDER = 0x5534;
    private static volatile boolean folderDone;
    private static volatile String[] folderFiles;

    private static final String RELEASES =
            "https://github.com/alexdia25/xu4-legacy-edition/releases/latest";
    private static final String LATEST_API =
            "https://api.github.com/repos/alexdia25/xu4-legacy-edition/releases/latest";

    @Override
    protected String[] getLibraries() {
        return new String[] { "main" };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        checkForUpdate();
    }

    /**
     * Once a day, ask GitHub for the latest full release (pre-releases are
     * left out) and, if it is newer than this one, offer the download page.
     * "Not This One" stops the notice until a later release comes out.
     * Builds that are not a release (no plain version number) never ask.
     */
    private void checkForUpdate() {
        final String current;
        try {
            current = getPackageManager().getPackageInfo(getPackageName(), 0).versionName;
        } catch (Exception e) {
            return;
        }
        if (current == null || !current.matches("\\d+(\\.\\d+)*"))
            return;
        final SharedPreferences prefs = getSharedPreferences("update", MODE_PRIVATE);
        long now = System.currentTimeMillis();
        if (now - prefs.getLong("checked", 0) < 24L * 60 * 60 * 1000)
            return;

        new Thread(() -> {
            String latest = latestRelease();
            if (latest == null)
                return;
            prefs.edit().putLong("checked", now).apply();
            if (compareVersions(latest, current) <= 0 ||
                latest.equals(prefs.getString("skipped", "")))
                return;
            runOnUiThread(() -> {
                try {
                    new AlertDialog.Builder(this)
                        .setTitle("Update available")
                        .setMessage("XU4 Legacy Edition " + latest +
                                    " is out (you have " + current + ").")
                        .setPositiveButton("Download", (d, w) -> {
                            try {
                                startActivity(new Intent(Intent.ACTION_VIEW,
                                                         Uri.parse(RELEASES)));
                            } catch (Exception e) {
                            }
                        })
                        .setNegativeButton("Later", null)
                        .setNeutralButton("Not This One", (d, w) ->
                            prefs.edit().putString("skipped", latest).apply())
                        .show();
                } catch (Exception e) {
                }
            });
        }).start();
    }

    /**
     * Return the version of the latest release (its tag without "legacy-"),
     * or null if it cannot be found (no network, ...).
     */
    private static String latestRelease() {
        HttpURLConnection conn = null;
        try {
            conn = (HttpURLConnection) new URL(LATEST_API).openConnection();
            conn.setConnectTimeout(10000);
            conn.setReadTimeout(10000);
            conn.setRequestProperty("Accept", "application/vnd.github+json");
            conn.setRequestProperty("User-Agent", "xu4-legacy-edition");
            if (conn.getResponseCode() != 200)
                return null;
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            try (InputStream in = conn.getInputStream()) {
                byte[] buf = new byte[8192];
                int n;
                while ((n = in.read(buf)) > 0)
                    out.write(buf, 0, n);
            }
            String tag = new JSONObject(out.toString("UTF-8")).optString("tag_name");
            if (tag.startsWith("legacy-"))
                tag = tag.substring(7);
            return tag.matches("\\d+(\\.\\d+)*") ? tag : null;
        } catch (Exception e) {
            return null;
        } finally {
            if (conn != null)
                conn.disconnect();
        }
    }

    /**
     * Compare two dotted version numbers: 1.2 < 1.2.1 < 1.10.
     */
    static int compareVersions(String a, String b) {
        String[] pa = a.split("\\."), pb = b.split("\\.");
        for (int i = 0; i < Math.max(pa.length, pb.length); i++) {
            long va = i < pa.length ? parse(pa[i]) : 0;
            long vb = i < pb.length ? parse(pb[i]) : 0;
            if (va != vb)
                return va < vb ? -1 : 1;
        }
        return 0;
    }

    private static long parse(String n) {
        try {
            return Long.parseLong(n);
        } catch (NumberFormatException e) {
            return 0;
        }
    }

    /**
     * Return the file name of a document picked with the file dialog.
     */
    public static String displayName(String uri) {
        try (Cursor c = mSingleton.getContentResolver().query(Uri.parse(uri),
                new String[] { OpenableColumns.DISPLAY_NAME }, null, null, null)) {
            if (c != null && c.moveToFirst())
                return c.getString(0);
        } catch (Exception e) {
        }
        return null;
    }

    /**
     * Ask for a folder.  folderPickDone() is true once the player has
     * chosen, then folderPickFiles() has its files.
     */
    public static void pickFolder() {
        folderDone = false;
        folderFiles = null;
        mSingleton.runOnUiThread(() -> {
            try {
                mSingleton.startActivityForResult(
                        new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE), PICK_FOLDER);
            } catch (Exception e) {
                folderDone = true;
            }
        });
    }

    public static boolean folderPickDone() {
        return folderDone;
    }

    /**
     * Return the name & URI of each file in the chosen folder and the
     * folders inside it, one after the other, or null if none was chosen.
     */
    public static String[] folderPickFiles() {
        return folderFiles;
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode != PICK_FOLDER) {
            super.onActivityResult(requestCode, resultCode, data);
            return;
        }
        String[] files = null;
        if (resultCode == RESULT_OK && data != null && data.getData() != null) {
            Uri tree = data.getData();
            ArrayList<String> list = new ArrayList<>();
            try {
                listFolder(tree, DocumentsContract.getTreeDocumentId(tree), list, 0);
            } catch (Exception e) {
            }
            files = list.toArray(new String[0]);
        }
        folderFiles = files;
        folderDone = true;
    }

    private void listFolder(Uri tree, String docId, ArrayList<String> list, int depth) {
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, docId);
        String[] cols = { DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                          DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                          DocumentsContract.Document.COLUMN_MIME_TYPE };
        try (Cursor c = getContentResolver().query(children, cols, null, null, null)) {
            while (c != null && c.moveToNext()) {
                String id = c.getString(0);
                if (DocumentsContract.Document.MIME_TYPE_DIR.equals(c.getString(2))) {
                    if (depth < 2)
                        listFolder(tree, id, list, depth + 1);
                } else {
                    list.add(c.getString(1));
                    list.add(DocumentsContract.buildDocumentUriUsingTree(tree, id).toString());
                }
            }
        }
    }
}
