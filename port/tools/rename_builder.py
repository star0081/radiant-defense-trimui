import os
import struct

src = os.path.join(
    os.path.dirname(__file__),
    "..",
    "build",
    "host_classes",
    "com",
    "google",
    "android",
    "gms",
    "common",
    "api",
    "PlayBuilder.class",
)
dst = os.path.join(os.path.dirname(src), "GoogleApiClient$Builder.class")
data = open(src, "rb").read()
if data[:4] != b"\xca\xfe\xba\xbe":
    raise SystemExit("not a class file")
cp_count = struct.unpack(">H", data[8:10])[0]
i = 10
out = bytearray(data[:10])
idx = 1
old = b"PlayBuilder"
new = b"GoogleApiClient$Builder"
nrep = 0
while idx < cp_count:
    tag = data[i]
    if tag == 1:
        ln = struct.unpack(">H", data[i + 1 : i + 3])[0]
        raw = data[i + 3 : i + 3 + ln]
        raw2 = raw.replace(old, new)
        if raw2 != raw:
            nrep += 1
        out.append(1)
        out += struct.pack(">H", len(raw2))
        out += raw2
        i += 3 + ln
    elif tag in (7, 8, 16, 19, 20):
        out += data[i : i + 3]
        i += 3
    elif tag in (3, 4, 9, 10, 11, 12, 17, 18):
        out += data[i : i + 5]
        i += 5
    elif tag in (5, 6):
        out += data[i : i + 9]
        i += 9
        idx += 1
    elif tag == 15:
        out += data[i : i + 4]
        i += 4
    else:
        raise SystemExit("bad tag %d at %d" % (tag, i))
    idx += 1
out += data[i:]
open(dst, "wb").write(out)
os.remove(src)
print("replaced", nrep, "bytes", len(out))
