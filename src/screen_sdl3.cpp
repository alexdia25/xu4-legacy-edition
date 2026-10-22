/*
 * XU4 - SDL3 Screen Interface
 * Based on screen_glfw.cpp, Copyright (C) 2024  Karl Robillard
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"
#include "error.h"
#include "event.h"
#include "gamepad.h"
#include "gpu_opengl.h"
#include "image.h"
#include "settings.h"
#include "screen.h"
#include "u4.h"
#include "xu4.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>

#define MAX_CURSORS 4

struct ScreenSDL {
    SDL_Window* view;
    SDL_GLContext glContext;
    Controller* waitCon;
    updateScreenCallback update;
    SDL_Cursor* cursor[MAX_CURSORS];
    MouseCursor currentCursor;
    SDL_Gamepad* gamepad;
    OpenGLResources gpu;
};

#define SGL ((ScreenSDL*) xu4.screenSys)


#define CURSORSIZE 24
#define XPMSIZE    32
#include "cursors.h"

/**
 * Copy xpm into an Image32.
 */
static void _copyCursor(Image32* img, const char* const xpm[], int destY)
{
    RGBA white, black, empty;
    RGBA* dp;
    int row, col;

    rgba_set(white, 255, 255, 255, 255);
    rgba_set(black, 0, 0, 0, 255);
    rgba_set(empty, 0, 0, 0, 0);

    dp = (RGBA*) (img->pixels + (img->w * destY));

    for (row=0; row < CURSORSIZE; row++) {
        for (col=0; col < CURSORSIZE; col++) {
            switch (xpm[4+row][col]) {
                case 'X':
                    *dp++ = black;
                    break;
                case '.':
                    *dp++ = white;
                    break;
                case ' ':
                    *dp++ = empty;
                    break;
            }
        }
    }
}

static void _loadCursors(SDL_Cursor** cursor)
{
    static const uint8_t hotXY[2*MAX_CURSORS] = { 0,6,  6,0,  18,6,  6,18 };
    static const char* const* xpm[MAX_CURSORS] = {
        w_xpm, n_xpm, e_xpm, s_xpm      // Match MouseCursor order.
    };
    Image32 img;
    image32_allocPixels(&img, CURSORSIZE, CURSORSIZE);

    for (int i = 0; i < MAX_CURSORS; ++i) {
        _copyCursor(&img, xpm[i], 0);
        SDL_Surface* surf = SDL_CreateSurfaceFrom(img.w, img.h,
                                SDL_PIXELFORMAT_RGBA32, img.pixels, img.w * 4);
        cursor[i] = surf ? SDL_CreateColorCursor(surf, hotXY[i*2],
                                                       hotXY[i*2+1]) : NULL;
        SDL_DestroySurface(surf);
    }

    image32_freePixels(&img);
}

#if defined(__linux__)
extern Image* loadImage_png(U4FILE *file);

static void _setWindowIcon(SDL_Window* view, const char* filename) {
    U4FILE* fp = u4fopen_stdio(filename);
    if (fp) {
        Image* img = loadImage_png(fp);
        u4fclose(fp);
        if (img) {
            SDL_Surface* surf = SDL_CreateSurfaceFrom(img->w, img->h,
                                SDL_PIXELFORMAT_RGBA32, img->pixels, img->w * 4);
            if (surf) {
                SDL_SetWindowIcon(view, surf);
                SDL_DestroySurface(surf);
            }
            delete img;
        }
    }
}
#endif


#include "gpu_opengl.cpp"


static void dispatchKey(int key)
{
#ifdef DEBUG
    xu4.eventHandler->recordKey(key);
#endif
    Controller* controller = SGL->waitCon;
    if (! controller)
        controller = xu4.eventHandler->getController();
    if (controller->notifyKeyPressed(key)) {
        updateScreenCallback updateScreen = SGL->update;
        if (updateScreen)
            (*updateScreen)();
    }
}

