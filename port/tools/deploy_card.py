# Copy the Radiant Defense port onto the TrimUI card using the existing layout.
import os
import shutil

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BUILD = os.path.join(ROOT, "port", "build")
JRE_SRC = os.path.join(BUILD, "jre-extract", "jdk8u504-b01-jre")
CARD = "G:\\"
GAME = os.path.join(CARD, "Data", "ports", "radiantdefense")
LAUNCH = os.path.join(CARD, "Roms", "PORTS", "Radiant Defense.sh")

SCRIPT = """#!/bin/sh
# TrimUI Smart Pro 1.1.1 POSIX. Stock has no bash.
# Radiant Defense. Do not change the framebuffer.

if [ -f /mnt/SDCARD/System/etc/ex_config ]; then
  . /mnt/SDCARD/System/etc/ex_config
fi

export PATH="/mnt/SDCARD/System/bin:/usr/bin:/usr/sbin:/bin:/sbin:$PATH"

GAMEDIR="/mnt/SDCARD/Data/ports/radiantdefense"
LOGDIR="$GAMEDIR/logs"
LOG="$LOGDIR/radiant.log"
PM="/mnt/SDCARD/Apps/PortMaster/PortMaster"
JAVA="$GAMEDIR/jre/bin/java"

mkdir -p "$LOGDIR" "$GAMEDIR/files" "$GAMEDIR/cache" 2>/dev/null || true
mkdir -p /data/data/net.hexage.defense 2>/dev/null || true

if [ -f "$LOG" ]; then
  mv -f "$LOG" "$LOG.1" 2>/dev/null || true
fi

echo "===== radiant defense start =====" > "$LOG"
date >> "$LOG" 2>/dev/null
echo "id=$(id)" >> "$LOG"
uname -a >> "$LOG" 2>/dev/null

cd "$GAMEDIR" || {
  echo "cannot cd $GAMEDIR" >> "$LOG"
  exit 1
}

if [ ! -f "$JAVA" ]; then
  echo "missing $JAVA" >> "$LOG"
  sync
  exit 1
fi

echo performance >/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null
echo 1800000 >/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq 2>/dev/null
echo 1800000 >/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq 2>/dev/null

export LD_LIBRARY_PATH="/usr/trimui/lib:$GAMEDIR:$GAMEDIR/jre/lib/aarch64:$GAMEDIR/jre/lib/aarch64/jli${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export SDL_VIDEO_GL_DRIVER=libGLESv2.so
export SDL_OPENGL_ES_DRIVER=1
export SDL_GAMECONTROLLERCONFIG_FILE="$PM/gamecontrollerdb.txt"
export MALLOC_ARENA_MAX=2

chmod a+x "$JAVA" 2>/dev/null || true

echo "----- java -version -----" >> "$LOG"
if ! "$JAVA" -version >> "$LOG" 2>&1; then
  echo "java failed to start" >> "$LOG"
  sync
  exit 1
fi

echo "----- game -----" >> "$LOG"
"$JAVA" -Xmx192m -Xverify:none \\
  -Djava.library.path="$GAMEDIR" \\
  -Dport.home="$GAMEDIR" \\
  -Dport.apk="$GAMEDIR/game.apk" \\
  -Duser.language=ru \\
  -Duser.country=RU \\
  -Djava.io.tmpdir="$GAMEDIR/cache" \\
  -cp "$GAMEDIR/shim.jar:$GAMEDIR/radiant.jar" \\
  port.Main >> "$LOG" 2>&1
echo "java exit $?" >> "$LOG"
echo "===== radiant defense end =====" >> "$LOG"
sync
"""

PORT_JSON = """{
  "version": 3,
  "name": "radiantdefense.zip",
  "items": [
    "Radiant Defense.sh",
    "radiantdefense/"
  ],
  "items_opt": null,
  "attr": {
    "title": "Radiant Defense",
    "porter": ["star0081"],
    "desc": "Hexage Radiant Defense 2.3.15 for TrimUI Smart Pro stock 1.1.1. Local runtime of the installed APK.",
    "inst": "Launch Radiant Defense from Ports.",
    "genres": ["strategy"],
    "rtr": true,
    "exp": true,
    "runtime": "jre8",
    "reqs": ["opengl"],
    "arch": ["aarch64"]
  }
}
"""


def copy_tree(src, dst):
    os.makedirs(dst, exist_ok=True)
    for dirpath, dirnames, filenames in os.walk(src):
        rel = os.path.relpath(dirpath, src)
        if rel == "man" or rel.startswith("man" + os.sep):
            dirnames[:] = []
            continue
        target_dir = dst if rel == "." else os.path.join(dst, rel)
        os.makedirs(target_dir, exist_ok=True)
        for name in filenames:
            s = os.path.join(dirpath, name)
            d = os.path.join(target_dir, name)
            if os.path.islink(s):
                link = os.readlink(s)
                real = link if os.path.isabs(link) else os.path.join(os.path.dirname(s), link)
                if os.path.isfile(real):
                    shutil.copy2(real, d)
                continue
            shutil.copy2(s, d)


def main():
    if not os.path.isdir(os.path.join(CARD, "Roms", "PORTS")):
        raise SystemExit("card layout not found at " + CARD)
    os.makedirs(GAME, exist_ok=True)
    for sub in ("logs", "files", "cache"):
        os.makedirs(os.path.join(GAME, sub), exist_ok=True)
    pairs = [
        (os.path.join(BUILD, "shim.jar"), os.path.join(GAME, "shim.jar")),
        (os.path.join(BUILD, "radiant.jar"), os.path.join(GAME, "radiant.jar")),
        (os.path.join(BUILD, "libmojo.so"), os.path.join(GAME, "libmojo.so")),
        (os.path.join(ROOT, "Radiant_Defense_v.2.3.15.Unlocked.Rus.apk"), os.path.join(GAME, "game.apk")),
    ]
    for src, dst in pairs:
        if not os.path.isfile(src):
            raise SystemExit("missing " + src)
        print("copy", os.path.basename(dst), os.path.getsize(src))
        shutil.copy2(src, dst)
    print("copy jre")
    copy_tree(JRE_SRC, os.path.join(GAME, "jre"))
    open(os.path.join(GAME, "port.json"), "w", encoding="utf-8", newline="\n").write(PORT_JSON)
    data = SCRIPT.replace("\r\n", "\n").replace("\r", "\n")
    if not data.endswith("\n"):
        data += "\n"
    open(LAUNCH, "wb").write(data.encode("utf-8"))
    open(os.path.join(ROOT, "port", "Radiant Defense.sh"), "wb").write(data.encode("utf-8"))
    raw = open(LAUNCH, "rb").read()
    print("launcher bytes", len(raw), "cr", raw.count(b"\r"), "lf", raw.count(b"\n"))
    java = os.path.join(GAME, "jre", "bin", "java")
    print("java", os.path.isfile(java), os.path.getsize(java) if os.path.isfile(java) else 0)
    print("done", GAME)


if __name__ == "__main__":
    main()
