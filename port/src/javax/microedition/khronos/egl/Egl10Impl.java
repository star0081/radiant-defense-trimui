package javax.microedition.khronos.egl;

import port.Natives;

public final class Egl10Impl implements EGL10 {
    public static final Egl10Impl INSTANCE = new Egl10Impl();
    private static boolean logged;

    private static void logOnce(String msg) {
        if (!logged) {
            logged = true;
            System.out.println(msg);
        }
    }

    public boolean eglChooseConfig(EGLDisplay display, int[] attrib, EGLConfig[] configs, int configSize, int[] numConfig) {
        if (numConfig != null && numConfig.length > 0) {
            numConfig[0] = 1;
        }
        if (configs != null && configs.length > 0) {
            configs[0] = EglObjs.CONFIG;
        }
        return true;
    }

    public EGLContext eglCreateContext(EGLDisplay display, EGLConfig config, EGLContext share, int[] attrib) {
        logOnce("eglCreateContext");
        return EglObjs.CONTEXT;
    }

    public EGLSurface eglCreateWindowSurface(EGLDisplay display, EGLConfig config, Object win, int[] attrib) {
        logOnce("eglCreateWindowSurface");
        try {
            Natives.makeCurrent();
        } catch (Throwable t) {
            t.printStackTrace();
        }
        return EglObjs.SURFACE;
    }

    public EGLSurface eglCreatePbufferSurface(EGLDisplay display, EGLConfig config, int[] attrib) {
        return EglObjs.SURFACE;
    }

    public boolean eglDestroyContext(EGLDisplay display, EGLContext context) {
        return true;
    }

    public boolean eglDestroySurface(EGLDisplay display, EGLSurface surface) {
        return true;
    }

    public boolean eglGetConfigAttrib(EGLDisplay display, EGLConfig config, int attribute, int[] value) {
        if (value == null || value.length == 0) {
            return false;
        }
        if (attribute == 12339) {
            value[0] = 4;
        } else if (attribute == 12324 || attribute == 12323 || attribute == 12322 || attribute == 12321) {
            value[0] = 8;
        } else if (attribute == 12325) {
            value[0] = 24;
        } else if (attribute == 12327) {
            value[0] = 12344;
        } else if (attribute == 12334) {
            value[0] = 0;
        } else if (attribute == 12352) {
            value[0] = 4;
        } else {
            value[0] = 8;
        }
        return true;
    }

    public EGLDisplay eglGetDisplay(Object nativeDisplay) {
        return EglObjs.DISPLAY;
    }

    public int eglGetError() {
        return 12288;
    }

    public boolean eglInitialize(EGLDisplay display, int[] majorMinor) {
        if (majorMinor != null && majorMinor.length > 0) {
            majorMinor[0] = 1;
        }
        if (majorMinor != null && majorMinor.length > 1) {
            majorMinor[1] = 4;
        }
        return true;
    }

    public boolean eglMakeCurrent(EGLDisplay display, EGLSurface draw, EGLSurface read, EGLContext context) {
        if (draw == EGL10.EGL_NO_SURFACE || context == EGL10.EGL_NO_CONTEXT) {
            return true;
        }
        try {
            Natives.makeCurrent();
        } catch (Throwable t) {
            t.printStackTrace();
            return false;
        }
        return true;
    }

    public boolean eglQuerySurface(EGLDisplay display, EGLSurface surface, int attribute, int[] value) {
        if (value != null && value.length > 0) {
            if (attribute == 12375) {
                value[0] = 1280;
            } else if (attribute == 12374) {
                value[0] = 720;
            } else {
                value[0] = 0;
            }
        }
        return true;
    }

    public String eglQueryString(EGLDisplay display, int name) {
        return "RadiantEGL";
    }

    public boolean eglSwapBuffers(EGLDisplay display, EGLSurface surface) {
        try {
            Natives.swap();
        } catch (Throwable t) {
            t.printStackTrace();
            return false;
        }
        return true;
    }

    public boolean eglTerminate(EGLDisplay display) {
        return true;
    }

    public boolean eglWaitGL() {
        return true;
    }

    public boolean eglWaitNative(int engine, Object bindTarget) {
        return true;
    }
}
