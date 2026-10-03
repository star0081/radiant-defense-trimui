# Builds shim class files and GLES JNI from the Radiant Defense APK.
import os, struct, zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
APK = os.path.join(os.path.dirname(ROOT), "Radiant_Defense_v.2.3.15.Unlocked.Rus.apk")
OUT = os.path.join(ROOT, "build", "shim_classes")
GLES_C = os.path.join(ROOT, "native", "gles_jni.c")
NATIVE_TXT = os.path.join(ROOT, "build", "native_methods.txt")

EXTENDS = {
    "android/app/Activity": "android/content/ContextWrapper",
    "android/content/ContextWrapper": "android/content/Context",
    "android/content/Context": "java/lang/Object",
    "android/view/SurfaceView": "android/view/View",
    "android/view/ViewGroup": "android/view/View",
    "android/view/TextureView": "android/view/View",
    "android/widget/FrameLayout": "android/view/ViewGroup",
    "android/widget/LinearLayout": "android/view/ViewGroup",
    "android/widget/RelativeLayout": "android/view/ViewGroup",
    "android/app/Dialog": "android/view/View",
    "android/app/Application": "android/content/ContextWrapper",
    "android/app/Service": "android/content/ContextWrapper",
}

SKIP_PREFIX = (
    "java/", "javax/", "sun/", "jdk/", "org/w3c/", "org/xml/", "org/json/",
)
# org/json is NOT in Java 8. Do not skip it.
SKIP_PREFIX = (
    "java/", "javax/", "sun/", "jdk/", "org/w3c/", "org/xml/",
)
KEEP_EVEN_IF_SKIP = ("javax/microedition/",)
HAND_PREFIX = (
    "javax/microedition/khronos/",
    "android/opengl/GLES20",
    "android/opengl/GLES10",
    "android/opengl/GLES11",
    "android/opengl/GLES30",
    "android/opengl/GLUtils",
    "port/",
)

OPSIZE = [1] * 256
for op, n in {
    0x02: 2, 0x03: 3, 0x05: 2, 0x06: 3, 0x08: 2, 0x09: 3,
    0x13: 2, 0x14: 3, 0x15: 2, 0x16: 2, 0x17: 3, 0x18: 5, 0x19: 2,
    0x1A: 2, 0x1B: 3, 0x1C: 2, 0x1F: 2, 0x20: 2, 0x22: 2, 0x23: 2,
    0x24: 3, 0x25: 3, 0x26: 3, 0x29: 2, 0x2A: 3, 0x2B: 3, 0x2C: 3,
}.items():
    OPSIZE[op] = n
for op in range(0x2D, 0x3E):
    OPSIZE[op] = 2
for op in range(0x44, 0x6E):
    OPSIZE[op] = 2
for op in range(0x6E, 0x73):
    OPSIZE[op] = 3
for op in range(0x74, 0x79):
    OPSIZE[op] = 3
for op in range(0x90, 0xB0):
    OPSIZE[op] = 2
for op in range(0xD0, 0xE3):
    OPSIZE[op] = 2


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


