"""Mark the four premium packs owned inside the game classes.

bf.a is the ownership mask. Loading a profile replaces it with the saved
value, so the load ORs in bits 0..4. The upgrades screen also bails out
when Play billing is missing; those two branches now fall through to the
list, which shows UNLOCKED from the same mask.
"""
import struct
import zipfile
from pathlib import Path

ROOT = Path(r"D:\Sandbox Cursor\RADIANT DEFENCE")
JAR = ROOT / "port" / "build" / "radiant.jar"
MASK = 31

# opcode -> length, not counting tableswitch/lookupswitch
OPLEN = {
    0x10: 2, 0x11: 3, 0x12: 2, 0x13: 3, 0x14: 3, 0x15: 2, 0x16: 2, 0x17: 2,
    0x18: 2, 0x19: 2, 0x36: 2, 0x37: 2, 0x38: 2, 0x39: 2, 0x3a: 2, 0x84: 3,
    0x99: 3, 0x9a: 3, 0x9b: 3, 0x9c: 3, 0x9d: 3, 0x9e: 3, 0x9f: 3, 0xa0: 3,
    0xa1: 3, 0xa2: 3, 0xa3: 3, 0xa4: 3, 0xa5: 3, 0xa6: 3, 0xa7: 3, 0xa8: 3,
    0xb2: 3, 0xb3: 3, 0xb4: 3, 0xb5: 3, 0xb6: 3, 0xb7: 3, 0xb8: 3, 0xb9: 5,
    0xa9: 2, 0xba: 5, 0xbb: 3, 0xbc: 2, 0xbd: 3, 0xc0: 3, 0xc1: 3, 0xc6: 3, 0xc7: 3, 0xc5: 4,
    0xc8: 5, 0xc9: 5,
}
BRANCH16 = {0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xc6, 0xc7}


def u2(b, p):
    return struct.unpack_from(">H", b, p)[0], p + 2


def u4(b, p):
    return struct.unpack_from(">I", b, p)[0], p + 4


def parse_cp(data):
    p = 8
    count, p = u2(data, p)
    cp = [None]
    i = 1
    while i < count:
        tag = data[p]
        p += 1
        if tag == 1:
            ln, p = u2(data, p)
            s = data[p:p + ln].decode("utf-8")
            p += ln
            cp.append(("utf8", s))
        elif tag in (7, 8, 16, 19, 20):
            v, p = u2(data, p)
            cp.append((tag, v))
        elif tag in (3, 4, 9, 10, 11, 12, 17, 18):
            v = struct.unpack_from(">I", data, p)[0]
            p += 4
            if tag == 12:
                cp.append(("nat", v >> 16, v & 0xFFFF))
            elif tag in (9, 10, 11):
                cp.append((tag, v >> 16, v & 0xFFFF))
            else:
                cp.append((tag, v))
        elif tag in (5, 6):
            p += 8
            cp.append((tag, None))
            cp.append(None)
            i += 1
        elif tag == 15:
            p += 3
            cp.append((tag, None))
        else:
            raise SystemExit("bad cp tag %s at %s" % (tag, p))
        i += 1
    return cp, p


def utf(cp, idx):
    item = cp[idx]
    if item[0] != "utf8":
        raise SystemExit("not utf8 %s" % (item,))
    return item[1]


def nat(cp, idx):
    item = cp[idx]
    return utf(cp, item[1]), utf(cp, item[2])


def method_name(cp, idx):
    # Methodref class, nat
    item = cp[idx]
    return nat(cp, item[2])


def field_name(cp, idx):
    item = cp[idx]
    cls = utf(cp, cp[item[1]][1])
    name, desc = nat(cp, item[2])
    return cls, name, desc


def skip_attrs(data, p, n):
    for _ in range(n):
        _, p = u2(data, p)
        ln, p = u4(data, p)
        p += ln
    return p


def find_method(data, cp, p, want_name, want_desc):
    access, p = u2(data, p)
    this, p = u2(data, p)
    super_, p = u2(data, p)
    ic, p = u2(data, p)
    p += ic * 2
    fc, p = u2(data, p)
    p = skip_fields(data, p, fc)
    mc, p = u2(data, p)
    for _ in range(mc):
        start = p
        _, p = u2(data, p)
        ni, p = u2(data, p)
        di, p = u2(data, p)
        ac, p = u2(data, p)
        name, desc = utf(cp, ni), utf(cp, di)
        attr_start = p
        if name == want_name and desc == want_desc:
            return start, attr_start, ac
        p = skip_attrs(data, p, ac)
    raise SystemExit("method %s %s missing" % (want_name, want_desc))


def skip_fields(data, p, n):
    for _ in range(n):
        p += 6
        ac, p = u2(data, p)
        p = skip_attrs(data, p, ac)
    return p


def code_attr(data, cp, p, ac):
    for _ in range(ac):
        ni, p2 = u2(data, p)
        ln, p2 = u4(data, p2)
        if utf(cp, ni) == "Code":
            return p, p2, ln
        p = p2 + ln
    raise SystemExit("no code")


def insn_len(code, ip):
    op = code[ip]
    if op in (0xAA, 0xAB):
        raise SystemExit("switch at %s" % ip)
    if op == 0xC4:  # wide
        op2 = code[ip + 1]
        return 6 if op2 == 0x84 else 4
    return OPLEN.get(op, 1)


def walk(code):
    ip = 0
    pcs = []
    while ip < len(code):
        pcs.append(ip)
        ip += insn_len(code, ip)
    if ip != len(code):
        raise SystemExit("walk ended at %s of %s" % (ip, len(code)))
    return pcs