static void keyHandler(const SDL_KeyboardEvent* ev)
{
    static const char shiftNum_US[] = ")!@#$%^&*(";
    SDL_Keycode token = ev->key;
    SDL_Keymod mods = ev->mod;
    int key;

    switch (token) {
        case SDLK_ESCAPE:
            key = U4_ESC;
            break;
        case SDLK_RETURN:
            key = U4_ENTER;
            break;
        case SDLK_TAB:
            key = U4_TAB;
            break;
        case SDLK_UP:
            key = U4_UP;
            break;
        case SDLK_DOWN:
            key = U4_DOWN;
            break;
        case SDLK_LEFT:
            key = U4_LEFT;
            break;
        case SDLK_RIGHT:
            key = U4_RIGHT;
            break;
        case SDLK_BACKSPACE:
        case SDLK_DELETE:
            key = U4_BACKSPACE;
            break;
        case SDLK_PAUSE:
            key = U4_PAUSE;
            break;
        case SDLK_KP_ENTER:
            key = U4_KEYPAD_ENTER;
            break;
        case SDLK_HOME:
            key = U4_HOME;
            break;
        case SDLK_END:
            key = U4_END;
            break;
        case SDLK_PAGEUP:
            key = U4_PAGE_UP;
            break;
        case SDLK_PAGEDOWN:
            key = U4_PAGE_DOWN;
            break;
        default:
            if (token >= SDLK_A && token <= SDLK_Z) {
                key = token;    // Lower case ASCII.
                if (mods & SDL_KMOD_CTRL) {
                    // Map to ASCII Control Code.
                    key -= 'a' - 1;
                } else if (mods & SDL_KMOD_SHIFT)
                    key -= 'a' - 'A';
            }
            else if (token >= SDLK_0 && token <= SDLK_9) {
                if (mods & SDL_KMOD_SHIFT)
                    key = shiftNum_US[token - SDLK_0];
                else
                    key = token;
            }
            else if (token >= SDLK_F1 && token <= SDLK_F12)
                key = U4_FKEY + (token - SDLK_F1);
            else if (token >= ' ' && token < 0x7F)
                key = token;
            else
                return;
            break;
    }

    if (mods & SDL_KMOD_ALT)
        key += U4_ALT;
    else if (mods & SDL_KMOD_GUI)
        key += U4_META;

    if (xu4.verbose)
        printf("key event: token %d, mod 0x%x; translated %d\n",
               (int) token, (int) mods, key);

    dispatchKey(key);
}

static inline void dispatchEvent(InputEvent* ie)
{
    Controller* controller = SGL->waitCon;
    if (! controller)
        controller = xu4.eventHandler->getController();
    controller->inputEvent(ie);
}

static void mouseButtonHandler(const SDL_MouseButtonEvent* ev)
{
    InputEvent ie;

    ie.type = ev->down ? IE_MOUSE_PRESS : IE_MOUSE_RELEASE;
    ie.n = ev->button;  // SDL_BUTTON_LEFT/MIDDLE/RIGHT match CMOUSE_*.
    ie.state = 0;
    ie.x = (int16_t) ev->x;
    ie.y = (int16_t) ev->y;

    dispatchEvent(&ie);
}

//----------------------------------------------------------------------------
// Gamepad

static void openGamepad(ScreenSDL* ss, SDL_JoystickID id)
{
    if (ss->gamepad)
        return;
    ss->gamepad = SDL_OpenGamepad(id);
    if (ss->gamepad && xu4.verbose)
        printf("Gamepad: %s\n", SDL_GetGamepadName(ss->gamepad));
    gamepad_reset();
}

static void closeGamepad(ScreenSDL* ss, SDL_JoystickID id)
{
    if (ss->gamepad && SDL_GetGamepadID(ss->gamepad) == id) {
        SDL_CloseGamepad(ss->gamepad);
        ss->gamepad = NULL;

        // Use another one if present.
        int count;
        SDL_JoystickID* ids = SDL_GetGamepads(&count);
        if (ids) {
            if (count > 0)
                openGamepad(ss, ids[0]);
            SDL_free(ids);
        }
    }
}

