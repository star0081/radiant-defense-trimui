package port;

import java.io.File;
import java.io.FileOutputStream;
import java.io.PrintStream;
import java.lang.reflect.Method;

public final class Main {
    public static void main(String[] args) throws Exception {
        String home = System.getProperty("port.home", ".");
        File logs = new File(home, "logs");
        logs.mkdirs();
        File crash = new File(logs, "crash.txt");
        PrintStream log = new PrintStream(new FileOutputStream(new File(logs, "java.txt"), false), true, "UTF-8");
        System.setOut(log);
        System.setErr(log);
        Thread.setDefaultUncaughtExceptionHandler(new Thread.UncaughtExceptionHandler() {
            public void uncaughtException(Thread t, Throwable e) {
                e.printStackTrace(log);
                try {
                    PrintStream ps = new PrintStream(new FileOutputStream(crash, true), true, "UTF-8");
                    ps.println("thread " + t.getName());
                    e.printStackTrace(ps);
                    ps.close();
                } catch (Exception ignored) {
                }
            }
        });
        System.out.println("radiant port java start");
        System.out.println("home=" + home);
        System.out.println("apk=" + System.getProperty("port.apk"));
        try {
            System.loadLibrary("mojo");
            System.out.println("libmojo loaded");
        } catch (Throwable t) {
            t.printStackTrace(log);
        }
        Sys.init();
        Class<?> actClass = Class.forName("net.hexage.defense.MainActivity");
        Object act = actClass.newInstance();
        Sys.activity = act;
        invoke(actClass, act, "onCreate", new Class[] {android.os.Bundle.class}, new Object[] {null});
        System.out.println("onCreate returned");
        invoke(actClass, act, "onStart", new Class[0], new Object[0]);
        System.out.println("onStart returned");
        invoke(actClass, act, "onResume", new Class[0], new Object[0]);
        System.out.println("onResume returned");
        Sys.flushSurface();
        System.out.println("surface flushed, waiting");
        long limit = Long.getLong("port.exitms", 0L);
        long started = System.currentTimeMillis();
        while (limit == 0L || System.currentTimeMillis() - started < limit) {
            try {
                Natives.pump();
            } catch (Throwable t) {
                t.printStackTrace(log);
            }
            Thread.sleep(16);
        }
        System.out.println("port.exitms reached");
        System.exit(0);
    }

    private static void invoke(Class<?> cls, Object obj, String name, Class<?>[] types, Object[] args) throws Exception {
        Method m = find(cls, name, types);
        if (m == null) {
            System.out.println("missing " + name);
            return;
        }
        m.setAccessible(true);
        try {
            m.invoke(obj, args);
        } catch (java.lang.reflect.InvocationTargetException e) {
            if (e.getCause() != null) {
                e.getCause().printStackTrace(System.out);
            }
            throw e;
        }
    }

    private static Method find(Class<?> cls, String name, Class<?>[] types) {
        for (Class<?> c = cls; c != null; c = c.getSuperclass()) {
            try {
                return c.getDeclaredMethod(name, types);
            } catch (NoSuchMethodException ignored) {
            }
        }
        return null;
    }
}
