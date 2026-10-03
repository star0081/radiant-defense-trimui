import struct, zipfile
from collections import Counter

apk = r"D:\Sandbox Cursor\RADIANT DEFENCE\Radiant_Defense_v.2.3.15.Unlocked.Rus.apk"
data = zipfile.ZipFile(apk).read("classes.dex")

def uleb(data, off):
    result = 0
    shift = 0
    while True:
        b = data[off]
        off += 1
        result |= (b & 0x7F) << shift
        if (b & 0x80) == 0:
            return result, off
        shift += 7

def get_str(idx):
    so = struct.unpack_from("<I", data, string_ids_off + idx * 4)[0]
    _, p = uleb(data, so)
    end = data.index(b"\x00", p)
    return data[p:end].decode("utf-8", "replace")

string_ids_size = struct.unpack_from("<I", data, 56)[0]
string_ids_off = struct.unpack_from("<I", data, 60)[0]
type_ids_size = struct.unpack_from("<I", data, 64)[0]
type_ids_off = struct.unpack_from("<I", data, 68)[0]
proto_ids_size = struct.unpack_from("<I", data, 72)[0]
proto_ids_off = struct.unpack_from("<I", data, 76)[0]
field_ids_size = struct.unpack_from("<I", data, 80)[0]
field_ids_off = struct.unpack_from("<I", data, 84)[0]
method_ids_size = struct.unpack_from("<I", data, 88)[0]
method_ids_off = struct.unpack_from("<I", data, 92)[0]
class_defs_size = struct.unpack_from("<I", data, 96)[0]
class_defs_off = struct.unpack_from("<I", data, 100)[0]

types = [get_str(struct.unpack_from("<I", data, type_ids_off + i * 4)[0]) for i in range(type_ids_size)]
defined = set()
for i in range(class_defs_size):
    class_idx = struct.unpack_from("<I", data, class_defs_off + i * 32)[0]
    defined.add(types[class_idx])

ext_methods = Counter()
ext_fields = Counter()
for i in range(method_ids_size):
    class_idx, proto_idx, name_idx = struct.unpack_from("<HHI", data, method_ids_off + i * 8)
    desc = types[class_idx]
    if desc not in defined:
        ext_methods[desc.split("/")[0] if desc.startswith("L") else desc] += 1
for i in range(field_ids_size):
    class_idx, type_idx, name_idx = struct.unpack_from("<HHI", data, field_ids_off + i * 8)
    desc = types[class_idx]
    if desc not in defined:
        ext_fields[desc.split("/")[0] if desc.startswith("L") else desc] += 1

print("classes defined", len(defined))
print("method ids", method_ids_size, "field ids", field_ids_size)
print("EXT METHODS by prefix")
for k, v in ext_methods.most_common(30):
    print(f"  {v:5} {k}")
print("EXT FIELDS by prefix")
for k, v in ext_fields.most_common(20):
    print(f"  {v:5} {k}")

pkgs = Counter()
for i in range(method_ids_size):
    class_idx, proto_idx, name_idx = struct.unpack_from("<HHI", data, method_ids_off + i * 8)
    desc = types[class_idx]
    if desc not in defined and desc.startswith("Landroid/"):
        pkg = desc[1:].rsplit("/", 1)[0]
        pkgs[pkg] += 1
print("ANDROID method pkgs")
for k, v in pkgs.most_common(40):
    print(f"  {v:5} {k}")
