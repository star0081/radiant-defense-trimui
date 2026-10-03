package port;

public final class Natives {
    public static native void setApk(String path);
    public static native void makeCurrent();
    public static native void swap();
    public static native void pump();
}
