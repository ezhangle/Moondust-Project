/*
  SDL_mixer:  An audio mixer library based on the SDL library
  Copyright (C) 1997-2017 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

/* $Id$ */

/* This file supports streaming WAV files, without volume adjustment */

#include <stdlib.h>
#include <string.h>

#include <SDL2/SDL_audio.h>
#include <SDL2/SDL_mutex.h>
#include <SDL2/SDL_rwops.h>
#include <SDL2/SDL_endian.h>

#include <SDL_mixer_ext/SDL_mixer_ext.h>
#include "wavestream.h"

/*
    Taken with permission from SDL_wave.h, part of the SDL library,
    available at: http://www.libsdl.org/
    and placed under the same license as this mixer library.
*/

/* WAVE files are little-endian */

/*******************************************/
/* Define values for Microsoft WAVE format */
/*******************************************/
#define RIFF        0x46464952      /* "RIFF" */
#define WAVE        0x45564157      /* "WAVE" */
#define FMT         0x20746D66      /* "fmt " */
#define DATA        0x61746164      /* "data" */
#define SMPL        0x6c706d73      /* "smpl" */
#define PCM_CODE    1
#define ADPCM_CODE  2
#define WAVE_MONO   1
#define WAVE_STEREO 2

typedef struct {
/* Not saved in the chunk we read:
    Uint32  chunkID;
    Uint32  chunkLen;
*/
    Uint16  encoding;
    Uint16  channels;       /* 1 = mono, 2 = stereo */
    Uint32  frequency;      /* One of 11025, 22050, or 44100 Hz */
    Uint32  byterate;       /* Average bytes per second */
    Uint16  blockalign;     /* Bytes per sample block */
    Uint16  bitspersample;      /* One of 8, 12, 16, or 4 for ADPCM */
} WaveFMT;

typedef struct {
    Uint32 identifier;
    Uint32 type;
    Uint32 start;
    Uint32 end;
    Uint32 fraction;
    Uint32 play_count;
} SampleLoop;

typedef struct {
/* Not saved in the chunk we read:
    Uint32  chunkID;
    Uint32  chunkLen;
*/
    Uint32  manufacturer;
    Uint32  product;
    Uint32  sample_period;
    Uint32  MIDI_unity_note;
    Uint32  MIDI_pitch_fraction;
    Uint32  SMTPE_format;
    Uint32  SMTPE_offset;
    Uint32  sample_loops;
    Uint32  sampler_data;
    SampleLoop *loops;
} SamplerChunk;

/*********************************************/
/* Define values for AIFF (IFF audio) format */
/*********************************************/
#define FORM        0x4d524f46      /* "FORM" */
#define AIFF        0x46464941      /* "AIFF" */
#define SSND        0x444e5353      /* "SSND" */
#define COMM        0x4d4d4f43      /* "COMM" */

/* This is the format of the audio mixer data */
static SDL_AudioSpec mixer;

/* Function to load the WAV/AIFF stream */
static SDL_bool LoadWAVStream(WAVStream *wave);
static SDL_bool LoadAIFFStream(WAVStream *wave);

/* Initialize the WAVStream player, with the given mixer settings
   This function returns 0, or -1 if there was an error.
 */
int WAVStream_Init(SDL_AudioSpec *mixerfmt)
{
    mixer = *mixerfmt;
    return(0);
}

void WAVStream_SetVolume(WAVStream *music, int volume)
{
    music->volume = volume;
}

