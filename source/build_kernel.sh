#!/bin/sh
# Build the NoovexOS kernel from source (32-bit x86, freestanding).
# Requires: gcc-multilib (32-bit), binutils.  Run from this source/ folder.
set -e
K=kernel_v33.c
echo "[1/3] compiling kernel ($K) ..."
gcc -m32 -ffreestanding -fno-pie -no-pie -fno-pic -fno-stack-protector \
    -fno-strict-aliasing -fno-delete-null-pointer-checks -fno-builtin \
    -fno-asynchronous-unwind-tables -nostdlib -Os \
    -I. -Iheaders -c "$K" -o kernel.o
echo "[2/3] linking ..."
ld -m elf_i386 -no-pie -nostdlib -T kernel.ld -o kernel.elf entry.o kernel.o
echo "[3/3] objcopy -> kernel.bin ..."
objcopy -O binary -R .bss kernel.elf kernel.bin
SZ=$(stat -c%s kernel.bin)
echo "kernel.bin = $SZ bytes  (HARD LIMIT 524288 - stage2 loads only 512KB)"
[ "$SZ" -le 524288 ] || { echo "ERROR: kernel exceeds 512KB loader limit"; exit 1; }
echo "OK. Now run ./splice.sh to make an ISO + USB image."
