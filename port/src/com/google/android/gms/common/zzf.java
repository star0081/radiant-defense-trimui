package com.google.android.gms.common;

import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;

/** Replaces the Play Services signature check. Its bytecode crashes the interpreter. */
public class zzf {
    private static final zzf INST = new zzf();

    private zzf() {}

    public static zzf zzoO() {
        return INST;
    }

    zzd.zza zza(PackageInfo info, zzd.zza... sigs) {
        return null;
    }

    public boolean zza(PackageInfo info, boolean any) {
        return false;
    }

    public boolean zza(PackageManager pm, PackageInfo info) {
        return false;
    }
}