/* Load a WAV stream from the given RWops object */
WAVStream *WAVStream_LoadSong_RW(SDL_RWops *src, int freesrc)
{
    WAVStream *wave;
    SDL_bool loaded = SDL_FALSE;

    if (!mixer.format) {
        Mix_SetError("WAV music output not started");
        return(NULL);
    }

    wave = (WAVStream *)SDL_malloc(sizeof *wave);
    if (wave)
    {
        Uint32 magic;

        SDL_zerop(wave);
        wave->volume = MIX_MAX_VOLUME;
        wave->src = src;
        wave->freesrc = (SDL_bool)freesrc;
        MyResample_zero(&wave->resample);

        magic = SDL_ReadLE32(src);
        if (magic == RIFF || magic == WAVE) {
            loaded = LoadWAVStream(wave);
        } else if (magic == FORM) {
            loaded = LoadAIFFStream(wave);
        } else {
            Mix_SetError("Unknown WAVE format");
        }

        if (!loaded)
        {
            /* Don't free SRC on error (to avoid crash in future) */
            wave->freesrc = SDL_FALSE;
            WAVStream_FreeSong(wave);
            return(NULL);
        }

        #if 0
        if(wave->spec.format == AUDIO_S16)
        {
            /* Build resampler for Signed-16 PCM files only! */
            MyResample_init(&wave->resample,
                            wave->spec.freq,
                            mixer.freq,
                            mixer.channels,
                            AUDIO_S16);
            SDL_BuildAudioCVT(&wave->cvt,
                wave->spec.format, wave->spec.channels,
                mixer.freq, /* HACK: Use same frequency as mixer to don't use internal resampler */
                mixer.format, mixer.channels, mixer.freq);
        }
        else
        #endif
        {
            /* Overwise, use SDL's <s>shitty</s> hardcoded resamplers */
            SDL_BuildAudioCVT(&wave->cvt,
                wave->spec.format, wave->spec.channels, wave->spec.freq,
                mixer.format, mixer.channels, mixer.freq);
        }
    }
    else
    {
        SDL_OutOfMemory();
        return(NULL);
    }
    return(wave);
}

/* Start playback of a given WAV stream */
void WAVStream_Start(WAVStream *wave)
{
    int i;
    for (i = 0; i < wave->numloops; ++i) {
        WAVLoopPoint *loop = &wave->loops[i];
        loop->active = SDL_TRUE;
        loop->current_play_count = loop->initial_play_count;
    }
    SDL_RWseek(wave->src, wave->start, RW_SEEK_SET);
    wave->playing = 1;
}

/* Play some of a stream previously started with WAVStream_Start() */
static void PlaySome(WAVStream* music)
{
    long len;
    Uint8 data[4096];
    SDL_AudioCVT *cvt;
    struct MyResampler  *resampler;

    Sint64 pos, stop;
    WAVLoopPoint *loop;
    Sint64 loop_start = 0;
    Sint64 loop_stop = 0;
    int i;

    pos = SDL_RWtell(music->src);
    stop = music->stop;
    loop = NULL;
    for(i = 0; i < music->numloops; ++i)
    {
        loop = &music->loops[i];
        if (loop->active) {
            const unsigned int bytes_per_sample = (SDL_AUDIO_BITSIZE(music->spec.format) / 8) * music->spec.channels;
            loop_start = music->start + loop->start * bytes_per_sample;
            loop_stop = music->start + (loop->stop + 1) * bytes_per_sample;
            if (pos >= loop_start && pos < loop_stop)
            {
                stop = loop_stop;
                break;
            }
        }
        loop = NULL;
    }

    len = sizeof(data);

    /* At least at the time of writing, SDL_ConvertAudio()
       does byte-order swapping starting at the end of the
       buffer. Thus, if we are reading 16-bit samples, we
       had better make damn sure that we get an even
       number of bytes, or we'll get garbage.
     */
    if((music->cvt.src_format & 0x0010) && (len & 1))
    {
        len--;
    }

    len = (long)SDL_RWread(music->src, data, 1, (size_t)len);
    if(loop && SDL_RWtell(music->src) >= stop)
    {
        if (loop->current_play_count == 1) {
            loop->active = SDL_FALSE;
        } else {
            if (loop->current_play_count > 0)
            {
                --loop->current_play_count;
            }
            SDL_RWseek(music->src, loop_start, RW_SEEK_SET);
        }
    }

    if(len <= 0)
        return;

    cvt = &music->cvt;
    resampler = &music->resample;

    if ( cvt->buf ) {
        SDL_free(cvt->buf);
    }
    cvt->buf = (Uint8 *)SDL_malloc(sizeof(data) * (size_t)cvt->len_mult * (size_t)resampler->len_mult);
    if ( cvt->buf )
    {
        if( music->resample.needed )
        {
            MyResample_addSource(resampler, data, (int)len);
            MyResample_Process(resampler);
            SDL_memcpy(cvt->buf, resampler->buf, (size_t)resampler->buf_len);
            cvt->len = resampler->buf_len;
            cvt->len_cvt = resampler->buf_len;
        }
        else
        {
            cvt->len = (int)len;
            cvt->len_cvt = (int)len;
            SDL_memcpy(cvt->buf, data, (size_t)len);
        }
        if ( cvt->needed )
        {
            SDL_ConvertAudio(cvt);
        }
        music->len_available = cvt->len_cvt;
        music->snd_available = cvt->buf;
    }
    else
    {
        SDL_SetError("Out of memory");
        music = NULL;
    }
}

