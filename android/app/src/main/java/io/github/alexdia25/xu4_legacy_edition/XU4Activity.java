package io.github.alexdia25.xu4_legacy_edition;

import android.app.AlertDialog;
import android.content.Intent;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.Uri;
import android.provider.DocumentsContract;
import android.os.Build;
import android.os.Bundle;
import android.util.TypedValue;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.provider.OpenableColumns;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
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
        hideSystemBars();
        checkForUpdate();
    }

    @Override
    protected void onResume() {
        super.onResume();
        hideSystemBars();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus)
            hideSystemBars();
    }

    /**
     * Play on the whole screen with the status & navigation bars hidden
     * (a swipe from the edge shows them for a moment).  SDL asks for this
     * with the old system UI flags, which newer Android versions & some
     * handhelds no longer honour, leaving the navigation bar over the
     * bottom of the game.
     */
    private void hideSystemBars() {
        Window window = getWindow();
        if (window == null)
            return;
        if (Build.VERSION.SDK_INT >= 28) {
            WindowManager.LayoutParams lp = window.getAttributes();
            lp.layoutInDisplayCutoutMode = (Build.VERSION.SDK_INT >= 30)
                ? WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS
                : WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            window.setAttributes(lp);
        }
        if (Build.VERSION.SDK_INT >= 30) {
            window.setDecorFitsSystemWindows(false);
            WindowInsetsController ic = window.getInsetsController();
            if (ic != null) {
                ic.setSystemBarsBehavior(
                    WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
                ic.hide(WindowInsets.Type.statusBars() |
                        WindowInsets.Type.navigationBars());
            }
        } else {
            window.getDecorView().setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_FULLSCREEN |
                    View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                    View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
                    View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                    View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
                    View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        }
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
     * SDL_ShowMessageBox().  SDL's own box puts the text & buttons in a plain
     * layout, so a long text (such as the first run's request for the game
     * files) runs off the bottom of a phone screen with the buttons, and
     * nothing can be chosen.  Here the text scrolls (drag, or up & down on a
     * gamepad or keyboard) and the buttons always stay on screen.  A gamepad
     * moves between the buttons with left & right, presses one with A and
     * takes the Escape button (Quit, Cancel) with B or Back.
     */
    @Override
    protected void messageboxCreateAndShow(Bundle args) {
        final int[] buttonFlags = args.getIntArray("buttonFlags");
        final int[] buttonIds = args.getIntArray("buttonIds");
        final String[] buttonTexts = args.getStringArray("buttonTexts");
        final int count = buttonTexts.length;
        final boolean column = count > 3;
        final int pad = dp(20);

        final AlertDialog dialog = new AlertDialog.Builder(this).create();
        dialog.setTitle(args.getString("title"));
        dialog.setCancelable(false);
        dialog.setOnDismissListener(d -> {
            synchronized (messageboxSelection) {
                messageboxSelection.notify();
            }
        });

        TextView message = new TextView(this);
        message.setText(args.getString("message"));
        message.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
        message.setPadding(pad, dp(8), pad, dp(8));
        final ScrollView textScroll = new ScrollView(this);
        textScroll.setFocusable(false);
        textScroll.setVerticalScrollBarEnabled(true);
        textScroll.setScrollbarFadingEnabled(false);
        textScroll.addView(message);

        LinearLayout buttons = new LinearLayout(this);
        buttons.setOrientation(column ? LinearLayout.VERTICAL
                                      : LinearLayout.HORIZONTAL);
        buttons.setGravity(buttonGravity(column));
        buttons.setPadding(dp(12), dp(4), dp(12), dp(8));

        Button enterButton = null, escapeButton = null;
        final Button[] all = new Button[count];
        for (int i = 0; i < count; ++i) {
            final int id = buttonIds[i];
            Button b = new Button(this, null, android.R.attr.buttonBarButtonStyle);
            b.setText(buttonTexts[i]);
            b.setOnClickListener(v -> {
                messageboxSelection[0] = id;
                dialog.dismiss();
            });
            if ((buttonFlags[i] & 1) != 0)      // RETURNKEY_DEFAULT
                enterButton = b;
            if ((buttonFlags[i] & 2) != 0)      // ESCAPEKEY_DEFAULT
                escapeButton = b;
            all[i] = b;
            buttons.addView(b, new LinearLayout.LayoutParams(
                    column ? ViewGroup.LayoutParams.MATCH_PARENT
                           : ViewGroup.LayoutParams.WRAP_CONTENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT));
        }
        final Button defaultButton = (enterButton != null) ? enterButton
                                   : (count > 0 ? all[0] : null);
        final Button cancelButton = escapeButton;

        // The text takes what room is left by the buttons; a long list of
        // buttons scrolls too.
        LinearLayout content = new LinearLayout(this);
        content.setOrientation(LinearLayout.VERTICAL);
        content.addView(textScroll, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f));
        if (column) {
            ScrollView buttonScroll = new ScrollView(this);
            buttonScroll.addView(buttons);
            content.addView(buttonScroll, new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f));
        } else {
            content.addView(buttons, new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT));
        }
        // Weights only share out a known height: make the content as tall
        // as it needs, up to what fits on the screen under the title.
        final LinearLayout frame = new LinearLayout(this) {
            @Override
            protected void onMeasure(int wSpec, int hSpec) {
                int screen = getResources().getDisplayMetrics().heightPixels;
                int max = Math.max(dp(120), screen - dp(110));
                int mode = MeasureSpec.getMode(hSpec);
                int size = MeasureSpec.getSize(hSpec);
                if (mode == MeasureSpec.UNSPECIFIED || size > max)
                    size = max;
                int natural = naturalHeight(content, wSpec);
                int h = Math.min(natural, size);
                super.onMeasure(wSpec, MeasureSpec.makeMeasureSpec(h,
                                MeasureSpec.EXACTLY));
            }
        };
        frame.setOrientation(LinearLayout.VERTICAL);
        frame.addView(content, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));
        dialog.setView(frame);

        dialog.setOnKeyListener((d, keyCode, event) -> {
            boolean up = event.getAction() == KeyEvent.ACTION_UP;
            View focus = dialog.getCurrentFocus();
            switch (keyCode) {
            case KeyEvent.KEYCODE_BUTTON_B:
            case KeyEvent.KEYCODE_BACK:
            case KeyEvent.KEYCODE_ESCAPE:
                if (cancelButton == null)
                    return keyCode != KeyEvent.KEYCODE_BACK;
                if (up)
                    cancelButton.performClick();
                return true;
            case KeyEvent.KEYCODE_BUTTON_A:
            case KeyEvent.KEYCODE_BUTTON_START:
            case KeyEvent.KEYCODE_DPAD_CENTER:
            case KeyEvent.KEYCODE_ENTER:
            case KeyEvent.KEYCODE_NUMPAD_ENTER:
                if (up) {
                    if (focus instanceof Button)
                        focus.performClick();
                    else if (defaultButton != null)
                        defaultButton.performClick();
                }
                return true;
            case KeyEvent.KEYCODE_PAGE_UP:
            case KeyEvent.KEYCODE_BUTTON_L1:
                if (! up)
                    textScroll.pageScroll(View.FOCUS_UP);
                return true;
            case KeyEvent.KEYCODE_PAGE_DOWN:
            case KeyEvent.KEYCODE_BUTTON_R1:
                if (! up)
                    textScroll.pageScroll(View.FOCUS_DOWN);
                return true;
            case KeyEvent.KEYCODE_DPAD_UP:
            case KeyEvent.KEYCODE_DPAD_DOWN:
                if (! column) {
                    // Up & down scroll the text; the buttons are in a row.
                    if (! up) {
                        int step = Math.max(dp(40), textScroll.getHeight() / 3);
                        textScroll.smoothScrollBy(0,
                            keyCode == KeyEvent.KEYCODE_DPAD_UP ? -step : step);
                    }
                    return true;
                }
                // fall through
            case KeyEvent.KEYCODE_DPAD_LEFT:
            case KeyEvent.KEYCODE_DPAD_RIGHT:
            case KeyEvent.KEYCODE_TAB:
                // The first press picks the default button.
                if (! (focus instanceof Button) && defaultButton != null) {
                    if (! up)
                        defaultButton.requestFocusFromTouch();
                    return true;
                }
                return false;
            }
            return false;
        });

        dialog.show();
        if (dialog.getWindow() != null) {
            // Keep the game's full screen (no status or navigation bar).
            dialog.getWindow().getDecorView().setSystemUiVisibility(
                    getWindow().getDecorView().getSystemUiVisibility());
        }
        if (defaultButton != null)
            defaultButton.requestFocus();
    }

    private static int buttonGravity(boolean column) {
        return column ? android.view.Gravity.FILL_HORIZONTAL
                      : android.view.Gravity.END | android.view.Gravity.CENTER_VERTICAL;
    }

    private static int naturalHeight(LinearLayout content, int wSpec) {
        int h = content.getPaddingTop() + content.getPaddingBottom();
        for (int i = 0; i < content.getChildCount(); ++i) {
            View c = content.getChildAt(i);
            View inner = (c instanceof ScrollView && ((ScrollView) c).getChildCount() > 0)
                       ? ((ScrollView) c).getChildAt(0) : c;
            int w = View.MeasureSpec.getSize(wSpec);
            inner.measure(View.MeasureSpec.makeMeasureSpec(w, View.MeasureSpec.EXACTLY),
                          View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
            h += inner.getMeasuredHeight();
        }
        return h;
    }

    private int dp(int v) {
        return Math.round(v * getResources().getDisplayMetrics().density);
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
