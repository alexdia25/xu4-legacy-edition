/*
 * sound_sdl3.cpp
 *
 * Sound effects, music & speech mixed by SDL3 audio streams.
 * Ogg Vorbis is decoded with stb_vorbis, WAVE with SDL & rFX sound
 * effects are synthesized with sfx_gen (from Faun).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <SDL3/SDL.h>
#include "sound.h"

#include "a2disk.h"
#include "a2music.h"

#include "config.h"
#include "context.h"
#include "debug.h"
#include "error.h"
#include "event.h"
#include "settings.h"
#include "u4file.h"
#include "xu4.h"

#define STB_VORBIS_HEADER_ONLY
#include "support/stb_vorbis.c"
#include "support/sfx_gen.h"
#include "support/well512.h"

#define config_soundFile(id)    xu4.config->soundFile(id)
#define config_musicFile(id)    xu4.config->musicFile(id)
#define config_voiceParts(id)   xu4.config->voiceParts(id)
#define BUFFER_LIMIT    SOUND_MAX
#define SOURCE_LIMIT    8       // Sound effect sources.
#define SID_END         7
#define MIX_RATE        44100
#define MIX_CHANNELS    2
#define FRAME_BYTES     (sizeof(float) * MIX_CHANNELS)
#define A2MUSIC_RATE    44100

// Module music entry naming a tune on the Apple II disks ("disk/FILE:tune").
#define XU4_A2MUSIC     CDI32(0xDA,0x7A,0xF0,0xA2)
#define BUFFER_MS_FAILED    1

#define MAKE_ID(a,b,c,d)    (a | b<<8 | c<<16 | d<<24)
#define ID_RIFF     MAKE_ID('R','I','F','F')
#define ID_OGGS     MAKE_ID('O','g','g','S')
#define ID_RFX_     MAKE_ID('r','F','X',' ')

enum PlayMode {
    PLAY_ONCE    = 1,
    PLAY_LOOP    = 2,
    PLAY_FADE_IN = 4
};

// Decoded sound effect samples (float, stereo, MIX_RATE).
struct SoundBuffer {
    float* samples;
    uint32_t frames;
};

// Music player state.  This is used by the audio thread in music_feed(),
// so it must only be changed while the music stream is locked.
struct MusicPlayer {
    std::vector<uint8_t> ogg;       // Vorbis file data.
    std::vector<float> pcm;         // Apple II music (stereo).
    stb_vorbis* vorbis;
    uint32_t pcmPos;
    int channels;
    int rate;
    int mode;
    bool playing;
    float fade;                     // Current fade gain (0.0 - 1.0).
    float fadeStep;                 // Fade gain change per frame.
};

struct SpeechStream {
    std::vector<uint8_t> ogg;
    stb_vorbis* vorbis;
};

static SDL_AudioDeviceID audioDev;
static SDL_AudioStream* sfxStream[SOURCE_LIMIT];
static SDL_AudioStream* speechStream;
static SDL_AudioStream* musicStream;
static MusicPlayer music;
static SpeechStream speech;
static SoundBuffer buffers[BUFFER_LIMIT];

static int currentTrack;
static int currentDialog;
static int musicEnabled;
static int nextSource;              // Use sources in round-robin order.
static int musicFadeMs;
static float soundVolume = 0.0f;
static float musicVolume = 0.0f;
static uint16_t bufferMs[BUFFER_LIMIT];
static uint16_t bufferResGroup[BUFFER_LIMIT];

//----------------------------------------------------------------------------

static Well512 sfxRng;

extern "C" int sfx_random(int range) {
    return well512_genU32(&sfxRng) % range;
}

/*
 * Read a file or a portion of one into memory.
 * If bytes is zero then the entire file is read.
 */
static bool readFileChunk(const char* path, uint32_t offset, uint32_t bytes,
                          std::vector<uint8_t>& data)
{
    FILE* fp = fopen(path, "rb");
    if (! fp)
        return false;
    if (! bytes) {
        fseek(fp, 0, SEEK_END);
        long len = ftell(fp);
        bytes = (len > 0) ? len : 0;
    }
    data.resize(bytes);
    bool ok = (fseek(fp, offset, SEEK_SET) == 0) &&
              (fread(data.data(), 1, bytes, fp) == bytes);
    fclose(fp);
    return ok && bytes > 0;
}

