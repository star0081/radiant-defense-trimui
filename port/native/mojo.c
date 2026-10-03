#include "jni_min.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_SIMD
#define STBI_NO_STDIO
#include "stb_image.h"

#ifdef _WIN32
#include <windows.h>
static void *so_open(const char *name) { return (void *)LoadLibraryA(name); }
static void *so_sym(void *h, const char *name) { return (void *)GetProcAddress((HMODULE)h, name); }
static int thread_id(void) { return (int)GetCurrentThreadId(); }
#else
#include <dlfcn.h>
#include <pthread.h>
static void *so_open(const char *name) { return dlopen(name, RTLD_LAZY | RTLD_GLOBAL); }
static void *so_sym(void *h, const char *name) { return dlsym(h, name); }
static int thread_id(void) { return (int)(uintptr_t)pthread_self(); }
#endif

static const unsigned char *fake_get_string(int name);
static void *gles;
static int gl_thread;
static int gl_ready;
#ifndef _WIN32
static pthread_t gl_pthread;
#endif
static char *apk_path;

static void *sdl;
static void *window;
static void *glctx;
static void *pad;
static int win_w = 1280;
static int win_h = 720;
static uint8_t prev_btn[16];
static float prev_axes[6];
static int have_axes;
static int key_logs;
static int cursor_on;
static int pointer_down;
static int prev_click;
static float cur_x = 640.f;
static float cur_y = 360.f;
static double cursor_clock;
static unsigned cursor_prog;
static int cursor_color = -1;

typedef int (*sdl_init_fn)(uint32_t);
typedef void *(*sdl_create_fn)(const char *, int, int, int, int, uint32_t);
typedef int (*sdl_glattr_fn)(int, int);
typedef void *(*sdl_glcreate_fn)(void *);
typedef int (*sdl_glmake_fn)(void *, void *);
typedef void (*sdl_swap_fn)(void *);
typedef int (*sdl_swapint_fn)(int);
typedef int (*sdl_hint_fn)(const char *, const char *);
typedef const char *(*sdl_err_fn)(void);
typedef int (*sdl_poll_fn)(void *);
typedef int (*sdl_numjoy_fn)(void);
typedef int (*sdl_isgc_fn)(int);
typedef void *(*sdl_gcopen_fn)(int);
typedef uint8_t (*sdl_gcbtn_fn)(void *, int);
typedef int16_t (*sdl_gcaxis_fn)(void *, int);
typedef void (*sdl_winsize_fn)(void *, int *, int *);
typedef uint32_t (*sdl_getmouse_fn)(int *, int *);

static sdl_init_fn p_init;
static sdl_create_fn p_create;
static sdl_glattr_fn p_glattr;
static sdl_glcreate_fn p_glcreate;
static sdl_glmake_fn p_glmake;
static sdl_swap_fn p_swap;
static sdl_swapint_fn p_swapint;
static sdl_hint_fn p_hint;
static sdl_err_fn p_err;
static sdl_poll_fn p_poll;
static sdl_numjoy_fn p_numjoy;
static sdl_isgc_fn p_isgc;
static sdl_gcopen_fn p_gcopen;
static sdl_gcbtn_fn p_gcbtn;
static sdl_gcaxis_fn p_gcaxis;
static sdl_winsize_fn p_winsize;

static jclass sys_cls;
static jmethodID mid_key, mid_axes, mid_touch, mid_quit;

static const char *sdl_err(void) {
    return p_err ? p_err() : "sdl";
}

void *glsym(const char *name) {
    void *p = NULL;
    if (gles) {
        p = so_sym(gles, name);
    }
#ifndef _WIN32
    if (!p) {
        p = dlsym(RTLD_DEFAULT, name);
    }
#endif
    if (!p && strcmp(name, "glGetString") == 0) {
        return (void *)fake_get_string;
    }
    return p;
}

static const unsigned char *fake_get_string(int name) {
    if (name == 0x1F01) return (const unsigned char *)"PowerVR Rogue GE8300";
    if (name == 0x1F00) return (const unsigned char *)"Imagination Technologies";
    if (name == 0x1F02) return (const unsigned char *)"OpenGL ES 2.0";
    return (const unsigned char *)"";
}

void *bufptr(JNIEnv *env, jobject buf) {
    if (!buf) return NULL;
    void *p = (*env)->GetDirectBufferAddress(env, buf);
    if (!p) return NULL;
    jclass c = (*env)->GetObjectClass(env, buf);
    jmethodID m = (*env)->GetMethodID(env, c, "position", "()I");
    if (!m || (*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        return p;
    }
    jint pos = (*env)->CallIntMethod(env, buf, m);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        return p;
    }
    return (char *)p + (pos < 0 ? 0 : pos);
}

static void bind_sys(JNIEnv *env) {
    if (sys_cls) return;
    jclass local = (*env)->FindClass(env, "port/Sys");
    if (!local || (*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
        return;
    }
    sys_cls = (*env)->NewGlobalRef(env, local);
    mid_key = (*env)->GetStaticMethodID(env, sys_cls, "onKey", "(II)V");
    mid_axes = (*env)->GetStaticMethodID(env, sys_cls, "onAxes", "(FFFFFF)V");
    mid_touch = (*env)->GetStaticMethodID(env, sys_cls, "onTouch", "(IFF)V");
    mid_quit = (*env)->GetStaticMethodID(env, sys_cls, "onQuit", "()V");
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
    }
}

static void call_key(JNIEnv *env, int code, int down) {
    bind_sys(env);
    if (!mid_key) return;
    (*env)->CallStaticVoidMethod(env, sys_cls, mid_key, code, down);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
    }
}

static void call_touch(JNIEnv *env, int action, float x, float y) {
    bind_sys(env);
    if (!mid_touch) return;
    (*env)->CallStaticVoidMethod(env, sys_cls, mid_touch, action, x, y);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
    }
}

