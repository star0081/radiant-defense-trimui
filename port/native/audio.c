#include "jni_min.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#ifdef _WIN32
#include <windows.h>
static void *so_open(const char *name) { return (void *)LoadLibraryA(name); }
static void *so_sym(void *h, const char *name) { return (void *)GetProcAddress((HMODULE)h, name); }
#else
#include <dlfcn.h>
#include <pthread.h>
static void *so_open(const char *name) { return dlopen(name, RTLD_LAZY | RTLD_GLOBAL); }
static void *so_sym(void *h, const char *name) { return dlsym(h, name); }
#endif

#define NDEBUG
#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_MAX_CHANNELS 2
#include "stb_vorbis.c"

/* Effects are 22050 mono, music is 44100 stereo. Both live as stored zip
   entries named assets/<name>. Volume 0..100 matches the original millibel
   curve: gain = 10^(vol/40 - 2.5). Sixteen voices, same as OpenSL. */

#define MAX_SOUNDS 96
#define MAX_VOICES 16
#define RING_FRAMES 16384

typedef struct {
    short *pcm;
    int frames;
    int rate;
    int channels;
} Sound;

typedef struct {
    int on;
    int loop;
    float gain;
    const short *pcm;
    int frames;
    int rate;
    int channels;
    double pos;
} Voice;

typedef struct {
    char name[64];
    int off;
    int len;
} ZipEnt;

typedef void (*acb_fn)(void *, uint8_t *, int);
typedef struct {
    int freq;
    uint16_t format;
    uint8_t channels;
    uint8_t silence;
    uint16_t samples;
    uint16_t padding;
    uint32_t size;
    acb_fn callback;
    void *userdata;
} ASpec;

typedef int (*sdl_init_fn)(uint32_t);
typedef uint32_t (*sdl_open_fn)(const char *, int, const ASpec *, ASpec *, int);
typedef void (*sdl_pause_fn)(uint32_t, int);
typedef void (*sdl_close_fn)(uint32_t);
typedef const char *(*sdl_err_fn)(void);

static sdl_init_fn p_init;
static sdl_init_fn p_sub;
static sdl_open_fn p_open;
static sdl_pause_fn p_pause;
static sdl_close_fn p_close;
static sdl_err_fn p_err;

static pthread_mutex_t state_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t music_mu = PTHREAD_MUTEX_INITIALIZER;

static char *apk_path;
static ZipEnt zents[80];
static int nzents;
static int dev_rate;
static uint32_t dev_id;
static int paused;
static int audio_ready;

static Sound sounds[MAX_SOUNDS];
static int nsounds;
static Voice voices[MAX_VOICES];

static unsigned char *music_bytes;
static int music_len;
static stb_vorbis *music_v;
static int music_rate;
static int music_loop;
static int music_on;
static int music_eof;
static int music_done;
static float music_gain;
static short ring[RING_FRAMES * 2];
static int ring_r;
static int ring_n;
static double ring_frac;

static unsigned rd16(const unsigned char *p) {
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}