def s16(b, p):
    return struct.unpack_from(">h", b, p)[0]


def p16(v):
    if v < -32768 or v > 32767:
        raise SystemExit("branch overflow %s" % v)
    return struct.pack(">h", v)


def adjust_code(code, exc, insert_at, extra):
    n = len(extra)
    new = bytearray(code[:insert_at]) + extra + bytearray(code[insert_at:])
    for ip in walk(code):
        op = code[ip]
        if op not in BRANCH16:
            continue
        rel = s16(code, ip + 1)
        target = ip + rel
        new_ip = ip + (n if ip >= insert_at else 0)
        new_target = target + (n if target >= insert_at else 0)
        struct.pack_into(">h", new, new_ip + 1, new_target - new_ip)
    new_exc = bytearray()
    for i in range(0, len(exc), 8):
        start, end, handler, catch = struct.unpack_from(">HHHH", exc, i)
        start = start + n if start >= insert_at else start
        end = end + n if end > insert_at else end
        handler = handler + n if handler >= insert_at else handler
        new_exc += struct.pack(">HHHH", start, end, handler, catch)
    return new, new_exc


def patch_method(data, cp, method_start, mutate):
    p = method_start
    p += 6  # access, name, desc
    ac, p = u2(data, p)
    attr_pos, code_pos, attr_len = code_attr(data, cp, p, ac)
    # code_pos points at the start of the attribute info (after length)
    max_stack, q = u2(data, code_pos)
    max_locals, q = u2(data, q)
    code_len, q = u4(data, q)
    code = bytearray(data[q:q + code_len])
    q += code_len
    exc_n, q = u2(data, q)
    exc = bytearray(data[q:q + exc_n * 8])
    q += exc_n * 8
    sub_n, q = u2(data, q)
    sub_start = q
    sub_end = code_pos + attr_len
    new_code, new_exc, new_stack = mutate(code, exc, max_stack)
    if new_code == code and new_exc == exc and new_stack == max_stack:
        return data
    # rebuild attribute body
    body = struct.pack(">HHI", new_stack, max_locals, len(new_code))
    body += new_code
    body += struct.pack(">H", len(new_exc) // 8)
    body += new_exc
    body += data[sub_start - 2:sub_end]  # includes sub_n and sub attrs, unchanged
    # data[sub_start-2] is the u2 sub_n which we already... wait
    # body currently has max_stack, locals, code, exc. The original tail from
    # sub_n through end of attribute should be appended once.
    # I included sub_n by starting at sub_start-2. Good.
    new_attr = body
    if len(new_attr) != attr_len + (len(new_code) - len(code)) + (len(new_exc) - len(exc)):
        # sub attrs copied from original; length delta is only code and exc
        pass
    delta = len(new_attr) - attr_len
    # attribute: u2 name, u4 len, body
    name_pos = attr_pos
    out = bytearray(data[:name_pos + 2])
    out += struct.pack(">I", len(new_attr))
    out += new_attr
    out += data[code_pos + attr_len:]
    return bytes(out)


def patch_bf(data):
    cp, p = parse_cp(data)
    start, _, _ = find_method(data, cp, p, "a", "()V")

    def mutate(code, exc, stack):
        if code[15] != 0x9A or code[78] != 0x9A:
            raise SystemExit("bf.a() branches moved: %02x %02x" % (code[15], code[78]))
        code = bytearray(code)
        # Connection Failed and Connecting... both skip straight to the list.
        code[15] = 0xA7
        code[78] = 0xA7
        return code, exc, stack

    return patch_method(data, cp, start, mutate)


def patch_af(data):
    cp, p = parse_cp(data)
    start, _, _ = find_method(data, cp, p, "a", "(Lnet/hexage/defense/fj;I)V")

    def mutate(code, exc, stack):
        # aload_1, invokevirtual fj.j()I, putstatic bf.a
        pat = bytes([0x2B, 0xB6])
        found = -1
        for ip in walk(code):
            if code[ip:ip + 2] != pat:
                continue
            idx = struct.unpack_from(">H", code, ip + 2)[0]
            name, desc = method_name(cp, idx)
            if name != "j" or desc != "()I":
                continue
            nxt = ip + 4
            if code[nxt] != 0xB3:
                continue
            fidx = struct.unpack_from(">H", code, nxt + 1)[0]
            cls, fname, fdesc = field_name(cp, fidx)
            if cls == "net/hexage/defense/bf" and fname == "a" and fdesc == "I":
                found = nxt
                break
        if found < 0:
            raise SystemExit("premium load site not found")
        extra = bytes([0x10, MASK, 0x80])  # bipush 31, ior
        code, exc = adjust_code(code, exc, found, extra)
        return code, exc, max(stack, 4)

    return patch_method(data, cp, start, mutate)


def main():
    blob = JAR.read_bytes()
    src = zipfile.ZipFile(JAR)
    bf = patch_bf(src.read("net/hexage/defense/bf.class"))
    af = patch_af(src.read("net/hexage/defense/af.class"))
    tmp = JAR.with_suffix(".jar.tmp")
    with zipfile.ZipFile(tmp, "w") as out:
        for info in src.infolist():
            data = src.read(info.filename)
            if info.filename == "net/hexage/defense/bf.class":
                data = bf
            elif info.filename == "net/hexage/defense/af.class":
                data = af
            out.writestr(info, data)
    src.close()
    tmp.replace(JAR)
    print("patched", JAR, "size", JAR.stat().st_size, "was", len(blob))


if __name__ == "__main__":
    main()