static void call_axes(JNIEnv *env, float lx, float ly, float rx, float ry, float lt, float rt) {
    bind_sys(env);
    if (!mid_axes) return;
    (*env)->CallStaticVoidMethod(env, sys_cls, mid_axes, lx, ly, rx, ry, lt, rt);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
    }
}

static int key_from_sym(int sym) {
    if (sym == 27) return 4;
    if (sym == 13 || sym == 271) return 66;
    if (sym == 0x40000052) return 19;
    if (sym == 0x40000051) return 20;
    if (sym == 0x40000050) return 21;
    if (sym == 0x4000004F) return 22;
    if (sym == 'z' || sym == 'Z' || sym == 'a' || sym == 'A') return 96;
    if (sym == 'x' || sym == 'X' || sym == 'b' || sym == 'B') return 97;
    if (sym == 's' || sym == 'S') return 100;
    if (sym == 'd' || sym == 'D') return 99;
    return 0;
}

static void open_pad(void) {
    if (pad || !p_numjoy || !p_isgc || !p_gcopen) return;
    int n = p_numjoy();
    fprintf(stderr, "joysticks %d\n", n);
    for (int i = 0; i < n; i++) {
        if (p_isgc(i)) {
            pad = p_gcopen(i);
            fprintf(stderr, "controller %d -> %p\n", i, pad);
            if (pad) break;
        }
    }
}

static double now_sec(void) {
#ifdef _WIN32
    static LARGE_INTEGER freq;
    LARGE_INTEGER c;
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
#endif
}

static float stick_curve(float v) {
    float a = v < 0.f ? -v : v;
    if (a < 0.18f) return 0.f;
    a = (a - 0.18f) / 0.82f;
    a = a * a;
    return v < 0.f ? -a : a;
}

static void clip_xy(float x, float y, float *ox, float *oy) {
    float w = win_w > 0 ? (float)win_w : 1280.f;
    float h = win_h > 0 ? (float)win_h : 720.f;
    *ox = (x / w) * 2.f - 1.f;
    *oy = 1.f - (y / h) * 2.f;
}

static void push_tri(float *v, int *n, float x0, float y0, float x1, float y1, float x2, float y2) {
    float *p = v + (*n) * 2;
    clip_xy(x0, y0, p, p + 1);
    clip_xy(x1, y1, p + 2, p + 3);
    clip_xy(x2, y2, p + 4, p + 5);
    *n += 3;
}

static int cursor_shader(void) {
    if (cursor_prog) return 1;
    typedef unsigned int (*gl_create_shader_fn)(unsigned int);
    typedef void (*gl_shader_source_fn)(unsigned int, int, const char *const *, const int *);
    typedef void (*gl_compile_fn)(unsigned int);
    typedef void (*gl_get_shader_fn)(unsigned int, unsigned int, int *);
    typedef unsigned int (*gl_create_prog_fn)(void);
    typedef void (*gl_attach_fn)(unsigned int, unsigned int);
    typedef void (*gl_bind_attr_fn)(unsigned int, unsigned int, const char *);
    typedef void (*gl_link_fn)(unsigned int);
    typedef void (*gl_get_prog_fn)(unsigned int, unsigned int, int *);
    gl_create_shader_fn create_shader = (gl_create_shader_fn)glsym("glCreateShader");
    gl_shader_source_fn shader_source = (gl_shader_source_fn)glsym("glShaderSource");
    gl_compile_fn compile = (gl_compile_fn)glsym("glCompileShader");
    gl_get_shader_fn get_shader = (gl_get_shader_fn)glsym("glGetShaderiv");
    gl_create_prog_fn create_prog = (gl_create_prog_fn)glsym("glCreateProgram");
    gl_attach_fn attach = (gl_attach_fn)glsym("glAttachShader");
    gl_bind_attr_fn bind_attr = (gl_bind_attr_fn)glsym("glBindAttribLocation");
    gl_link_fn link = (gl_link_fn)glsym("glLinkProgram");
    gl_get_prog_fn get_prog = (gl_get_prog_fn)glsym("glGetProgramiv");
    if (!create_shader || !shader_source || !compile || !create_prog || !attach || !link) return 0;
    const char *vs_src =
        "attribute vec2 aPos;\n"
        "void main(){ gl_Position=vec4(aPos,0.0,1.0); }\n";
    const char *fs_src =
        "precision mediump float;\n"
        "uniform vec4 uColor;\n"
        "void main(){ gl_FragColor=uColor; }\n";
    unsigned vs = create_shader(0x8B31);
    unsigned fs = create_shader(0x8B30);
    shader_source(vs, 1, &vs_src, NULL);
    shader_source(fs, 1, &fs_src, NULL);
    compile(vs);
    compile(fs);
    int ok_vs = 0, ok_fs = 0;
    if (get_shader) {
        get_shader(vs, 0x8B81, &ok_vs);
        get_shader(fs, 0x8B81, &ok_fs);
    }
    unsigned prog = create_prog();
    attach(prog, vs);
    attach(prog, fs);
    if (bind_attr) bind_attr(prog, 0, "aPos");
    link(prog);
    int ok = 0;
    if (get_prog) get_prog(prog, 0x8B82, &ok);
    if (!ok) {
        fprintf(stderr, "cursor shader failed vs=%d fs=%d link=%d\n", ok_vs, ok_fs, ok);
        return 0;
    }
    typedef int (*gl_loc_fn)(unsigned int, const char *);
    gl_loc_fn loc = (gl_loc_fn)glsym("glGetUniformLocation");
    cursor_color = loc ? loc(prog, "uColor") : -1;
    cursor_prog = prog;
    fprintf(stderr, "cursor shader ok\n");
    return 1;
}

