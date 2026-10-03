package javax.microedition.khronos.egl;

public final class EglObjs {
    public static final Object DEFAULT_DISPLAY = new Object();
    public static final DisplayImpl NO_DISPLAY = new DisplayImpl(true);
    public static final ContextImpl NO_CONTEXT = new ContextImpl(true);
    public static final SurfaceImpl NO_SURFACE = new SurfaceImpl(true);
    public static final DisplayImpl DISPLAY = new DisplayImpl(false);
    public static final ConfigImpl CONFIG = new ConfigImpl();
    public static final ContextImpl CONTEXT = new ContextImpl(false);
    public static final SurfaceImpl SURFACE = new SurfaceImpl(false);

    private EglObjs() {
    }
}