def load_dex(path):
    if path.lower().endswith(".apk"):
        data = zipfile.ZipFile(path).read("classes.dex")
    else:
        data = open(path, "rb").read()
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

    def get_str(idx):
        so = struct.unpack_from("<I", data, string_ids_off + idx * 4)[0]
        _, p = uleb(data, so)
        end = data.index(b"\x00", p)
        return data[p:end].decode("utf-8", "replace")

    types = [get_str(struct.unpack_from("<I", data, type_ids_off + i * 4)[0]) for i in range(type_ids_size)]

    def proto_desc(proto_idx):
        shorty_idx, ret_idx, params_off = struct.unpack_from("<III", data, proto_ids_off + proto_idx * 12)
        ret = types[ret_idx]
        params = ""
        if params_off:
            n = struct.unpack_from("<I", data, params_off)[0]
            for i in range(n):
                t = struct.unpack_from("<H", data, params_off + 4 + i * 2)[0]
                params += types[t]
        return "(" + params + ")" + ret

    defined = set()
    interfaces_of = {}
    supers = {}
    for i in range(class_defs_size):
        base = class_defs_off + i * 32
        class_idx, access, super_idx, ifaces_off = struct.unpack_from("<IIII", data, base)
        name = types[class_idx]
        defined.add(name)
        if super_idx != 0xFFFFFFFF and super_idx < len(types):
            supers[name] = types[super_idx]
        ifaces = []
        if ifaces_off:
            n = struct.unpack_from("<I", data, ifaces_off)[0]
            for k in range(n):
                t = struct.unpack_from("<H", data, ifaces_off + 4 + k * 2)[0]
                ifaces.append(types[t])
        interfaces_of[name] = ifaces

    methods = []
    for i in range(method_ids_size):
        class_idx, proto_idx, name_idx = struct.unpack_from("<HHI", data, method_ids_off + i * 8)
        methods.append((types[class_idx], get_str(name_idx), proto_desc(proto_idx)))

    fields = []
    for i in range(field_ids_size):
        class_idx, type_idx, name_idx = struct.unpack_from("<HHI", data, field_ids_off + i * 8)
        fields.append((types[class_idx], get_str(name_idx), types[type_idx]))

    # static vs instance from bytecode
    field_static = {}
    method_static = {}
    iface_invoke = set()

    def mark_field(idx, static):
        field_static[idx] = field_static.get(idx, False) or static

    def mark_method(idx, static):
        prev = method_static.get(idx)
        if prev is None:
            method_static[idx] = static
        elif prev != static:
            method_static[idx] = static  # prefer the later observation; logged below

    for i in range(class_defs_size):
        base = class_defs_off + i * 32
        class_data_off = struct.unpack_from("<I", data, base + 24)[0]
        if not class_data_off:
            continue
        p = class_data_off
        sf, p = uleb(data, p)
        inf, p = uleb(data, p)
        dm, p = uleb(data, p)
        vm, p = uleb(data, p)
        for _ in range(sf + inf):
            _, p = uleb(data, p)
            _, p = uleb(data, p)
        for _ in range(dm + vm):
            _, p = uleb(data, p)
            _, p = uleb(data, p)
            code_off, p = uleb(data, p)
            if not code_off:
                continue
            insns_size = struct.unpack_from("<I", data, code_off + 12)[0]
            ip = 0
            ins = code_off + 16
            while ip < insns_size:
                unit = struct.unpack_from("<H", data, ins + ip * 2)[0]
                if unit == 0x0100:
                    size = struct.unpack_from("<H", data, ins + (ip + 1) * 2)[0]
                    ip += 4 + size * 2
                    continue
                if unit == 0x0200:
                    size = struct.unpack_from("<H", data, ins + (ip + 1) * 2)[0]
                    ip += 2 + size * 4
                    continue
                if unit == 0x0300:
                    width = struct.unpack_from("<H", data, ins + (ip + 1) * 2)[0]
                    size = struct.unpack_from("<I", data, ins + (ip + 2) * 2)[0]
                    nbytes = size * width
                    units = (nbytes + 1) // 2
                    ip += 4 + units
                    continue
                op = unit & 0xFF
                sz = OPSIZE[op]
                if op in (0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F):
                    mark_field(struct.unpack_from("<H", data, ins + (ip + 1) * 2)[0], False)
                elif op in (0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D):
                    mark_field(struct.unpack_from("<H", data, ins + (ip + 1) * 2)[0], True)
                elif op in (0x6E, 0x6F, 0x70, 0x74, 0x75, 0x76):
                    mark_method(struct.unpack_from("<H", data, ins + (ip + 1) * 2)[0], False)
                elif op in (0x71, 0x77):
                    mark_method(struct.unpack_from("<H", data, ins + (ip + 1) * 2)[0], True)
                elif op in (0x72, 0x78):
                    mi = struct.unpack_from("<H", data, ins + (ip + 1) * 2)[0]
                    mark_method(mi, False)
                    iface_invoke.add(mi)
                ip += sz
                if sz <= 0:
                    break

    native = []
    defined_methods = {}
    # native methods defined in dex
    for i in range(class_defs_size):
        base = class_defs_off + i * 32
        class_idx = struct.unpack_from("<I", data, base)[0]
        class_data_off = struct.unpack_from("<I", data, base + 24)[0]
        if not class_data_off:
            continue
        cname = types[class_idx]
        p = class_data_off
        sf, p = uleb(data, p)
        inf, p = uleb(data, p)
        dm, p = uleb(data, p)
        vm, p = uleb(data, p)
        fidx = 0
        for _ in range(sf + inf):
            diff, p = uleb(data, p)
            _, p = uleb(data, p)
            fidx += diff
        midx = 0
        for mi in range(dm + vm):
            diff, p = uleb(data, p)
            access, p = uleb(data, p)
            _, p = uleb(data, p)
            if mi == dm:
                midx = 0
            midx += diff
            if midx < 0 or midx >= len(methods):
                continue
            owner, mname, desc = methods[midx]
            defined_methods.setdefault(cname, set()).add((mname, desc))
            if access & 0x100:
                native.append((cname, mname, desc, bool(access & 0x8)))

    return {
        "types": types,
        "defined": defined,
        "interfaces_of": interfaces_of,
        "supers": supers,
        "methods": methods,
        "fields": fields,
        "field_static": field_static,
        "method_static": method_static,
        "iface_invoke": iface_invoke,
        "native": native,
        "defined_methods": defined_methods,
    }


def want_type(name, defined):
    if not name.startswith("L") or not name.endswith(";"):
        return False
    internal = name[1:-1]
    if internal in defined or name in defined:
        return False
    if any(internal.startswith(p) for p in HAND_PREFIX):
        return False
    if any(internal.startswith(p) for p in KEEP_EVEN_IF_SKIP):
        return True
    if any(internal.startswith(p) for p in SKIP_PREFIX):
        return False
    return True


def internal(desc):
    if desc.startswith("L") and desc.endswith(";"):
        return desc[1:-1]
    return desc


