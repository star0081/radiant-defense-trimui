package javax.microedition.khronos.egl;

import javax.microedition.khronos.opengles.GL;

public final class ContextImpl extends EGLContext {
    public final boolean none;

    public ContextImpl(boolean none) {
        this.none = none;
    }

    public GL getGL() {
        return null;
    }
}