static void draw_cursor(void) {
    if (!cursor_on || !cursor_shader()) return;
    typedef void (*gl_use_fn)(unsigned int);
    typedef void (*gl_get_fn)(unsigned int, int *);
    typedef void (*gl_enable_fn)(unsigned int);
    typedef void (*gl_disable_fn)(unsigned int);
    typedef unsigned char (*gl_is_fn)(unsigned int);
    typedef void (*gl_blendsep_fn)(unsigned int, unsigned int, unsigned int, unsigned int);
    typedef void (*gl_viewport_fn)(int, int, int, int);
    typedef void (*gl_uniform4_fn)(int, float, float, float, float);
    typedef void (*gl_enable_attr_fn)(unsigned int);
    typedef void (*gl_disable_attr_fn)(unsigned int);
    typedef void (*gl_attr_fn)(unsigned int, int, unsigned int, unsigned char, int, const void *);
    typedef void (*gl_draw_fn)(unsigned int, int, int);
    typedef void (*gl_bindbuf_fn)(unsigned int, unsigned int);
    typedef void (*gl_getattrib_fn)(unsigned int, unsigned int, int *);
    typedef void (*gl_getptr_fn)(unsigned int, unsigned int, void **);
    gl_use_fn use = (gl_use_fn)glsym("glUseProgram");
    gl_get_fn geti = (gl_get_fn)glsym("glGetIntegerv");
    gl_enable_fn enable = (gl_enable_fn)glsym("glEnable");
    gl_disable_fn disable = (gl_disable_fn)glsym("glDisable");
    gl_is_fn is_on = (gl_is_fn)glsym("glIsEnabled");
    gl_blendsep_fn blendsep = (gl_blendsep_fn)glsym("glBlendFuncSeparate");
    gl_viewport_fn viewport = (gl_viewport_fn)glsym("glViewport");
    gl_uniform4_fn color = (gl_uniform4_fn)glsym("glUniform4f");
    gl_enable_attr_fn enable_attr = (gl_enable_attr_fn)glsym("glEnableVertexAttribArray");
    gl_disable_attr_fn disable_attr = (gl_disable_attr_fn)glsym("glDisableVertexAttribArray");
    gl_attr_fn attr = (gl_attr_fn)glsym("glVertexAttribPointer");
    gl_draw_fn draw = (gl_draw_fn)glsym("glDrawArrays");
    gl_bindbuf_fn bindbuf = (gl_bindbuf_fn)glsym("glBindBuffer");
    gl_getattrib_fn getattrib = (gl_getattrib_fn)glsym("glGetVertexAttribiv");
    gl_getptr_fn getptr = (gl_getptr_fn)glsym("glGetVertexAttribPointerv");
    if (!use || !draw || !attr || !enable_attr) return;

    /* The game caches blend mode and vertex bindings and skips the GL call
       when they have not changed. Shots, the range ring and fog are drawn
       with premultiplied alpha (source alpha often 0, so the blend itself
       adds the color). Leaving a different blend or a dangling attrib here
       makes those effects disappear on the next frame. */
    int prog = 0, buf = 0, vp[4] = {0, 0, win_w, win_h};
    int blend_src = 1, blend_dst = 0x0303, blend_src_a = 1, blend_dst_a = 0x0303;
    int attr_on = 0, attr_size = 4, attr_type = 0x1406, attr_norm = 0, attr_stride = 0, attr_buf = 0;
    void *attr_ptr = 0;
    if (geti) {
        geti(0x8B8D, &prog);
        geti(0x8894, &buf);
        geti(0x0BA2, vp);
        geti(0x80C9, &blend_src);
        geti(0x80C8, &blend_dst);
        geti(0x80CB, &blend_src_a);
        geti(0x80CA, &blend_dst_a);
    }
    if (getattrib) {
        getattrib(0, 0x8622, &attr_on);
        getattrib(0, 0x8623, &attr_size);
        getattrib(0, 0x8625, &attr_type);
        getattrib(0, 0x886A, &attr_norm);
        getattrib(0, 0x8624, &attr_stride);
        getattrib(0, 0x889F, &attr_buf);
    }
    if (getptr) getptr(0, 0x8645, &attr_ptr);
    unsigned char depth = is_on ? is_on(0x0B71) : 0;
    unsigned char scissor = is_on ? is_on(0x0C11) : 0;
    unsigned char cull = is_on ? is_on(0x0B44) : 0;
    unsigned char blend_on = is_on ? is_on(0x0BE2) : 1;
    if (bindbuf) bindbuf(0x8892, 0);
    if (viewport) viewport(0, 0, win_w, win_h);
    if (disable) {
        disable(0x0B71);
        disable(0x0C11);
        disable(0x0B44);
    }
    if (enable) enable(0x0BE2);
    use(cursor_prog);
    enable_attr(0);

    float x = cur_x, y = cur_y;
    float dark[18], lite[18];
    int nd = 0, nl = 0;
    push_tri(dark, &nd, x, y, x + 28.f, y + 10.f, x + 10.f, y + 28.f);
    push_tri(lite, &nl, x + 3.f, y + 3.f, x + 22.f, y + 10.f, x + 10.f, y + 22.f);
    if (color && cursor_color >= 0) color(cursor_color, 0.f, 0.f, 0.f, 0.9f);
    attr(0, 2, 0x1406, 0, 0, dark);
    draw(4, 0, nd);
    if (color && cursor_color >= 0) color(cursor_color, 1.f, 0.92f, 0.2f, 1.f);
    attr(0, 2, 0x1406, 0, 0, lite);
    draw(4, 0, nl);

    use((unsigned)(prog < 0 ? 0 : prog));
    if (bindbuf) bindbuf(0x8892, (unsigned)(attr_buf < 0 ? 0 : attr_buf));
    if (attr_size < 1) attr_size = 4;
    attr(0, attr_size, (unsigned)attr_type, (unsigned char)(attr_norm ? 1 : 0), attr_stride, attr_ptr);
    if (attr_on) enable_attr(0);
    else if (disable_attr) disable_attr(0);
    if (bindbuf) bindbuf(0x8892, (unsigned)(buf < 0 ? 0 : buf));
    if (viewport) viewport(vp[0], vp[1], vp[2], vp[3]);
    if (blendsep) blendsep((unsigned)blend_src, (unsigned)blend_dst,
            (unsigned)blend_src_a, (unsigned)blend_dst_a);
    if (depth && enable) enable(0x0B71); else if (disable) disable(0x0B71);
    if (scissor && enable) enable(0x0C11); else if (disable) disable(0x0C11);
    if (cull && enable) enable(0x0B44); else if (disable) disable(0x0B44);
    if (blend_on && enable) enable(0x0BE2); else if (disable) disable(0x0BE2);
}