class Pool:
    def __init__(self):
        self.items = [None]
        self.cache = {}

    def _add(self, key, item):
        if key in self.cache:
            return self.cache[key]
        self.items.append(item)
        idx = len(self.items) - 1
        self.cache[key] = idx
        return idx

    def utf(self, s):
        return self._add(("u", s), ("utf", s))

    def cls(self, name):
        return self._add(("c", name), ("class", self.utf(name)))

    def nat(self, name, desc):
        return self._add(("n", name, desc), ("nat", self.utf(name), self.utf(desc)))

    def mref(self, owner, name, desc):
        return self._add(("m", owner, name, desc), ("mref", self.cls(owner), self.nat(name, desc)))

    def strc(self, s):
        return self._add(("s", s), ("str", self.utf(s)))

    def emit(self):
        out = bytearray()
        out += struct.pack(">H", len(self.items))
        for it in self.items[1:]:
            k = it[0]
            if k == "utf":
                b = it[1].encode("utf-8")
                out += bytes([1]) + struct.pack(">H", len(b)) + b
            elif k == "class":
                out += bytes([7]) + struct.pack(">H", it[1])
            elif k == "nat":
                out += bytes([12]) + struct.pack(">HH", it[1], it[2])
            elif k == "mref":
                out += bytes([10]) + struct.pack(">HH", it[1], it[2])
            elif k == "str":
                out += bytes([8]) + struct.pack(">H", it[1])
        return out


def parse_desc(desc):
    i = 1
    args = []
    while desc[i] != ")":
        start = i
        while desc[i] == "[":
            i += 1
        if desc[i] == "L":
            i = desc.index(";", i) + 1
        else:
            i += 1
        args.append(desc[start:i])
    return args, desc[i + 1 :]


def slot_size(t):
    return 2 if t in ("J", "D") else 1


def iconst(n):
    if n == -1:
        return bytes([2])
    if 0 <= n <= 5:
        return bytes([3 + n])
    if -128 <= n <= 127:
        return bytes([16, n & 0xFF])
    return bytes([17]) + struct.pack(">h", n)


def load_local(t, slot):
    if t in ("J",):
        op = 22
    elif t in ("D",):
        op = 24
    elif t in ("F",):
        op = 23
    elif t in ("Z", "B", "C", "S", "I"):
        op = 21
    else:
        op = 25
    if slot < 4 and op == 21:
        return bytes([26 + slot])
    if slot < 4 and op == 25:
        return bytes([42 + slot])
    if slot < 4 and op == 22:
        return bytes([30 + slot])
    if slot < 4 and op == 23:
        return bytes([34 + slot])
    if slot < 4 and op == 24:
        return bytes([38 + slot])
    return bytes([op, slot])


def ldc(idx):
    if idx <= 255:
        return bytes([18, idx])
    return bytes([19]) + struct.pack(">H", idx)


def invokestatic(idx):
    return bytes([184]) + struct.pack(">H", idx)


def invokevirtual(idx):
    return bytes([182]) + struct.pack(">H", idx)


def invokespecial(idx):
    return bytes([183]) + struct.pack(">H", idx)


def checkcast(idx):
    return bytes([192]) + struct.pack(">H", idx)


def box_type(t):
    if t == "Z":
        return "java/lang/Boolean", "(Z)Ljava/lang/Boolean;", "booleanValue", "()Z", "ireturn"
    if t == "J":
        return "java/lang/Long", "(J)Ljava/lang/Long;", "longValue", "()J", "lreturn"
    if t == "F":
        return "java/lang/Float", "(F)Ljava/lang/Float;", "floatValue", "()F", "freturn"
    if t == "D":
        return "java/lang/Double", "(D)Ljava/lang/Double;", "doubleValue", "()D", "dreturn"
    if t in ("B", "C", "S", "I"):
        return "java/lang/Integer", "(I)Ljava/lang/Integer;", "intValue", "()I", "ireturn"
    return None


RETOP = {
    "ireturn": 172,
    "lreturn": 173,
    "freturn": 174,
    "dreturn": 175,
    "areturn": 176,
    "return": 177,
}