int WAVStream_PlaySome(WAVStream *music, Uint8 *stream, int len)
{
    int mixable;

    if(!music || !music->playing)
        return 0;

    while ((SDL_RWtell(music->src) < music->stop) && (len > 0))
    {
        if ( !music->len_available )
        {
            PlaySome(music);
        }
        mixable = len;
        if ( mixable > music->len_available ) {
            mixable = music->len_available;
        }
        if( music->volume == MIX_MAX_VOLUME ) {
            SDL_memcpy(stream, music->snd_available, (size_t)mixable);
        } else {
            SDL_MixAudioFormat(stream, music->snd_available, mixer.format,
                               (Uint32)mixable, music->volume);
        }
        music->len_available -= mixable;
        music->snd_available += mixable;
        len -= mixable;
        stream += mixable;
    }
    return len;
}

/* Stop playback of a stream previously started with WAVStream_Start() */
void WAVStream_Stop(WAVStream *wave)
{
    wave->playing = 0;
}

/* Close the given WAV stream */
void WAVStream_FreeSong(WAVStream *wave)
{
    if (wave) {
        /* Clean up associated data */
        if (wave->loops) {
            SDL_free(wave->loops);
        }
        if (wave->cvt.buf) {
            SDL_free(wave->cvt.buf);
        }
        if (wave->freesrc) {
            SDL_RWclose(wave->src);
        }
        SDL_free(wave);
    }
}

/* Return non-zero if a stream is currently playing */
int WAVStream_Active(WAVStream *wave)
{
    int active;

    active = 0;
    if(wave && wave->playing && (SDL_RWtell(wave->src) < wave->stop))
    {
        active = 1;
    }

    return(active);
}

static SDL_bool ParseFMT(WAVStream *wave, Uint32 chunk_length)
{
    /* SDL_RWops *src = wave->src; */
    SDL_AudioSpec *spec = &wave->spec;
    WaveFMT *format;
    Uint8 *data;
    SDL_bool loaded = SDL_FALSE;

    if (chunk_length < sizeof(*format)) {
        Mix_SetError("Wave format chunk too small");
        return SDL_FALSE;
    }

    data = (Uint8 *)SDL_malloc(chunk_length);
    if (!data) {
        Mix_SetError("Out of memory");
        return SDL_FALSE;
    }
    if (!SDL_RWread(wave->src, data, chunk_length, 1)) {
        Mix_SetError("Couldn't read %d bytes from WAV file", chunk_length);
        return SDL_FALSE;
    }
    format = (WaveFMT *)data;

    /* Decode the audio data format */
    switch (SDL_SwapLE16(format->encoding)) {
        case PCM_CODE:
            /* We can understand this */
            break;
        default:
            Mix_SetError("Unknown WAVE data format");
            goto done;
    }
    spec->freq = SDL_SwapLE32((Sint32)format->frequency);
    switch (SDL_SwapLE16(format->bitspersample)) {
        case 8:
            spec->format = AUDIO_U8;
            break;
        case 16:
            spec->format = AUDIO_S16;
            break;
        default:
            Mix_SetError("Unknown PCM data format");
            goto done;
    }
    spec->channels = (Uint8) SDL_SwapLE16(format->channels);
    spec->samples = 4096;       /* Good default buffer size */

    loaded = SDL_TRUE;

done:
    SDL_free(data);
    return loaded;
}

