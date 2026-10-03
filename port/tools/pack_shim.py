import os
import zipfile

root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "build"))
out = os.path.join(root, "shim.jar")
trees = ["shim_classes", "host_classes"]
files = {}
for tree in trees:
    base = os.path.join(root, tree)
    for dirpath, _, names in os.walk(base):
        for name in names:
            full = os.path.join(dirpath, name)
            rel = os.path.relpath(full, base).replace("\\", "/")
            files[rel] = full
tmp = out + ".tmp"
with zipfile.ZipFile(tmp, "w", compression=zipfile.ZIP_DEFLATED) as z:
    for rel in sorted(files):
        z.write(files[rel], rel)
os.replace(tmp, out)
print("entries", len(files), "bytes", os.path.getsize(out))
print("builder", "com/google/android/gms/common/api/GoogleApiClient$Builder.class" in files)
print("offline", "com/google/android/gms/common/api/OfflineClient.class" in files)
print("sys", "port/Sys.class" in files)