def build_class(internal_name, super_name, is_iface, methods, fields, native_methods=False, ifaces=None):
    ifaces = ifaces or []
    pool = Pool()
    this_idx = pool.cls(internal_name)
    iface_idxs = [pool.cls(n) for n in ifaces]
    super_idx = pool.cls(super_name if not is_iface else "java/lang/Object")
    code_utf = pool.utf("Code")
    call_ref = pool.mref(
        "port/Sys",
        "call",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;",
    )
    ctor_ref = pool.mref("port/Sys", "ctor", "(Ljava/lang/String;Ljava/lang/Object;[Ljava/lang/Object;)V")
    obj_cls = pool.cls("java/lang/Object")
    super_init = pool.mref(super_name if not is_iface else "java/lang/Object", "<init>", "()V")
    owner_str = pool.strc(internal_name)

    method_bytes = []

    def add_method(access, name, desc, code):
        name_i = pool.utf(name)
        desc_i = pool.utf(desc)
        if code is None:
            method_bytes.append(struct.pack(">HHH", access, name_i, desc_i) + struct.pack(">H", 0))
            return
        attr = struct.pack(">HH", code[0], code[1]) + struct.pack(">I", len(code[2])) + code[2]
        attr += struct.pack(">HH", 0, 0)
        method_bytes.append(
            struct.pack(">HHH", access, name_i, desc_i)
            + struct.pack(">H", 1)
            + struct.pack(">HI", code_utf, len(attr))
            + attr
        )

    # fields
    field_bytes = []
    seen_f = set()
    for fname, fdesc, static in fields:
        if (fname, fdesc) in seen_f:
            continue
        seen_f.add((fname, fdesc))
        acc = 0x0019 if is_iface else (0x0001 | (0x0008 if static else 0))
        field_bytes.append(struct.pack(">HHH", acc, pool.utf(fname), pool.utf(fdesc)) + struct.pack(">H", 0))

    seen_m = set()
    for mname, desc, static in methods:
        if (mname, desc) in seen_m:
            continue
        seen_m.add((mname, desc))
        if is_iface:
            add_method(0x0401, mname, desc, None)
            continue
        if native_methods:
            acc = 0x0101 | (0x0008 if static else 0)
            add_method(acc, mname, desc, None)
            continue
        if mname == "<clinit>":
            add_method(0x0008, mname, desc, (0, 0, bytes([177])))
            continue
        args, ret = parse_desc(desc)
        if mname == "<init>":
            code = bytearray()
            code += bytes([42])  # aload_0
            code += invokespecial(super_init)
            # Sys.ctor(owner, this, args)
            code += ldc(owner_str)
            code += bytes([42])
            code += iconst(len(args))
            code += bytes([189]) + struct.pack(">H", obj_cls)  # anewarray Object
            slot = 1
            for i, a in enumerate(args):
                code += bytes([89])  # dup
                code += iconst(i)
                code += load_local(a, slot)
                boxed = box_type(a)
                if boxed:
                    code += invokestatic(pool.mref(boxed[0], "valueOf", boxed[1]))
                code += bytes([83])  # aastore
                slot += slot_size(a)
            code += invokestatic(ctor_ref)
            code += bytes([177])
            locals_n = max(1, slot)
            add_method(0x0001, mname, desc, (32, locals_n, bytes(code)))
            continue
        # trampoline
        code = bytearray()
        code += ldc(owner_str)
        code += ldc(pool.strc(mname))
        code += ldc(pool.strc(desc))
        if static:
            code += bytes([1])  # aconst_null self
            base_slot = 0
        else:
            code += bytes([42])  # aload_0
            base_slot = 1
        code += iconst(len(args))
        code += bytes([189]) + struct.pack(">H", obj_cls)
        slot = base_slot
        for i, a in enumerate(args):
            code += bytes([89])
            code += iconst(i)
            code += load_local(a, slot)
            boxed = box_type(a)
            if boxed:
                code += invokestatic(pool.mref(boxed[0], "valueOf", boxed[1]))
            code += bytes([83])
            slot += slot_size(a)
        code += invokestatic(call_ref)
        if ret == "V":
            code += bytes([87, 177])  # pop, return
        else:
            boxed = box_type(ret)
            if boxed:
                code += checkcast(pool.cls(boxed[0]))
                code += invokevirtual(pool.mref(boxed[0], boxed[2], boxed[3]))
                code += bytes([RETOP[boxed[4]]])
            else:
                code += checkcast(pool.cls(internal(ret) if ret.startswith("L") else ret))
                code += bytes([176])
        add_method(0x0001 | (0x0008 if static else 0), mname, desc, (32, max(slot, 1), bytes(code)))

    if not is_iface and not any(m[0] == "<init>" and m[1] == "()V" for m in methods):
        code = bytes([42]) + invokespecial(super_init) + bytes([177])
        add_method(0x0001, "<init>", "()V", (1, 1, code))

    access = 0x0601 if is_iface else 0x0021
    body = bytearray()
    body += b"\xCA\xFE\xBA\xBE"
    body += struct.pack(">HH", 0, 52)
    body += pool.emit()
    body += struct.pack(">HH", access, this_idx)
    body += struct.pack(">H", super_idx)
    body += struct.pack(">H", len(iface_idxs))
    for ix in iface_idxs:
        body += struct.pack(">H", ix)
    body += struct.pack(">H", len(field_bytes))
    for fb in field_bytes:
        body += fb
    body += struct.pack(">H", len(method_bytes))
    for mb in method_bytes:
        body += mb
    body += struct.pack(">H", 0)
    return bytes(body)


def jni_mangle(desc):
    out = []
    for ch in desc:
        if ch == "/":
            out.append("_")
        elif ch == "_":
            out.append("_1")
        elif ch == ";":
            out.append("_2")
        elif ch == "[":
            out.append("_3")
        elif ch == "$":
            out.append("_00024")
        else:
            out.append(ch)
    return "".join(out)


def jni_func(owner, name, desc, overload):
    base = "Java_" + owner.replace("/", "_").replace("$", "_00024") + "_" + name.replace("_", "_1")
    if overload:
        base += "__" + jni_mangle(desc)
    return base


def c_type(t):
    return {
        "Z": "jboolean",
        "B": "jbyte",
        "C": "jchar",
        "S": "jshort",
        "I": "jint",
        "J": "jlong",
        "F": "jfloat",
        "D": "jdouble",
    }.get(t, "jobject")