/*
 * Convert samples to the mixing format and store them in a SoundBuffer.
 */
static bool storeSamples(SoundBuffer* buf, const SDL_AudioSpec* spec,
                         const void* data, int len)
{
    static const SDL_AudioSpec mixSpec =
        { SDL_AUDIO_F32, MIX_CHANNELS, MIX_RATE };
    Uint8* out;
    int outLen;

    if (! SDL_ConvertAudioSamples(spec, (const Uint8*) data, len,
                                  &mixSpec, &out, &outLen))
        return false;
    buf->samples = (float*) out;
    buf->frames = outLen / FRAME_BYTES;
    return buf->frames > 0;
}

/*
 * Decode a WAVE, Ogg Vorbis, or rFX file into a SoundBuffer.
 */
static const char* decodeSound(SoundBuffer* buf, const uint8_t* data,
                               size_t size)
{
    SDL_AudioSpec spec;

    if (size < 12)
        return "File too small";

    uint32_t id = MAKE_ID(data[0], data[1], data[2], data[3]);
    if (id == ID_RIFF) {
        Uint8* wav;
        Uint32 wavLen;
        SDL_IOStream* io = SDL_IOFromConstMem(data, size);
        if (! SDL_LoadWAV_IO(io, true, &spec, &wav, &wavLen))
            return SDL_GetError();
        bool ok = storeSamples(buf, &spec, wav, wavLen);
        SDL_free(wav);
        return ok ? NULL : "WAVE conversion failed";
    }
    else if (id == ID_OGGS) {
        int channels, rate;
        short* pcm;
        int frames = stb_vorbis_decode_memory(data, (int) size,
                                              &channels, &rate, &pcm);
        if (frames <= 0)
            return "Ogg decode failed";
        spec.format = SDL_AUDIO_S16;
        spec.channels = channels;
        spec.freq = rate;
        bool ok = storeSamples(buf, &spec, pcm,
                               frames * channels * sizeof(short));
        free(pcm);
        return ok ? NULL : "Ogg conversion failed";
    }
    else if (id == ID_RFX_) {
        SfxParams sp;
        int version = data[4] | data[5] << 8;
        if (version != 200)
            return "rFX file version not supported";
        if (size < 8 + sizeof(SfxParams))
            return "rFX file too small";
        memcpy(&sp, data + 8, sizeof(SfxParams));

        SfxSynth* synth = sfx_allocSynth(SFX_F32, MIX_RATE, 6);
        if (! synth)
            return "rFX synth allocation failed";
        well512_init(&sfxRng, sp.randSeed);
        int frames = sfx_generateWave(synth, &sp);
        spec.format = SDL_AUDIO_F32;
        spec.channels = 1;
        spec.freq = MIX_RATE;
        bool ok = storeSamples(buf, &spec, synth->samples.f,
                               frames * sizeof(float));
        free(synth);
        return ok ? NULL : "rFX conversion failed";
    }
    return "Unsupported sound format";
}

//----------------------------------------------------------------------------

static void music_close()
{
    if (music.vorbis) {
        stb_vorbis_close(music.vorbis);
        music.vorbis = NULL;
    }
    music.ogg.clear();
    music.pcm.clear();
    music.playing = false;
}

/*
 * Read frames from the current music track, looping if required.
 * Return the number of frames read.
 */
static int music_read(float* out, int frames)
{
    int total = 0;
    int ch = music.channels;

    while (total < frames) {
        int n;
        if (music.vorbis) {
            n = stb_vorbis_get_samples_float_interleaved(music.vorbis, ch,
                                    out + total*ch, (frames - total)*ch);
        } else {
            uint32_t avail = music.pcm.size() / ch - music.pcmPos;
            n = frames - total;
            if ((uint32_t) n > avail)
                n = avail;
            memcpy(out + total*ch, music.pcm.data() + music.pcmPos*ch,
                   n * ch * sizeof(float));
            music.pcmPos += n;
        }

        if (n > 0) {
            total += n;
        } else {
            if (! (music.mode & PLAY_LOOP))
                break;
            if (music.vorbis)
                stb_vorbis_seek_start(music.vorbis);
            else if (music.pcmPos == 0)
                break;              // Empty buffer.
            else
                music.pcmPos = 0;
        }
    }
    return total;
}

