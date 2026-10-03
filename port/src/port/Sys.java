package port;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.RandomAccessFile;
import java.lang.reflect.Array;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

public final class Sys {
    public static Object activity;
    public static final Object NONE = new Object();
    public static final Object NULL = new Object();

    static File home;
    static String apk = "";
    static ZipFile zip;
    static Map<String, Ze> zipIndex = new HashMap<String, Ze>();
    static final Map<Object, Object> bases = new IdentityHashMap<Object, Object>();
    static final Map<String, Object> singles = new HashMap<String, Object>();
    static final Map<Object, Ev> evs = new IdentityHashMap<Object, Ev>();
    static final Map<Object, Object> msgTarget = new IdentityHashMap<Object, Object>();
    static boolean finishing;
    static final Map<Object, Afd> afds = new IdentityHashMap<Object, Afd>();
    static final List<Object> callbacks = new ArrayList<Object>();
    static final HashSet<String> seen = new HashSet<String>();
    static Object holder;
    static Object cfg;
    static int opens;

    static final class Ze {
        int method;
        long offset;
        long length;
    }

    static final class Afd {
        long offset;
        long length;
        String name;
    }

    static final class Ev {
        int action;
        int source;
        int keyCode;
        int pointerCount = 1;
        float x;
        float y;
        float[] axis = new float[24];
    }