def c_ret(t):
    if t == "V":
        return "void"
    return c_type(t)


def emit_gles(methods):
    # methods: list of (name, desc)
    names = {}
    for n, d in methods:
        names.setdefault(n, []).append(d)
    lines = []
    lines.append("/* generated */")
    lines.append('#include "jni_min.h"')
    lines.append("#include <stdint.h>")
    lines.append("#include <stdio.h>")
    lines.append("void *glsym(const char *name);")
    lines.append("void *bufptr(JNIEnv *env, jobject buf);")
    lines.append("static int gles_miss;")
    lines.append("")
    for name, desc in methods:
        args, ret = parse_desc(desc)
        overload = len(names[name]) > 1
        fn = jni_func("android/opengl/GLES20", name, desc, overload)
        params = ["JNIEnv *env", "jclass clazz"]
        for i, a in enumerate(args):
            params.append("%s a%d" % (c_type(a), i))
        lines.append("JNIEXPORT %s JNICALL %s(%s) {" % (c_ret(ret), fn, ", ".join(params)))
        body = gles_body(name, desc, args, ret)
        for ln in body:
            lines.append("    " + ln)
        lines.append("}")
        lines.append("")
    os.makedirs(os.path.dirname(GLES_C), exist_ok=True)
    open(GLES_C, "w", newline="\n").write("\n".join(lines))
    print("gles methods", len(methods), "->", GLES_C)


def gles_body(name, desc, args, ret):
    zero = "return;" if ret == "V" else ("return 0;" if ret != "F" and ret != "D" and not ret.startswith("L") and not ret.startswith("[") else ("return 0;" if ret in ("F", "D") else "return NULL;"))
    if ret == "F" or ret == "D":
        zero = "return 0;"
    # strings
    if name == "glGetString" and desc == "(I)Ljava/lang/String;":
        return [
            'typedef const unsigned char *(*fn)(int);',
            'fn f = (fn)glsym("glGetString");',
            'if (!f) return NULL;',
            'const unsigned char *s = f(a0);',
            'if (!s) return NULL;',
            'return (*env)->NewStringUTF(env, (const char *)s);',
        ]
    if name in ("glGetShaderInfoLog", "glGetProgramInfoLog") and desc.startswith("(I)"):
        gname = name
        return [
            'typedef void (*fn)(unsigned int, int, int *, char *);',
            'fn f = (fn)glsym("%s");' % gname,
            'if (!f) return NULL;',
            'char buf[2048]; int n = 0; buf[0] = 0;',
            'f((unsigned int)a0, (int)sizeof(buf), &n, buf);',
            'return (*env)->NewStringUTF(env, buf);',
        ]
    if name == "glShaderSource" and "Ljava/lang/String;" in desc:
        return [
            'typedef void (*fn)(unsigned int, int, const char **, const int *);',
            'fn f = (fn)glsym("glShaderSource");',
            'if (!f || !a1) return;',
            'const char *s = (*env)->GetStringUTFChars(env, a1, NULL);',
            'f((unsigned int)a0, 1, &s, NULL);',
            '(*env)->ReleaseStringUTFChars(env, a1, s);',
        ]
    if name in ("glGetAttribLocation", "glGetUniformLocation"):
        return [
            'typedef int (*fn)(unsigned int, const char *);',
            'fn f = (fn)glsym("%s");' % name,
            'if (!f || !a1) return -1;',
            'const char *s = (*env)->GetStringUTFChars(env, a1, NULL);',
            'int r = f((unsigned int)a0, s);',
            '(*env)->ReleaseStringUTFChars(env, a1, s);',
            'return r;',
        ]
    if name in ("glGetShaderiv", "glGetProgramiv") and "[I" in desc:
        return [
            'typedef void (*fn)(unsigned int, unsigned int, int *);',
            'fn f = (fn)glsym("%s");' % name,
            'if (!f || !a2) return;',
            'jint *p = (*env)->GetIntArrayElements(env, a2, NULL);',
            'f((unsigned int)a0, (unsigned int)a1, p + a3);',
            '(*env)->ReleaseIntArrayElements(env, a2, p, 0);',
        ]
    if name == "glGetIntegerv":
        return [
            'typedef void (*fn)(unsigned int, int *);',
            'fn f = (fn)glsym("glGetIntegerv");',
            'if (!f || !a1) return;',
            'jint *p = (*env)->GetIntArrayElements(env, a1, NULL);',
            'f((unsigned int)a0, p + a2);',
            '(*env)->ReleaseIntArrayElements(env, a1, p, 0);',
        ]
    if name.startswith("glUniform") and ("[F" in desc or "[I" in desc):
        arr_i = next(i for i, a in enumerate(args) if a.startswith("["))
        return uniform_call(name, args, arr_i, "Float" if "[F" in desc else "Int")
    # int[] with offset as last arg: (I[II)V or (I[II)V and (II[II)
    if desc.endswith("[II)V") and args[0] == "I" and args[-1] == "I":
        # glGen*, glDelete*, glGetIntegerv style. Find the array arg.
        arr_i = next(i for i, a in enumerate(args) if a.startswith("["))
        off_i = len(args) - 1
        n_i = 0 if name.startswith("glGen") or name.startswith("glDelete") else None
        return int_array_call(name, args, arr_i, off_i, n_i)
    if "Ljava/nio/Buffer;" in desc or "Ljava/nio/FloatBuffer;" in desc:
        return buffer_call(name, args, ret)
    if desc.endswith("[FI)V") or desc.endswith("[II)V"):
        arr_i = next(i for i, a in enumerate(args) if a.startswith("["))
        return uniform_call(name, args, arr_i, "Float" if "[F" in desc else "Int")
    # primitive-only
    if all(a in "ZBCSIJFD" for a in args):
        return prim_call(name, args, ret)
    return ['if (gles_miss < 40) { fprintf(stderr, "gles noop %s%s\\n", "%s", "%s"); gles_miss++; }' % (name, desc, name, desc), zero]


