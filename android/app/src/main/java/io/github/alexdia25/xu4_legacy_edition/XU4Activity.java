package io.github.alexdia25.xu4_legacy_edition;

import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.provider.DocumentsContract;
import android.provider.OpenableColumns;
import java.util.ArrayList;
import org.libsdl.app.SDLActivity;

/**
 * XU4 runs as an SDL3 app; SDL, Boron & the game are all in libmain.so.
 * The helpers here are for importing the game files (src/android.cpp).
 */
public class XU4Activity extends SDLActivity {
    private static final int PICK_FOLDER = 0x5534;
    private static volatile boolean folderDone;
    private static volatile String[] folderFiles;

    @Override
    protected String[] getLibraries() {
        return new String[] { "main" };
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