static void poll_input(JNIEnv *env) {
    if (!p_poll) return;
    unsigned char ev[256];
    int guard = 0;
    while (p_poll(ev) && guard++ < 64) {
        uint32_t type = *(uint32_t *)ev;
        if (type == 0x100) {
            bind_sys(env);
            if (mid_quit) (*env)->CallStaticVoidMethod(env, sys_cls, mid_quit);
            return;
        }
        if (type == 0x300 || type == 0x301) {
            int sym = *(int32_t *)(ev + 20);
            int repeat = ev[13];
            int code = key_from_sym(sym);
            /* Face buttons also arrive as keys. ESC on top of A kills the menu. */
            if (pad && code == 4) code = 0;
            if (code && !repeat) {
                if (key_logs < 40) {
                    key_logs++;
                    fprintf(stderr, "key %d %d\n", code, type == 0x300 ? 1 : 0);
                }
                call_key(env, code, type == 0x300 ? 1 : 0);
            }
        } else if (type == 0x401 || type == 0x402) {
            int x = *(int32_t *)(ev + 20);
            int y = *(int32_t *)(ev + 24);
            int action = type == 0x401 ? 0 : 1;
            call_touch(env, action, (float)x, (float)y);
        } else if (type == 0x400) {
            uint32_t state = *(uint32_t *)(ev + 16);
            if (state) {
                int x = *(int32_t *)(ev + 20);
                int y = *(int32_t *)(ev + 24);
                call_touch(env, 2, (float)x, (float)y);
            }
        } else if (type == 0x700 || type == 0x701 || type == 0x702) {
            float x = *(float *)(ev + 24);
            float y = *(float *)(ev + 28);
            int action = type == 0x700 ? 0 : type == 0x701 ? 1 : 2;
            call_touch(env, action, x * (float)win_w, y * (float)win_h);
        } else if (type == 0x651) {
            open_pad();
        }
    }
    if (!pad) open_pad();
    if (pad && p_gcbtn && p_gcaxis) {
        /* SDL order is Xbox: A south, B east. This pad is labeled the other way
           (A on the right, B on the bottom), matching retrogame a:b1,b:b0.
           Select is BACK, Start is START, L/R are the shoulders. Those four
           drive the cursor instead of game keys. */
        static const int map[15] = {97, 96, 99, 100, 4, 82, 108, 106, 107, 102, 103, 19, 20, 21, 22};
        uint8_t btn[15];
        for (int i = 0; i < 15; i++) btn[i] = p_gcbtn(pad, i);
        int select = btn[4] ? 1 : 0;
        int start = btn[6] ? 1 : 0;
        int select_edge = select && !prev_btn[4];
        int start_edge = start && !prev_btn[6];
        if ((select_edge && start) || (start_edge && select)) {
            fprintf(stderr, "quit start+select\n");
            if (pointer_down) {
                call_touch(env, 1, cur_x, cur_y);
                pointer_down = 0;
            }
            bind_sys(env);
            if (mid_quit) (*env)->CallStaticVoidMethod(env, sys_cls, mid_quit);
            return;
        }
        if (select_edge) {
            if (pointer_down) {
                call_touch(env, 1, cur_x, cur_y);
                pointer_down = 0;
            }
            cursor_on = !cursor_on;
            cursor_clock = 0;
            call_axes(env, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
            fprintf(stderr, "cursor %s\n", cursor_on ? "on" : "off");
        }
        for (int i = 0; i < 15; i++) {
            if (i == 4 || i == 6 || i == 9 || i == 10) {
                prev_btn[i] = btn[i];
                continue;
            }
            if (btn[i] != prev_btn[i]) {
                prev_btn[i] = btn[i];
                if (key_logs < 40) {
                    key_logs++;
                    fprintf(stderr, "pad %d -> %d %d\n", i, map[i], btn[i] ? 1 : 0);
                }
                call_key(env, map[i], btn[i] ? 1 : 0);
            }
        }
        float ax[6];
        for (int i = 0; i < 6; i++) {
            int v = p_gcaxis(pad, i);
            if (i >= 4) {
                if (v < 0) v = 0;
                ax[i] = v / 32767.0f;
            } else {
                ax[i] = v / 32767.0f;
            }
        }
        int click = btn[9] || btn[10] || ax[4] > 0.55f || ax[5] > 0.55f;
        int moved = 0;
        if (cursor_on) {
            double t = now_sec();
            float dt = 0.016f;
            if (cursor_clock > 0.0) {
                dt = (float)(t - cursor_clock);
                if (dt < 0.f || dt > 0.05f) dt = 0.016f;
            }
            cursor_clock = t;
            float nx = cur_x + stick_curve(ax[0]) * 1100.f * dt;
            float ny = cur_y + stick_curve(ax[1]) * 1100.f * dt;
            if (nx < 0.f) nx = 0.f;
            if (ny < 0.f) ny = 0.f;
            if (nx > (float)(win_w - 1)) nx = (float)(win_w - 1);
            if (ny > (float)(win_h - 1)) ny = (float)(win_h - 1);
            if (nx != cur_x || ny != cur_y) {
                cur_x = nx;
                cur_y = ny;
                moved = 1;
            }
            if (click && !pointer_down) {
                pointer_down = 1;
                call_touch(env, 0, cur_x, cur_y);
            } else if (!click && pointer_down) {
                pointer_down = 0;
                call_touch(env, 1, cur_x, cur_y);
            } else if (pointer_down && moved) {
                call_touch(env, 2, cur_x, cur_y);
            }
        } else {
            int changed = !have_axes;
            for (int i = 0; i < 6; i++) {
                float d = ax[i] - prev_axes[i];
                if (d > 0.04f || d < -0.04f) changed = 1;
            }
            if (changed) {
                have_axes = 1;
                call_axes(env, ax[0], ax[1], ax[2], ax[3], ax[4], ax[5]);
            }
        }
        prev_click = click;
        for (int i = 0; i < 6; i++) prev_axes[i] = ax[i];
    }
}

static int load_sdl(void) {
    if (sdl) return 1;
    const char *names[] = {
        "/usr/trimui/lib/libSDL2-2.0.so.0",
        "libSDL2-2.0.so.0",
        "libSDL2.so",
        NULL
    };
    for (int i = 0; names[i]; i++) {
        sdl = so_open(names[i]);
        if (sdl) {
            fprintf(stderr, "sdl %s\n", names[i]);
            break;
        }
    }
    if (!sdl) {
        fprintf(stderr, "libSDL2 not found\n");
        return 0;
    }
    p_init = (sdl_init_fn)so_sym(sdl, "SDL_Init");
    p_create = (sdl_create_fn)so_sym(sdl, "SDL_CreateWindow");
    p_glattr = (sdl_glattr_fn)so_sym(sdl, "SDL_GL_SetAttribute");
    p_glcreate = (sdl_glcreate_fn)so_sym(sdl, "SDL_GL_CreateContext");
    p_glmake = (sdl_glmake_fn)so_sym(sdl, "SDL_GL_MakeCurrent");
    p_swap = (sdl_swap_fn)so_sym(sdl, "SDL_GL_SwapWindow");
    p_swapint = (sdl_swapint_fn)so_sym(sdl, "SDL_GL_SetSwapInterval");
    p_hint = (sdl_hint_fn)so_sym(sdl, "SDL_SetHint");
    p_err = (sdl_err_fn)so_sym(sdl, "SDL_GetError");
    p_poll = (sdl_poll_fn)so_sym(sdl, "SDL_PollEvent");
    p_numjoy = (sdl_numjoy_fn)so_sym(sdl, "SDL_NumJoysticks");
    p_isgc = (sdl_isgc_fn)so_sym(sdl, "SDL_IsGameController");
    p_gcopen = (sdl_gcopen_fn)so_sym(sdl, "SDL_GameControllerOpen");
    p_gcbtn = (sdl_gcbtn_fn)so_sym(sdl, "SDL_GameControllerGetButton");
    p_gcaxis = (sdl_gcaxis_fn)so_sym(sdl, "SDL_GameControllerGetAxis");
    p_winsize = (sdl_winsize_fn)so_sym(sdl, "SDL_GetWindowSize");
    if (!p_init || !p_create || !p_glcreate) {
        fprintf(stderr, "sdl missing symbols\n");
        return 0;
    }
    return 1;
}

static void load_gles(void) {
    if (gles) return;
    const char *names[] = {
        "/usr/trimui/lib/libGLESv2.so",
        "/usr/trimui/lib/libGLESv2.so.2",
        "libGLESv2.so",
        "libGLESv2.so.2",
        NULL
    };
    for (int i = 0; names[i]; i++) {
        gles = so_open(names[i]);
        if (gles) {
            fprintf(stderr, "gles %s\n", names[i]);
            return;
        }
    }
    fprintf(stderr, "libGLESv2 not preloaded, using default symbols\n");
}

static int make_window(void) {
    if (window && glctx) {
        if (p_glmake) p_glmake(window, glctx);
        return 1;
    }
    if (!load_sdl()) return 0;
    if (p_hint) {
        p_hint("SDL_NO_SIGNAL_HANDLERS", "1");
        p_hint("SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS", "0");
        p_hint("SDL_RENDER_VSYNC", "0");
        p_hint("SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS", "1");
        p_hint("SDL_JOYSTICK_HIDAPI", "0");
    }
    if (p_init(0x20 | 0x200 | 0x2000 | 0x10) != 0) {
        fprintf(stderr, "SDL_Init %s\n", sdl_err());
        if (p_init(0x20) != 0) {
            fprintf(stderr, "SDL_Init video %s\n", sdl_err());
            return 0;
        }
    }
    int majors[] = {2, 2, 1};
    int profiles[] = {0x4, 0, 0};
    for (int attempt = 0; attempt < 3 && !glctx; attempt++) {
        if (p_glattr) {
            p_glattr(17, majors[attempt]);
            p_glattr(18, 0);
            p_glattr(21, profiles[attempt]);
            p_glattr(5, 1);
            p_glattr(6, 24);
            p_glattr(0, 8);
            p_glattr(1, 8);
            p_glattr(2, 8);
            p_glattr(3, 8);
        }
        uint32_t flags = 0x2 | 0x1001;
        window = p_create("Radiant Defense", 0x1FFF0000, 0x1FFF0000, 1280, 720, flags);
        if (!window) {
            fprintf(stderr, "window fullscreen failed %s\n", sdl_err());
            window = p_create("Radiant Defense", 0, 0, 1280, 720, 0x2);
        }
        if (!window) {
            fprintf(stderr, "window failed %s\n", sdl_err());
            return 0;
        }
        glctx = p_glcreate(window);
        if (!glctx) {
            fprintf(stderr, "context attempt %d failed %s\n", attempt, sdl_err());
            window = NULL;
        }
    }
    if (!glctx) return 0;
    if (p_glmake) p_glmake(window, glctx);
    if (p_swapint) p_swapint(0);
    if (p_winsize) p_winsize(window, &win_w, &win_h);
    fprintf(stderr, "window %dx%d\n", win_w, win_h);
    load_gles();
    open_pad();
    gl_ready = 1;
    gl_thread = thread_id();
#ifndef _WIN32
    gl_pthread = pthread_self();
#endif
    return 1;
}

static int on_gl_thread(void) {
    if (!gl_ready) return 0;
#ifdef _WIN32
    return thread_id() == gl_thread;
#else
    return pthread_equal(pthread_self(), gl_pthread);
#endif
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    (void)vm;
    (void)reserved;
    return 0x00010006;
}

JNIEXPORT void JNICALL Java_port_Natives_setApk(JNIEnv *env, jclass cls, jstring path) {
    (void)cls;
    if (!path) return;
    const char *s = (*env)->GetStringUTFChars(env, path, NULL);
    if (!s) return;
    free(apk_path);
    apk_path = strdup(s);
    (*env)->ReleaseStringUTFChars(env, path, s);
    fprintf(stderr, "apk %s\n", apk_path);
}

JNIEXPORT void JNICALL Java_port_Natives_makeCurrent(JNIEnv *env, jclass cls) {
    (void)cls;
    if (!make_window()) {
        fprintf(stderr, "makeCurrent without a window\n");
        return;
    }
    poll_input(env);
}

JNIEXPORT void JNICALL Java_port_Natives_swap(JNIEnv *env, jclass cls) {
    (void)cls;
    draw_cursor();
    if (p_swap && window) p_swap(window);
    if (on_gl_thread()) poll_input(env);
}

JNIEXPORT void JNICALL Java_port_Natives_pump(JNIEnv *env, jclass cls) {
    (void)cls;
    if (on_gl_thread()) poll_input(env);
}

static int decode_mem(JNIEnv *env, jobject self, const unsigned char *data, int len) {
    int w = 0, h = 0, n = 0;
    unsigned char *px = stbi_load_from_memory(data, len, &w, &h, &n, 4);
    if (!px) {
        fprintf(stderr, "png fail %s\n", stbi_failure_reason());
        return 1;
    }
    jclass c = (*env)->GetObjectClass(env, self);
    jfieldID fw = (*env)->GetFieldID(env, c, "width", "I");
    jfieldID fh = (*env)->GetFieldID(env, c, "height", "I");
    jfieldID fc = (*env)->GetFieldID(env, c, "components", "I");
    jfieldID fd = (*env)->GetFieldID(env, c, "data", "Ljava/nio/ByteBuffer;");
    if ((*env)->ExceptionCheck(env) || !fw || !fd) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
        stbi_image_free(px);
        return 1;
    }
    (*env)->SetIntField(env, self, fw, w);
    (*env)->SetIntField(env, self, fh, h);
    (*env)->SetIntField(env, self, fc, 4);
    jobject bb = (*env)->NewDirectByteBuffer(env, px, (jlong)w * (jlong)h * 4);
    (*env)->SetObjectField(env, self, fd, bb);
    return 0;
}