static SDL_bool ParseDATA(WAVStream *wave, Uint32 chunk_length)
{
    wave->start = SDL_RWtell(wave->src);
    wave->stop = wave->start + chunk_length;
    SDL_RWseek(wave->src, chunk_length, RW_SEEK_CUR);
    return SDL_TRUE;
}

static SDL_bool AddLoopPoint(WAVStream *wave, Uint32 play_count, Uint32 start, Uint32 stop)
{
    WAVLoopPoint *loop;
    WAVLoopPoint *loops = (WAVLoopPoint *)SDL_realloc(wave->loops, (size_t)(wave->numloops + 1)*sizeof(*wave->loops));
    if (!loops) {
        Mix_SetError("Out of memory");
        return SDL_FALSE;
    }

    loop = &loops[ wave->numloops ];
    loop->start = start;
    loop->stop = stop;
    loop->initial_play_count = play_count;
    loop->current_play_count = play_count;

    wave->loops = loops;
    ++wave->numloops;
    return SDL_TRUE;
}

static SDL_bool ParseSMPL(WAVStream *wave, Uint32 chunk_length)
{
    SamplerChunk *chunk;
    Uint8 *data;
    int i, loops = 0;
    SDL_bool loaded = SDL_FALSE;

    data = (Uint8 *)SDL_malloc(chunk_length);
    if (!data) {
        Mix_SetError("Out of memory");
        return SDL_FALSE;
    }
    if (!SDL_RWread(wave->src, data, chunk_length, 1)) {
        Mix_SetError("Couldn't read %d bytes from WAV file", chunk_length);
        return SDL_FALSE;
    }
    chunk = (SamplerChunk *)data;
    loops = SDL_SwapLE32(chunk->sample_loops);

    for (i = 0; i < loops; ++i)
    {
        const Uint32 LOOP_TYPE_FORWARD = 0;
        Uint32 loop_type = SDL_SwapLE32(chunk->loops[i].type);
        if (loop_type == LOOP_TYPE_FORWARD) {
            AddLoopPoint(wave, SDL_SwapLE32(chunk->loops[i].play_count), SDL_SwapLE32(chunk->loops[i].start), SDL_SwapLE32(chunk->loops[i].end));
        }
    }

    loaded = SDL_TRUE;

/* done: */
    SDL_free(data);
    return loaded;
}

static SDL_bool LoadWAVStream(WAVStream *wave)
{
    SDL_RWops *src = wave->src;
    Uint32 chunk_type;
    Uint32 chunk_length;
    SDL_bool found_FMT = SDL_FALSE;
    SDL_bool found_DATA = SDL_FALSE;

    /* WAV magic header */
    Uint32 wavelen;
    Uint32 WAVEmagic;

    /* Check the magic header */
    wavelen = SDL_ReadLE32(src);
    WAVEmagic = SDL_ReadLE32(src);

    /* Read the chunks */
    for (; ;) {
        chunk_type = SDL_ReadLE32(src);
        chunk_length = SDL_ReadLE32(src);

        if (chunk_length == 0)
            break;

        switch (chunk_type)
        {
        case FMT:
            found_FMT = SDL_TRUE;
            if (!ParseFMT(wave, chunk_length))
                return SDL_FALSE;
            break;
        case DATA:
            found_DATA = SDL_TRUE;
            if (!ParseDATA(wave, chunk_length))
                return SDL_FALSE;
            break;
        case SMPL:
            if (!ParseSMPL(wave, chunk_length))
                return SDL_FALSE;
            break;
        default:
            SDL_RWseek(src, chunk_length, RW_SEEK_CUR);
            break;
        }
    }

    if (!found_FMT) {
        Mix_SetError("Bad WAV file (no FMT chunk)");
        return SDL_FALSE;
    }

    if (!found_DATA) {
        Mix_SetError("Bad WAV file (no DATA chunk)");
        return SDL_FALSE;
    }

    return SDL_TRUE;
}