def int_array_call(name, args, arr_i, off_i, n_i):
    lines = [
        'typedef void (*fn)(int, unsigned int *);',
        'fn f = (fn)glsym("%s");' % name,
        'if (!f || !a%d) return;' % arr_i,
        'jint *p = (*env)->GetIntArrayElements(env, a%d, NULL);' % arr_i,
    ]
    if n_i is None:
        lines.append('f(a0, (unsigned int *)(p + a%d));' % off_i)
    else:
        lines.append('f(a%d, (unsigned int *)(p + a%d));' % (n_i, off_i))
    lines.append('(*env)->ReleaseIntArrayElements(env, a%d, p, 0);' % arr_i)
    return lines


def float_or_int_array(name, args, arr_i, ret):
    kind = "Float" if args[arr_i].startswith("[F") else "Int"
    cty = "float" if kind == "Float" else "unsigned int"
    jty = "jfloat" if kind == "Float" else "jint"
    # build a typedef from primitive args before the array, plus pointer
    return [
        '/* %s */' % name,
        'typedef void (*fn)(int, int, unsigned char, %s *);' % cty if "Matrix" in name or name.startswith("glUniform") else
        'typedef void (*fn)();',
        'fn f = (fn)glsym("%s");' % name,
        'if (!f) return%s;' % ("" if ret == "V" else " 0"),
        '%s *p = (*env)->Get%sArrayElements(env, a%d, NULL);' % (jty, kind, arr_i),
        '/* offset is last int if present */',
        'int off = 0;',
        'if (%d >= 1 && "%s" == "I") off = 0;' % (0, "x"),
    ] + uniform_call(name, args, arr_i, kind) 


def uniform_call(name, args, arr_i, kind):
    jty = "jfloat" if kind == "Float" else "jint"
    # last arg is offset
    off = len(args) - 1
    lines = []
    if name.startswith("glUniformMatrix"):
        lines = [
            'typedef void (*fn)(int, int, unsigned char, const float *);',
            'fn f = (fn)glsym("%s");' % name,
            'if (!f || !a%d) return;' % arr_i,
            'jfloat *p = (*env)->GetFloatArrayElements(env, a%d, NULL);' % arr_i,
            'f(a0, a1, (unsigned char)a2, p + a%d);' % off,
            '(*env)->ReleaseFloatArrayElements(env, a%d, p, 0);' % arr_i,
        ]
    elif name.startswith("glUniform") and kind == "Float":
        lines = [
            'typedef void (*fn)(int, int, const float *);',
            'fn f = (fn)glsym("%s");' % name,
            'if (!f || !a%d) return;' % arr_i,
            'jfloat *p = (*env)->GetFloatArrayElements(env, a%d, NULL);' % arr_i,
            'f(a0, a1, p + a%d);' % off,
            '(*env)->ReleaseFloatArrayElements(env, a%d, p, 0);' % arr_i,
        ]
    else:
        lines = [
            'typedef void (*fn)(int, int, const int *);',
            'fn f = (fn)glsym("%s");' % name,
            'if (!f || !a%d) { if (f) {} return; }' % arr_i,
            '%s *p = (*env)->Get%sArrayElements(env, a%d, NULL);' % (jty, "Float" if kind == "Float" else "Int", arr_i),
            'f(a0, a1, (const int *)(p + a%d));' % off,
            '(*env)->Release%sArrayElements(env, a%d, p, 0);' % ("Float" if kind == "Float" else "Int", arr_i),
        ]
    return lines


