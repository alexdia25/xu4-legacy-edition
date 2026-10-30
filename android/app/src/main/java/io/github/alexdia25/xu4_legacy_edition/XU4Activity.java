package io.github.alexdia25.xu4_legacy_edition;

import org.libsdl.app.SDLActivity;

/**
 * XU4 runs as an SDL3 app; SDL, Boron & the game are all in libmain.so.
 */
public class XU4Activity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "main" };
    }
}