    public static void init() {
        home = new File(System.getProperty("port.home", "."));
        apk = System.getProperty("port.apk", "");
        new File(home, "files").mkdirs();
        new File(home, "cache").mkdirs();
        new File(home, "logs").mkdirs();
        try {
            Class<?> ver = Class.forName("android.os.Build$VERSION");
            setStatic(ver, "SDK_INT", Integer.valueOf(21));
            setStatic(ver, "SDK", "21");
            setStatic(ver, "RELEASE", "5.0.2");
            setStatic(ver, "CODENAME", "REL");
            Class<?> build = Class.forName("android.os.Build");
            setStatic(build, "MODEL", "TrimUI");
            setStatic(build, "BOARD", "");
            setStatic(build, "DEVICE", "");
            setStatic(build, "MANUFACTURER", "TrimUI");
            setStatic(build, "BRAND", "TrimUI");
            setStatic(build, "PRODUCT", "SmartPro");
            setStatic(build, "HARDWARE", "sun50iw10");
            setStatic(build, "DISPLAY", "SmartPro");
            setStatic(build, "ID", "TRIMUI");
            setStatic(build, "TAGS", "release-keys");
            setStatic(build, "TYPE", "user");
            setStatic(build, "USER", "star");
            setStatic(build, "HOST", "trimui");
            setStatic(build, "FINGERPRINT", "TrimUI/SmartPro:5.0.2/21:user/release-keys");
            System.out.println("sdk=" + getStatic(ver, "SDK_INT") + " model=" + getStatic(build, "MODEL"));
        } catch (Throwable t) {
            t.printStackTrace();
        }
        try {
            Natives.setApk(apk);
        } catch (Throwable t) {
            System.out.println("setApk skipped: " + t);
        }
        try {
            indexZip();
            System.out.println("zip entries " + zipIndex.size());
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }

    public static void ctor(String owner, Object self, Object[] args) {
        if (self == null) {
            return;
        }
        if (args != null && args.length > 0 && args[0] != null && !(args[0] instanceof String) && !(args[0] instanceof Number) && !(args[0] instanceof Boolean)) {
            bases.put(self, args[0]);
        }
        if ("android/util/DisplayMetrics".equals(owner)) {
            fillDm(self);
        }
        if ("android/content/res/Configuration".equals(owner)) {
            fillCfg(self);
        }
    }

    public static Object call(String owner, String name, String desc, Object self, Object[] args) {
        if (args == null) {
            args = new Object[0];
        }
        Object v = dispatch(owner, name, desc, self, args);
        if (v == NONE) {
            note(owner, name, desc);
            v = fallback(owner, desc, self);
        } else if (v == NULL) {
            v = null;
        }
        return coerce(desc, v);
    }

    static Object coerce(String desc, Object v) {
        String ret = retOf(desc);
        if ("Z".equals(ret)) {
            return v instanceof Boolean ? v : Boolean.FALSE;
        }
        if ("B".equals(ret) || "C".equals(ret) || "S".equals(ret) || "I".equals(ret)) {
            return v instanceof Number ? Integer.valueOf(((Number) v).intValue()) : Integer.valueOf(0);
        }
        if ("J".equals(ret)) {
            return v instanceof Number ? Long.valueOf(((Number) v).longValue()) : Long.valueOf(0L);
        }
        if ("F".equals(ret)) {
            return v instanceof Number ? Float.valueOf(((Number) v).floatValue()) : Float.valueOf(0f);
        }
        if ("D".equals(ret)) {
            return v instanceof Number ? Double.valueOf(((Number) v).doubleValue()) : Double.valueOf(0d);
        }
        return v;
    }

    public static void flushSurface() {
        System.out.println("flushSurface n=" + callbacks.size());
        Object h = holder();
        for (int i = 0; i < callbacks.size(); i++) {
            Object cb = callbacks.get(i);
            try {
                Class<?> sh = Class.forName("android.view.SurfaceHolder");
                Method created = cb.getClass().getMethod("surfaceCreated", sh);
                created.invoke(cb, h);
                Method changed = cb.getClass().getMethod("surfaceChanged", sh, Integer.TYPE, Integer.TYPE, Integer.TYPE);
                changed.invoke(cb, h, Integer.valueOf(0), Integer.valueOf(1280), Integer.valueOf(720));
                System.out.println("surface delivered to " + cb.getClass().getName());
            } catch (Throwable t) {
                t.printStackTrace();
            }
        }
    }

    public static void onQuit() {
        System.out.println("quit");
        System.exit(0);
    }

    public static void onKey(int code, int down) {
        if (activity == null) {
            return;
        }
        if (down != 0) {
            System.out.println("key " + code);
        }
        try {
            Object ev = spawn("Landroid/view/KeyEvent;");
            Ev st = new Ev();
            st.action = down != 0 ? 0 : 1;
            st.keyCode = code;
            st.source = 0x101;
            evs.put(ev, st);
            Method m = activity.getClass().getMethod("dispatchKeyEvent", Class.forName("android.view.KeyEvent"));
            m.invoke(activity, ev);
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }

    public static void onTouch(int action, float x, float y) {
        if (activity == null) {
            return;
        }
        try {
            Object ev = spawn("Landroid/view/MotionEvent;");
            Ev st = new Ev();
            st.action = action;
            st.x = x;
            st.y = y;
            st.source = 4098;
            evs.put(ev, st);
            Method m = activity.getClass().getMethod("dispatchTouchEvent", Class.forName("android.view.MotionEvent"));
            m.invoke(activity, ev);
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }

    public static void onAxes(float lx, float ly, float rx, float ry, float lt, float rt) {
        if (activity == null) {
            return;
        }
        try {
            Object ev = spawn("Landroid/view/MotionEvent;");
            Ev st = new Ev();
            st.action = 2;
            st.source = 0x01000010;
            st.axis[0] = lx;
            st.axis[1] = ly;
            st.axis[11] = rx;
            st.axis[14] = ry;
            st.axis[17] = lt;
            st.axis[18] = rt;
            evs.put(ev, st);
            Method m = activity.getClass().getMethod("dispatchGenericMotionEvent", Class.forName("android.view.MotionEvent"));
            m.invoke(activity, ev);
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }

    public static byte[] readAsset(String name) {
        try {
            InputStream in = openAsset(name);
            ByteArrayOutputStream bos = new ByteArrayOutputStream();
            byte[] buf = new byte[8192];
            int n;
            while ((n = in.read(buf)) >= 0) {
                bos.write(buf, 0, n);
            }
            in.close();
            return bos.toByteArray();
        } catch (Exception e) {
            System.out.println("readAsset " + name + " " + e);
            return null;
        }
    }

    static Object dispatch(String owner, String name, String desc, Object self, Object[] args) {
        if ("android/opengl/Matrix".equals(owner)) {
            matrix(name, args);
            return NULL;
        }
        if ("android/os/SystemClock".equals(owner)) {
            if ("sleep".equals(name)) {
                try {
                    Natives.pump();
                } catch (Throwable ignored) {
                }
                long ms = num(args, 0);
                if (ms > 0) {
                    try {
                        Thread.sleep(ms);
                    } catch (InterruptedException ignored) {
                    }
                }
                return NULL;
            }
            if ("uptimeMillis".equals(name) || "elapsedRealtime".equals(name) || "currentThreadTimeMillis".equals(name)) {
                return Long.valueOf(System.currentTimeMillis());
            }
            if ("elapsedRealtimeNanos".equals(name)) {
                return Long.valueOf(System.nanoTime());
            }
        }
        if ("android/util/Log".equals(owner)) {
            if (!"isLoggable".equals(name)) {
                System.out.println("android.util.Log." + name + " " + argText(args));
            }
            if (desc.endsWith(")Z")) {
                return Boolean.FALSE;
            }
            if (desc.endsWith(")V")) {
                return NULL;
            }
            return Integer.valueOf(0);
        }
        if ("android/os/Process".equals(owner)) {
            if ("myPid".equals(name) || "myTid".equals(name) || "myUid".equals(name)) {
                return Integer.valueOf("myUid".equals(name) ? 10000 : 1);
            }
            return NULL;
        }
        if (name.equals("getPackageName") || name.equals("getOpPackageName")) {
            return "net.hexage.defense";
        }
        if (name.equals("getPackageCodePath") || name.equals("getPackageResourcePath")) {
            return apk;
        }
        if (name.equals("checkPermission") || name.equals("checkCallingOrSelfPermission") || name.equals("checkCallingPermission")) {
            return Integer.valueOf(-1);
        }
        if (name.equals("hasSystemFeature")) {
            String f = args.length > 0 && args[0] != null ? String.valueOf(args[0]) : "";
            return Boolean.valueOf("android.hardware.touchscreen".equals(f));
        }
        if (name.equals("getInstallerPackageName")) {
            return NULL;
        }
        if (name.equals("getPackageInfo")) {
            return packageInfo();
        }
        if (name.equals("getApplicationInfo")) {
            return applicationInfo();
        }
        if (name.equals("getApplicationLabel") || name.equals("getApplicationIcon")) {
            return name.equals("getApplicationLabel") ? "Radiant Defense" : NULL;
        }
        if (name.equals("getSystemService")) {
            return service(args.length > 0 ? String.valueOf(args[0]) : "");
        }
        if (name.equals("getAssets")) {
            return singleton("android.content.res.AssetManager");
        }
        if (name.equals("getResources")) {
            return singleton("android.content.res.Resources");
        }
        if (name.equals("getConfiguration")) {
            return configuration();
        }
        if (name.equals("getAssets") ) {
            return singleton("android.content.res.AssetManager");
        }
        if (name.equals("getPackageManager")) {
            return singleton("android.content.pm.PackageManager");
        }
        if (name.equals("getWindow")) {
            return singleton("android.view.Window");
        }
        if (name.equals("getWindowManager")) {
            return singleton("android.view.WindowManager");
        }
        if (name.equals("getDefaultDisplay")) {
            return singleton("android.view.Display");
        }
        if (name.equals("getIntent")) {
            return singleton("android.content.Intent");
        }
        if (name.equals("getApplicationContext") || name.equals("getBaseContext") || name.equals("getContext")) {
            if (activity != null && (self == null || self == activity || bases.get(self) == null)) {
                if ("getContext".equals(name)) {
                    Object c = bases.get(self);
                    if (c != null) {
                        return c;
                    }
                }
                return activity;
            }
            Object c = self == null ? null : bases.get(self);
            if (c != null) {
                return c;
            }
            return activity != null ? activity : self;
        }
        if (name.equals("getClassLoader")) {
            return Sys.class.getClassLoader();
        }
        if (name.equals("getContentResolver")) {
            return singleton("android.content.ContentResolver");
        }
        if (name.equals("getTheme")) {
            return singleton("android.content.res.Resources$Theme");
        }
        if (name.equals("getMainLooper") || name.equals("myLooper") || name.equals("getLooper")) {
            return singleton("android.os.Looper");
        }
        if (name.equals("getHolder")) {
            return holder();
        }
        if (name.equals("addCallback")) {
            if (args.length > 0 && args[0] != null) {
                callbacks.add(args[0]);
                System.out.println("addCallback " + args[0].getClass().getName());
            }
            return NULL;
        }
        if (name.equals("removeCallback")) {
            if (args.length > 0) {
                callbacks.remove(args[0]);
            }
            return NULL;
        }
        if (name.equals("getCurrentModeType")) {
            return Integer.valueOf(1);
        }
        if (name.equals("getDefaultSensor")) {
            return NULL;
        }
        if (name.equals("getInputDeviceIds")) {
            return new int[] {1};
        }
        if (name.equals("getInputDevice")) {
            return singleton("android.view.InputDevice");
        }
        if (name.equals("isVirtual")) {
            return Boolean.FALSE;
        }
        if (name.equals("getSources")) {
            return Integer.valueOf(0x01000010);
        }
        if (name.equals("getKeyboardType")) {
            return Integer.valueOf(0);
        }
        if (name.equals("getDevice") || name.equals("getName") && owner.contains("InputDevice")) {
            return "TrimUI Pad";
        }
        if (name.equals("getMetrics") || name.equals("getRealMetrics")) {
            if (args.length > 0) {
                fillDm(args[0]);
            }
            return NULL;
        }
        if (name.equals("getDisplayMetrics") && args.length == 0) {
            Object dm = spawn("Landroid/util/DisplayMetrics;");
            fillDm(dm);
            return dm;
        }
        if (name.equals("getWidth")) {
            return Integer.valueOf(owner.contains("Bitmap") ? 0 : 1280);
        }
        if (name.equals("getHeight")) {
            return Integer.valueOf(owner.contains("Bitmap") ? 0 : 720);
        }
        if (name.equals("getRotation")) {
            return Integer.valueOf(0);
        }
        if (name.equals("getRefreshRate")) {
            return Float.valueOf(60f);
        }
        if (name.equals("hasCategory")) {
            return Boolean.FALSE;
        }
        if (name.equals("open") || name.equals("openNonAsset")) {
            String asset = args.length > 0 && args[0] != null ? String.valueOf(args[0]) : "";
            if (opens < 40) {
                opens++;
                System.out.println("asset open " + asset);
            }
            try {
                return openAsset(asset);
            } catch (IOException e) {
                sneaky(e);
            }
            return null;
        }
        if (name.equals("openFd")) {
            String asset = args.length > 0 && args[0] != null ? String.valueOf(args[0]) : "";
            if (opens < 80) {
                opens++;
                System.out.println("asset openFd " + asset);
            }
            return openFd(asset);
        }
        if (name.equals("list")) {
            return new String[0];
        }
        Afd afd = self == null ? null : afds.get(self);
        if (afd != null) {
            if ("getStartOffset".equals(name)) {
                return Long.valueOf(afd.offset);
            }
            if ("getLength".equals(name) || "getDeclaredLength".equals(name)) {
                return Long.valueOf(afd.length);
            }
            if ("createInputStream".equals(name)) {
                try {
                    return openAsset(afd.name);
                } catch (IOException e) {
                    sneaky(e);
                }
                return null;
            }
            if ("close".equals(name)) {
                return NULL;
            }
        }
        Ev ev = self == null ? null : evs.get(self);
        if (ev != null) {
            if ("getAction".equals(name) || "getActionMasked".equals(name)) {
                return Integer.valueOf(ev.action);
            }
            if ("getKeyCode".equals(name)) {
                return Integer.valueOf(ev.keyCode);
            }
            if ("getSource".equals(name)) {
                return Integer.valueOf(ev.source);
            }
            if ("getX".equals(name) || "getY".equals(name) || "getRawX".equals(name) || "getRawY".equals(name)
                    || "getHistoricalX".equals(name) || "getHistoricalY".equals(name)) {
                return Float.valueOf(name.contains("Y") || name.contains("y") ? ev.y : ev.x);
            }
            if ("getPointerCount".equals(name)) {
                return Integer.valueOf(ev.pointerCount);
            }
            if ("getPointerId".equals(name) || "findPointerIndex".equals(name)) {
                return Integer.valueOf(0);
            }
            if ("getAxisValue".equals(name)) {
                int ax = (int) num(args, 0);
                if (ax < 0 || ax >= ev.axis.length) {
                    return Float.valueOf(0);
                }
                return Float.valueOf(ev.axis[ax]);
            }
            if ("getButtonState".equals(name) || "getMetaState".equals(name) || "getFlags".equals(name)
                    || "getEdgeFlags".equals(name) || "getRepeatCount".equals(name) || "getModifiers".equals(name)
                    || "getUnicodeChar".equals(name) || "getHistorySize".equals(name)) {
                return Integer.valueOf(0);
            }
            if ("getCharacters".equals(name) || "getDeviceName".equals(name)) {
                return "";
            }
            if ("getDeviceId".equals(name)) {
                return Integer.valueOf(1);
            }
            if ("getEventTime".equals(name) || "getDownTime".equals(name)) {
                return Long.valueOf(System.currentTimeMillis());
            }
        }
        if (name.equals("getFilesDir") || name.equals("getDataDir") || name.equals("getCodeCacheDir") || name.equals("getNoBackupFilesDir")) {
            return filesDir();
        }
        if (name.equals("getCacheDir") || name.equals("getExternalCacheDir")) {
            File c = new File(home, "cache");
            c.mkdirs();
            return c;
        }
        if (name.equals("getExternalFilesDir") || name.equals("getObbDir") || name.equals("getExternalStorageDirectory")
                || name.equals("getDataDirectory") || name.equals("getRootDirectory")) {
            return filesDir();
        }
        if (name.equals("getFileStreamPath") || name.equals("getDatabasePath") || name.equals("getDir")) {
            String n = args.length > 0 && args[0] != null ? String.valueOf(args[0]) : "file";
            return new File(filesDir(), n);
        }
        if (name.equals("openFileInput")) {
            String n = String.valueOf(args[0]);
            try {
                return new FileInputStream(new File(filesDir(), n));
            } catch (FileNotFoundException e) {
                sneaky(e);
            }
            return null;
        }
        if (name.equals("openFileOutput")) {
            String n = String.valueOf(args[0]);
            boolean append = args.length > 1 && (num(args, 1) & 32768L) != 0;
            try {
                return new FileOutputStream(new File(filesDir(), n), append);
            } catch (FileNotFoundException e) {
                sneaky(e);
            }
            return null;
        }
        if (name.equals("deleteFile")) {
            String n = String.valueOf(args[0]);
            return Boolean.valueOf(new File(filesDir(), n).delete());
        }
        if (name.equals("fileList")) {
            String[] list = filesDir().list();
            return list == null ? new String[0] : list;
        }
        if (name.equals("runOnUiThread") || name.equals("post") || name.equals("postDelayed") || name.equals("postAtFrontOfQueue")) {
            for (int i = 0; i < args.length; i++) {
                if (args[i] instanceof Runnable) {
                    try {
                        ((Runnable) args[i]).run();
                    } catch (Throwable t) {
                        t.printStackTrace();
                    }
                }
            }
            return Boolean.TRUE;
        }
        if (name.equals("requestFeature") || name.equals("requestFocus") || name.equals("requestFocusFromTouch")
                || name.equals("hasFocus") || name.equals("isFocused")) {
            return Boolean.TRUE;
        }
        if (name.equals("finish") || name.equals("finishAffinity") || name.equals("onBackPressed")) {
            finishing = true;
            boolean inFrame = false;
            StackTraceElement[] stack = Thread.currentThread().getStackTrace();
            for (int i = 0; i < stack.length; i++) {
                String cn = stack[i].getClassName();
                if ("r".equals(stack[i].getMethodName()) && cn.startsWith("mojo.")) {
                    inFrame = true;
                    break;
                }
            }
            System.out.println("activity " + name + (inFrame ? " during frame" : " quitting") + " " + owner);
            if (!inFrame) {
                int shown = 0;
                for (int i = 0; i < stack.length && shown < 12; i++) {
                    String cn = stack[i].getClassName();
                    if (cn.startsWith("java.") || cn.startsWith("sun.") || cn.equals("port.Sys")) {
                        continue;
                    }
                    System.out.println("  at " + cn + "." + stack[i].getMethodName());
                    shown++;
                }
                System.exit(0);
            }
            return NULL;
        }
        if (name.equals("isFinishing") || name.equals("isDestroyed")) {
            return Boolean.valueOf(finishing);
        }
        if (name.equals("isRestricted")) {
            return Boolean.FALSE;
        }
        if ("android/os/Handler".equals(owner) && name.startsWith("obtainMessage")) {
            return obtainMessage(self, args);
        }
        if ("android/os/Message".equals(owner) && "obtain".equals(name)) {
            return obtainMessage(null, new Object[0]);
        }
        if ("android/os/Handler".equals(owner) && (name.startsWith("sendMessage") || name.startsWith("sendEmptyMessage"))) {
            if (name.startsWith("sendEmptyMessage")) {
                deliver(self, obtainMessage(self, args.length > 0 ? new Object[] {args[0]} : new Object[0]));
            } else if (args.length > 0) {
                deliver(self, args[0]);
            }
            return Boolean.TRUE;
        }
        if ("android/os/Message".equals(owner) && "sendToTarget".equals(name)) {
            deliver(msgTarget.get(self), self);
            return NULL;
        }
        if ("android/os/Handler".equals(owner) && "dispatchMessage".equals(name) && args.length > 0) {
            deliver(self, args[0]);
            return NULL;
        }
        if (name.equals("moveTaskToBack")) {
            return Boolean.TRUE;
        }
        if (name.equals("setContentView")) {
            System.out.println("setContentView " + (args.length > 0 && args[0] != null ? args[0].getClass().getName() : "null"));
            return NULL;
        }
        if ("android/graphics/BitmapFactory".equals(owner) && name.startsWith("decode")) {
            System.out.println("BitmapFactory." + name);
            return NULL;
        }
        if ("android/text/TextUtils".equals(owner) && "isEmpty".equals(name)) {
            Object o = args.length > 0 ? args[0] : null;
            return Boolean.valueOf(o == null || String.valueOf(o).length() == 0);
        }
        if (name.equals("toString")) {
            return self == null ? owner : self.getClass().getName();
        }
        if (name.equals("hashCode")) {
            return Integer.valueOf(System.identityHashCode(self));
        }
        if (name.equals("equals")) {
            return Boolean.valueOf(self == (args.length > 0 ? args[0] : null));
        }
        if (name.equals("getString") && args.length > 0 && args[0] instanceof String) {
            return NULL;
        }
        return NONE;
    }

    static Object fallback(String owner, String desc, Object self) {
        String ret = retOf(desc);
        if (self != null && ret.equals("L" + owner + ";")) {
            return self;
        }
        if ("V".equals(ret)) {
            return null;
        }
        if ("Z".equals(ret)) {
            return Boolean.FALSE;
        }
        if ("J".equals(ret)) {
            return Long.valueOf(0);
        }
        if ("F".equals(ret)) {
            return Float.valueOf(0);
        }
        if ("D".equals(ret)) {
            return Double.valueOf(0);
        }
        if ("Ljava/lang/String;".equals(ret)) {
            return "";
        }
        if ("Ljava/util/List;".equals(ret) || "Ljava/util/ArrayList;".equals(ret) || "Ljava/util/Collection;".equals(ret)) {
            return new ArrayList<Object>();
        }
        if ("Ljava/util/Map;".equals(ret) || "Ljava/util/HashMap;".equals(ret)) {
            return new HashMap<Object, Object>();
        }
        if ("Ljava/util/Set;".equals(ret)) {
            return new HashSet<Object>();
        }
        if (ret.startsWith("[")) {
            return emptyArray(ret);
        }
        if (ret.startsWith("L")) {
            Object o = spawn(ret);
            if (o != null) {
                return o;
            }
        }
        return Integer.valueOf(0);
    }

    static Object packageInfo() {
        Object info = singleton("android.content.pm.PackageInfo");
        set(info, "versionName", "2.3.15");
        set(info, "versionCode", Integer.valueOf(220315));
        set(info, "packageName", "net.hexage.defense");
        set(info, "firstInstallTime", Long.valueOf(1400000000000L));
        set(info, "lastUpdateTime", Long.valueOf(1400000000000L));
        return info;
    }

    static Object applicationInfo() {
        Object info = singleton("android.content.pm.ApplicationInfo");
        set(info, "sourceDir", apk);
        set(info, "publicSourceDir", apk);
        set(info, "dataDir", filesDir().getAbsolutePath());
        set(info, "nativeLibraryDir", home.getAbsolutePath());
        set(info, "packageName", "net.hexage.defense");
        set(info, "flags", Integer.valueOf(0));
        set(info, "targetSdkVersion", Integer.valueOf(25));
        return info;
    }

    static Object service(String name) {
        System.out.println("getSystemService " + name);
        if ("uimode".equals(name)) {
            return singleton("android.app.UiModeManager");
        }
        if ("input".equals(name)) {
            return singleton("android.hardware.input.InputManager");
        }
        if ("sensor".equals(name)) {
            return singleton("android.hardware.SensorManager");
        }
        if ("window".equals(name)) {
            return singleton("android.view.WindowManager");
        }
        if ("audio".equals(name)) {
            return singleton("android.media.AudioManager");
        }
        if ("connectivity".equals(name)) {
            return singleton("android.net.ConnectivityManager");
        }
        if ("phone".equals(name)) {
            return singleton("android.telephony.TelephonyManager");
        }
        if ("activity".equals(name)) {
            return singleton("android.app.ActivityManager");
        }
        if ("layout_inflater".equals(name)) {
            return singleton("android.view.LayoutInflater");
        }
        if ("power".equals(name)) {
            return singleton("android.os.PowerManager");
        }
        if ("wifi".equals(name)) {
            return singleton("android.net.wifi.WifiManager");
        }
        if ("notification".equals(name)) {
            return singleton("android.app.NotificationManager");
        }
        if ("keyguard".equals(name)) {
            return singleton("android.app.KeyguardManager");
        }
        if ("clipboard".equals(name)) {
            return singleton("android.content.ClipboardManager");
        }
        if ("location".equals(name)) {
            return singleton("android.location.LocationManager");
        }
        if ("vibrator".equals(name)) {
            return singleton("android.os.Vibrator");
        }
        if ("appops".equals(name)) {
            return singleton("android.app.AppOpsManager");
        }
        System.out.println("unknown service " + name);
        return null;
    }

    static Object holder() {
        if (holder == null) {
            holder = spawn("Landroid/view/SurfaceHolder;");
        }
        return holder;
    }

    static Object configuration() {
        if (cfg == null) {
            cfg = spawn("Landroid/content/res/Configuration;");
            fillCfg(cfg);
        }
        return cfg;
    }

    static Object singleton(String binary) {
        Object o = singles.get(binary);
        if (o == null) {
            o = spawn("L" + binary.replace('.', '/') + ";");
            if (o != null) {
                singles.put(binary, o);
            }
        }
        return o;
    }

    static Object spawn(String desc) {
        try {
            if (desc.startsWith("[")) {
                return emptyArray(desc);
            }
            if (!desc.startsWith("L") || !desc.endsWith(";")) {
                return null;
            }
            String bin = desc.substring(1, desc.length() - 1).replace('/', '.');
            if (bin.equals("java.lang.String")) {
                return "";
            }
            Class<?> c = Class.forName(bin);
            if (c.isInterface()) {
                c = Class.forName("port.impl." + bin);
            }
            return c.newInstance();
        } catch (Throwable t) {
            System.out.println("spawn " + desc + " " + t);
            return null;
        }
    }

    static Object emptyArray(String desc) {
        try {
            if ("[I".equals(desc)) return new int[0];
            if ("[B".equals(desc)) return new byte[0];
            if ("[F".equals(desc)) return new float[0];
            if ("[Z".equals(desc)) return new boolean[0];
            if ("[J".equals(desc)) return new long[0];
            if ("[S".equals(desc)) return new short[0];
            if ("[C".equals(desc)) return new char[0];
            if ("[D".equals(desc)) return new double[0];
            if (desc.startsWith("[L") && desc.endsWith(";")) {
                Class<?> c = Class.forName(desc.substring(2, desc.length() - 1).replace('/', '.'));
                return Array.newInstance(c, 0);
            }
        } catch (Throwable t) {
            return null;
        }
        return null;
    }

    static void fillDm(Object dm) {
        set(dm, "widthPixels", Integer.valueOf(1280));
        set(dm, "heightPixels", Integer.valueOf(720));
        set(dm, "xdpi", Float.valueOf(320f));
        set(dm, "ydpi", Float.valueOf(320f));
        set(dm, "density", Float.valueOf(2f));
        set(dm, "scaledDensity", Float.valueOf(2f));
        set(dm, "densityDpi", Integer.valueOf(320));
        set(dm, "noncompatWidthPixels", Integer.valueOf(1280));
        set(dm, "noncompatHeightPixels", Integer.valueOf(720));
        set(dm, "noncompatDensity", Float.valueOf(2f));
        set(dm, "noncompatXdpi", Float.valueOf(320f));
        set(dm, "noncompatYdpi", Float.valueOf(320f));
    }

    static void fillCfg(Object c) {
        set(c, "keyboard", Integer.valueOf(1));
        set(c, "keyboardHidden", Integer.valueOf(1));
        set(c, "navigation", Integer.valueOf(1));
        set(c, "orientation", Integer.valueOf(2));
        set(c, "screenLayout", Integer.valueOf(2));
        set(c, "uiMode", Integer.valueOf(1));
        set(c, "densityDpi", Integer.valueOf(320));
        set(c, "screenWidthDp", Integer.valueOf(640));
        set(c, "screenHeightDp", Integer.valueOf(360));
        set(c, "smallestScreenWidthDp", Integer.valueOf(360));
        set(c, "fontScale", Float.valueOf(1f));
        set(c, "locale", Locale.getDefault());
    }

    static Object openFd(String name) {
        Ze z = zipIndex.get("assets/" + name);
        if (z == null) {
            z = zipIndex.get(name);
        }
        if (z == null || z.method != 0) {
            sneaky(new FileNotFoundException(name));
            return null;
        }
        Object afd = spawn("Landroid/content/res/AssetFileDescriptor;");
        Afd st = new Afd();
        st.offset = z.offset;
        st.length = z.length;
        st.name = name;
        afds.put(afd, st);
        return afd;
    }

    static InputStream openAsset(String name) throws IOException {
        if (zip == null) {
            zip = new ZipFile(apk);
        }
        String path = name.startsWith("/") ? name.substring(1) : name;
        if (!path.startsWith("assets/")) {
            path = "assets/" + path;
        }
        ZipEntry e = zip.getEntry(path);
        if (e == null) {
            throw new FileNotFoundException(name);
        }
        return zip.getInputStream(e);
    }

    static void indexZip() throws IOException {
        RandomAccessFile f = new RandomAccessFile(apk, "r");
        try {
            long len = f.length();
            int scan = (int) Math.min(len, 66000);
            byte[] tail = new byte[scan];
            f.seek(len - scan);
            f.readFully(tail);
            int eocd = -1;
            for (int i = tail.length - 22; i >= 0; i--) {
                if ((tail[i] & 255) == 0x50 && (tail[i + 1] & 255) == 0x4b && (tail[i + 2] & 255) == 5 && (tail[i + 3] & 255) == 6) {
                    eocd = i;
                    break;
                }
            }
            if (eocd < 0) {
                throw new IOException("EOCD missing");
            }
            int cdSize = le32(tail, eocd + 12);
            int cdOff = le32(tail, eocd + 16);
            byte[] cd = new byte[cdSize];
            f.seek(cdOff & 0xffffffffL);
            f.readFully(cd);
            int p = 0;
            byte[] lh = new byte[30];
            while (p + 46 <= cd.length && le32(cd, p) == 0x02014b50) {
                int method = le16(cd, p + 10);
                int uncomp = le32(cd, p + 24);
                int nameLen = le16(cd, p + 28);
                int extraLen = le16(cd, p + 30);
                int comment = le16(cd, p + 32);
                int local = le32(cd, p + 42);
                String entry = new String(cd, p + 46, nameLen, "UTF-8");
                f.seek(local & 0xffffffffL);
                f.readFully(lh);
                int ln = le16(lh, 26);
                int le = le16(lh, 28);
                Ze z = new Ze();
                z.method = method;
                z.offset = (local & 0xffffffffL) + 30L + ln + le;
                z.length = uncomp & 0xffffffffL;
                zipIndex.put(entry, z);
                p += 46 + nameLen + extraLen + comment;
            }
        } finally {
            f.close();
        }
    }

    static File filesDir() {
        File f = new File(home == null ? new File(".") : home, "files");
        f.mkdirs();
        return f;
    }

    static void matrix(String name, Object[] args) {
        if ("multiplyMM".equals(name) && args.length >= 6) {
            float[] result = (float[]) args[0];
            int ro = (int) num(args, 1);
            float[] lhs = (float[]) args[2];
            int lo = (int) num(args, 3);
            float[] rhs = (float[]) args[4];
            int ho = (int) num(args, 5);
            float[] tmp = new float[16];
            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) {
                    tmp[i * 4 + j] =
                            lhs[lo + j] * rhs[ho + i * 4] +
                            lhs[lo + 4 + j] * rhs[ho + i * 4 + 1] +
                            lhs[lo + 8 + j] * rhs[ho + i * 4 + 2] +
                            lhs[lo + 12 + j] * rhs[ho + i * 4 + 3];
                }
            }
            System.arraycopy(tmp, 0, result, ro, 16);
            return;
        }
        if ("multiplyMV".equals(name) && args.length >= 6) {
            float[] result = (float[]) args[0];
            int ro = (int) num(args, 1);
            float[] lhs = (float[]) args[2];
            int lo = (int) num(args, 3);
            float[] vec = (float[]) args[4];
            int vo = (int) num(args, 5);
            float x = vec[vo], y = vec[vo + 1], z = vec[vo + 2], w = vec[vo + 3];
            float[] tmp = new float[4];
            for (int j = 0; j < 4; j++) {
                tmp[j] = lhs[lo + j] * x + lhs[lo + 4 + j] * y + lhs[lo + 8 + j] * z + lhs[lo + 12 + j] * w;
            }
            System.arraycopy(tmp, 0, result, ro, 4);
            return;
        }
        if ("setIdentityM".equals(name) && args.length >= 2) {
            float[] m = (float[]) args[0];
            int o = (int) num(args, 1);
            for (int i = 0; i < 16; i++) {
                m[o + i] = 0;
            }
            m[o] = m[o + 5] = m[o + 10] = m[o + 15] = 1;
        }
    }

    static int deliverDepth;

    static Object obtainMessage(Object handler, Object[] args) {
        Object msg = spawn("Landroid/os/Message;");
        if (msg == null) {
            return null;
        }
        if (args != null && args.length > 0 && args[0] instanceof Number) {
            set(msg, "what", Integer.valueOf((int) num(args, 0)));
        }
        if (args != null && args.length >= 3 && args[1] instanceof Number && args[2] instanceof Number) {
            set(msg, "arg1", Integer.valueOf((int) num(args, 1)));
            set(msg, "arg2", Integer.valueOf((int) num(args, 2)));
        }
        if (args != null && args.length > 0) {
            Object last = args[args.length - 1];
            if (last != null && !(last instanceof Number)) {
                set(msg, "obj", last);
            }
        }
        if (handler != null) {
            msgTarget.put(msg, handler);
        }
        return msg;
    }

    static void deliver(Object handler, Object msg) {
        if (handler == null || msg == null || deliverDepth > 8) {
            return;
        }
        deliverDepth++;
        try {
            Method hm = null;
            Class<?> message = Class.forName("android.os.Message");
            for (Class<?> c = handler.getClass(); c != null; c = c.getSuperclass()) {
                try {
                    hm = c.getDeclaredMethod("handleMessage", message);
                    break;
                } catch (NoSuchMethodException ignored) {
                }
            }
            if (hm != null) {
                hm.setAccessible(true);
                hm.invoke(handler, msg);
            }
        } catch (Throwable t) {
            t.printStackTrace();
        } finally {
            deliverDepth--;
        }
    }

    static void note(String owner, String name, String desc) {
        if (seen.size() > 250) {
            return;
        }
        String key = owner + " " + name + " " + desc;
        if (seen.add(key)) {
            System.out.println("stub " + key);
        }
    }

    static String retOf(String desc) {
        return desc.substring(desc.lastIndexOf(')') + 1);
    }

    static long num(Object[] args, int i) {
        if (args == null || i >= args.length || !(args[i] instanceof Number)) {
            return 0;
        }
        return ((Number) args[i]).longValue();
    }

    static String argText(Object[] args) {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < args.length && i < 4; i++) {
            if (i > 0) sb.append(' ');
            sb.append(args[i]);
        }
        return sb.toString();
    }

    static void set(Object o, String name, Object value) {
        if (o == null) {
            return;
        }
        for (Class<?> c = o.getClass(); c != null; c = c.getSuperclass()) {
            try {
                Field f = c.getDeclaredField(name);
                f.setAccessible(true);
                unlock(f);
                f.set(o, value);
                return;
            } catch (NoSuchFieldException ignored) {
            } catch (Throwable t) {
                System.out.println("set " + name + " " + t);
                return;
            }
        }
    }

    static void setStatic(Class<?> c, String name, Object value) {
        try {
            Field f = c.getDeclaredField(name);
            f.setAccessible(true);
            unlock(f);
            f.set(null, value);
        } catch (Throwable t) {
            System.out.println("no field " + c.getName() + "." + name);
        }
    }

    static Object getStatic(Class<?> c, String name) {
        try {
            Field f = c.getDeclaredField(name);
            f.setAccessible(true);
            return f.get(null);
        } catch (Throwable t) {
            return null;
        }
    }

    static void unlock(Field f) {
        try {
            Field mod = Field.class.getDeclaredField("modifiers");
            mod.setAccessible(true);
            mod.setInt(f, f.getModifiers() & ~Modifier.FINAL);
        } catch (Throwable ignored) {
        }
    }

    static int le16(byte[] b, int o) {
        return (b[o] & 255) | ((b[o + 1] & 255) << 8);
    }

    static int le32(byte[] b, int o) {
        return (b[o] & 255) | ((b[o + 1] & 255) << 8) | ((b[o + 2] & 255) << 16) | ((b[o + 3] & 255) << 24);
    }

    @SuppressWarnings("unchecked")
    static <E extends Throwable> void sneaky(Throwable e) throws E {
        throw (E) e;
    }
}
