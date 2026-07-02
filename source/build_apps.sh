#!/bin/sh
# Build the userspace apps (NoovexCraft, PY IDLE, editor, pymini) as flat ELFs
# and wrap them in NVXA containers for the NVXFS app-disk (VHD).
set -e
cd apps
LG=divmod.o
gcc -m32 -ffreestanding -fno-pie -fno-pic -fno-stack-protector -fno-builtin -nostdlib -Os -c divmod.c -o divmod.o
LINK="-m32 -ffreestanding -no-pie -nostdlib -nostartfiles -static -Wl,-e_start -Wl,-Ttext=0x04000000 -Wl,--nmagic -Wl,--build-id=none -Os"
for app in noovexcraft editor; do
  gcc -m32 -ffreestanding -fno-pie -fno-pic -fno-stack-protector -fno-builtin -nostdlib -Os -DNVX_LIBC_IMPL -I. -c $app.c -o $app.o
  gcc $LINK -o $app.elf $app.o $LG
  objcopy --strip-all $app.elf
  echo "built $app.elf"
done
# pymini standalone (PYRUN) and the IDLE (embeds pymini with -DIDE)
gcc -m32 -ffreestanding -fno-pie -fno-pic -fno-stack-protector -fno-builtin -nostdlib -Os -DNOOVEX -DNVX_LIBC_IMPL -I. -c pymini.c -o pymini.o
gcc $LINK -o pyrun.elf pymini.o $LG; objcopy --strip-all pyrun.elf
gcc -m32 -ffreestanding -fno-pie -fno-pic -fno-stack-protector -fno-builtin -nostdlib -Os -I. -c pyidle.c -o pyidle.o
gcc $LINK -o pyidle.elf pyidle.o $LG; objcopy --strip-all pyidle.elf
echo "apps built. Wrap each .elf in a 64-byte NVXA header (magic 'NVXA', name[32]@8, elf_off=64@56, elf_len@60) and place on the NVXFS VHD."
