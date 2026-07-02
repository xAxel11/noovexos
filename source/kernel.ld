ENTRY(_kstart)
SECTIONS {
  . = 0x10000;
  .text   : { *(.text.boot) *(.text .text.* .text.unlikely) }
  .rodata : { *(.rodata .rodata.*) }
  .data   : { *(.data .data.*) }
  . = ALIGN(16);
  __file_end = .;
  . = 0x00C00000;
  __bss_start = .;
  .bss (NOLOAD) : { *(.bss .bss.* COMMON) }
  . = ALIGN(16);
  __bss_end = .;
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.eh_frame_hdr) }
}
