#!/bin/sh
# Splice kernel.bin into the base ISO and produce a bootable ISO + USB image.
# Needs ../iso/_NoovexDesktop-base.iso as the splice base.
python3 - <<'PY'
KOFF=45568; KMAX=557056; LOADLIM=524288
sig=bytes.fromhex('66b810008ed88ec0')
kern=open('kernel.bin','rb').read()
assert kern[:8]==sig, "bad kernel signature"
assert len(kern)<=LOADLIM, "kernel exceeds 512KB stage2 load limit"
base=open('../iso/_NoovexDesktop-base.iso','rb').read()
out=bytearray(base); out[KOFF:KOFF+KMAX]=kern+b'\x00'*(KMAX-len(kern))
open('NoovexOS-new.iso','wb').write(bytes(out))
# USB image = a 1.44MB slice of the ISO with an MBR from any existing USB image
usb=bytearray(out[43008:43008+1474560])
# copy MBR partition table + boot sig from a shipped image
ref=open('../img/NoovexOS-V33-USB.img','rb').read()
usb[446:512]=ref[446:512]
open('NoovexOS-new-USB.img','wb').write(bytes(usb))
print("wrote NoovexOS-new.iso and NoovexOS-new-USB.img")
PY