/*
 * SDL audio stream callback which decodes music on demand.
 * The stream lock is held while this is called.
 */
static void SDLCALL music_feed(void*, SDL_AudioStream* stream,
                               int additional, int)
{
    float buf[4096];

    while (additional > 0 && music.playing) {
        int ch = music.channels;
        int frames = additional / (sizeof(float) * ch);
        if (frames < 1)
            frames = 1;
        int maxFrames = 4096 / ch;
        if (frames > maxFrames)
            frames = maxFrames;

        int n = music_read(buf, frames);
        if (n < 1) {
            music.playing = false;
            break;
        }

        // Apply fade.
        if (music.fadeStep != 0.0f || music.fade != 1.0f) {
            float* it = buf;
            for (int i = 0; i < n; ++i) {
                music.fade += music.fadeStep;
                if (music.fade >= 1.0f) {
                    music.fade = 1.0f;
                    music.fadeStep = 0.0f;
                } else if (music.fade <= 0.0f) {
                    music.fade = 0.0f;
                    music.fadeStep = 0.0f;
                    music.playing = false;
                    n = i;
                    break;
                }
                for (int c = 0; c < ch; ++c)
                    *it++ *= music.fade;
            }
        }

        int bytes = n * ch * sizeof(float);
        if (bytes)
            SDL_PutAudioStreamData(stream, buf, bytes);
        additional -= bytes;
    }
}

/*
 * Begin playing a music source which has been set up in the music struct.
 * Must be called with the music stream locked.
 */
static void music_begin(int mode)
{
    SDL_AudioSpec spec = { SDL_AUDIO_F32, music.channels, music.rate };

    SDL_ClearAudioStream(musicStream);
    SDL_SetAudioStreamFormat(musicStream, &spec, NULL);
    music.mode = mode;
    music.pcmPos = 0;
    music.playing = true;
    if (mode & PLAY_FADE_IN) {
        music.fade = 0.0f;
        music.fadeStep = (musicFadeMs > 0) ?
                            1000.0f / (float(musicFadeMs) * music.rate) : 1.0f;
    } else {
        music.fade = 1.0f;
        music.fadeStep = 0.0f;
    }
}

static bool music_openOgg(std::vector<uint8_t>& data, int mode)
{
    int err;
    stb_vorbis* vorb = stb_vorbis_open_memory(data.data(), (int) data.size(),
                                              &err, NULL);
    if (! vorb) {
        errorWarning("Music: Ogg open failed (%d)", err);
        return false;
    }
    stb_vorbis_info info = stb_vorbis_get_info(vorb);

    SDL_LockAudioStream(musicStream);
    music_close();
    music.ogg.swap(data);
    music.vorbis = vorb;
    music.channels = info.channels;
    music.rate = info.sample_rate;
    music_begin(mode);
    SDL_UnlockAudioStream(musicStream);
    return true;
}

//----------------------------------------------------------------------------

/*
 * Initialize sound & music service.
 */
int soundInit()
{
    static const SDL_AudioSpec mixSpec =
        { SDL_AUDIO_F32, MIX_CHANNELS, MIX_RATE };
    int i;

    currentTrack = MUSIC_NONE;
    currentDialog = 0;
    nextSource = 0;
    musicFadeMs = 0;
    memset(bufferMs, 0, sizeof(bufferMs));
    memset(bufferResGroup, 0, sizeof(bufferResGroup));
    memset(buffers, 0, sizeof(buffers));

    if (! SDL_InitSubSystem(SDL_INIT_AUDIO))
        goto fail;
    audioDev = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (! audioDev)
        goto fail;

    for (i = 0; i < SOURCE_LIMIT; ++i) {
        sfxStream[i] = SDL_CreateAudioStream(&mixSpec, NULL);
        if (! sfxStream[i])
            goto fail;
    }
    speechStream = SDL_CreateAudioStream(&mixSpec, NULL);
    musicStream  = SDL_CreateAudioStream(&mixSpec, NULL);
    if (! speechStream || ! musicStream)
        goto fail;
    SDL_SetAudioStreamGetCallback(musicStream, music_feed, NULL);

    if (! SDL_BindAudioStreams(audioDev, sfxStream, SOURCE_LIMIT) ||
        ! SDL_BindAudioStream(audioDev, speechStream) ||
        ! SDL_BindAudioStream(audioDev, musicStream))
        goto fail;

    musicEnabled = 1;
    musicSetVolume(xu4.settings->musicVol);
    soundSetVolume(xu4.settings->soundVol);
    return 1;

fail:
    errorWarning("Audio: %s", SDL_GetError());
    soundDelete();
    soundVolume = musicVolume = 0.0f;
    return 0;
}

