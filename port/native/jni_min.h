#ifndef PORT_JNI_MIN_H
#define PORT_JNI_MIN_H
#include <jni.h>
void *glsym(const char *name);
void *bufptr(JNIEnv *env, jobject buf);
#endif