/* I couldn't get SANE_to_double() to work, so I stole this from libsndfile.
 * I don't pretend to fully understand it.
 */

static Uint32 SANE_to_Uint32 (Uint8 *sanebuf)
{
    /* Negative number? */
    if (sanebuf[0] & 0x80)
        return 0;

    /* Less than 1? */
    if (sanebuf[0] <= 0x3F)
        return 1;

    /* Way too big? */
    if (sanebuf[0] > 0x40)
        return 0x4000000;

    /* Still too big? */
    if (sanebuf[0] == 0x40 && sanebuf[1] > 0x1C)
        return 800000000;

    return (Uint32)((sanebuf[2] << 23) | (sanebuf[3] << 15) | (sanebuf[4] << 7) |
                    (sanebuf[5] >> 1)) >> (29 - sanebuf[1]);
}

static SDL_bool LoadAIFFStream(WAVStream *wave)
{
    SDL_RWops *src = wave->src;
    SDL_AudioSpec *spec = &wave->spec;
    SDL_bool found_SSND = SDL_FALSE;
    SDL_bool found_COMM = SDL_FALSE;

    Uint32 chunk_type;
    Uint32 chunk_length;
    Sint64 next_chunk;

    /* AIFF magic header */
    Uint32 AIFFmagic;
    /* SSND chunk        */
    Uint32 offset;
    Uint32 blocksize;
    /* COMM format chunk */
    Uint16 channels = 0;
    Uint32 numsamples = 0;
    Uint16 samplesize = 0;
    Uint8 sane_freq[10];
    Uint32 frequency = 0;

    /* Check the magic header */
    chunk_length = SDL_ReadBE32(src);
    AIFFmagic = SDL_ReadLE32(src);
    if (AIFFmagic != AIFF) {
        Mix_SetError("Unrecognized file type (not AIFF)");
        return SDL_FALSE;
    }

    /* From what I understand of the specification, chunks may appear in
     * any order, and we should just ignore unknown ones.
     *
     * TODO: Better sanity-checking. E.g. what happens if the AIFF file
     *       contains compressed sound data?
     */
    do {
        chunk_type      = SDL_ReadLE32(src);
        chunk_length    = SDL_ReadBE32(src);
        next_chunk      = SDL_RWtell(src) + chunk_length;

        /* Paranoia to avoid infinite loops */
        if (chunk_length == 0)
            break;

        switch (chunk_type) {
        case SSND:
            found_SSND = SDL_TRUE;
            offset = SDL_ReadBE32(src);
            blocksize = SDL_ReadBE32(src);
            wave->start = SDL_RWtell(src) + offset;
            break;

        case COMM:
            found_COMM = SDL_TRUE;

            /* Read the audio data format chunk */
            channels = SDL_ReadBE16(src);
            numsamples = SDL_ReadBE32(src);
            samplesize = SDL_ReadBE16(src);
            SDL_RWread(src, sane_freq, sizeof(sane_freq), 1);
            frequency = SANE_to_Uint32(sane_freq);
            break;

        default:
            break;
        }
    } while ((!found_SSND || !found_COMM)
         && SDL_RWseek(src, next_chunk, RW_SEEK_SET) != -1);

    if (!found_SSND) {
        Mix_SetError("Bad AIFF file (no SSND chunk)");
        return SDL_FALSE;
    }

    if (!found_COMM) {
        Mix_SetError("Bad AIFF file (no COMM chunk)");
        return SDL_FALSE;
    }

    wave->stop = wave->start + channels * numsamples * (samplesize / 8);

    /* Decode the audio data format */
    SDL_memset(spec, 0, (sizeof *spec));
    spec->freq = (int)frequency;
    switch (samplesize) {
        case 8:
            spec->format = AUDIO_S8;
            break;
        case 16:
            spec->format = AUDIO_S16MSB;
            break;
        default:
            Mix_SetError("Unknown samplesize in data format");
            return SDL_FALSE;
    }
    spec->channels = (Uint8) channels;
    spec->samples = 4096;       /* Good default buffer size */

    return SDL_TRUE;
}
