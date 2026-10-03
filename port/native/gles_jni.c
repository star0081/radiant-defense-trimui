/* generated */
#include "jni_min.h"
#include <stdint.h>
#include <stdio.h>
void *glsym(const char *name);
void *bufptr(JNIEnv *env, jobject buf);
static int gles_miss;

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glActiveTexture(JNIEnv *env, jclass clazz, jint a0) {
    typedef void (*fn)(int);
    fn f = (fn)glsym("glActiveTexture");
    if (!f) return;
    f(a0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glAttachShader(JNIEnv *env, jclass clazz, jint a0, jint a1) {
    typedef void (*fn)(int, int);
    fn f = (fn)glsym("glAttachShader");
    if (!f) return;
    f(a0, a1);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glBindBuffer(JNIEnv *env, jclass clazz, jint a0, jint a1) {
    typedef void (*fn)(int, int);
    fn f = (fn)glsym("glBindBuffer");
    if (!f) return;
    f(a0, a1);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glBindTexture(JNIEnv *env, jclass clazz, jint a0, jint a1) {
    typedef void (*fn)(int, int);
    fn f = (fn)glsym("glBindTexture");
    if (!f) return;
    f(a0, a1);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glBlendFunc(JNIEnv *env, jclass clazz, jint a0, jint a1) {
    typedef void (*fn)(int, int);
    fn f = (fn)glsym("glBlendFunc");
    if (!f) return;
    f(a0, a1);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glBufferData(JNIEnv *env, jclass clazz, jint a0, jint a1, jobject a2, jint a3) {
    typedef void (*fn)(unsigned int, intptr_t, const void *, unsigned int);
    fn f = (fn)glsym("glBufferData");
    if (!f) return;
    void *p = bufptr(env, a2);
    f((unsigned int)a0, (intptr_t)a1, p, (unsigned int)a3);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glBufferSubData(JNIEnv *env, jclass clazz, jint a0, jint a1, jint a2, jobject a3) {
    typedef void (*fn)(unsigned int, intptr_t, intptr_t, const void *);
    fn f = (fn)glsym("glBufferSubData");
    if (!f) return;
    void *p = bufptr(env, a3);
    f((unsigned int)a0, (intptr_t)a1, (intptr_t)a2, p);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glClear(JNIEnv *env, jclass clazz, jint a0) {
    typedef void (*fn)(int);
    fn f = (fn)glsym("glClear");
    if (!f) return;
    f(a0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glClearColor(JNIEnv *env, jclass clazz, jfloat a0, jfloat a1, jfloat a2, jfloat a3) {
    typedef void (*fn)(float, float, float, float);
    fn f = (fn)glsym("glClearColor");
    if (!f) return;
    f(a0, a1, a2, a3);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glCompileShader(JNIEnv *env, jclass clazz, jint a0) {
    typedef void (*fn)(int);
    fn f = (fn)glsym("glCompileShader");
    if (!f) return;
    f(a0);
}

JNIEXPORT jint JNICALL Java_android_opengl_GLES20_glCreateProgram(JNIEnv *env, jclass clazz) {
    typedef int (*fn)(void);
    fn f = (fn)glsym("glCreateProgram");
    if (!f) return 0;
    return f();
}

JNIEXPORT jint JNICALL Java_android_opengl_GLES20_glCreateShader(JNIEnv *env, jclass clazz, jint a0) {
    typedef int (*fn)(int);
    fn f = (fn)glsym("glCreateShader");
    if (!f) return 0;
    return f(a0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glDeleteBuffers(JNIEnv *env, jclass clazz, jint a0, jobject a1, jint a2) {
    typedef void (*fn)(int, unsigned int *);
    fn f = (fn)glsym("glDeleteBuffers");
    if (!f || !a1) return;
    jint *p = (*env)->GetIntArrayElements(env, a1, NULL);
    f(a0, (unsigned int *)(p + a2));
    (*env)->ReleaseIntArrayElements(env, a1, p, 0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glDeleteProgram(JNIEnv *env, jclass clazz, jint a0) {
    typedef void (*fn)(int);
    fn f = (fn)glsym("glDeleteProgram");
    if (!f) return;
    f(a0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glDeleteShader(JNIEnv *env, jclass clazz, jint a0) {
    typedef void (*fn)(int);
    fn f = (fn)glsym("glDeleteShader");
    if (!f) return;
    f(a0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glDeleteTextures(JNIEnv *env, jclass clazz, jint a0, jobject a1, jint a2) {
    typedef void (*fn)(int, unsigned int *);
    fn f = (fn)glsym("glDeleteTextures");
    if (!f || !a1) return;
    jint *p = (*env)->GetIntArrayElements(env, a1, NULL);
    f(a0, (unsigned int *)(p + a2));
    (*env)->ReleaseIntArrayElements(env, a1, p, 0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glDisable(JNIEnv *env, jclass clazz, jint a0) {
    typedef void (*fn)(int);
    fn f = (fn)glsym("glDisable");
    if (!f) return;
    f(a0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glDisableVertexAttribArray(JNIEnv *env, jclass clazz, jint a0) {
    typedef void (*fn)(int);
    fn f = (fn)glsym("glDisableVertexAttribArray");
    if (!f) return;
    f(a0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glDrawArrays(JNIEnv *env, jclass clazz, jint a0, jint a1, jint a2) {
    typedef void (*fn)(int, int, int);
    fn f = (fn)glsym("glDrawArrays");
    if (!f) return;
    f(a0, a1, a2);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glDrawElements(JNIEnv *env, jclass clazz, jint a0, jint a1, jint a2, jint a3) {
    typedef void (*fn)(int, int, int, int);
    fn f = (fn)glsym("glDrawElements");
    if (!f) return;
    f(a0, a1, a2, a3);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glEnable(JNIEnv *env, jclass clazz, jint a0) {
    typedef void (*fn)(int);
    fn f = (fn)glsym("glEnable");
    if (!f) return;
    f(a0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glEnableVertexAttribArray(JNIEnv *env, jclass clazz, jint a0) {
    typedef void (*fn)(int);
    fn f = (fn)glsym("glEnableVertexAttribArray");
    if (!f) return;
    f(a0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGenBuffers(JNIEnv *env, jclass clazz, jint a0, jobject a1, jint a2) {
    typedef void (*fn)(int, unsigned int *);
    fn f = (fn)glsym("glGenBuffers");
    if (!f || !a1) return;
    jint *p = (*env)->GetIntArrayElements(env, a1, NULL);
    f(a0, (unsigned int *)(p + a2));
    (*env)->ReleaseIntArrayElements(env, a1, p, 0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGenTextures(JNIEnv *env, jclass clazz, jint a0, jobject a1, jint a2) {
    typedef void (*fn)(int, unsigned int *);
    fn f = (fn)glsym("glGenTextures");
    if (!f || !a1) return;
    jint *p = (*env)->GetIntArrayElements(env, a1, NULL);
    f(a0, (unsigned int *)(p + a2));
    (*env)->ReleaseIntArrayElements(env, a1, p, 0);
}

JNIEXPORT jint JNICALL Java_android_opengl_GLES20_glGetAttribLocation(JNIEnv *env, jclass clazz, jint a0, jobject a1) {
    typedef int (*fn)(unsigned int, const char *);
    fn f = (fn)glsym("glGetAttribLocation");
    if (!f || !a1) return -1;
    const char *s = (*env)->GetStringUTFChars(env, a1, NULL);
    int r = f((unsigned int)a0, s);
    (*env)->ReleaseStringUTFChars(env, a1, s);
    return r;
}

JNIEXPORT jobject JNICALL Java_android_opengl_GLES20_glGetProgramInfoLog(JNIEnv *env, jclass clazz, jint a0) {
    typedef void (*fn)(unsigned int, int, int *, char *);
    fn f = (fn)glsym("glGetProgramInfoLog");
    if (!f) return NULL;
    char buf[2048]; int n = 0; buf[0] = 0;
    f((unsigned int)a0, (int)sizeof(buf), &n, buf);
    return (*env)->NewStringUTF(env, buf);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetProgramiv(JNIEnv *env, jclass clazz, jint a0, jint a1, jobject a2, jint a3) {
    typedef void (*fn)(unsigned int, unsigned int, int *);
    fn f = (fn)glsym("glGetProgramiv");
    if (!f || !a2) return;
    jint *p = (*env)->GetIntArrayElements(env, a2, NULL);
    f((unsigned int)a0, (unsigned int)a1, p + a3);
    (*env)->ReleaseIntArrayElements(env, a2, p, 0);
}

JNIEXPORT jobject JNICALL Java_android_opengl_GLES20_glGetShaderInfoLog(JNIEnv *env, jclass clazz, jint a0) {
    typedef void (*fn)(unsigned int, int, int *, char *);
    fn f = (fn)glsym("glGetShaderInfoLog");
    if (!f) return NULL;
    char buf[2048]; int n = 0; buf[0] = 0;
    f((unsigned int)a0, (int)sizeof(buf), &n, buf);
    return (*env)->NewStringUTF(env, buf);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetShaderiv(JNIEnv *env, jclass clazz, jint a0, jint a1, jobject a2, jint a3) {
    typedef void (*fn)(unsigned int, unsigned int, int *);
    fn f = (fn)glsym("glGetShaderiv");
    if (!f || !a2) return;
    jint *p = (*env)->GetIntArrayElements(env, a2, NULL);
    f((unsigned int)a0, (unsigned int)a1, p + a3);
    (*env)->ReleaseIntArrayElements(env, a2, p, 0);
}

JNIEXPORT jobject JNICALL Java_android_opengl_GLES20_glGetString(JNIEnv *env, jclass clazz, jint a0) {
    typedef const unsigned char *(*fn)(int);
    fn f = (fn)glsym("glGetString");
    if (!f) return NULL;
    const unsigned char *s = f(a0);
    if (!s) return NULL;
    return (*env)->NewStringUTF(env, (const char *)s);
}

JNIEXPORT jint JNICALL Java_android_opengl_GLES20_glGetUniformLocation(JNIEnv *env, jclass clazz, jint a0, jobject a1) {
    typedef int (*fn)(unsigned int, const char *);
    fn f = (fn)glsym("glGetUniformLocation");
    if (!f || !a1) return -1;
    const char *s = (*env)->GetStringUTFChars(env, a1, NULL);
    int r = f((unsigned int)a0, s);
    (*env)->ReleaseStringUTFChars(env, a1, s);
    return r;
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glLinkProgram(JNIEnv *env, jclass clazz, jint a0) {
    typedef void (*fn)(int);
    fn f = (fn)glsym("glLinkProgram");
    if (!f) return;
    f(a0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glPixelStorei(JNIEnv *env, jclass clazz, jint a0, jint a1) {
    typedef void (*fn)(int, int);
    fn f = (fn)glsym("glPixelStorei");
    if (!f) return;
    f(a0, a1);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glScissor(JNIEnv *env, jclass clazz, jint a0, jint a1, jint a2, jint a3) {
    typedef void (*fn)(int, int, int, int);
    fn f = (fn)glsym("glScissor");
    if (!f) return;
    f(a0, a1, a2, a3);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glShaderSource(JNIEnv *env, jclass clazz, jint a0, jobject a1) {
    typedef void (*fn)(unsigned int, int, const char **, const int *);
    fn f = (fn)glsym("glShaderSource");
    if (!f || !a1) return;
    const char *s = (*env)->GetStringUTFChars(env, a1, NULL);
    f((unsigned int)a0, 1, &s, NULL);
    (*env)->ReleaseStringUTFChars(env, a1, s);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glTexImage2D(JNIEnv *env, jclass clazz, jint a0, jint a1, jint a2, jint a3, jint a4, jint a5, jint a6, jint a7, jobject a8) {
    typedef void (*fn)(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void *);
    fn f = (fn)glsym("glTexImage2D");
    if (!f) return;
    void *p = bufptr(env, a8);
    f((unsigned int)a0, a1, a2, a3, a4, a5, (unsigned int)a6, (unsigned int)a7, p);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glTexParameteri(JNIEnv *env, jclass clazz, jint a0, jint a1, jint a2) {
    typedef void (*fn)(int, int, int);
    fn f = (fn)glsym("glTexParameteri");
    if (!f) return;
    f(a0, a1, a2);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glTexSubImage2D(JNIEnv *env, jclass clazz, jint a0, jint a1, jint a2, jint a3, jint a4, jint a5, jint a6, jint a7, jobject a8) {
    typedef void (*fn)(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void *);
    fn f = (fn)glsym("glTexSubImage2D");
    if (!f) return;
    void *p = bufptr(env, a8);
    f((unsigned int)a0, a1, a2, a3, a4, a5, (unsigned int)a6, (unsigned int)a7, p);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glUniform1i(JNIEnv *env, jclass clazz, jint a0, jint a1) {
    typedef void (*fn)(int, int);
    fn f = (fn)glsym("glUniform1i");
    if (!f) return;
    f(a0, a1);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glUniform4f(JNIEnv *env, jclass clazz, jint a0, jfloat a1, jfloat a2, jfloat a3, jfloat a4) {
    typedef void (*fn)(int, float, float, float, float);
    fn f = (fn)glsym("glUniform4f");
    if (!f) return;
    f(a0, a1, a2, a3, a4);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glUniformMatrix4fv(JNIEnv *env, jclass clazz, jint a0, jint a1, jboolean a2, jobject a3, jint a4) {
    typedef void (*fn)(int, int, unsigned char, const float *);
    fn f = (fn)glsym("glUniformMatrix4fv");
    if (!f || !a3) return;
    jfloat *p = (*env)->GetFloatArrayElements(env, a3, NULL);
    f(a0, a1, (unsigned char)a2, p + a4);
    (*env)->ReleaseFloatArrayElements(env, a3, p, 0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glUseProgram(JNIEnv *env, jclass clazz, jint a0) {
    typedef void (*fn)(int);
    fn f = (fn)glsym("glUseProgram");
    if (!f) return;
    f(a0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glVertexAttribPointer(JNIEnv *env, jclass clazz, jint a0, jint a1, jint a2, jboolean a3, jint a4, jint a5) {
    typedef void (*fn)(int, int, int, unsigned char, int, int);
    fn f = (fn)glsym("glVertexAttribPointer");
    if (!f) return;
    f(a0, a1, a2, a3, a4, a5);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glViewport(JNIEnv *env, jclass clazz, jint a0, jint a1, jint a2, jint a3) {
    typedef void (*fn)(int, int, int, int);
    fn f = (fn)glsym("glViewport");
    if (!f) return;
    f(a0, a1, a2, a3);
}