static unsigned rd32(const unsigned char *p) {
    return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

static int load_sdl_audio(void) {
    if (p_open) return 1;
    const char *names[] = {
        "/usr/trimui/lib/libSDL2-2.0.so.0",
        "libSDL2-2.0.so.0",
        "libSDL2.so",
        NULL
    };
    void *sdl = NULL;
    for (int i = 0; names[i]; i++) {
        sdl = so_open(names[i]);
        if (sdl) break;
    }
    if (!sdl) {
        fprintf(stderr, "audio sdl missing\n");
        return 0;
    }
    p_init = (sdl_init_fn)so_sym(sdl, "SDL_Init");
    p_sub = (sdl_init_fn)so_sym(sdl, "SDL_InitSubSystem");
    p_open = (sdl_open_fn)so_sym(sdl, "SDL_OpenAudioDevice");
    p_pause = (sdl_pause_fn)so_sym(sdl, "SDL_PauseAudioDevice");
    p_close = (sdl_close_fn)so_sym(sdl, "SDL_CloseAudioDevice");
    p_err = (sdl_err_fn)so_sym(sdl, "SDL_GetError");
    if (!p_open || !p_pause) {
        fprintf(stderr, "audio sdl symbols missing\n");
        return 0;
    }
    return 1;
}

static const char *sdl_err(void) {
    return p_err ? p_err() : "";
}

static int index_apk(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    long sz = ftell(f);
    if (sz < 22) { fclose(f); return 0; }
    long scan = sz > 66000 ? 66000 : sz;
    unsigned char *tail = (unsigned char *)malloc((size_t)scan);
    if (!tail) { fclose(f); return 0; }
    if (fseek(f, sz - scan, SEEK_SET) != 0 || fread(tail, 1, (size_t)scan, f) != (size_t)scan) {
        free(tail);
        fclose(f);
        return 0;
    }
    int e = -1;
    for (int i = (int)scan - 22; i >= 0; i--) {
        if (tail[i] == 0x50 && tail[i + 1] == 0x4b && tail[i + 2] == 0x05 && tail[i + 3] == 0x06) {
            e = i;
            break;
        }
    }
    if (e < 0) { free(tail); fclose(f); return 0; }
    unsigned cd_size = rd32(tail + e + 12);
    unsigned cd_off = rd32(tail + e + 16);
    free(tail);
    if ((long)cd_off >= sz || cd_size < 46) { fclose(f); return 0; }
    unsigned char *cd = (unsigned char *)malloc(cd_size);
    if (!cd) { fclose(f); return 0; }
    if (fseek(f, (long)cd_off, SEEK_SET) != 0 || fread(cd, 1, cd_size, f) != cd_size) {
        free(cd);
        fclose(f);
        return 0;
    }
    unsigned p = 0;
    nzents = 0;
    while (p + 46 <= cd_size && nzents < 80) {
        if (rd32(cd + p) != 0x02014b50) break;
        unsigned method = rd16(cd + p + 10);
        unsigned uncomp = rd32(cd + p + 24);
        unsigned nlen = rd16(cd + p + 28);
        unsigned elen = rd16(cd + p + 30);
        unsigned clen = rd16(cd + p + 32);
        unsigned local = rd32(cd + p + 42);
        if (p + 46 + nlen > cd_size) break;
        if (method == 0 && nlen >= 4 && nlen < 64 && memcmp(cd + p + 46 + nlen - 4, ".ogg", 4) == 0) {
            unsigned char lh[30];
            if (fseek(f, (long)local, SEEK_SET) == 0 && fread(lh, 1, 30, f) == 30 && rd32(lh) == 0x04034b50) {
                unsigned ln = rd16(lh + 26);
                unsigned le = rd16(lh + 28);
                ZipEnt *ze = &zents[nzents++];
                memcpy(ze->name, cd + p + 46, nlen);
                ze->name[nlen] = 0;
                ze->off = (int)(local + 30u + ln + le);
                ze->len = (int)uncomp;
            }
        }
        p += 46 + nlen + elen + clen;
    }
    free(cd);
    fclose(f);
    fprintf(stderr, "audio zip %d\n", nzents);
    return nzents > 0;
}

static int zip_find(const char *want, int *off, int *len) {
    char alt[96];
    const char *cands[2];
    int n = 1;
    cands[0] = want;
    if (want && strncmp(want, "assets/", 7) != 0) {
        snprintf(alt, sizeof alt, "assets/%s", want);
        cands[1] = alt;
        n = 2;
    }
    for (int c = 0; c < n; c++) {
        for (int i = 0; i < nzents; i++) {
            if (strcmp(zents[i].name, cands[c]) == 0) {
                *off = zents[i].off;
                *len = zents[i].len;
                return 1;
            }
        }
    }
    return 0;
}

static unsigned char *read_span(int off, int len) {
    if (!apk_path || len <= 0 || len > 12 * 1024 * 1024) return NULL;
    FILE *f = fopen(apk_path, "rb");
    if (!f) return NULL;
    if (fseek(f, off, SEEK_SET) != 0) { fclose(f); return NULL; }
    unsigned char *b = (unsigned char *)malloc((size_t)len);
    if (!b) { fclose(f); return NULL; }
    if (fread(b, 1, (size_t)len, f) != (size_t)len) {
        free(b);
        fclose(f);
        return NULL;
    }
    fclose(f);
    return b;
}

static float vol_gain(int vol) {
    if (vol <= 0) return 0.f;
    if (vol > 100) vol = 100;
    return powf(10.f, ((float)vol / 40.f) - 2.5f);
}

static int samp(float s) {
    if (s >= 0.f) s += 0.5f;
    else s -= 0.5f;
    if (s > 32767.f) return 32767;
    if (s < -32768.f) return -32768;
    return (int)s;
}

static void mix_voice(int *acc, int nframes, Voice *v) {
    if (!v->on || !v->pcm || v->frames <= 1 || v->rate <= 0 || dev_rate <= 0) {
        if (v->on && v->frames <= 1) v->on = 0;
        return;
    }
    double step = (double)v->rate / (double)dev_rate;
    int ch = v->channels < 1 ? 1 : v->channels;
    for (int i = 0; i < nframes; i++) {
        int i0 = (int)v->pos;
        if (i0 >= v->frames) {
            if (v->loop) {
                v->pos = 0.0;
                i0 = 0;
            } else {
                v->on = 0;
                return;
            }
        }
        int i1 = i0 + 1;
        if (i1 >= v->frames) i1 = v->loop ? 0 : i0;
        float frac = (float)(v->pos - (double)i0);
        if (ch == 1) {
            float s = ((float)v->pcm[i0] * (1.f - frac) + (float)v->pcm[i1] * frac) * v->gain;
            int a = samp(s);
            acc[i * 2] += a;
            acc[i * 2 + 1] += a;
        } else {
            float l = ((float)v->pcm[i0 * 2] * (1.f - frac) + (float)v->pcm[i1 * 2] * frac) * v->gain;
            float r = ((float)v->pcm[i0 * 2 + 1] * (1.f - frac) + (float)v->pcm[i1 * 2 + 1] * frac) * v->gain;
            acc[i * 2] += samp(l);
            acc[i * 2 + 1] += samp(r);
        }
        v->pos += step;
    }
}

static void ring_write(const short *src, int frames) {
    if (frames > RING_FRAMES - ring_n) frames = RING_FRAMES - ring_n;
    int w = ring_r + ring_n;
    if (w >= RING_FRAMES) w -= RING_FRAMES;
    for (int i = 0; i < frames; i++) {
        ring[w * 2] = src[i * 2];
        ring[w * 2 + 1] = src[i * 2 + 1];
        w++;
        if (w == RING_FRAMES) w = 0;
    }
    ring_n += frames;
}

static int ring_peek(short *dst, int frames) {
    if (frames > ring_n) frames = ring_n;
    int r = ring_r;
    for (int i = 0; i < frames; i++) {
        dst[i * 2] = ring[r * 2];
        dst[i * 2 + 1] = ring[r * 2 + 1];
        r++;
        if (r == RING_FRAMES) r = 0;
    }
    return frames;
}

static void ring_drop(int frames) {
    if (frames > ring_n) frames = ring_n;
    ring_r += frames;
    if (ring_r >= RING_FRAMES) ring_r %= RING_FRAMES;
    ring_n -= frames;
}

static void mix_music(int *acc, int nframes) {
    if (dev_rate <= 0) return;
    int rate = music_rate > 0 ? music_rate : 44100;
    double step = (double)rate / (double)dev_rate;
    if (step < 0.01) step = 0.01;
    int n = nframes;
    int need = (int)(ring_frac + (double)n * step) + 2;
    if (need > 2000) {
        n = (int)((2000.0 - 2.0 - ring_frac) / step);
        if (n < 1) n = 1;
        if (n > nframes) n = nframes;
        need = (int)(ring_frac + (double)n * step) + 2;
    }
    short tmp[2002 * 2];
    if (need > 2000) need = 2000;
    int copied = ring_peek(tmp, need);
    double p = ring_frac;
    int produced = 0;
    for (; produced < n; produced++) {
        int i0 = (int)p;
        if (i0 + 1 >= copied) break;
        float frac = (float)(p - (double)i0);
        float l = ((float)tmp[i0 * 2] * (1.f - frac) + (float)tmp[(i0 + 1) * 2] * frac) * music_gain;
        float r = ((float)tmp[i0 * 2 + 1] * (1.f - frac) + (float)tmp[(i0 + 1) * 2 + 1] * frac) * music_gain;
        acc[produced * 2] += samp(l);
        acc[produced * 2 + 1] += samp(r);
        p += step;
    }
    int drop = (int)p;
    if (drop > copied) drop = copied;
    ring_frac = p - (double)drop;
    if (ring_frac < 0.0) ring_frac = 0.0;
    ring_drop(drop);
    if (music_eof && ring_n < 2) {
        music_eof = 0;
        music_on = 0;
        music_done = 1;
        ring_n = 0;
        ring_r = 0;
        ring_frac = 0.0;
    }
}

static void audio_cb(void *ud, uint8_t *stream, int len) {
    (void)ud;
    if (!stream || len <= 0) return;
    if (!audio_ready || paused || dev_rate <= 0 || (len & 3)) {
        memset(stream, 0, (size_t)len);
        return;
    }
    int frames = len / 4;
    int16_t *out = (int16_t *)stream;
    int done = 0;
    while (done < frames) {
        int n = frames - done;
        if (n > 256) n = 256;
        int acc[256 * 2];
        memset(acc, 0, sizeof acc);
        pthread_mutex_lock(&state_mu);
        if (!paused) {
            for (int i = 0; i < MAX_VOICES; i++) mix_voice(acc, n, &voices[i]);
            mix_music(acc, n);
        }
        pthread_mutex_unlock(&state_mu);
        for (int i = 0; i < n * 2; i++) {
            int s = acc[i];
            if (s > 32767) s = 32767;
            if (s < -32768) s = -32768;
            out[done * 2 + i] = (int16_t)s;
        }
        done += n;
    }
}

static void music_close(void) {
    if (music_v) {
        stb_vorbis_close(music_v);
        music_v = NULL;
    }
    free(music_bytes);
    music_bytes = NULL;
    music_len = 0;
}

static void music_produce(int target) {
    if (target > RING_FRAMES - 64) target = RING_FRAMES - 64;
    if (target < 1) return;
    for (;;) {
        int have = 0;
        int go = 0;
        pthread_mutex_lock(&state_mu);
        have = ring_n;
        go = music_on && !paused && !music_eof;
        pthread_mutex_unlock(&state_mu);
        if (!go || have >= target) break;
        short tmp[1024 * 2];
        int n = 0;
        int ended = 0;
        pthread_mutex_lock(&music_mu);
        if (music_v) {
            n = stb_vorbis_get_samples_short_interleaved(music_v, 2, tmp, 1024 * 2);
            if (n <= 0) {
                if (music_loop && stb_vorbis_seek_start(music_v)) {
                    n = stb_vorbis_get_samples_short_interleaved(music_v, 2, tmp, 1024 * 2);
                }
                if (n <= 0) ended = 1;
            }
        } else {
            ended = 1;
        }
        pthread_mutex_unlock(&music_mu);
        if (ended) {
            pthread_mutex_lock(&state_mu);
            music_eof = 1;
            pthread_mutex_unlock(&state_mu);
            break;
        }
        pthread_mutex_lock(&state_mu);
        ring_write(tmp, n);
        pthread_mutex_unlock(&state_mu);
    }
}

static void fire_complete(JNIEnv *env) {
    jclass c = (*env)->FindClass(env, "mojo/SoundEngineSL");
    if ((*env)->ExceptionCheck(env) || !c) {
        (*env)->ExceptionClear(env);
        return;
    }
    jmethodID m = (*env)->GetStaticMethodID(env, c, "onMusicCompletion", "()V");
    if ((*env)->ExceptionCheck(env) || !m) {
        (*env)->ExceptionClear(env);
        return;
    }
    (*env)->CallStaticVoidMethod(env, c, m);
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
}

static char *dup_jstr(JNIEnv *env, jstring s) {
    if (!s) return NULL;
    const char *u = (*env)->GetStringUTFChars(env, s, NULL);
    if (!u) return NULL;
    char *c = strdup(u);
    (*env)->ReleaseStringUTFChars(env, s, u);
    return c;
}

JNIEXPORT jint JNICALL Java_mojo_SoundEngineSL_startup(JNIEnv *env, jclass cls, jobject assets, jstring path, jboolean flag) {
    (void)cls;
    (void)assets;
    (void)flag;
    if (audio_ready) return 0;
    char *s = dup_jstr(env, path);
    fprintf(stderr, "audio startup %s\n", s ? s : "");
    free(apk_path);
    apk_path = s;
    if (apk_path) index_apk(apk_path);
    if (!load_sdl_audio()) return 0;
    if (p_sub) {
        if (p_sub(0x10) != 0) fprintf(stderr, "audio subsystem %s\n", sdl_err());
    } else if (p_init) {
        p_init(0x10);
    }
    ASpec want;
    ASpec got;
    memset(&want, 0, sizeof want);
    memset(&got, 0, sizeof got);
    want.freq = 44100;
    want.format = 0x8010;
    want.channels = 2;
    want.samples = 1024;
    want.callback = audio_cb;
    dev_id = p_open(NULL, 0, &want, &got, 0x01 | 0x08);
    if (!dev_id) {
        fprintf(stderr, "audio open %s\n", sdl_err());
        return 0;
    }
    if (got.format != 0x8010 || got.channels != 2 || got.freq < 8000) {
        fprintf(stderr, "audio format %u ch %u rate %d\n", got.format, got.channels, got.freq);
        p_close(dev_id);
        dev_id = 0;
        return 0;
    }
    dev_rate = got.freq;
    audio_ready = 1;
    paused = 0;
    p_pause(dev_id, 0);
    fprintf(stderr, "audio open %d\n", dev_rate);
    return 0;
}

JNIEXPORT jint JNICALL Java_mojo_SoundEngineSL_EffectLoad(JNIEnv *env, jclass cls, jstring name) {
    (void)cls;
    char *s = dup_jstr(env, name);
    int off = 0, len = 0;
    int id = 0;
    if (!s || !zip_find(s, &off, &len)) {
        fprintf(stderr, "effect miss %s\n", s ? s : "");
        free(s);
        return 0;
    }
    unsigned char *raw = read_span(off, len);
    if (!raw) {
        fprintf(stderr, "effect read %s\n", s);
        free(s);
        return 0;
    }
    int ch = 0, rate = 0;
    short *pcm = NULL;
    int frames = stb_vorbis_decode_memory(raw, len, &ch, &rate, &pcm);
    free(raw);
    if (frames <= 0 || !pcm) {
        fprintf(stderr, "effect bad %s\n", s);
        free(pcm);
        free(s);
        return 0;
    }
    pthread_mutex_lock(&state_mu);
    if (nsounds < MAX_SOUNDS) {
        sounds[nsounds].pcm = pcm;
        sounds[nsounds].frames = frames;
        sounds[nsounds].rate = rate > 0 ? rate : 22050;
        sounds[nsounds].channels = ch > 0 ? ch : 1;
        nsounds++;
        id = nsounds;
        pcm = NULL;
    }
    pthread_mutex_unlock(&state_mu);
    if (pcm) free(pcm);
    if (id > 0 && id <= 8) fprintf(stderr, "effect %s %d\n", s, frames);
    free(s);
    return id;
}

JNIEXPORT jint JNICALL Java_mojo_SoundEngineSL_EffectPlay(JNIEnv *env, jclass cls, jint id, jboolean loop, jint vol, jint unused) {
    (void)env;
    (void)cls;
    (void)unused;
    int voice = 0;
    pthread_mutex_lock(&state_mu);
    if (id < 1 || id > nsounds) {
        pthread_mutex_unlock(&state_mu);
        return 0;
    }
    Sound *snd = &sounds[id - 1];
    for (int i = 0; i < MAX_VOICES; i++) {
        if (!voices[i].on) {
            voices[i].on = 1;
            voices[i].loop = loop ? 1 : 0;
            voices[i].gain = vol_gain(vol);
            voices[i].pcm = snd->pcm;
            voices[i].frames = snd->frames;
            voices[i].rate = snd->rate;
            voices[i].channels = snd->channels;
            voices[i].pos = 0.0;
            voice = i + 1;
            break;
        }
    }
    pthread_mutex_unlock(&state_mu);
    return voice;
}

JNIEXPORT void JNICALL Java_mojo_SoundEngineSL_EffectStop(JNIEnv *env, jclass cls, jint id) {
    (void)env;
    (void)cls;
    int i = id - 1;
    if (i < 0 || i >= MAX_VOICES) return;
    pthread_mutex_lock(&state_mu);
    voices[i].on = 0;
    voices[i].pcm = NULL;
    pthread_mutex_unlock(&state_mu);
}

JNIEXPORT void JNICALL Java_mojo_SoundEngineSL_MusicPlay(JNIEnv *env, jclass cls, jstring name, jint vol, jboolean loop) {
    (void)cls;
    char *s = dup_jstr(env, name);
    fprintf(stderr, "music %s\n", s ? s : "");
    int off = 0, len = 0;
    unsigned char *raw = NULL;
    if (s && zip_find(s, &off, &len)) raw = read_span(off, len);
    if (!raw) {
        fprintf(stderr, "music miss %s\n", s ? s : "");
        free(s);
        return;
    }
    int err = 0;
    stb_vorbis *v = stb_vorbis_open_memory(raw, len, &err, NULL);
    if (!v) {
        fprintf(stderr, "music bad %s %d\n", s, err);
        free(raw);
        free(s);
        return;
    }
    stb_vorbis_info info = stb_vorbis_get_info(v);
    pthread_mutex_lock(&music_mu);
    music_close();
    music_bytes = raw;
    music_len = len;
    music_v = v;
    music_loop = loop ? 1 : 0;
    pthread_mutex_unlock(&music_mu);
    pthread_mutex_lock(&state_mu);
    ring_r = 0;
    ring_n = 0;
    ring_frac = 0.0;
    music_rate = info.sample_rate > 0 ? (int)info.sample_rate : 44100;
    music_gain = vol_gain(vol);
    music_on = 1;
    music_eof = 0;
    music_done = 0;
    pthread_mutex_unlock(&state_mu);
    music_produce(8000);
    free(s);
}

JNIEXPORT void JNICALL Java_mojo_SoundEngineSL_MusicStop(JNIEnv *env, jclass cls) {
    (void)env;
    (void)cls;
    pthread_mutex_lock(&state_mu);
    music_on = 0;
    music_eof = 0;
    music_done = 0;
    ring_n = 0;
    ring_r = 0;
    ring_frac = 0.0;
    pthread_mutex_unlock(&state_mu);
    pthread_mutex_lock(&music_mu);
    music_close();
    pthread_mutex_unlock(&music_mu);
}

JNIEXPORT void JNICALL Java_mojo_SoundEngineSL_MusicVolume(JNIEnv *env, jclass cls, jint vol) {
    (void)env;
    (void)cls;
    pthread_mutex_lock(&state_mu);
    music_gain = vol_gain(vol);
    pthread_mutex_unlock(&state_mu);
}

JNIEXPORT void JNICALL Java_mojo_SoundEngineSL_pause(JNIEnv *env, jobject self) {
    (void)env;
    (void)self;
    paused = 1;
    if (dev_id && p_pause) p_pause(dev_id, 1);
}

JNIEXPORT void JNICALL Java_mojo_SoundEngineSL_resume(JNIEnv *env, jobject self) {
    (void)env;
    (void)self;
    paused = 0;
    if (dev_id && p_pause) p_pause(dev_id, 0);
}

JNIEXPORT void JNICALL Java_mojo_SoundEngineSL_shutdown(JNIEnv *env, jobject self) {
    (void)env;
    (void)self;
    paused = 1;
    if (dev_id && p_pause) p_pause(dev_id, 1);
    if (dev_id && p_close) p_close(dev_id);
    dev_id = 0;
    audio_ready = 0;
    pthread_mutex_lock(&state_mu);
    for (int i = 0; i < nsounds; i++) free(sounds[i].pcm);
    memset(sounds, 0, sizeof sounds);
    memset(voices, 0, sizeof voices);
    nsounds = 0;
    ring_n = 0;
    music_on = 0;
    pthread_mutex_unlock(&state_mu);
    pthread_mutex_lock(&music_mu);
    music_close();
    pthread_mutex_unlock(&music_mu);
}

/* Catalog bits 1..4 are Burning, Science, Explosive and Xenobiology.
   Bit 0 is the bundle sku. 31 marks every pack owned. The level copies
   this mask when a mission starts, so it has to be set before that. */
static void unlock_premium(JNIEnv *env) {
    static jclass cls;
    static jfieldID mask;
    static int logged;
    if (!mask) {
        jclass local = (*env)->FindClass(env, "net/hexage/defense/bf");
        if ((*env)->ExceptionCheck(env) || !local) {
            (*env)->ExceptionClear(env);
            return;
        }
        cls = (*env)->NewGlobalRef(env, local);
        (*env)->DeleteLocalRef(env, local);
        mask = (*env)->GetStaticFieldID(env, cls, "a", "I");
        if ((*env)->ExceptionCheck(env) || !mask) {
            (*env)->ExceptionClear(env);
            mask = NULL;
            return;
        }
    }
    jint v = (*env)->GetStaticIntField(env, cls, mask);
    if ((v & 31) == 31) return;
    (*env)->SetStaticIntField(env, cls, mask, v | 31);
    if (!logged) {
        logged = 1;
        fprintf(stderr, "premium packs on\n");
    }
}

JNIEXPORT void JNICALL Java_mojo_SoundEngineSL_update(JNIEnv *env, jobject self) {
    (void)self;
    unlock_premium(env);
    if (!audio_ready) return;
    music_produce(12000);
    int fire = 0;
    pthread_mutex_lock(&state_mu);
    if (music_done) {
        music_done = 0;
        fire = 1;
    }
    pthread_mutex_unlock(&state_mu);
    if (fire) fire_complete(env);
}
