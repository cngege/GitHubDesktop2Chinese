import struct, sys

path = sys.argv[1]
f = open(path, "rb").read()
print("size", len(f))
e_lfanew = struct.unpack_from("<I", f, 0x3c)[0]
print("e_lfanew", hex(e_lfanew), "sig", f[e_lfanew:e_lfanew+4])
coff = e_lfanew + 4
machine, nsec, tds, ptrsym, nsym, optsz, chars = struct.unpack_from("<HHIIIHH", f, coff)
print("machine", hex(machine), "nsec", nsec, "optsz", optsz, "chars", hex(chars),
      "timestamp", tds, "symptr", ptrsym, "nsym", nsym)
opt = coff + 20
magic = struct.unpack_from("<H", f, opt)[0]
print("magic", hex(magic))
ep = struct.unpack_from("<I", f, opt+16)[0]
print("EntryPoint RVA", hex(ep))
imgbase = struct.unpack_from("<I", f, opt+28)[0]
print("ImageBase", hex(imgbase))
subsystem = struct.unpack_from("<H", f, opt+68)[0]
print("Subsystem", subsystem)
dllchar = struct.unpack_from("<H", f, opt+70)[0]
print("DllCharacteristics", hex(dllchar))
ndirs = struct.unpack_from("<I", f, opt+92)[0]
print("NumDirs", ndirs)
names = ["export","import","resource","exception","security","basereloc","debug","arch",
         "globalptr","tls","loadcfg","boundimport","iat","delayimport","clr","reserved"]
for i in range(min(ndirs, 16)):
    rva, size = struct.unpack_from("<II", f, opt+96+8*i)
    if rva or size:
        print(f"dir{i:2d} {names[i]:12s} RVA={rva:#010x} size={size:#x}")
secoff = opt + optsz
print("--- sections ---")
secs = []
for i in range(nsec):
    o = secoff + i*40
    name = f[o:o+8].rstrip(b"\0").decode('latin1')
    vsize, vaddr, rsize, raddr = struct.unpack_from("<IIII", f, o+8)
    sflags = struct.unpack_from("<I", f, o+36)[0]
    secs.append((name, vaddr, vsize, raddr, rsize, sflags))
    print(f"{name:10s} VA={vaddr:#010x} VSize={vsize:#010x} RawPtr={raddr:#010x} RawSize={rsize:#x} Flags={sflags:#010x}")
print("--- overlay ---")
end = max(r + rs for _,_,_,r,rs,_ in secs)
print("end of image", hex(end), "overlay", len(f)-end)

def rva2off(rva):
    for _, va, vs, rp, rs, _ in secs:
        if va <= rva < va + max(vs, rs):
            return rp + (rva - va)
    return None

# resources
for i in range(min(ndirs, 16)):
    rva, size = struct.unpack_from("<II", f, opt+96+8*i)
    if not rva:
        continue
    if names[i] == "resource":
        off = rva2off(rva)
        print("resource raw off", hex(off))