static void readGamepad(SDL_Gamepad* gp)
{
    static const SDL_GamepadButton map[PAD_BUTTON_COUNT] = {
        SDL_GAMEPAD_BUTTON_SOUTH,       // PAD_A
        SDL_GAMEPAD_BUTTON_EAST,        // PAD_B
        SDL_GAMEPAD_BUTTON_WEST,        // PAD_X
        SDL_GAMEPAD_BUTTON_NORTH,       // PAD_Y
        SDL_GAMEPAD_BUTTON_BACK,
        SDL_GAMEPAD_BUTTON_START,
        SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
        SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
        SDL_GAMEPAD_BUTTON_DPAD_UP,
        SDL_GAMEPAD_BUTTON_DPAD_DOWN,
        SDL_GAMEPAD_BUTTON_DPAD_LEFT,
        SDL_GAMEPAD_BUTTON_DPAD_RIGHT
    };
    PadState ps;

    ps.buttons = 0;
    for (int i = 0; i < PAD_BUTTON_COUNT; ++i) {
        if (SDL_GetGamepadButton(gp, map[i]))
            ps.buttons |= PAD_BIT(i);
    }
    ps.axisX = SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
    ps.axisY = SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;

    gamepad_update(&ps, (uint32_t) SDL_GetTicks(), dispatchKey);
}

//----------------------------------------------------------------------------

extern int screenInitState(ScreenState*, const Settings*, int dw, int dh);

