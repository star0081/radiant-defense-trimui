package javax.microedition.khronos.egl;

import javax.microedition.khronos.opengles.GL;

public abstract class EGLContext {
    public static EGL getEGL() {
        return Egl10Impl.INSTANCE;
    }

    public GL getGL() {
        return null;
    }
}