def buffer_call(name, args, ret):
    # Find buffer arg index
    bi = next(i for i, a in enumerate(args) if "Buffer" in a or a.startswith("[") )
    prims = [(i, a) for i, a in enumerate(args) if a in "ZBCSIJFD"]
    # Common GL buffer entry points by name
    if name in ("glVertexAttribPointer",):
        return [
            'typedef void (*fn)(unsigned int, int, unsigned int, unsigned char, int, const void *);',
            'fn f = (fn)glsym("glVertexAttribPointer");',
            'if (!f) return;',
            'void *p = bufptr(env, a5);',
            'f((unsigned int)a0, a1, (unsigned int)a2, (unsigned char)a3, a4, p);',
        ]
    if name in ("glDrawElements",):
        return [
            'typedef void (*fn)(unsigned int, int, unsigned int, const void *);',
            'fn f = (fn)glsym("glDrawElements");',
            'if (!f) return;',
            'void *p = bufptr(env, a3);',
            'f((unsigned int)a0, a1, (unsigned int)a2, p);',
        ]
    if name in ("glBufferData",):
        return [
            'typedef void (*fn)(unsigned int, intptr_t, const void *, unsigned int);',
            'fn f = (fn)glsym("glBufferData");',
            'if (!f) return;',
            'void *p = bufptr(env, a2);',
            'f((unsigned int)a0, (intptr_t)a1, p, (unsigned int)a3);',
        ]
    if name in ("glBufferSubData",):
        return [
            'typedef void (*fn)(unsigned int, intptr_t, intptr_t, const void *);',
            'fn f = (fn)glsym("glBufferSubData");',
            'if (!f) return;',
            'void *p = bufptr(env, a3);',
            'f((unsigned int)a0, (intptr_t)a1, (intptr_t)a2, p);',
        ]
    if name in ("glTexImage2D",):
        return [
            'typedef void (*fn)(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void *);',
            'fn f = (fn)glsym("glTexImage2D");',
            'if (!f) return;',
            'void *p = bufptr(env, a8);',
            'f((unsigned int)a0, a1, a2, a3, a4, a5, (unsigned int)a6, (unsigned int)a7, p);',
        ]
    if name in ("glTexSubImage2D",):
        return [
            'typedef void (*fn)(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void *);',
            'fn f = (fn)glsym("glTexSubImage2D");',
            'if (!f) return;',
            'void *p = bufptr(env, a8);',
            'f((unsigned int)a0, a1, a2, a3, a4, a5, (unsigned int)a6, (unsigned int)a7, p);',
        ]
    if name in ("glReadPixels",):
        return [
            'typedef void (*fn)(int, int, int, int, unsigned int, unsigned int, void *);',
            'fn f = (fn)glsym("glReadPixels");',
            'if (!f) return;',
            'void *p = bufptr(env, a6);',
            'f(a0, a1, a2, a3, (unsigned int)a4, (unsigned int)a5, p);',
        ]
    z = "return;" if ret == "V" else "return 0;"
    return [
        'if (gles_miss < 30) { fprintf(stderr, "gles buf %s\\n", "%s"); gles_miss++; }' % (name, name),
        z,
    ]


def prim_call(name, args, ret):
    cmap = {"Z": "unsigned char", "B": "char", "C": "unsigned short", "S": "short", "I": "int", "J": "long long", "F": "float", "D": "double"}
    ctypes = ", ".join(cmap[a] for a in args) or "void"
    if ret == "V":
        rty = "void"
        call = "f(%s);" % ", ".join("a%d" % i for i in range(len(args)))
        retstmt = ""
    elif ret == "F":
        rty = "float"
        call = "return f(%s);" % ", ".join("a%d" % i for i in range(len(args)))
        retstmt = ""
    elif ret in ("D",):
        rty = "double"
        call = "return f(%s);" % ", ".join("a%d" % i for i in range(len(args)))
        retstmt = ""
    elif ret in ("J",):
        rty = "long long"
        call = "return f(%s);" % ", ".join("a%d" % i for i in range(len(args)))
        retstmt = ""
    else:
        rty = "int"
        call = "return f(%s);" % ", ".join("a%d" % i for i in range(len(args)))
        retstmt = ""
    if not args:
        call = call.replace("f()", "f()")
    lines = [
        'typedef %s (*fn)(%s);' % (rty, ctypes if args else "void"),
        'fn f = (fn)glsym("%s");' % name,
    ]
    if ret == "V":
        lines.append('if (!f) return;')
        lines.append(call if call != "f();" else "f();")
    else:
        lines.append('if (!f) return 0;')
        lines.append(call)
    return lines