void soundDelete()
{
    int i;

    // Destroying the streams unbinds them & stops music_feed.
    for (i = 0; i < SOURCE_LIMIT; ++i) {
        SDL_DestroyAudioStream(sfxStream[i]);
        sfxStream[i] = NULL;
    }
    SDL_DestroyAudioStream(speechStream);
    SDL_DestroyAudioStream(musicStream);
    speechStream = musicStream = NULL;

    if (audioDev) {
        SDL_CloseAudioDevice(audioDev);
        audioDev = 0;
    }

    music_close();
    if (speech.vorbis) {
        stb_vorbis_close(speech.vorbis);
        speech.vorbis = NULL;
    }
    speech.ogg.clear();

    for (i = 0; i < BUFFER_LIMIT; ++i) {
        SDL_free(buffers[i].samples);
        buffers[i].samples = NULL;
    }

    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void soundSuspend(int halt)
{
    if (! audioDev)
        return;
    if (halt)
        SDL_PauseAudioDevice(audioDev);
    else
        SDL_ResumeAudioDevice(audioDev);
}

void soundFreeResourceGroup(uint16_t group)
{
    int i;
    for (i = 0; i < BUFFER_LIMIT; ++i) {
        if (bufferMs[i] > BUFFER_MS_FAILED && bufferResGroup[i] == group) {
            bufferMs[i] = bufferResGroup[i] = 0;
            SDL_free(buffers[i].samples);
            buffers[i].samples = NULL;
            buffers[i].frames = 0;
        }
    }
}

static int loadSoundBuffer(int sound)
{
    std::vector<uint8_t> data;
    const char* error = "";
    SoundBuffer* buf = buffers + sound;
    uint16_t ms;

#ifdef CONF_MODULE
    const CDIEntry* ent = config_soundFile(sound);
    if (ent && readFileChunk(xu4.config->modulePath(ent), ent->offset,
                             ent->bytes, data))
        error = decodeSound(buf, data.data(), data.size());
#else
    const char* pathname = config_soundFile(sound);
    if (pathname && readFileChunk(pathname, 0, 0, data))
        error = decodeSound(buf, data.data(), data.size());
#endif

    if (! error) {
        uint32_t len = buf->frames * 1000 / MIX_RATE;
        ms = (len > BUFFER_MS_FAILED) ? (uint16_t) len : BUFFER_MS_FAILED+1;
    } else {
        // Mark buffer as loaded even upon failure so we don't keep trying
        // to load bad or nonexistent data.
        if (error[0])
            errorWarning("Sound %d: %s", sound, error);
        ms = BUFFER_MS_FAILED;
    }

    bufferMs[sound] = ms;
    bufferResGroup[sound] = xu4.resGroup;
    return ms;
}

void soundPlay(Sound sound, int limitMSec)
{
    ASSERT(sound < SOUND_MAX, "Invalid soundPlay() id");

    // Do nothing if muted or soundInit failed.
    if (soundVolume <= 0.0f)
        return;

    if (bufferMs[sound] == 0)
        loadSoundBuffer(sound);

    const SoundBuffer* buf = buffers + sound;
    if (! buf->samples)
        return;

    uint32_t frames = buf->frames;
    SDL_AudioStream* stream;

    // The source SID_END is reserved for when limitMSec is used.
    if (limitMSec > 0) {
        stream = sfxStream[SID_END];
        uint32_t limit = uint32_t(limitMSec) * MIX_RATE / 1000;
        if (frames > limit)
            frames = limit;
    } else {
        stream = sfxStream[nextSource];
        if (++nextSource >= SID_END)
            nextSource = 0;
    }

    SDL_ClearAudioStream(stream);
    SDL_PutAudioStreamData(stream, buf->samples, frames * FRAME_BYTES);
}

/*
 * Play a line of spoken dialogue.
 */
void soundSpeakLine(int streamId, int line, bool wait) {
#ifdef CONF_MODULE
    if (soundVolume <= 0.0f || streamId < 1)
        return;

    const float* streamPart = config_voiceParts(streamId);
    if (! streamPart)
        return;
    streamPart += line * 2;
    if (streamPart[0] < 0.3f)           // Ignore NUL entries.
        return;

    if (streamId != currentDialog) {
        currentDialog = 0;
        if (speech.vorbis) {
            stb_vorbis_close(speech.vorbis);
            speech.vorbis = NULL;
        }

        const CDIEntry* ent = config_musicFile(streamId);
        if (ent && readFileChunk(xu4.config->modulePath(ent), ent->offset,
                                 ent->bytes, speech.ogg)) {
            int err;
            speech.vorbis = stb_vorbis_open_memory(speech.ogg.data(),
                                    (int) speech.ogg.size(), &err, NULL);
        }
        if (! speech.vorbis) {
            errorWarning("Dialogue audio stream %d not found", streamId);
            return;
        }
        currentDialog = streamId;
    }

    {
    stb_vorbis_info info = stb_vorbis_get_info(speech.vorbis);
    SDL_AudioSpec spec = { SDL_AUDIO_F32, info.channels,
                           (int) info.sample_rate };
    int frames = int(streamPart[0] * info.sample_rate);
    std::vector<float> pcm(frames * info.channels);

    stb_vorbis_seek(speech.vorbis,
                    (unsigned int) (streamPart[1] * info.sample_rate));
    frames = stb_vorbis_get_samples_float_interleaved(speech.vorbis,
                                info.channels, pcm.data(), pcm.size());

    SDL_ClearAudioStream(speechStream);
    SDL_SetAudioStreamFormat(speechStream, &spec, NULL);
    SDL_PutAudioStreamData(speechStream, pcm.data(),
                           frames * info.channels * sizeof(float));
    }

    if (wait)
        EventHandler::wait_msecs(int(1000.0f * streamPart[0]));
#else
    (void) streamId;
    (void) line;
#endif
}

/*
 * Return duration in milliseconds.
 */
int soundDuration(Sound sound)
{
    int ms = bufferMs[sound];
    if (ms == 0)
        ms = loadSoundBuffer(sound);
    if (ms == BUFFER_MS_FAILED)
        return 0;
    return ms;
}

/*
 * Stop all sound effects.  Use musicStop() to halt music playback.
 */
void soundStop() {
    for (int i = 0; i < SOURCE_LIMIT; ++i)
        SDL_ClearAudioStream(sfxStream[i]);
}

#ifdef CONF_MODULE
/*
 * Render a tune from the Apple II disks and start playing it.
 */
static bool music_playApple2(const CDIEntry* ent, int mode)
{
    std::vector<uint8_t> text;
    if (! readFileChunk(xu4.config->modulePath(ent), ent->offset,
                        ent->bytes, text))
        return false;

    std::string spec((const char*) text.data(), text.size());
    size_t colon = spec.find(':');
    if (colon == std::string::npos)
        return false;
    int tune = atoi(spec.c_str() + colon + 1);
    spec.resize(colon);

    U4FILE* file = a2_open(spec.c_str());
    if (! file)
        return false;
    std::vector<uint8_t> data(file->length());
    data.resize(file->read(data.data(), 1, data.size()));
    delete file;

    std::vector<int16_t> samples;
    if (! a2_renderMusic(data.data(), data.size(), tune, A2MUSIC_RATE,
                         samples) || samples.size() < 2)
        return false;

    std::vector<float> pcm(samples.size());
    for (size_t i = 0; i < samples.size(); ++i)
        pcm[i] = samples[i] * (1.0f / 32768.0f);

    SDL_LockAudioStream(musicStream);
    music_close();
    music.pcm.swap(pcm);
    music.channels = 2;
    music.rate = A2MUSIC_RATE;
    music_begin(mode);
    SDL_UnlockAudioStream(musicStream);
    return true;
}
#endif

/*
 * Start playing a music track.
 *
 * Return true if the stream begins playing from the start.  If the stream
 * is already playing or it cannot be loaded then false is returned.
 */
static bool music_start(int track, int mode) {
    ASSERT(track < MUSIC_MAX, "Invalid music_start() track id");

    // Track already loaded
    if (track == currentTrack || ! musicStream)
        return false;

    std::vector<uint8_t> data;
#ifdef CONF_MODULE
    const CDIEntry* ent = config_musicFile(track);
    if (! ent)
        return false;
    if ((ent->cdi & CDI_MASK_FORMAT) == (XU4_A2MUSIC & CDI_MASK_FORMAT)) {
        if (! music_playApple2(ent, mode))
            return false;
    } else {
        if (! readFileChunk(xu4.config->modulePath(ent), ent->offset,
                            ent->bytes, data) ||
            ! music_openOgg(data, mode))
            return false;
    }
#else
    const char* pathname = config_musicFile(track);
    if (! pathname || ! readFileChunk(pathname, 0, 0, data) ||
        ! music_openOgg(data, mode))
        return false;
#endif

    currentTrack = track;
    return true;
}

void musicPlay(int track)
{
    if (musicEnabled && musicVolume > 0.0f)
        music_start(track, PLAY_LOOP);
}

void musicPlayLocale()
{
    musicPlay(c->location->map->music);
}

void musicStop()
{
    currentTrack = MUSIC_NONE;
    if (musicStream) {
        SDL_LockAudioStream(musicStream);
        music_close();
        SDL_ClearAudioStream(musicStream);
        SDL_UnlockAudioStream(musicStream);
    }
}

void musicFadeOut(int msec)
{
    if (currentTrack != MUSIC_NONE) {
        currentTrack = MUSIC_NONE;
        if (xu4.settings->volumeFades && msec > 0 && musicStream) {
            musicFadeMs = msec;
            SDL_LockAudioStream(musicStream);
            if (music.playing)
                music.fadeStep = -1000.0f / (float(msec) * music.rate);
            SDL_UnlockAudioStream(musicStream);
        } else
            musicStop();
    }
}

void musicFadeIn(int msec, bool loadFromMap)
{
    int mode = PLAY_LOOP;

    if (xu4.settings->volumeFades && msec > 0) {
        musicFadeMs = msec;
        mode |= PLAY_FADE_IN;
    }

    if (loadFromMap || currentTrack == MUSIC_NONE)
        music_start(c->location->map->music, mode);
}

void musicUpdate() {}

void musicSetVolume(int volume)
{
    musicVolume = float(volume) / MAX_VOLUME;
    if (musicStream)
        SDL_SetAudioStreamGain(musicStream, musicVolume);
}

int musicVolumeDec()
{
    if (xu4.settings->musicVol > 0)
        musicSetVolume(--xu4.settings->musicVol);
    return (xu4.settings->musicVol * 100 / MAX_VOLUME);  // percentage
}

int musicVolumeInc()
{
    if (xu4.settings->musicVol < MAX_VOLUME)
        musicSetVolume(++xu4.settings->musicVol);
    return (xu4.settings->musicVol * 100 / MAX_VOLUME);  // percentage
}

/**
 * Toggle the music on/off.
 */
bool musicToggle()
{
    musicEnabled = ! musicEnabled;
    if (musicEnabled)
        musicFadeIn(1000, true);
    else
        musicFadeOut(1000);

    return musicEnabled;
}

/*
 * Set volume for sound effects and spoken dialogue.
 */
void soundSetVolume(int volume) {
    soundVolume = float(volume) / MAX_VOLUME;
    for (int i = 0; i < SOURCE_LIMIT; ++i) {
        if (sfxStream[i])
            SDL_SetAudioStreamGain(sfxStream[i], soundVolume);
    }
    if (speechStream)
        SDL_SetAudioStreamGain(speechStream, soundVolume);
}

int soundVolumeDec()
{
    if (xu4.settings->soundVol > 0)
        soundSetVolume(--xu4.settings->soundVol);
    return (xu4.settings->soundVol * 100 / MAX_VOLUME);  // percentage
}

int soundVolumeInc()
{
    if (xu4.settings->soundVol < MAX_VOLUME)
        soundSetVolume(++xu4.settings->soundVol);
    return (xu4.settings->soundVol * 100 / MAX_VOLUME);  // percentage
}