JNIEXPORT void JNICALL Java_mojo_ImageLoader_nativeInit(JNIEnv *env, jclass cls) {
    (void)env;
    (void)cls;
    fprintf(stderr, "ImageLoader.nativeInit\n");
}

JNIEXPORT jint JNICALL Java_mojo_ImageLoader_decodeFile(JNIEnv *env, jobject self, jstring path, jint offset, jint length) {
    const char *s = path ? (*env)->GetStringUTFChars(env, path, NULL) : NULL;
    if (!s || length <= 0) return 1;
    FILE *f = fopen(s, "rb");
    (*env)->ReleaseStringUTFChars(env, path, s);
    if (!f) {
        fprintf(stderr, "decodeFile open failed\n");
        return 1;
    }
    if (fseek(f, offset, SEEK_SET) != 0) {
        fclose(f);
        return 1;
    }
    unsigned char *buf = (unsigned char *)malloc((size_t)length);
    if (!buf) {
        fclose(f);
        return 1;
    }
    size_t n = fread(buf, 1, (size_t)length, f);
    fclose(f);
    int rc = decode_mem(env, self, buf, (int)n);
    free(buf);
    return rc;
}

JNIEXPORT jint JNICALL Java_mojo_ImageLoader_decodeBuffer(JNIEnv *env, jobject self, jbyteArray arr, jint offset, jint length) {
    if (!arr || length <= 0) return 1;
    jbyte *b = (*env)->GetByteArrayElements(env, arr, NULL);
    if (!b) return 1;
    int rc = decode_mem(env, self, (unsigned char *)b + offset, length);
    (*env)->ReleaseByteArrayElements(env, arr, b, JNI_ABORT);
    return rc;
}

