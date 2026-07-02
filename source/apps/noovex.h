/* ============================================================
   NoovexOS SDK - syscall ABI v1 (stable). INT 0x80, number in eax.
   Build:  gcc -m32 -ffreestanding -no-pie -nostdlib -nostartfiles -static \
           -Wl,-e_start -Wl,-Ttext=0x04000000 -Wl,--nmagic -O2 -o APP.ELF app.c
   ============================================================ */
#ifndef NOOVEX_H
#define NOOVEX_H
typedef unsigned int u32;

/* --- console + system --- */
static inline void nvx_exit(void){ __asm__ volatile("int $0x80"::"a"(0)); }
static inline void nvx_print(const char* s){ __asm__ volatile("int $0x80"::"a"(1),"b"(s):"memory"); }
static inline int  nvx_readkey(void){ int r; __asm__ volatile("int $0x80":"=a"(r):"a"(2)); return r; }
static inline void nvx_putc(char c){ __asm__ volatile("int $0x80"::"a"(3),"b"((int)c)); }
static inline void nvx_clear(void){ __asm__ volatile("int $0x80"::"a"(4)); }
static inline void nvx_beep(int hz){ __asm__ volatile("int $0x80"::"a"(5),"b"(hz)); }
static inline void nvx_delay(int t){ __asm__ volatile("int $0x80"::"a"(6),"b"(t)); }

/* --- info --- */
static inline int nvx_getinfo(int what){ int r; __asm__ volatile("int $0x80":"=a"(r):"a"(7),"b"(what)); return r; }
#define NVX_WIDTH 0
#define NVX_HEIGHT 1
#define NVX_RAMMB 2
#define NVX_TICKS 3
#define NVX_HASDISK 4
static inline int nvx_width(void){ return nvx_getinfo(NVX_WIDTH); }
static inline int nvx_height(void){ return nvx_getinfo(NVX_HEIGHT); }

/* --- graphics (colors are 0x00RRGGBB) --- */
static inline void nvx_pixel(int x,int y,u32 rgb){ __asm__ volatile("int $0x80"::"a"(8),"b"(x),"c"(y),"d"(rgb)); }
static inline void nvx_rect(int x,int y,int w,int h,u32 rgb){ __asm__ volatile("int $0x80"::"a"(9),"b"(x),"c"(y),"d"(w),"S"(h),"D"(rgb)); }
static inline void nvx_text(int x,int y,const char* s,u32 rgb){ __asm__ volatile("int $0x80"::"a"(10),"b"(x),"c"(y),"d"(s),"S"(rgb):"memory"); }
static inline void nvx_clearrgb(u32 rgb){ __asm__ volatile("int $0x80"::"a"(14),"b"(rgb)); }

/* --- files --- */
static inline int nvx_fileread(const char* name,char* buf,int max){ int r; __asm__ volatile("int $0x80":"=a"(r):"a"(11),"b"(name),"c"(buf),"d"(max):"memory"); return r; }
static inline int nvx_filewrite(const char* name,const char* buf,int len){ int r; __asm__ volatile("int $0x80":"=a"(r):"a"(12),"b"(name),"c"(buf),"d"(len):"memory"); return r; }
static inline int nvx_filelist(int index,char* namebuf){ int r; __asm__ volatile("int $0x80":"=a"(r):"a"(13),"b"(index),"c"(namebuf):"memory"); return r; }

/* --- misc --- */
static inline u32 nvx_random(void){ u32 r; __asm__ volatile("int $0x80":"=a"(r):"a"(15)); return r; }

/* --- USB FAT files (for large data like DOOM WADs) --- */
static inline int nvx_usb_mount(void){ int r; __asm__ volatile("int $0x80":"=a"(r):"a"(16)); return r; }          /* -> file count */
static inline int nvx_usb_name(int idx,char* buf16){ int r; __asm__ volatile("int $0x80":"=a"(r):"a"(17),"b"(idx),"c"(buf16):"memory"); return r; }  /* -> namelen, -1 */
static inline int nvx_usb_size(int idx){ int r; __asm__ volatile("int $0x80":"=a"(r):"a"(18),"b"(idx)); return r; }  /* -> bytes, -1 */
static inline int nvx_usb_read(int idx,char* buf,int max){ int r; __asm__ volatile("int $0x80":"=a"(r):"a"(19),"b"(idx),"c"(buf),"d"(max):"memory"); return r; }  /* -> bytes read */

/* --- fast framebuffer + timing (for real-time apps / DOOM) --- */
static inline u32* nvx_fb(void){ u32 r; __asm__ volatile("int $0x80":"=a"(r):"a"(20),"b"(0)); return (u32*)r; }     /* visible framebuffer base (write base[y*pitch+x]) */
static inline int  nvx_fb_pitch(void){ int r; __asm__ volatile("int $0x80":"=a"(r):"a"(20),"b"(1)); return r; }    /* pixels per row */
static inline int  nvx_fb_w(void){ int r; __asm__ volatile("int $0x80":"=a"(r):"a"(20),"b"(2)); return r; }
static inline int  nvx_fb_h(void){ int r; __asm__ volatile("int $0x80":"=a"(r):"a"(20),"b"(3)); return r; }
static inline u32  nvx_ticks_ms(void){ u32 r; __asm__ volatile("int $0x80":"=a"(r):"a"(21)); return r; }            /* monotonic milliseconds */
static inline int  nvx_readraw(void){ int r; __asm__ volatile("int $0x80":"=a"(r):"a"(22)); return r; }              /* non-blocking raw scancode (bit7=release), 0=none */
static inline void nvx_mouse(int* out){ __asm__ volatile("int $0x80"::"a"(25),"b"(out):"memory"); }  /* out[0]=x out[1]=y out[2]=buttons out[3]=relative dx */
static inline int  nvx_http_get(const char* url,char* buf,int max){ int r; __asm__ volatile("int $0x80":"=a"(r):"a"(23),"b"(url),"c"(buf),"d"(max):"memory"); return r; } /* fetch HTTP/HTTPS URL -> body length, -1 fail */
static inline int  nvx_remove(const char* name){ int r; __asm__ volatile("int $0x80":"=a"(r):"a"(24),"b"(name):"memory"); return r; }  /* delete file from NVXFS -> 0 ok, -1 not found */

/* scancodes for nvx_readkey */
#define KEY_ESC 0x01
#define KEY_ENTER 0x1C
#define KEY_SPACE 0x39
#define KEY_UP 0x48
#define KEY_DOWN 0x50
#define KEY_LEFT 0x4B
#define KEY_RIGHT 0x4D
#endif
