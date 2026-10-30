/*
 * Web (Emscripten) support: the game data, settings & saves are kept in
 * /xu4, which the page (web/shell.html) mounts from the browser's IndexedDB
 * before the game starts and writes back while it runs.  render.pak & the
 * modules are packed in /data.  When the Ultima IV files are missing, the
 * page asks the player to choose them.
 */

#include <emscripten.h>
#include "u4file.h"

// Show the page's file chooser and wait until the chosen files are saved
// in /xu4.  Returns the number of files saved, or 0 if none were chosen.
EM_ASYNC_JS(int, webChooseGameFiles, (int retry), {
    return await Module.xu4ChooseFiles(retry);
});

void webImportGameData() {
    int retry = 0;
    while (! u4fsetup()) {
        webChooseGameFiles(retry);
        retry = 1;
    }
}