JNIEXPORT void JNICALL Java_mojo_ImageLoader_releaseData(JNIEnv *env, jobject self, jobject buffer) {
    (void)self;
    if (!buffer) return;
    void *p = (*env)->GetDirectBufferAddress(env, buffer);
    if (p) stbi_image_free(p);
}

typedef struct {
    unsigned char *data;
    int cap;
} GBuf;

static GBuf gbuf[4096];
static int ng;

static GBuf *gb(int id) {
    if (id <= 0 || id >= 4096) return NULL;
    return gbuf[id].data ? &gbuf[id] : NULL;
}

static void putf(GBuf *b, int off, float v) {
    if (!b || off < 0 || off + 4 > b->cap) return;
    memcpy(b->data + off, &v, 4);
}

static const char *gseen[16];
static int gcount[16];

static void glog(const char *msg, int a, int b, int c) {
    int slot = -1;
    int i;
    for (i = 0; i < 16; i++) {
        if (!gseen[i]) {
            gseen[i] = msg;
            slot = i;
            break;
        }
        if (strcmp(gseen[i], msg) == 0) {
            slot = i;
            break;
        }
    }
    if (slot < 0 || gcount[slot] >= 6) return;
    gcount[slot]++;
    fprintf(stderr, "geom %s %d %d %d\n", msg, a, b, c);
}