def main():
    dex = load_dex(APK)
    defined_internal = set()
    for t in dex["defined"]:
        if t.startswith("L") and t.endswith(";"):
            defined_internal.add(t[1:-1])
        else:
            defined_internal.add(t)

    iface_types = set()
    class_types = set()
    for name, ifaces in dex["interfaces_of"].items():
        for it in ifaces:
            if it.startswith("L"):
                iface_types.add(it[1:-1])
    for t in dex["types"]:
        pass
    # anything used as a superclass of a dex class is a class
    # (stored only as game supers; our EXTENDS handles framework)

    # gather methods/fields per external class
    cls_methods = {}
    cls_fields = {}
    gles = []
    for i, (owner, name, desc) in enumerate(dex["methods"]):
        if owner == "Landroid/opengl/GLES20;":
            gles.append((name, desc))
            continue
        if not want_type(owner, dex["defined"]):
            continue
        internal_name = owner[1:-1]
        static = dex["method_static"].get(i, name in ("<clinit>",) or False)
        if name == "<init>":
            static = False
        cls_methods.setdefault(internal_name, []).append((name, desc, static))
    for i, (owner, name, desc) in enumerate(dex["fields"]):
        if not want_type(owner, dex["defined"]):
            continue
        internal_name = owner[1:-1]
        static = dex["field_static"].get(i, False)
        cls_fields.setdefault(internal_name, []).append((name, desc, static))

    parents = set(EXTENDS.values())
    iface_owners = set()
    for mi in dex["iface_invoke"]:
        if mi < len(dex["methods"]):
            owner = dex["methods"][mi][0]
            if owner.startswith("L") and owner.endswith(";"):
                iface_owners.add(owner[1:-1])

    grafted = 0
    for i, (owner, name, desc) in enumerate(dex["methods"]):
        if name in ("<init>", "<clinit>"):
            continue
        owned = dex["defined_methods"].get(owner)
        if not owned or (name, desc) in owned:
            continue
        if dex["method_static"].get(i, False):
            continue
        cur = dex["supers"].get(owner)
        seen_walk = set()
        while cur and cur in dex["defined"] and cur not in seen_walk:
            seen_walk.add(cur)
            cur = dex["supers"].get(cur)
        if not cur or not want_type(cur, dex["defined"]):
            continue
        internal = cur[1:-1]
        cls_methods.setdefault(internal, []).append((name, desc, False))
        grafted += 1
    print("grafted inherited methods", grafted)

    extended = set()
    for s in dex["supers"].values():
        if s.startswith("L") and s.endswith(";"):
            extended.add(s[1:-1])
    for t in dex["types"]:
        if want_type(t, dex["defined"]):
            cls_methods.setdefault(t[1:-1], [])

    os.makedirs(OUT, exist_ok=True)
    nclass = 0
    niface = 0
    emitted = set()
    for internal_name in sorted(set(list(cls_methods) + list(cls_fields))):
        if any(internal_name.startswith(p) for p in HAND_PREFIX):
            continue
        methods = list(cls_methods.get(internal_name, []))
        fields = list(cls_fields.get(internal_name, []))
        has_static = any(m[2] and m[0] not in ("<clinit>", "<init>") for m in methods)
        has_ctor = any(m[0] == "<init>" for m in methods)
        is_iface = internal_name in iface_types or internal_name in iface_owners
        if internal_name in EXTENDS or internal_name in parents or internal_name in (
            "android/view/View", "android/os/Build", "android/os/Build$VERSION",
        ):
            is_iface = False
        if has_static or has_ctor:
            is_iface = False
        if (internal_name.endswith("Listener") or internal_name.endswith("Callback")) and not has_static and not has_ctor:
            if internal_name not in EXTENDS:
                is_iface = True
        if internal_name in extended:
            is_iface = False
        super_name = "java/lang/Object"
        if not is_iface:
            super_name = EXTENDS.get(internal_name, "java/lang/Object")
        if is_iface:
            methods = [m for m in methods if m[0] not in ("<init>", "<clinit>")]
            static_fields = [f for f in fields if f[2]]
            inst_fields = [f for f in fields if not f[2]]
            data = build_class(internal_name, "java/lang/Object", True, methods, static_fields, False)
            path = os.path.join(OUT, internal_name + ".class")
            os.makedirs(os.path.dirname(path), exist_ok=True)
            open(path, "wb").write(data)
            nclass += 1
            niface += 1
            emitted.add(internal_name)
            impl = "port/impl/" + internal_name
            impl_methods = [m for m in methods if not m[2]]
            data = build_class(impl, "java/lang/Object", False, impl_methods, inst_fields, False, [internal_name])
            path = os.path.join(OUT, impl + ".class")
            os.makedirs(os.path.dirname(path), exist_ok=True)
            open(path, "wb").write(data)
            nclass += 1
            continue
        data = build_class(internal_name, super_name, False, methods, fields, False)
        path = os.path.join(OUT, internal_name + ".class")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        open(path, "wb").write(data)
        nclass += 1
        emitted.add(internal_name)
    needed = set(EXTENDS) | set(EXTENDS.values())
    needed.discard("java/lang/Object")
    for internal_name in sorted(needed):
        if internal_name in emitted:
            continue
        super_name = EXTENDS.get(internal_name, "java/lang/Object")
        data = build_class(internal_name, super_name, False, [], [], False)
        path = os.path.join(OUT, internal_name + ".class")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        open(path, "wb").write(data)
        nclass += 1
        emitted.add(internal_name)
        print("added empty", internal_name, "extends", super_name)
    print("stub classes", nclass, "interfaces", niface)

    # GLES20 native class
    gles_unique = []
    seen = set()
    for n, d in gles:
        if (n, d) in seen:
            continue
        seen.add((n, d))
        gles_unique.append((n, d))
    if gles_unique:
        methods = [(n, d, True) for n, d in gles_unique]
        data = build_class("android/opengl/GLES20", "java/lang/Object", False, methods, [], True)
        path = os.path.join(OUT, "android", "opengl", "GLES20.class")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        open(path, "wb").write(data)
        emit_gles(gles_unique)

    os.makedirs(os.path.dirname(NATIVE_TXT), exist_ok=True)
    with open(NATIVE_TXT, "w", encoding="utf-8") as f:
        for item in dex["native"]:
            f.write("%s %s %s static=%s\n" % item)
    print("native methods", len(dex["native"]))


if __name__ == "__main__":
    main()
