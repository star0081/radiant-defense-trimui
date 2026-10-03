#!/bin/sh
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
export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-alsa}"
export MALLOC_ARENA_MAX=2

chmod a+x "$JAVA" 2>/dev/null || true

echo "----- java -version -----" >> "$LOG"
if ! "$JAVA" -version >> "$LOG" 2>&1; then
  echo "java failed to start" >> "$LOG"
  sync
  exit 1
fi

echo "----- game -----" >> "$LOG"
"$JAVA" -Xmx192m -Xverify:none \
  -Djava.library.path="$GAMEDIR" \
  -Dport.home="$GAMEDIR" \
  -Dport.apk="$GAMEDIR/game.apk" \
  -Duser.language=ru \
  -Duser.country=RU \
  -Djava.io.tmpdir="$GAMEDIR/cache" \
  -cp "$GAMEDIR/shim.jar:$GAMEDIR/radiant.jar" \
  port.Main >> "$LOG" 2>&1
echo "java exit $?" >> "$LOG"
echo "===== radiant defense end =====" >> "$LOG"
sync