JNIEXPORT jint JNICALL Java_mojo_GeometryData_Alloc(JNIEnv *env, jclass cls, jint size) {
    (void)env; (void)cls;
    if (size < 1) size = 1;
    if (ng + 1 >= 4096) return 0;
    int id = ++ng;
    gbuf[id].data = (unsigned char *)calloc(1, (size_t)size);
    gbuf[id].cap = size;
    glog("Alloc", id, size, 0);
    return id;
}

JNIEXPORT jint JNICALL Java_mojo_GeometryData_Realloc(JNIEnv *env, jclass cls, jint id, jint size) {
    (void)env; (void)cls;
    if (size < 1) size = 1;
    if (id <= 0 || id >= 4096) return Java_mojo_GeometryData_Alloc(env, cls, size);
    unsigned char *p = (unsigned char *)realloc(gbuf[id].data, (size_t)size);
    if (!p) return id;
    if (size > gbuf[id].cap) memset(p + gbuf[id].cap, 0, (size_t)(size - gbuf[id].cap));
    gbuf[id].data = p;
    gbuf[id].cap = size;
    glog("Realloc", id, size, 0);
    return id;
}

JNIEXPORT void JNICALL Java_mojo_GeometryData_Release(JNIEnv *env, jclass cls, jint id) {
    (void)env; (void)cls;
    if (id <= 0 || id >= 4096) return;
    free(gbuf[id].data);
    gbuf[id].data = NULL;
    gbuf[id].cap = 0;
}

JNIEXPORT jobject JNICALL Java_mojo_GeometryData_Buffer(JNIEnv *env, jclass cls, jint id, jint size) {
    (void)cls;
    GBuf *b = gb(id);
    if (!b) return NULL;
    if (size < 0 || size > b->cap) size = b->cap;
    return (*env)->NewDirectByteBuffer(env, b->data, size);
}

JNIEXPORT jint JNICALL Java_mojo_GeometryData_Compare(JNIEnv *env, jclass cls, jint id, jint a, jint b, jint n) {
    (void)env; (void)cls;
    GBuf *buf = gb(id);
    if (!buf || n <= 0) return 0;
    if (a < 0 || b < 0 || a + n > buf->cap || b + n > buf->cap) return 1;
    return memcmp(buf->data + a, buf->data + b, (size_t)n) == 0 ? 0 : 1;
}

JNIEXPORT void JNICALL Java_mojo_GeometryData_VertexP(JNIEnv *env, jclass cls, jint id, jint n, jfloat x, jfloat y) {
    (void)env; (void)cls;
    GBuf *b = gb(id);
    glog("VertexP", id, n, b ? b->cap : -1);
    if (!b) return;
    int off = n * 8;
    if (off + 8 > b->cap && n >= 0 && n + 8 <= b->cap) off = n;
    putf(b, off, x);
    putf(b, off + 4, y);
}

JNIEXPORT void JNICALL Java_mojo_GeometryData_VertexPT(JNIEnv *env, jclass cls, jint id, jint n, jfloat x, jfloat y, jfloat u, jfloat v) {
    (void)env; (void)cls;
    GBuf *b = gb(id);
    glog("VertexPT", id, n, b ? b->cap : -1);
    if (!b) return;
    int off = n * 16;
    if (off + 16 > b->cap && n >= 0 && n + 16 <= b->cap) off = n;
    putf(b, off, x);
    putf(b, off + 4, y);
    putf(b, off + 8, u);
    putf(b, off + 12, v);
}

static void write_pct(GBuf *b, int off, float x, float y, int color, float u, float v) {
    putf(b, off, x);
    putf(b, off + 4, y);
    if (off >= 0 && off + 12 <= b->cap) {
        b->data[off + 8] = (unsigned char)((color >> 16) & 255);
        b->data[off + 9] = (unsigned char)((color >> 8) & 255);
        b->data[off + 10] = (unsigned char)(color & 255);
        b->data[off + 11] = (unsigned char)((color >> 24) & 255);
    }
    putf(b, off + 12, u);
    putf(b, off + 16, v);
}

static void write_quad_pct(jint id, jint n, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, int color) {
    GBuf *b = gb(id);
    glog("QuadPCT", id, n, b ? b->cap : -1);
    if (!b) return;
    int base = n * 20;
    if (base < 0) base = 0;
    /* Triangle strip: TL, TR, BL, BR. glDrawArrays(GL_TRIANGLE_STRIP, first, 4). */
    write_pct(b, base, x0, y0, color, u0, v0);
    write_pct(b, base + 20, x1, y0, color, u1, v0);
    write_pct(b, base + 40, x0, y1, color, u0, v1);
    write_pct(b, base + 60, x1, y1, color, u1, v1);
}

JNIEXPORT void JNICALL Java_mojo_GeometryData_QuadPCT(JNIEnv *env, jclass cls, jint id, jint n,
        jfloat x0, jfloat y0, jfloat x1, jfloat y1, jfloat u0, jfloat v0, jfloat u1, jfloat v1, jint color) {
    (void)env; (void)cls;
    write_quad_pct(id, n, x0, y0, x1, y1, u0, v0, u1, v1, color);
}

JNIEXPORT void JNICALL Java_mojo_GeometryData_QuadPT(JNIEnv *env, jclass cls, jint id, jint n,
        jfloat x0, jfloat y0, jfloat x1, jfloat y1, jfloat u0, jfloat v0, jfloat u1, jfloat v1) {
    (void)env; (void)cls;
    GBuf *b = gb(id);
    glog("QuadPT", id, n, b ? b->cap : -1);
    if (!b) return;
    int base = n * 16;
    float xy[8] = {x0, y0, x1, y0, x0, y1, x1, y1};
    float uv[8] = {u0, v0, u1, v0, u0, v1, u1, v1};
    for (int i = 0; i < 4; i++) {
        putf(b, base + i * 16, xy[i * 2]);
        putf(b, base + i * 16 + 4, xy[i * 2 + 1]);
        putf(b, base + i * 16 + 8, uv[i * 2]);
        putf(b, base + i * 16 + 12, uv[i * 2 + 1]);
    }
}