void screenInit_sys(const Settings* settings, ScreenState* state, int reset)
{
    ScreenSDL* ss;
    const char* gpuError;
    int scale = settings->scale;
    int dw = U4_SCREEN_W * scale;
    int dh = ((settings->filter >= FILTER_POINT_43) ? 240 : U4_SCREEN_H) * scale;
    char title[MOD_NAME_LIMIT];

    xu4.config->gameTitle(title);

    if (settings->fullscreen) {
        const SDL_DisplayMode* mode =
            SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
        if (mode) {
            dw = mode->w;
            dh = mode->h;
        }
    }

    if (reset) {
        ss = SGL;
        gpu_free(&ss->gpu);

        SDL_SetWindowFullscreen(ss->view, settings->fullscreen);
        if (! settings->fullscreen)
            SDL_SetWindowSize(ss->view, dw, dh);
        SDL_SetWindowTitle(ss->view, title);
    } else {
        xu4.screenSys = ss = (ScreenSDL*) calloc(1, sizeof(ScreenSDL));

        if (! SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
            errorFatal("Unable to initialize SDL (%s)", SDL_GetError());

#if defined(USE_GLES)
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#else
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#endif
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

        SDL_WindowFlags flags = SDL_WINDOW_OPENGL;
        if (settings->fullscreen)
            flags |= SDL_WINDOW_FULLSCREEN;

        ss->view = SDL_CreateWindow(title, dw, dh, flags);
        if (! ss->view)
            errorFatal("Unable to create SDL window (%s)", SDL_GetError());

        ss->glContext = SDL_GL_CreateContext(ss->view);
        if (! ss->glContext)
            errorFatal("Unable to create OpenGL context (%s)", SDL_GetError());

#if defined(__linux__)
        _setWindowIcon(ss->view, "/usr/share/icons/hicolor/48x48/apps/xu4.png");
#endif
        _loadCursors(ss->cursor);

        // Gamepads already connected also arrive as
        // SDL_EVENT_GAMEPAD_ADDED events.
        gamepad_reset();
    }

    //----------------------------------

    scale = screenInitState(state, settings, dw, dh);

    /* enable or disable the mouse cursor */
    screenShowMouseCursor(settings->mouseOptions.enabled);

    SDL_GL_MakeCurrent(ss->view, ss->glContext);
    SDL_GL_SetSwapInterval(1);
#if !defined(__APPLE__) && !defined(USE_GLES)
    if (! reset) {
        if (! gladLoadGLLoader((GLADloadproc) SDL_GL_GetProcAddress))
            errorFatal("Unable to get OpenGL function addresses");
    }
#endif

    xu4.gpu = &ss->gpu;
    gpuError = gpu_init(&ss->gpu, state->aspectW, state->aspectH, scale,
                        settings->filter);
    if (gpuError)
        errorFatal("Unable to obtain OpenGL resource (%s)", gpuError);
}

void screenDelete_sys()
{
    ScreenSDL* ss = SGL;

    gpu_free(&ss->gpu);

    if (ss->gamepad)
        SDL_CloseGamepad(ss->gamepad);
    for (int i = 0; i < MAX_CURSORS; ++i) {
        if (ss->cursor[i])
            SDL_DestroyCursor(ss->cursor[i]);
    }
    SDL_GL_DestroyContext(ss->glContext);
    SDL_DestroyWindow(ss->view);
    SDL_Quit();

    free(ss);
    xu4.screenSys = NULL;
}

/**
 * Attempts to iconify the screen.
 */
void screenIconify() {
    SDL_MinimizeWindow(SGL->view);
}

//#define CPU_TEST
#include "cpuCounter.h"

extern void screenRender();

void screenSwapBuffers()
{
    CPU_START()
    screenRender();
    SDL_GL_SwapWindow(SGL->view);
    CPU_END("ut:")
}

extern void msecSleep(uint32_t);

void screenWait(int numberOfAnimationFrames) {
#ifndef GPU_RENDER
    screenUploadToGPU();
#endif

    assert(numberOfAnimationFrames >= 0);

    screenSwapBuffers();
    msecSleep((1000 * numberOfAnimationFrames) /
              xu4.settings->screenAnimationFramesPerSecond);
}

void screenSetMouseCursor(MouseCursor cursor)
{
    ScreenSDL* sg = SGL;
    if (cursor != sg->currentCursor) {
        SDL_Cursor* sc = (cursor == MC_DEFAULT) ? NULL
                                                : sg->cursor[cursor - 1];
        SDL_SetCursor(sc ? sc : SDL_GetDefaultCursor());
        sg->currentCursor = cursor;
    }
}

void screenShowMouseCursor(bool visible)
{
    if (visible)
        SDL_ShowCursor();
    else
        SDL_HideCursor();
}

/*
 * \\param waitCon  Input events are passed to this controller if not NULL.
 *                 Otherwise EventHandler::getController() (the currently
 *                 active one) will be used.
 */
void EventHandler::handleInputEvents(Controller* waitCon,
                                     updateScreenCallback update)
{
    ScreenSDL* ss = SGL;
    Controller* prevCon = ss->waitCon;
    updateScreenCallback prevUpdate = ss->update;
    SDL_Event event;
    InputEvent ie;

    ss->waitCon = waitCon;
    ss->update  = update;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_KEY_DOWN:
                keyHandler(&event.key);
                break;

            case SDL_EVENT_MOUSE_MOTION:
                ie.type  = IE_MOUSE_MOVE;
                ie.n     = 0;
                ie.state = 0;
                ie.x = (int16_t) event.motion.x;
                ie.y = (int16_t) event.motion.y;
                dispatchEvent(&ie);
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                mouseButtonHandler(&event.button);
                break;

            case SDL_EVENT_MOUSE_WHEEL:
                ie.type  = IE_MOUSE_WHEEL;
                ie.n     = 0;
                ie.state = 0;
                ie.x = (int16_t) event.wheel.x;
                ie.y = (int16_t) event.wheel.y;
                dispatchEvent(&ie);
                break;

            case SDL_EVENT_GAMEPAD_ADDED:
                openGamepad(ss, event.gdevice.which);
                break;

            case SDL_EVENT_GAMEPAD_REMOVED:
                closeGamepad(ss, event.gdevice.which);
                break;

            case SDL_EVENT_QUIT:
                quitGame();
                break;
        }
    }

    if (ss->gamepad)
        readGamepad(ss->gamepad);

    ss->waitCon = prevCon;
    ss->update  = prevUpdate;
}

/**
 * Sets the key-repeat characteristics of the keyboard.
 */
int EventHandler::setKeyRepeat(int delay, int interval) {
    (void) delay;
    (void) interval;
    return 0;
}
