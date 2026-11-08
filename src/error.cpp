/*
 * $Id$
 */

#include <cstdio>
#include <cstdarg>
#include <cstdlib>

#include "error.h"

#if defined(_WIN32) || defined(__CYGWIN__)

/*
 * Windows: errors shown in message box
 */

#include <windows.h>

void errorFatal(const char *fmt, ...) {
    char buffer[1000];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    MessageBox(NULL, buffer, "XU4 Error", MB_OK | MB_ICONERROR);

    exit(1);
}

void errorWarning(const char *fmt, ...) {
    char buffer[1000];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    MessageBox(NULL, buffer, "XU4 Warning", MB_OK | MB_ICONWARNING);
}

#elif defined(MACOSX)

/*
 * MacOS X: errors functios defined in objective-c code elsewhere.
 */

#elif GTK2_DIALOGS

/*
 * Linux/Unix with GTK2: errors shown in message box
 */

#include <gtk/gtk.h>

int need_gtk_init = 1;

void errorFatal(const char *fmt, ...) {
    char buffer[1000];
    va_list args;
    GtkWidget *dialog;

    if (need_gtk_init) {
        gtk_init(NULL, 0);
        need_gtk_init = 0;
    }

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    dialog = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "xu4: %s", buffer);
    va_end(args);

    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy (dialog);

    exit(1);
}

void errorWarning(const char *fmt, ...) {
    char buffer[1000];
    va_list args;
    GtkWidget *dialog;

    if (need_gtk_init) {
        gtk_init(NULL, 0);
        need_gtk_init = 0;
    }

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    dialog = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, "xu4: %s", buffer);
    va_end(args);

    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    while(gtk_events_pending())
        gtk_main_iteration();
}

#else

/*
 * Errors go to the standard error stream; a fatal error is also shown in an
 * SDL message box, as a game started from a launcher (Finder, Steam, a
 * desktop menu) has no terminal.
 */

#include <SDL3/SDL.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#ifndef VERSION
#define VERSION "unknown"
#endif

/*
 * The error box has Copy Details, which puts the message, the version &
 * the platform on the clipboard for a bug report.
 */
void errorFatal(const char *fmt, ...) {
    char buffer[1000];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    fprintf(stderr, "xu4: error: %s\n", buffer);

#ifdef __EMSCRIPTEN__
    // The page shows its own box, with the log and Export Saves.
    EM_ASM({ if (Module.xu4Error) Module.xu4Error(UTF8ToString($0)); },
           buffer);
#else
    const SDL_MessageBoxButtonData buttons[] = {
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT |
          SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Close" },
        { 0, 1, "Copy Details" }
    };
    SDL_MessageBoxData box = {
        SDL_MESSAGEBOX_ERROR, NULL, "XU4 Error", buffer, 2, buttons, NULL
    };
    int choice = 0;
    if (SDL_ShowMessageBox(&box, &choice) && choice == 1) {
        char details[1200];
        snprintf(details, sizeof(details), "XU4 Legacy Edition %s (%s)\n%s",
                 VERSION, SDL_GetPlatform(), buffer);
        if (SDL_SetClipboardText(details)) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "XU4 Error",
                    "The details were copied. Paste them into a bug "
                    "report, please.", NULL);
        }
    }
#endif

    exit(1);
}

void errorWarning(const char *fmt, ...) {
    va_list args;

    fprintf(stderr, "xu4: warning: ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

#endif