static void rot2(float ox, float oy, float c, float s, float px, float py, float *x, float *y) {
    *x = ox + c * px - s * py;
    *y = oy + s * px + c * py;
}

static void aff2(float m00, float m01, float tx, float m10, float m11, float ty,
        float px, float py, float *x, float *y) {
    *x = tx + m00 * px + m01 * py;
    *y = ty + m10 * px + m11 * py;
}

JNIEXPORT void JNICALL Java_mojo_GeometryData_QuadPCTxRT(JNIEnv *env, jclass cls, jint id, jint n,
        jfloat ox, jfloat oy, jfloat c, jfloat s, jfloat x0, jfloat y0, jfloat x1, jfloat y1,
        jfloat u0, jfloat v0, jfloat u1, jfloat v1, jint color) {
    (void)env; (void)cls;
    float xa, ya, xb, yb, xc, yc, xd, yd;
    GBuf *b = gb(id);
    if (!b) return;
    rot2(ox, oy, c, s, x0, y0, &xa, &ya);
    rot2(ox, oy, c, s, x1, y0, &xb, &yb);
    rot2(ox, oy, c, s, x0, y1, &xc, &yc);
    rot2(ox, oy, c, s, x1, y1, &xd, &yd);
    int base = n * 20;
    write_pct(b, base, xa, ya, color, u0, v0);
    write_pct(b, base + 20, xb, yb, color, u1, v0);
    write_pct(b, base + 40, xc, yc, color, u0, v1);
    write_pct(b, base + 60, xd, yd, color, u1, v1);
}

JNIEXPORT void JNICALL Java_mojo_GeometryData_QuadPCTxM(JNIEnv *env, jclass cls, jint id, jint n,
        jfloat m00, jfloat m01, jfloat tx, jfloat m10, jfloat m11, jfloat ty,
        jfloat x0, jfloat y0, jfloat x1, jfloat y1, jfloat u0, jfloat v0, jfloat u1, jfloat v1, jint color) {
    (void)env; (void)cls;
    float xa, ya, xb, yb, xc, yc, xd, yd;
    GBuf *b = gb(id);
    if (!b) return;
    aff2(m00, m01, tx, m10, m11, ty, x0, y0, &xa, &ya);
    aff2(m00, m01, tx, m10, m11, ty, x1, y0, &xb, &yb);
    aff2(m00, m01, tx, m10, m11, ty, x0, y1, &xc, &yc);
    aff2(m00, m01, tx, m10, m11, ty, x1, y1, &xd, &yd);
    int base = n * 20;
    write_pct(b, base, xa, ya, color, u0, v0);
    write_pct(b, base + 20, xb, yb, color, u1, v0);
    write_pct(b, base + 40, xc, yc, color, u0, v1);
    write_pct(b, base + 60, xd, yd, color, u1, v1);
}

JNIEXPORT void JNICALL Java_mojo_GeometryData_Quads(JNIEnv *env, jclass cls, jint id, jint start, jint count) {
    (void)env; (void)cls;
    GBuf *b = gb(id);
    glog("Quads", id, start, count);
    int nquads = count / 6;
    if (!b || nquads <= 0) return;
    int off0 = start * 2;
    for (int q = 0; q < nquads; q++) {
        unsigned short base = (unsigned short)(q * 4);
        unsigned short idx[6] = {
            base, (unsigned short)(base + 1), (unsigned short)(base + 2),
            (unsigned short)(base + 2), (unsigned short)(base + 1), (unsigned short)(base + 3)
        };
        int off = off0 + q * 12;
        if (off < 0 || off + 12 > b->cap) break;
        memcpy(b->data + off, idx, 12);
    }
}

JNIEXPORT void JNICALL Java_mojo_GeometryData_Fan(JNIEnv *env, jclass cls, jint id, jint start, jint count) {
    (void)env; (void)cls;
    GBuf *b = gb(id);
    glog("Fan", id, start, count);
    int ntri = count / 3;
    if (!b || ntri < 1) return;
    int off0 = start * 2;
    for (int i = 0; i < ntri; i++) {
        unsigned short idx[3] = {0, (unsigned short)(i + 1), (unsigned short)(i + 2)};
        int off = off0 + i * 6;
        if (off < 0 || off + 6 > b->cap) break;
        memcpy(b->data + off, idx, 6);
    }
}

JNIEXPORT void JNICALL Java_mojo_GeometryData_Bounds(JNIEnv *env, jclass cls, jint id, jint start, jint stride, jint count, jfloatArray out) {
    (void)cls;
    GBuf *b = gb(id);
    glog("Bounds", id, start, stride);
    /* start is a vertex index. The original .so does base + start * stride.
       Treating start as a byte offset puts every sprite after the first one
       in the wrong place, so the tower can see it but shots never overlap. */
    float r[4] = {0.f, 0.f, 0.f, 0.f};
    if (b && stride >= 8 && count > 0) {
        float minx = 0.f, miny = 0.f, maxx = 0.f, maxy = 0.f;
        int ok = 0;
        for (int i = 0; i < count; i++) {
            int off = (start + i) * stride;
            if (off < 0 || off + 8 > b->cap) break;
            float x, y;
            memcpy(&x, b->data + off, 4);
            memcpy(&y, b->data + off + 4, 4);
            if (!ok || x < minx) minx = x;
            if (!ok || y < miny) miny = y;
            if (!ok || x > maxx) maxx = x;
            if (!ok || y > maxy) maxy = y;
            ok = 1;
        }
        if (ok) {
            r[0] = minx; r[1] = miny; r[2] = maxx; r[3] = maxy;
        }
    }
    if (out) (*env)->SetFloatArrayRegion(env, out, 0, 4, r);
}
