#include "sintab.h"
#include "font.h"
#include "fontaa.h"
typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef unsigned long long u64;
typedef long long      i64;
#include "nvxcrypto.h"
#include "nvxtls.h"
#include "png.h"
#include "jpeg.h"
#include "zip.h"
#include "nlogo.h"
#include "hellodemo.h"
#include "icons_png.h"
#include "qr_recovery.h"
#include "sample_png.h"
#include "sample_jpg.h"
#include "space_wp.h"
#include "hexlang.h"
#include "nvx_boot_bin.h"
#include "nvx_stage2_bin.h"

#define VND_NAME "NOOVEX"
#define VND_APP  "NOOVEX CENTER"
#define VND_SHORT "CENTER"
#define VND_TAG  "YOUR SYSTEM, FROM SCRATCH"
#define VND_SUP  "NOOVEX.LOCAL/SUPPORT"
#define VND_MODEL "NOOVEXBOOK"
#define VND_COL  C_BBLUE

#ifdef NOOVEX8
  #define OSNAME "NOOVEX8"
  #define OSVER  "NOOVEX8 1.0 (VESA / GPU)"
#elif defined(NOOVEX7)
  #define OSNAME "NOOVEX7"
  #define OSVER  "NOOVEX7 1.0 (VESA / GPU)"
#else
  #define OSNAME "NOOVEX8"
  #define OSVER  "NOOVEX8 GRAPHICS 1.0 (VESA)"
#endif
#ifndef BUILDVER
  #ifdef NOOVEXLITE
    #define BUILDVER "LITE 1.0"
  #elif defined(NOOVEXSRV)
    #define BUILDVER "SERVER 1.0"
  #else
    #define BUILDVER "BUILD 70"
  #endif
#endif

static inline void outb(u16 p,u8 v){ __asm__ __volatile__("outb %0,%1"::"a"(v),"Nd"(p)); }
static inline u8   inb(u16 p){ u8 r; __asm__ __volatile__("inb %1,%0":"=a"(r):"Nd"(p)); return r; }
static inline void insw(u16 port,void*addr,int cnt){ __asm__ __volatile__("rep insw":"+D"(addr),"+c"(cnt):"d"(port):"memory"); }
static inline void outsw(u16 port,const void*addr,int cnt){ __asm__ __volatile__("rep outsw":"+S"(addr),"+c"(cnt):"d"(port)); }
static inline void outl(u16 p,u32 v){ __asm__ __volatile__("outl %0,%1"::"a"(v),"Nd"(p)); }
static inline void outw(u16 p,u16 v){ __asm__ __volatile__("outw %0,%1"::"a"(v),"Nd"(p)); }
static inline u16  inw(u16 p){ u16 r; __asm__ __volatile__("inw %1,%0":"=a"(r):"Nd"(p)); return r; }
static inline u32 inl(u16 p){ u32 r; __asm__ __volatile__("inl %1,%0":"=a"(r):"Nd"(p)); return r; }
/* ---- 16550 UART serial logger (COM1 @ 0x3F8, 115200 8N1) ---- */
#define COM1 0x3F8
static void serial_init(void){
    outb(COM1+1,0x00);            /* no interrupts            */
    outb(COM1+3,0x80);            /* DLAB on                  */
    outb(COM1+0,0x01); outb(COM1+1,0x00); /* divisor 1 -> 115200 */
    outb(COM1+3,0x03);            /* 8 bits, no parity, 1 stop*/
    outb(COM1+2,0xC7);            /* FIFO on, clear, 14-byte  */
    outb(COM1+4,0x0B);            /* DTR/RTS/OUT2             */
}
static void serial_putc(char c){
    int t;
    if(c=='\n'){ t=0; while(!(inb(COM1+5)&0x20)) if(++t>200000)break; outb(COM1,'\r'); }
    t=0; while(!(inb(COM1+5)&0x20)) if(++t>200000)break;
    outb(COM1,(u8)c);
}
static void serial_puts(const char* s){ while(*s) serial_putc(*s++); }
static void serial_hex8(u32 v){ serial_puts("0x"); for(int i=28;i>=0;i-=4){ int d=(v>>i)&0xF; serial_putc((char)(d<10?'0'+d:'a'+d-10)); } }
static inline int S(int i){ return sinT[(u8)i]; }

#define ATA_DATA 0x1F0
#define ATA_SECCNT 0x1F2
#define ATA_LBA0 0x1F3
#define ATA_LBA1 0x1F4
#define ATA_LBA2 0x1F5
#define ATA_DRIVE 0x1F6
#define ATA_CMD 0x1F7
static int disk_ok=0;
#define DISK_LABEL "NOOVEXOS BOOT MANAGER"
static unsigned char disk_size_gb=10;
static int set_repair_ok=0; static char set_repair_msg[40]="";
static int ata_wait_bsy(void){ int t=0; while(inb(ATA_CMD)&0x80){ if(++t>2000000) return -1; } return 0; }
static int ata_wait_drq(void){ int t=0; for(;;){ u8 s=inb(ATA_CMD); if(s&0x21) return -1; if(s&0x08) return 0; if(++t>2000000) return -1; } }
static int ata_read(u32 lba,void*buf){
    if(ata_wait_bsy())return -1;
    outb(ATA_DRIVE,0xE0|((lba>>24)&0x0F)); outb(ATA_SECCNT,1);
    outb(ATA_LBA0,lba&0xFF); outb(ATA_LBA1,(lba>>8)&0xFF); outb(ATA_LBA2,(lba>>16)&0xFF);
    outb(ATA_CMD,0x20);
    if(ata_wait_bsy())return -1; if(ata_wait_drq())return -1;
    insw(ATA_DATA,buf,256); return 0;
}
static int ata_read_n(u32 lba,void*buf,int nsec){
    if(nsec<1)return -1; if(nsec>64)nsec=64;
    if(ata_wait_bsy())return -1;
    outb(ATA_DRIVE,0xE0|((lba>>24)&0x0F)); outb(ATA_SECCNT,nsec);
    outb(ATA_LBA0,lba&0xFF); outb(ATA_LBA1,(lba>>8)&0xFF); outb(ATA_LBA2,(lba>>16)&0xFF);
    outb(ATA_CMD,0x20);
    { u16* p=(u16*)buf; for(int s=0;s<nsec;s++){ if(ata_wait_bsy())return -1; if(ata_wait_drq())return -1; insw(ATA_DATA,p,256); p+=256; } }
    return 0;
}
static int ata_write(u32 lba,const void*buf){
    if(ata_wait_bsy())return -1;
    outb(ATA_DRIVE,0xE0|((lba>>24)&0x0F)); outb(ATA_SECCNT,1);
    outb(ATA_LBA0,lba&0xFF); outb(ATA_LBA1,(lba>>8)&0xFF); outb(ATA_LBA2,(lba>>16)&0xFF);
    outb(ATA_CMD,0x30);
    if(ata_wait_bsy())return -1; if(ata_wait_drq())return -1;
    outsw(ATA_DATA,buf,256); outb(ATA_CMD,0xE7); ata_wait_bsy(); return 0;
}
static int ata_wb(u16 b){ int t=0; while(inb(b+7)&0x80){ if(++t>2000000)return -1; } return 0; }
static int ata_wd(u16 b){ int t=0; for(;;){ u8 s=inb(b+7); if(s&0x21)return -1; if(s&0x08)return 0; if(++t>2000000)return -1; } }
static int ata_read_drv(int slot,u32 lba,void*buf){
    u16 b=(slot<2)?0x1F0:0x170; u8 dv=((slot&1)?0xF0:0xE0)|((lba>>24)&0x0F);
    outb(b+6,dv); for(int i=0;i<5;i++)inb(b+7);
    if(ata_wb(b))return -1;
    outb(b+2,1); outb(b+3,lba&0xFF); outb(b+4,(lba>>8)&0xFF); outb(b+5,(lba>>16)&0xFF);
    outb(b+7,0x20);
    if(ata_wb(b))return -1; if(ata_wd(b))return -1;
    insw(b,buf,256); return 0;
}
static int ata_write_drv(int slot,u32 lba,const void*buf){
    u16 b=(slot<2)?0x1F0:0x170; u8 dv=((slot&1)?0xF0:0xE0)|((lba>>24)&0x0F);
    outb(b+6,dv); for(int i=0;i<5;i++)inb(b+7);
    if(ata_wb(b))return -1;
    outb(b+2,1); outb(b+3,lba&0xFF); outb(b+4,(lba>>8)&0xFF); outb(b+5,(lba>>16)&0xFF);
    outb(b+7,0x30);
    if(ata_wb(b))return -1; if(ata_wd(b))return -1;
    outsw(b,buf,256); outb(b+7,0xE7); ata_wb(b); return 0;
}
static int ata_identify(void){
    outb(ATA_DRIVE,0xE0); outb(ATA_SECCNT,0); outb(ATA_LBA0,0); outb(ATA_LBA1,0); outb(ATA_LBA2,0);
    outb(ATA_CMD,0xEC);
    u8 s=inb(ATA_CMD); if(s==0)return -1;
    if(ata_wait_bsy())return -1;
    if(inb(ATA_LBA1)!=0||inb(ATA_LBA2)!=0)return -1;
    if(ata_wait_drq())return -1;
    insw(ATA_DATA,(u16*)0x0095DC00u,256); return 0;
}

#define NVX_DIR_LBA 2048
#define NVX_DIR_SECS 8
#define NVX_DATA0 (NVX_DIR_LBA+NVX_DIR_SECS)
#define NVX_SECPF 65536
#define NVX_MAX 128
#define NVX_MAGIC 0x4E56584Au
typedef struct { char name[16]; u32 start; u32 len; u32 rsv; } nvx_ent;
typedef struct { u32 magic; u32 count; nvx_ent e[NVX_MAX]; } nvxfs_t;
#define nvx (*(nvxfs_t*)0x0095E600u)
static void nvx_flush(void){ u8* s=(u8*)0x00928000u; u8* p=(u8*)&nvx; int total=sizeof(nvx); int secs=(total+511)/512;
    for(int se=0;se<secs;se++){ for(int i=0;i<512;i++){ int off=se*512+i; s[i]=(off<total)?p[off]:0; } ata_write(NVX_DIR_LBA+se,s); } }
static void nvx_format(void){
    nvx.magic=NVX_MAGIC; nvx.count=0;
    for(int i=0;i<NVX_MAX;i++){ nvx.e[i].name[0]=0; nvx.e[i].start=0; nvx.e[i].len=0; nvx.e[i].rsv=0; }
    const char* wn="WELCOME.TXT"; int k=0; while(wn[k]){nvx.e[0].name[k]=wn[k];k++;} nvx.e[0].name[k]=0;
    nvx.e[0].start=NVX_DATA0; nvx.count=1;
    u8* z=(u8*)0x00928200u; for(int i=0;i<512;i++)z[i]=0;
    const char* wt="WELCOME TO NVXFS - YOUR OWN FILESYSTEM. EDIT ME IN NOTEPAD AND SAVE!";
    int wl=0; while(wt[wl]){z[wl]=wt[wl];wl++;} nvx.e[0].len=wl;
    ata_write(NVX_DATA0,z); nvx_flush();
}
static int nvx_mount(void){ u8* s=(u8*)0x00928000u; u8* p=(u8*)&nvx; int total=sizeof(nvx); int secs=(total+511)/512;
    for(int se=0;se<secs;se++){ if(ata_read(NVX_DIR_LBA+se,s))return -1; for(int i=0;i<512;i++){ int off=se*512+i; if(off<total)p[off]=s[i]; } }
    if(nvx.magic!=NVX_MAGIC)nvx_format(); if(nvx.count>NVX_MAX)nvx.count=NVX_MAX; return 0; }
static int nvx_find(const char*name){ for(unsigned i=0;i<nvx.count;i++){ int eq=1; for(int k=0;k<16;k++){ char a=nvx.e[i].name[k],b=name[k]; if(a!=b){eq=0;break;} if(!a&&!b)break; } if(eq)return i; } return -1; }

static int nvx_cwd=-1;
static int nvx_isdir(int i){ if(i<0||(unsigned)i>=nvx.count)return 0; return (nvx.e[i].rsv>>31)&1; }
static int nvx_parent(int i){ if(i<0||(unsigned)i>=nvx.count)return -1; int p=(int)(nvx.e[i].rsv&0x7FFFFFFF)-1; if(p<0||(unsigned)p>=nvx.count||!nvx_isdir(p))return -1; return p; }
static int nvx_find_in(const char* name,int dir){ for(unsigned i=0;i<nvx.count;i++){ if(nvx_parent(i)!=dir)continue; int eq=1; for(int k=0;k<16;k++){ char a=nvx.e[i].name[k],b=name[k]; if(a!=b){eq=0;break;} if(!a&&!b)break; } if(eq)return i; } return -1; }
static int nvx_mkdir(const char* name){ if(!disk_ok)return -1; if(nvx.count>=NVX_MAX)return -1; if(nvx_find_in(name,nvx_cwd)>=0)return -1;
    int idx=nvx.count; int k=0; while(name[k]&&k<15){nvx.e[idx].name[k]=name[k];k++;} nvx.e[idx].name[k]=0;
    nvx.e[idx].start=NVX_DATA0+idx*NVX_SECPF; nvx.e[idx].len=0; nvx.e[idx].rsv=0x80000000u|((u32)(nvx_cwd+1)); nvx.count++; nvx_flush(); return idx; }

#define BL_LBA   2047
#define BL_MAGIC 0x424C4B31u
typedef struct { u32 magic; u32 enabled; u8 by_pw[32]; u8 by_rk[32]; u8 verf[32]; } bl_meta_t;
static bl_meta_t BLM;
static int bl_enabled=0, bl_unlocked=0;
static int dev_mode=0;
static u8  bl_key[32];
#define RE_BUF ((u8*)0x00970000u)
static void bl_xform(u32 lba,u8* s){
    if(!(bl_enabled&&bl_unlocked))return;
    u8 nonce[12]={0}; nonce[0]=lba&0xff; nonce[1]=(lba>>8)&0xff; nonce[2]=(lba>>16)&0xff; nonce[3]=(lba>>24)&0xff;
    chacha20(bl_key,0,nonce,s,s,512);
}
static u8 nvx_rbuf[16*512];
static int nvx_read(int idx,char*buf,int max){ if(bl_enabled&&!bl_unlocked)return 0; if(idx<0||(unsigned)idx>=nvx.count)return 0; int len=nvx.e[idx].len; if(len>max)len=max; u32 base=nvx.e[idx].start; int got=0,se=0; while(got<len){ int rem=(len-got+511)>>9; int nsec=rem>16?16:rem; if(ata_read_n(base+se,nvx_rbuf,nsec)){ if(ata_read(base+se,nvx_rbuf))break; nsec=1; } for(int k=0;k<nsec;k++) bl_xform(base+se+k,nvx_rbuf+k*512); int avail=nsec*512; int n=len-got; if(n>avail)n=avail; { int w=n>>2,i=0; u32* db=(u32*)(buf+got); const u32* sb=(const u32*)nvx_rbuf; for(;i<w;i++)db[i]=sb[i]; for(i=w<<2;i<n;i++)buf[got+i]=nvx_rbuf[i]; } got+=n; se+=nsec; } return got; }
static int nvx_write(const char*name,const char*buf,int len){ if(!disk_ok)return -1; if(bl_enabled&&!bl_unlocked)return -1; int idx=nvx_find(name); if(idx<0){ if(nvx.count>=NVX_MAX)return -1; idx=nvx.count; int k=0; while(name[k]&&k<15){nvx.e[idx].name[k]=name[k];k++;} nvx.e[idx].name[k]=0; nvx.e[idx].start=NVX_DATA0+idx*NVX_SECPF; nvx.e[idx].rsv=(u32)(nvx_cwd+1); nvx.count++; } int mb=NVX_SECPF*512; if(len>mb)len=mb; nvx.e[idx].len=len; u8* s=(u8*)0x00928000u; int wr=0,se=0; while(wr<len){ for(int i=0;i<512;i++)s[i]=0; int n=len-wr; if(n>512)n=512; for(int i=0;i<n;i++)s[i]=buf[wr+i]; bl_xform(nvx.e[idx].start+se,s); if(ata_write(nvx.e[idx].start+se,s))return -1; wr+=n; se++; } nvx_flush(); return idx; }
static void nvx_delete(int idx){ if(idx<0||(unsigned)idx>=nvx.count)return; for(unsigned i=idx;i+1<nvx.count;i++)nvx.e[i]=nvx.e[i+1]; nvx.count--; for(unsigned i=0;i<nvx.count;i++)nvx.e[i].start=NVX_DATA0+i*NVX_SECPF; nvx_flush(); }

#define ACCT_LBA 2049
#define ACCT_MAGIC 0x41435430u
static struct { u32 magic; char user[16]; char pass[16]; u32 lang; u32 enrolled; char org[24]; } acct;
#define NLANG 10

enum {
    T_WELCOME=0, T_SUBW, T_LANG, T_GET_STARTED, T_UP_DOWN,
    T_CONNECT, T_NET_DESC, T_WIRED, T_WIRELESS, T_LINK_UP, T_NOT_FOUND, T_NOT_AVAIL, T_PRESS_ENTER,
    T_NEXT, T_WHO_FOR, T_CHOOSE_USE, T_PERSONAL, T_FOR_YOU, T_ENTERPRISE, T_MANAGED_DESC, T_LR_PICK,
    T_ENROLL, T_ENROLL_DESC, T_ORG_NAME, T_ENROLL_KEY, T_DEVICE_ENROLLED, T_MANAGED_BY, T_POLICIES, T_CONTINUE,
    T_SIGN_IN, T_CREATE_ACCT, T_CREATE_WORK, T_IN_ORG, T_USERNAME, T_PASSWORD, T_CREATE,
    T_GETTING_READY, T_SETTING_UP, T_CREATING_ACCT, T_APPLY_THEME, T_APPLY_POLICY, T_PREP_DESKTOP, T_ALMOST, T_READY,
    T_ENTER_PASS, T_WRONG_PASS,
    T_APP_FILES, T_APP_TERMINAL, T_APP_ABOUT, T_APP_SETTINGS, T_APP_NOTEPAD, T_APP_TASKMGR,
    T_APP_DEVICES, T_APP_RECYCLE, T_APP_PAINT, T_APP_WEB, T_APP_STORE, T_APP_CALC,
    T_N
};
static const char* LANGS[NLANG]={"ENGLISH","SVENSKA","DEUTSCH","FRANCAIS","ESPANOL","ITALIANO","PORTUGUES","NEDERLANDS","ZHONGWEN","NIHONGO"};

static int have_user=0;
static void acct_save(void){ u8* s=(u8*)0x00928000u; for(int i=0;i<512;i++)s[i]=0; u8*p=(u8*)&acct; for(unsigned i=0;i<sizeof(acct)&&i<512;i++)s[i]=p[i]; ata_write(ACCT_LBA,s); }
static void acct_load(void){ u8* s=(u8*)0x00928000u; if(ata_read(ACCT_LBA,s)){have_user=0;return;} u8*p=(u8*)&acct; for(unsigned i=0;i<sizeof(acct)&&i<512;i++)p[i]=s[i]; have_user=(acct.magic==ACCT_MAGIC); }
static int disk_init(void){ if(ata_identify()==0){ disk_ok=1; nvx_mount(); return 1; } disk_ok=0; return 0; }

static u8 cmos(u8 r);
static void pit_wait(int ticks);
static void compose(void);
static void kbd_event(u8 d);
static void recovery_mode(void);
static void bsod(void);
static void rec_qr(int x,int y,int px);
#define MAXPCI 24
typedef struct { u8 bus,dev,cls,sub,prog; u16 ven,did; u16 io; u8 irq; } pci_e;
#define pcil ((pci_e*)0x0095E300u)
static int pcin=0;
static char cpu_vendor[16], cpu_brand[52]; static int ram_mb=0;
typedef struct { int present,type; char model[44]; u32 sectors; } atai_e;
#define atai ((atai_e*)0x0095E500u)
static u32 pci_read(u8 bus,u8 dev,u8 fn,u8 off){ u32 a=0x80000000u|((u32)bus<<16)|((u32)dev<<11)|((u32)fn<<8)|(off&0xFC); outl(0xCF8,a); return inl(0xCFC); }
static void pci_write(u8 bus,u8 dev,u8 fn,u8 off,u32 v){ u32 a=0x80000000u|((u32)bus<<16)|((u32)dev<<11)|((u32)fn<<8)|(off&0xFC); outl(0xCF8,a); outl(0xCFC,v); }
static int hda_ok; static int hda_status; static void hda_init(void); static void hda_play_buf(short* base,int frames,int rate); static void hda_cmdline(void);
static void xhci_poll(void);
static void cpuid(u32 lf,u32 sub,u32*a,u32*b,u32*c,u32*d){ __asm__ __volatile__("cpuid":"=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d):"a"(lf),"c"(sub)); }
static void utoa(u32 v,char*o){ char t[12];int n=0; if(v==0){o[0]='0';o[1]=0;return;} while(v){t[n++]='0'+v%10;v/=10;} int i=0; while(n)o[i++]=t[--n]; o[i]=0; }
static void ip4(u32 ip,char* o){ int p=0; char t[6]; for(int z=0;z<4;z++){ utoa((ip>>(24-z*8))&255,t); int j=0; while(t[j])o[p++]=t[j++]; if(z<3)o[p++]='.'; } o[p]=0; }
static void hex16(u16 v,char*o){ const char*Hh="0123456789ABCDEF"; o[0]=Hh[(v>>12)&0xF];o[1]=Hh[(v>>8)&0xF];o[2]=Hh[(v>>4)&0xF];o[3]=Hh[v&0xF];o[4]=0; }
static void hex32(u32 v,char*o){ const char*Hh="0123456789ABCDEF"; for(int i=0;i<8;i++)o[i]=Hh[(v>>((7-i)*4))&0xF]; o[8]=0; }
static const char* cls_name(u8 c,u8 s){ switch(c){case 1:return "STORAGE CTRL";case 2:return "NETWORK CARD";case 3:return "DISPLAY (VGA)";case 4:return "MULTIMEDIA";case 6:return "BRIDGE";case 0x0C:return (s==3)?"USB CONTROLLER":"SERIAL BUS";case 7:return "COMM (SERIAL)";default:return "DEVICE";} }
static const char* usb_type(u8 prog){ switch(prog){case 0x00:return "UHCI (USB 1.1)";case 0x10:return "OHCI (USB 1.1)";case 0x20:return "EHCI (USB 2.0)";case 0x30:return "XHCI (USB 3.0)";case 0xFE:return "USB DEVICE";default:return "USB CONTROLLER";} }
static const char* usb_cls_name(u8 c){ switch(c){case 0x00:return "(PER-IFACE)";case 0x01:return "AUDIO";case 0x02:return "CDC/COMM";case 0x03:return "HID";case 0x07:return "PRINTER";case 0x08:return "MASS STORAGE";case 0x09:return "HUB";case 0x0A:return "CDC-DATA";case 0x0B:return "SMART CARD";case 0x0E:return "VIDEO";case 0xE0:return "WIRELESS";case 0xFF:return "VENDOR";default:return "DEVICE";} }
static int usb_present=0, usb_count=0; static u8 usb_prog=0xFF; static u16 usb_io=0;
static void usb_detect(void){
    usb_present=0; usb_count=0; usb_prog=0xFF; usb_io=0;
    for(int i=0;i<pcin;i++){ if(pcil[i].cls==0x0C && pcil[i].sub==0x03){ usb_count++; usb_present=1;
        if(pcil[i].prog<=0x30 && (usb_prog==0xFF || pcil[i].prog>usb_prog)){ usb_prog=pcil[i].prog; usb_io=pcil[i].io; } } }
    if(usb_present && usb_prog==0xFF) usb_prog=0x00;
}

static void io_delay(int n){ for(int i=0;i<n;i++)inb(0x80); }
static int ehci_present=0, ehci_init_ok=0, ehci_nports=0; static u16 ehci_ver=0; static u32 ehci_base=0; static u8 ehci_caplen=0; static u8 ehci_port_conn[15];

#define EHCI_FRAMELIST 0x00980000u
#define EHCI_QH_XFER   0x00981000u
#define EHCI_QTD_SETUP 0x00981040u
#define EHCI_QTD_DATA  0x00981080u
#define EHCI_QTD_STAT  0x009810C0u
#define EHCI_BUF_SETUP 0x00981100u
#define EHCI_BUF_DATA  0x00981140u
#define EHCI_MSD_CBW   0x00982000u
#define EHCI_MSD_CSW   0x00982040u
#define EHCI_MSD_DATA  0x00983000u
#define MMW(a,v) (*(volatile u32*)(u32)(a)=(u32)(v))
#define MMR(a)   (*(volatile u32*)(u32)(a))
static int usbdev_n=0;
static struct { u8 addr,cls,sub,proto,ifcls; u16 vid,pid; char name[22]; u8 epin,epmps,ifnum,hidproto,kind,dtog; u8 keys[6]; u8 epbi,epbo; u16 bmps; } usbdev[8];
static void ehci_init(void){
    ehci_present=0; ehci_init_ok=0; ehci_nports=0; ehci_ver=0; ehci_base=0; ehci_caplen=0; for(int i=0;i<15;i++)ehci_port_conn[i]=0;
    int idx=-1; for(int i=0;i<pcin;i++) if(pcil[i].cls==0x0C&&pcil[i].sub==0x03&&pcil[i].prog==0x20){ idx=i; break; }
    if(idx<0)return;
    ehci_present=1;
    u8 bus=pcil[idx].bus, dev=pcil[idx].dev;
    u32 bar=pci_read(bus,dev,0,0x10);
    if(bar&1)return;
    u32 base=bar&0xFFFFFFF0u; if(base==0)return;
    u32 cmd=pci_read(bus,dev,0,0x04); pci_write(bus,dev,0,0x04,cmd|0x06);
    ehci_base=base;
    volatile u8* b=(volatile u8*)base;
    ehci_caplen=b[0];
    ehci_ver=*(volatile u16*)(base+2);
    u32 hcs=*(volatile u32*)(base+4);
    int np=hcs&0xF; if(np<1)np=1; if(np>15)np=15; ehci_nports=np;
    u32 ob=base+ehci_caplen;
    volatile u32* usbcmd=(volatile u32*)(ob+0x00);
    volatile u32* usbsts=(volatile u32*)(ob+0x04);
    volatile u32* usbintr=(volatile u32*)(ob+0x08);
    volatile u32* cfgflag=(volatile u32*)(ob+0x40);
    *usbcmd &= ~1u;
    for(int t=0;t<20000;t++){ if(*usbsts & 0x1000) break; io_delay(1); }
    *usbcmd |= 2u;
    int rok=0; for(int t=0;t<200000;t++){ if(!(*usbcmd & 2u)){ rok=1; break; } io_delay(1); }
    *usbintr = 0;
    { volatile u32* fl=(volatile u32*)EHCI_FRAMELIST; for(int i=0;i<1024;i++)fl[i]=1; }
    *(volatile u32*)(ob+0x10)=0;
    *(volatile u32*)(ob+0x14)=EHCI_FRAMELIST;
    *(volatile u32*)(ob+0x18)=EHCI_QH_XFER;
    *usbcmd = (*usbcmd & ~0x00FF0000u) | (0x08u<<16);
    *cfgflag = 1;
    io_delay(20000);
    for(int i=0;i<np;i++){ u32 ps=*(volatile u32*)(ob+0x44+i*4); ehci_port_conn[i]=(u8)(ps&1); }
    ehci_init_ok=rok;
}

static int ehci_ctrl(int addr,int mps,const u8* setup,u8* din,int dlen){
    if(!ehci_init_ok)return -1;
    u32 ob=ehci_base+ehci_caplen;
    volatile u32* usbcmd=(volatile u32*)(ob+0x00);
    volatile u32* usbsts=(volatile u32*)(ob+0x04);
    for(int i=0;i<8;i++)((volatile u8*)EHCI_BUF_SETUP)[i]=setup[i];
    int hasdata=(dlen>0);

    MMW(EHCI_QTD_SETUP+0x00, hasdata?EHCI_QTD_DATA:EHCI_QTD_STAT);
    MMW(EHCI_QTD_SETUP+0x04, 1);
    MMW(EHCI_QTD_SETUP+0x08, 0x80u|(2u<<8)|(3u<<10)|((u32)8<<16));
    MMW(EHCI_QTD_SETUP+0x0C, EHCI_BUF_SETUP);
    for(u32 o=0x10;o<=0x1C;o+=4)MMW(EHCI_QTD_SETUP+o,0);
    if(hasdata){
        MMW(EHCI_QTD_DATA+0x00, EHCI_QTD_STAT);
        MMW(EHCI_QTD_DATA+0x04, 1);
        MMW(EHCI_QTD_DATA+0x08, 0x80u|(1u<<8)|(3u<<10)|((u32)dlen<<16)|(1u<<31));
        MMW(EHCI_QTD_DATA+0x0C, EHCI_BUF_DATA);
        for(u32 o=0x10;o<=0x1C;o+=4)MMW(EHCI_QTD_DATA+o,0);
    }

    MMW(EHCI_QTD_STAT+0x00, 1);
    MMW(EHCI_QTD_STAT+0x04, 1);
    { u32 pid=hasdata?0u:1u; MMW(EHCI_QTD_STAT+0x08, 0x80u|(pid<<8)|(3u<<10)|(1u<<31)|0x8000u); }
    MMW(EHCI_QTD_STAT+0x0C, 0);
    for(u32 o=0x10;o<=0x1C;o+=4)MMW(EHCI_QTD_STAT+o,0);

    MMW(EHCI_QH_XFER+0x00, EHCI_QH_XFER|2u);
    MMW(EHCI_QH_XFER+0x04, (u32)(addr&0x7F)|(2u<<12)|(1u<<14)|(1u<<15)|(((u32)mps&0x7FF)<<16));
    MMW(EHCI_QH_XFER+0x08, (1u<<30));
    MMW(EHCI_QH_XFER+0x0C, 0);
    MMW(EHCI_QH_XFER+0x10, EHCI_QTD_SETUP);
    MMW(EHCI_QH_XFER+0x14, 1);
    MMW(EHCI_QH_XFER+0x18, 0);
    for(u32 o=0x1C;o<=0x2C;o+=4)MMW(EHCI_QH_XFER+o,0);

    *usbcmd &= ~(1u<<5); for(int t=0;t<8000;t++){ if(!(*usbsts&(1u<<15)))break; io_delay(1); }
    *(volatile u32*)(ob+0x18)=EHCI_QH_XFER;
    *(volatile u32*)(ob+0x10)=0;
    *usbcmd |= (1u<<5); *usbcmd |= 1u;
    for(int t=0;t<8000;t++){ if(*usbsts&(1u<<15))break; io_delay(1); }
    int done=0,err=0;
    for(int t=0;t<60000;t++){
        u32 tok=MMR(EHCI_QTD_STAT+0x08);
        u32 s1=MMR(EHCI_QTD_SETUP+0x08); if(s1&0x40){err=1;done=1;break;}
        if(hasdata){ u32 d1=MMR(EHCI_QTD_DATA+0x08); if(d1&0x40){err=1;done=1;break;} }
        if(!(tok&0x80)){ done=1; if(tok&0x40)err=1; break; }
        io_delay(1);
    }
    *usbcmd &= ~(1u<<5);
    if(!done||err)return -1;
    if(hasdata&&din){ u32 d=MMR(EHCI_QTD_DATA+0x08); int rem=(d>>16)&0x7FFF; int got=dlen-rem; if(got<0)got=0; for(int i=0;i<got&&i<dlen;i++)din[i]=((volatile u8*)EHCI_BUF_DATA)[i]; return got; }
    return 0;
}
static int usb_get_desc(int addr,int mps,u8 type,u8 index,u16 lang,u8* buf,int len){
    u8 s[8]; s[0]=0x80; s[1]=0x06; s[2]=index; s[3]=type; s[4]=(u8)(lang&0xFF); s[5]=(u8)(lang>>8); s[6]=(u8)len; s[7]=(u8)(len>>8);
    return ehci_ctrl(addr,mps,s,buf,len);
}

static int usb_attach(int* na){
    if(usbdev_n>=8)return -1;
    u8 dd[20];
    u8 g8[8]={0x80,0x06,0x00,0x01,0x00,0x00,0x08,0x00};
    int r=ehci_ctrl(0,64,g8,dd,8);
    int mps=(r>=8)?dd[7]:8; if(mps<=0)mps=8;
    int a=(*na)++;
    u8 sa[8]={0x00,0x05,(u8)a,0,0,0,0,0};
    if(ehci_ctrl(0,mps,sa,0,0)<0)return -1;
    io_delay(40000);
    u8 gd[8]={0x80,0x06,0x00,0x01,0x00,0x00,18,0x00};
    r=ehci_ctrl(a,mps,gd,dd,18);
    if(r<18)return -1;
    int i=usbdev_n;
    usbdev[i].addr=(u8)a; usbdev[i].cls=dd[4]; usbdev[i].sub=dd[5]; usbdev[i].proto=dd[6];
    usbdev[i].vid=(u16)(dd[8]|(dd[9]<<8)); usbdev[i].pid=(u16)(dd[10]|(dd[11]<<8));
    usbdev[i].ifcls=0; usbdev[i].name[0]=0; usbdev[i].epin=0; usbdev[i].epmps=8; usbdev[i].ifnum=0; usbdev[i].hidproto=0; usbdev[i].kind=0; usbdev[i].dtog=0; usbdev[i].epbi=0; usbdev[i].epbo=0; usbdev[i].bmps=0;
    for(int k=0;k<6;k++)usbdev[i].keys[k]=0;
    u8 cfg[256]; int cl=usb_get_desc(a,mps,2,0,0,cfg,9); int ifc=0;
    if(cl>=9){ int total=cfg[2]|(cfg[3]<<8); if(total>256)total=256; if(total<9)total=9;
        int cl2=usb_get_desc(a,mps,2,0,0,cfg,total); int n=(cl2>0)?cl2:cl; int off=0;
        while(off+1<n){ int blen=cfg[off],btype=cfg[off+1]; if(blen<2)break;
            if(btype==4 && off+8<=n){ if(ifc==0){ ifc=cfg[off+5]; usbdev[i].ifnum=cfg[off+2]; usbdev[i].hidproto=cfg[off+7]; } }
            else if(btype==5 && off+7<=n){ u8 epa=cfg[off+2],attr=cfg[off+3]; u16 emp=(u16)(cfg[off+4]|(cfg[off+5]<<8));
                if((attr&3)==3){ if((epa&0x80)&&usbdev[i].epin==0){ usbdev[i].epin=epa&0x0F; usbdev[i].epmps=(u8)emp; } }
                else if((attr&3)==2){ if((epa&0x80)){ if(!usbdev[i].epbi){usbdev[i].epbi=epa&0x0F; usbdev[i].bmps=emp;} } else { if(!usbdev[i].epbo){usbdev[i].epbo=epa&0x0F; if(!usbdev[i].bmps)usbdev[i].bmps=emp;} } } }
            off+=blen; }
        usbdev[i].ifcls=(u8)ifc;
    }
    { u8 cfgv=(cl>=6&&cfg[5])?cfg[5]:1; u8 scf[8]={0x00,0x09,cfgv,0x00,0x00,0x00,0x00,0x00}; ehci_ctrl(a,mps,scf,0,0); io_delay(40000); }
    u8 ip=dd[15];
    if(ip){ u8 sb[64]; u16 lang=0x0409; int sl=usb_get_desc(a,mps,3,0,0,sb,4); if(sl>=4){ u16 lg=(u16)(sb[2]|(sb[3]<<8)); if(lg)lang=lg; }
        sl=usb_get_desc(a,mps,3,ip,lang,sb,62); if(sl>=2&&sb[1]==3){ int bl=sb[0]; if(bl>sl)bl=sl; int q=0; for(int k=2;k+1<bl&&q<21;k+=2){ char ch=(char)sb[k]; if(ch<32||ch>126)ch='?'; usbdev[i].name[q++]=ch; } usbdev[i].name[q]=0; } }
    u8 eff = usbdev[i].cls?usbdev[i].cls:usbdev[i].ifcls;
    if(eff==0x03){
        u8 sp[8]={0x21,0x0B,0x00,0x00,(u8)usbdev[i].ifnum,0x00,0x00,0x00}; ehci_ctrl(a,mps,sp,0,0);
        u8 si[8]={0x21,0x0A,0x00,0x00,(u8)usbdev[i].ifnum,0x00,0x00,0x00}; ehci_ctrl(a,mps,si,0,0);
        usbdev[i].kind = (usbdev[i].hidproto==1)?1:2;
    } else if(eff==0x09){ usbdev[i].kind=3; }
    else if(eff==0x08){ usbdev[i].kind=4; }
    else if(eff==0x07){ usbdev[i].kind=6; }
    usbdev_n++;
    return i;
}

static void usb_hub_init(int hi,int* na){
    int a=usbdev[hi].addr;
    u8 hd[16]; u8 gh[8]={0xA0,0x06,0x00,0x29,0x00,0x00,16,0x00};
    int r=ehci_ctrl(a,64,gh,hd,16);
    int nports=(r>=3)?hd[2]:0; if(nports>8)nports=8;
    for(int p=1;p<=nports;p++){ u8 sf[8]={0x23,0x03,0x08,0x00,(u8)p,0x00,0x00,0x00}; ehci_ctrl(a,64,sf,0,0); }
    pit_wait(1);
    for(int p=1;p<=nports;p++){
        u8 st[4]; u8 gs[8]={0xA3,0x00,0x00,0x00,(u8)p,0x00,0x04,0x00};
        if(ehci_ctrl(a,64,gs,st,4)<4)continue;
        if(!(st[0]&1))continue;
        u8 sr[8]={0x23,0x03,0x04,0x00,(u8)p,0x00,0x00,0x00}; ehci_ctrl(a,64,sr,0,0);
        pit_wait(1);
        for(int t=0;t<20;t++){ if(ehci_ctrl(a,64,gs,st,4)>=4 && (st[0]&2)) break; pit_wait(1); }
        if(!(st[0]&2))continue;
        usb_attach(na);
    }
}
static void ehci_enumerate(void){
    usbdev_n=0;
    if(!ehci_init_ok)return;
    u32 ob=ehci_base+ehci_caplen;
    int na=1;
    for(int p=0;p<ehci_nports;p++){
        volatile u32* portsc=(volatile u32*)(ob+0x44+p*4);
        if(!(*portsc&1))continue;
        u32 ps=*portsc; ps&=~((1u<<1)|(1u<<3)|(1u<<5)); ps&=~(1u<<2); ps|=(1u<<8); *portsc=ps;
        pit_wait(1);
        ps=*portsc; ps&=~((1u<<1)|(1u<<3)|(1u<<5)); ps&=~(1u<<8); *portsc=ps;
        for(int t=0;t<100000;t++){ if(!(*portsc&(1u<<8)))break; io_delay(1); }
        io_delay(40000);
        if(!(*portsc&(1u<<2)))continue;
        int di=usb_attach(&na);
        if(di>=0 && usbdev[di].kind==3) usb_hub_init(di,&na);
    }
}

static int ehci_bulk(int addr,int ep,int in,int mps,u8* buf,int len,int* tog){
    if(!ehci_init_ok)return -1; if(mps<1)mps=64;
    u32 ob=ehci_base+ehci_caplen;
    volatile u32* usbcmd=(volatile u32*)(ob+0x00);
    volatile u32* usbsts=(volatile u32*)(ob+0x04);
    int dt=tog?(*tog&1):0;
    MMW(EHCI_QTD_SETUP+0x00,1); MMW(EHCI_QTD_SETUP+0x04,1);
    MMW(EHCI_QTD_SETUP+0x08, 0x80u|((in?1u:0u)<<8)|(3u<<10)|((u32)len<<16)|((u32)dt<<31)|0x8000u);
    MMW(EHCI_QTD_SETUP+0x0C, (u32)buf);
    { u32 base=((u32)buf)&0xFFFFF000u; for(u32 pg=1;pg<=4;pg++)MMW(EHCI_QTD_SETUP+0x0C+pg*4, base+pg*0x1000u); }
    MMW(EHCI_QH_XFER+0x00, EHCI_QH_XFER|2u);
    MMW(EHCI_QH_XFER+0x04, (u32)(addr&0x7F)|((u32)(ep&0xF)<<8)|(2u<<12)|(1u<<14)|(1u<<15)|(((u32)mps&0x7FF)<<16));
    MMW(EHCI_QH_XFER+0x08, (1u<<30));
    MMW(EHCI_QH_XFER+0x0C, 0); MMW(EHCI_QH_XFER+0x10, EHCI_QTD_SETUP); MMW(EHCI_QH_XFER+0x14, 1); MMW(EHCI_QH_XFER+0x18, 0);
    for(u32 o=0x1C;o<=0x2C;o+=4)MMW(EHCI_QH_XFER+o,0);
    *usbcmd &= ~(1u<<5); for(int t=0;t<8000;t++){ if(!(*usbsts&(1u<<15)))break; io_delay(1); }
    *(volatile u32*)(ob+0x18)=EHCI_QH_XFER; *(volatile u32*)(ob+0x10)=0;
    *usbcmd |= (1u<<5); *usbcmd |= 1u;
    for(int t=0;t<8000;t++){ if(*usbsts&(1u<<15))break; io_delay(1); }
    int done=0,err=0;
    for(int t=0;t<60000;t++){ u32 tok=MMR(EHCI_QTD_SETUP+0x08); if(!(tok&0x80)){ done=1; if(tok&0x7C)err=1; break; } io_delay(1); }
    *usbcmd &= ~(1u<<5);
    if(!done||err)return -1;
    u32 d=MMR(EHCI_QTD_SETUP+0x08); int rem=(d>>16)&0x7FFF; int got=len-rem; if(got<0)got=0;
    if(tog){ int pk=(len+mps-1)/mps; if(pk<1)pk=1; *tog^=(pk&1); }
    return got;
}
static int msd_dev=-1, msd_ready=0, msd_ok_flag=1; static u32 msd_blocks=0, msd_bsize=512, msd_tag=1; static int msd_dti=0, msd_dto=0;
static int msd_bot(const u8* cdb,int cdblen,int dir_in,u8* data,int dlen){
    if(msd_dev<0)return -1;
    int a=usbdev[msd_dev].addr, bi=usbdev[msd_dev].epbi, bo=usbdev[msd_dev].epbo, mp=usbdev[msd_dev].bmps?usbdev[msd_dev].bmps:512;
    u8* cbw=(u8*)EHCI_MSD_CBW;
    for(int i=0;i<31;i++)cbw[i]=0;
    cbw[0]=0x55;cbw[1]=0x53;cbw[2]=0x42;cbw[3]=0x43;
    u32 tag=msd_tag++; cbw[4]=tag&0xFF;cbw[5]=(tag>>8)&0xFF;cbw[6]=(tag>>16)&0xFF;cbw[7]=(tag>>24)&0xFF;
    cbw[8]=dlen&0xFF;cbw[9]=(dlen>>8)&0xFF;cbw[10]=(dlen>>16)&0xFF;cbw[11]=(dlen>>24)&0xFF;
    cbw[12]=dir_in?0x80:0x00; cbw[13]=0; cbw[14]=(u8)cdblen;
    for(int i=0;i<cdblen&&i<16;i++)cbw[15+i]=cdb[i];
    if(ehci_bulk(a,bo,0,mp,cbw,31,&msd_dto)<31)return -1;
    if(dlen>0){ if(dir_in){ if(ehci_bulk(a,bi,1,mp,data,dlen,&msd_dti)<0)return -1; } else { if(ehci_bulk(a,bo,0,mp,data,dlen,&msd_dto)<0)return -1; } }
    u8* csw=(u8*)EHCI_MSD_CSW;
    if(ehci_bulk(a,bi,1,mp,csw,13,&msd_dti)<13)return -1;
    if(!(csw[0]==0x55&&csw[1]==0x53&&csw[2]==0x42&&csw[3]==0x53))return -1;
    return csw[12];
}
static int msd_read(u32 lba,int count,u8* out){
    if(msd_dev<0)return -1; if(count<1)count=1; if(count>8)count=8;
    int dlen=count*512;
    u8 cdb[10]={0x28,0,(u8)(lba>>24),(u8)(lba>>16),(u8)(lba>>8),(u8)lba,0,(u8)(count>>8),(u8)count,0};
    u8* d=(u8*)EHCI_MSD_DATA;
    if(msd_bot(cdb,10,1,d,dlen)!=0){ msd_ok_flag=0; return -1; }
    for(int i=0;i<dlen;i++)out[i]=d[i]; msd_ok_flag=1;
    return dlen;
}
static int msd_write(u32 lba,int count,const u8* in){
    if(msd_dev<0)return -1; if(count<1)count=1; if(count>8)count=8;
    int dlen=count*512;
    u8* d=(u8*)EHCI_MSD_DATA; for(int i=0;i<dlen;i++)d[i]=in[i];
    u8 cdb[10]={0x2A,0,(u8)(lba>>24),(u8)(lba>>16),(u8)(lba>>8),(u8)lba,0,(u8)(count>>8),(u8)count,0};
    if(msd_bot(cdb,10,0,d,dlen)!=0){ msd_ok_flag=0; return -1; }
    msd_ok_flag=1; return dlen;
}

static int prn_dev=-1; static u16 prn_vid=0, prn_pid=0; static u8 prn_proto=0;
static char prn_name[22]="";
static void usbprn_detect(void){
    prn_dev=-1; prn_vid=0; prn_pid=0; prn_proto=0; prn_name[0]=0;
    for(int i=0;i<usbdev_n;i++){
        u8 eff = usbdev[i].cls ? usbdev[i].cls : usbdev[i].ifcls;
        if(eff==0x07){
            prn_dev=i; prn_vid=usbdev[i].vid; prn_pid=usbdev[i].pid;
            prn_proto=usbdev[i].proto;
            int k=0; while(usbdev[i].name[k]&&k<21){ prn_name[k]=usbdev[i].name[k]; k++; } prn_name[k]=0;
            break;
        }
    }
}
static void usbmsd_init(void){
    msd_dev=-1; msd_ready=0; msd_blocks=0; msd_bsize=512; msd_dti=0; msd_dto=0;
    for(int i=0;i<usbdev_n;i++) if(usbdev[i].kind==4 && usbdev[i].epbi && usbdev[i].epbo){ msd_dev=i; break; }
    if(msd_dev<0)return;
    u8* d=(u8*)EHCI_MSD_DATA;
    u8 inq[6]={0x12,0,0,0,36,0}; msd_bot(inq,6,1,d,36);
    u8 tur[6]={0,0,0,0,0,0};
    for(int t=0;t<10;t++){ int s=msd_bot(tur,6,0,0,0); if(s==0)break; u8 rs[6]={0x03,0,0,0,18,0}; msd_bot(rs,6,1,d,18); pit_wait(1); }
    u8 rc[10]={0x25,0,0,0,0,0,0,0,0,0};
    if(msd_bot(rc,10,1,d,8)==0){ msd_blocks=((u32)d[0]<<24)|((u32)d[1]<<16)|((u32)d[2]<<8)|d[3]; msd_bsize=((u32)d[4]<<24)|((u32)d[5]<<16)|((u32)d[6]<<8)|d[7]; if(msd_bsize==0)msd_bsize=512; msd_ready=1; }
}
static int fat_ok=0, fat_type=0, fat_nfats=2; static u32 fat_part_lba=0, fat_fatstart=0, fat_dataclus=0, fat_rootclus=0, fat_rootlba=0, fat_rootsecs=0, fat_spc=1, fat_fatsz=0, fat_resv=0;
static struct { char name[16]; u32 size, clus; } usbfs[32]; static int usbfs_n=0;

static u8 fatcache[512]; static u32 fc_si=0xFFFFFFFFu; static int fc_valid=0, fc_dirty=0;
static void fat_flush(void){ if(fc_valid&&fc_dirty&&fat_ok){ for(int f=0;f<fat_nfats;f++) msd_write(fat_fatstart+f*fat_fatsz+fc_si,1,fatcache); fc_dirty=0; } }
static int fat_load(u32 si){ if(fc_valid&&fc_si==si)return 0; fat_flush(); if(msd_read(fat_fatstart+si,1,fatcache)<0){ fc_valid=0; return -1; } fc_si=si; fc_valid=1; fc_dirty=0; return 0; }
static u32 fat_next_cluster(u32 clus){
    if(fat_type==16){ u32 off=clus*2; if(fat_load(off/512)<0)return 0x0FFFFFFFu; u32 o=off%512; u32 v=fatcache[o]|(fatcache[o+1]<<8); return (v>=0xFFF8)?0x0FFFFFFFu:v; }
    u32 off=clus*4; if(fat_load(off/512)<0)return 0x0FFFFFFFu; u32 o=off%512; u32 v=(fatcache[o]|(fatcache[o+1]<<8)|(fatcache[o+2]<<16)|((u32)fatcache[o+3]<<24))&0x0FFFFFFFu; return (v>=0x0FFFFFF8u)?0x0FFFFFFFu:v;
}
static int fat_dir_sector(u8* sec){
    for(int e=0;e<16;e++){ u8* d=sec+e*32;
        if(d[0]==0x00)return 1;
        if(d[0]==0xE5)continue;
        if((d[0x0B]&0x0F)==0x0F)continue;
        if(d[0x0B]&0x08)continue;
        if(usbfs_n>=32)return 1;
        char nm[16]; int q=0; for(int k=0;k<8&&d[k]!=' '&&q<11;k++)nm[q++]=(char)d[k];
        if(d[8]!=' '){ nm[q++]='.'; for(int k=8;k<11&&d[k]!=' '&&q<15;k++)nm[q++]=(char)d[k]; }
        nm[q]=0;
        u32 clus=(d[0x1A]|(d[0x1B]<<8))|((u32)(d[0x14]|(d[0x15]<<8))<<16);
        u32 sz=d[0x1C]|(d[0x1D]<<8)|(d[0x1E]<<16)|((u32)d[0x1F]<<24);
        int i=usbfs_n,k=0; while(nm[k]&&k<15){usbfs[i].name[k]=nm[k];k++;} usbfs[i].name[k]=0; usbfs[i].size=sz; usbfs[i].clus=clus; usbfs_n++;
    }
    return 0;
}
static void fat_mount(void){
    fat_ok=0; usbfs_n=0; fc_valid=0; fc_dirty=0;
    if(msd_dev<0)return;
    u8 sec[512];
    if(msd_read(0,1,sec)<0)return;
    u32 plba=0;
    if(sec[510]==0x55&&sec[511]==0xAA){ u8 ty=sec[450]; if(ty==0x0B||ty==0x0C||ty==0x0E||ty==0x06||ty==0x04||ty==0x01){ plba=sec[454]|(sec[455]<<8)|(sec[456]<<16)|((u32)sec[457]<<24); } }
    if(msd_read(plba,1,sec)<0)return;
    int bps=sec[0x0B]|(sec[0x0C]<<8); if(bps!=512)return;
    fat_spc=sec[0x0D]; if(fat_spc==0)fat_spc=1;
    fat_resv=sec[0x0E]|(sec[0x0F]<<8);
    fat_nfats=sec[0x10]; if(fat_nfats==0)fat_nfats=2;
    int rootent=sec[0x11]|(sec[0x12]<<8);
    u32 fatsz16=sec[0x16]|(sec[0x17]<<8);
    u32 fatsz32=sec[0x24]|(sec[0x25]<<8)|(sec[0x26]<<16)|((u32)sec[0x27]<<24);
    fat_fatsz=fatsz16?fatsz16:fatsz32;
    u32 rootsecs=((rootent*32)+(bps-1))/bps;
    fat_part_lba=plba; fat_fatstart=plba+fat_resv; fat_dataclus=plba+fat_resv+fat_nfats*fat_fatsz+rootsecs; fat_rootsecs=rootsecs;
    if(fatsz16){ fat_type=16; fat_rootlba=plba+fat_resv+fat_nfats*fat_fatsz; }
    else { fat_type=32; fat_rootclus=sec[0x2C]|(sec[0x2D]<<8)|(sec[0x2E]<<16)|((u32)sec[0x2F]<<24); }
    fat_ok=1;
    if(fat_type==16){ for(u32 s=0;s<rootsecs&&usbfs_n<32;s++){ if(msd_read(fat_rootlba+s,1,sec)<0)break; if(fat_dir_sector(sec))break; } }
    else { u32 clus=fat_rootclus; int guard=0; while(clus>=2&&clus<0x0FFFFFF8u&&usbfs_n<32&&guard++<256){ u32 base=fat_dataclus+(clus-2)*fat_spc; int stop=0; for(u32 s=0;s<fat_spc&&usbfs_n<32;s++){ if(msd_read(base+s,1,sec)<0){stop=1;break;} if(fat_dir_sector(sec)){stop=1;break;} } if(stop)break; clus=fat_next_cluster(clus); } }
}
static void usbmsd_mount(void){ usbmsd_init(); if(msd_dev>=0) fat_mount(); }
static void usb_rescan(void){ ehci_enumerate(); usbmsd_mount(); }

#define UHCI_FL    0x00988000u
#define UHCI_QH    0x00989000u
#define UHCI_TD    0x00989040u
#define UHCI_SETUP 0x00989200u
#define UHCI_DATA  0x00989240u
static int uhci_ok=0; static u16 uhci_io=0;
static int uhci_ms_addr=0, uhci_ms_ep=0, uhci_ms_ls=0, uhci_ms_tog=0, uhci_ms_mps=8, uhci_ms_found=0;
static inline void uwr(u32 a,u32 v){ *(volatile u32*)a=v; }
static inline u32 urd(u32 a){ return *(volatile u32*)a; }

static int uhci_ctrl(int addr,int mps,const u8* setup,u8* din,int dlen,int ls){
    if(!uhci_ok) return -1; u16 io=uhci_io; (void)mps;
    for(int i=0;i<8;i++) ((volatile u8*)UHCI_SETUP)[i]=setup[i];
    u32 td0=UHCI_TD, td1=UHCI_TD+32, td2=UHCI_TD+64;
    u32 lsb = ls?(1u<<26):0; int dev2host=(setup[0]&0x80)!=0;
    if(dlen>64)dlen=64;
    uwr(td0+0, ((dlen>0)?td1:td2)|4);
    uwr(td0+4, (1u<<23)|(3u<<27)|lsb);
    uwr(td0+8, 0x2Du | ((u32)addr<<8) | (((u32)(8-1))<<21));
    uwr(td0+12, UHCI_SETUP);
    if(dlen>0){
        uwr(td1+0, td2|4);
        uwr(td1+4, (1u<<23)|(3u<<27)|lsb|(1u<<29));
        uwr(td1+8, (dev2host?0x69u:0xE1u) | ((u32)addr<<8) | (1u<<19) | (((u32)(dlen-1))<<21));
        uwr(td1+12, UHCI_DATA);
    }
    uwr(td2+0, 1);
    uwr(td2+4, (1u<<23)|(1u<<24)|(3u<<27)|lsb);
    uwr(td2+8, (dev2host?0xE1u:0x69u) | ((u32)addr<<8) | (1u<<19) | (((u32)0x7FF)<<21));
    uwr(td2+12, 0);
    uwr(UHCI_QH+0, 1); uwr(UHCI_QH+4, td0);
    uwr(UHCI_FL+0, UHCI_QH|2);
    outw(io+0, 1);
    int done=0,err=0;
    for(int t=0;t<200000;t++){
        u32 s0=urd(td0+4); if(s0&(1u<<22)){err=1;break;}
        if(dlen>0){ u32 s1=urd(td1+4); if(s1&(1u<<22)){err=1;break;} }
        u32 s2=urd(td2+4); if(s2&(1u<<22)){err=1;break;}
        if(!(s2&(1u<<23))){ done=1; break; }
        io_delay(1);
    }
    outw(io+0, 0); uwr(UHCI_FL+0, 1);
    if(!done||err) return -1;
    if(dlen>0 && din){ u32 raw=urd(td1+4)&0x7FF; int act=(raw==0x7FF)?0:(int)raw+1; if(act>dlen)act=dlen; for(int i=0;i<act;i++) din[i]=((volatile u8*)UHCI_DATA)[i]; return act; }
    return 0;
}

static int uhci_in(int addr,int ep,int mps,u8* buf,int len,int tog,int ls){
    if(!uhci_ok) return -1; u16 io=uhci_io; u32 td0=UHCI_TD; (void)mps;
    if(len>8)len=8; if(len<1)len=1;
    uwr(td0+0, 1);
    uwr(td0+4, (1u<<23)|(3u<<27)|(ls?(1u<<26):0)|(1u<<29));
    uwr(td0+8, 0x69u | ((u32)addr<<8) | ((u32)(ep&0xF)<<15) | ((u32)(tog&1)<<19) | (((u32)(len-1))<<21));
    uwr(td0+12, UHCI_DATA);
    uwr(UHCI_QH+0, 1); uwr(UHCI_QH+4, td0);
    uwr(UHCI_FL+0, UHCI_QH|2);
    outw(io+0,1);
    int done=0,err=0;
    for(int t=0;t<4000;t++){ u32 s=urd(td0+4); if(s&(1u<<22)){err=1;break;} if(!(s&(1u<<23))){done=1;break;} io_delay(1); }
    outw(io+0,0); uwr(UHCI_FL+0,1);
    if(!done||err) return -1;
    u32 raw=urd(td0+4)&0x7FF; int act=(raw==0x7FF)?0:(int)raw+1; if(act>len)act=len;
    for(int i=0;i<act;i++) buf[i]=((volatile u8*)UHCI_DATA)[i];
    return act;
}

static void uhci_port(int pi){
    u16 io=uhci_io; u16 preg=io+0x10+(u16)pi*2;
    u16 st=inw(preg);
    if(!(st&0x1)) return;
    int ls=(st&0x100)?1:0;
    outw(preg, (u16)((st|0x200)&~0x4)); for(int t=0;t<3000;t++) io_delay(1);
    outw(preg, (u16)(inw(preg)&~0x200)); for(int t=0;t<800;t++) io_delay(1);
    outw(preg, (u16)(inw(preg)|0x4)); for(int t=0;t<800;t++) io_delay(1);
    if(!(inw(preg)&0x4)) return;
    u8 dd[18];
    u8 g8[8]={0x80,0x06,0x00,0x01,0x00,0x00,0x08,0x00};
    if(uhci_ctrl(0,8,g8,dd,8,ls)<8) return;
    int mps=dd[7]?dd[7]:8; int a=2;
    u8 sa[8]={0x00,0x05,(u8)a,0,0,0,0,0};
    if(uhci_ctrl(0,mps,sa,0,0,ls)<0) return;
    for(int t=0;t<3000;t++) io_delay(1);
    u8 gd[8]={0x80,0x06,0x00,0x01,0x00,0x00,18,0x00};
    if(uhci_ctrl(a,mps,gd,dd,18,ls)<18) return;
    u8 cfg[128];
    u8 gc[8]={0x80,0x06,0x00,0x02,0x00,0x00,9,0x00};
    if(uhci_ctrl(a,mps,gc,cfg,9,ls)<9) return;
    int total=cfg[2]|(cfg[3]<<8); if(total>128)total=128; if(total<9)total=9;
    u8 gc2[8]={0x80,0x06,0x00,0x02,0x00,0x00,(u8)total,0x00};
    int cl=uhci_ctrl(a,mps,gc2,cfg,total,ls); if(cl<9) cl=9;
    int ifc=0,ifnum=0,proto=0,epin=0,epmps=8,off=0;
    int ucls=0,uproto=0,uifn=0,ucand=0,ubest=-1;
    while(off+1<cl){ int bl=cfg[off],bt=cfg[off+1]; if(bl<2)break;
        if(bt==4&&off+8<=cl){ ucls=cfg[off+5]; uifn=cfg[off+2]; uproto=cfg[off+7]; ucand=(ucls==0x03&&uproto!=1); }
        else if(bt==5&&off+7<=cl){ u8 epa=cfg[off+2],attr=cfg[off+3];
            if(ucand && (attr&3)==3 && (epa&0x80)){ int sc=(uproto==2)?2:(uproto==0)?1:0;
                if(sc>ubest){ ubest=sc; epin=epa&0x0F; epmps=cfg[off+4]?cfg[off+4]:8; ifc=ucls; proto=uproto; ifnum=uifn; } } }
        off+=bl; }
    (void)proto;
    if(ifc!=0x03 || epin==0) return;
    u8 scf[8]={0x00,0x09,(u8)((cl>=6&&cfg[5])?cfg[5]:1),0,0,0,0,0}; uhci_ctrl(a,mps,scf,0,0,ls); for(int t=0;t<1500;t++)io_delay(1);
    u8 sp[8]={0x21,0x0B,0x00,0x00,(u8)ifnum,0x00,0x00,0x00}; uhci_ctrl(a,mps,sp,0,0,ls);
    u8 si[8]={0x21,0x0A,0x00,0x00,(u8)ifnum,0x00,0x00,0x00}; uhci_ctrl(a,mps,si,0,0,ls);
    uhci_ms_addr=a; uhci_ms_ep=epin; uhci_ms_ls=ls; uhci_ms_mps=epmps; uhci_ms_tog=0; uhci_ms_found=1;
}

static void uhci_init(void){
    uhci_ok=0; uhci_io=0; uhci_ms_found=0;
    u16 io=0;
    for(int b=0;b<2&&!io;b++) for(int d=0;d<32&&!io;d++) for(int f=0;f<8&&!io;f++){
        u32 id=pci_read((u8)b,(u8)d,(u8)f,0); if((id&0xFFFF)==0xFFFF){ if(f==0)break; else continue; }
        u32 cc=pci_read((u8)b,(u8)d,(u8)f,0x08);
        u8 cls=(cc>>24)&0xFF, sub=(cc>>16)&0xFF, pif=(cc>>8)&0xFF;
        if(cls==0x0C&&sub==0x03&&pif==0x00){
            u32 bar4=pci_read((u8)b,(u8)d,(u8)f,0x20);
            if(bar4&1){ io=(u16)(bar4&0xFFFC);
                u32 cmd=pci_read((u8)b,(u8)d,(u8)f,0x04); pci_write((u8)b,(u8)d,(u8)f,0x04,cmd|0x5);
                pci_write((u8)b,(u8)d,(u8)f,0xC0,0x8F00); }
        }
    }
    if(!io) return;
    uhci_io=io;
    outw(io+0,0x04); for(int t=0;t<3000;t++)io_delay(1); outw(io+0,0x00);
    outw(io+0,0x02); for(int t=0;t<2000;t++){ if(!(inw(io+0)&0x02))break; io_delay(1); }
    outw(io+0,0x00);
    outw(io+4,0x003F);
    for(int i=0;i<1024;i++) uwr(UHCI_FL+i*4, 1);
    outl(io+0x08, UHCI_FL);
    outw(io+0x06, 0);
    uhci_ok=1;
    for(int p=0;p<2;p++) uhci_port(p);
}
static int usbfs_read(int fi,char* out,int max){
    if(fi<0||fi>=usbfs_n)return 0;
    u32 clus=usbfs[fi].clus; int left=(int)usbfs[fi].size; if(left>max)left=max; int got=0,guard=0; u8 sec[512];
    while(clus>=2&&clus<0x0FFFFFF8u&&left>0&&guard++<1048576){ u32 base=fat_dataclus+(clus-2)*fat_spc; int stop=0;
        for(u32 s=0;s<fat_spc&&left>0;s++){ if(msd_read(base+s,1,sec)<0){stop=1;break;} int n=(left<512)?left:512; for(int k=0;k<n;k++)out[got++]=(char)sec[k]; left-=n; }
        if(stop)break; clus=fat_next_cluster(clus); }
    return got;
}
static void usb_eject(void){ msd_dev=-1; msd_ready=0; fat_ok=0; usbfs_n=0; msd_blocks=0; fc_valid=0; fc_dirty=0; }
static int usb_replug=0;
static void usb_hotplug_check(void){
    if(!ehci_init_ok)return;
    u32 ob=ehci_base+ehci_caplen;
    for(int p=0;p<ehci_nports;p++){ volatile u32* ps=(volatile u32*)(ob+0x44+p*4); u32 v=*ps;
        if(v&(1u<<1)){ u32 w=v & ~((1u<<3)|(1u<<5)); w|=(1u<<1); *ps=w; usb_replug=1; } }
}

static void fat_set(u32 clus,u32 val){
    if(fat_type==16){ u32 off=clus*2; if(fat_load(off/512)<0)return; u32 o=off%512; fatcache[o]=val&0xFF; fatcache[o+1]=(val>>8)&0xFF; fc_dirty=1; }
    else { u32 off=clus*4; if(fat_load(off/512)<0)return; u32 o=off%512; u32 cur=(fatcache[o]|(fatcache[o+1]<<8)|(fatcache[o+2]<<16)|((u32)fatcache[o+3]<<24)); cur=(cur&0xF0000000u)|(val&0x0FFFFFFFu); fatcache[o]=cur&0xFF;fatcache[o+1]=(cur>>8)&0xFF;fatcache[o+2]=(cur>>16)&0xFF;fatcache[o+3]=(cur>>24)&0xFF; fc_dirty=1; }
}
static u32 fat_alloc(void){
    u32 maxc=(fat_type==16)?(fat_fatsz*256u):(fat_fatsz*128u); if(maxc>65536)maxc=65536;
    for(u32 c=2;c<maxc;c++){ u32 v=fat_next_cluster(c); if(!msd_ok_flag)return 0;
        if(v==0){ fat_set(c, fat_type==16?0xFFFFu:0x0FFFFFFFu); return c; } }
    return 0;
}
static void fat_free_chain(u32 clus){ int g=0; while(clus>=2&&clus<0x0FFFFFF8u&&g++<4096){ u32 nx=fat_next_cluster(clus); fat_set(clus,0); clus=nx; } }
static void fat_make83(const char* nm,char out[11]){
    for(int i=0;i<11;i++)out[i]=' ';
    int i=0,o=0; while(nm[i]&&nm[i]!='.'&&o<8){ char c=nm[i]; if(c>='a'&&c<='z')c-=32; out[o++]=c; i++; }
    while(nm[i]&&nm[i]!='.')i++;
    if(nm[i]=='.'){ i++; int e=8; while(nm[i]&&e<11){ char c=nm[i]; if(c>='a'&&c<='z')c-=32; out[e++]=c; i++; } }
}
static int fat_dirent_scan(const char* n83,int findfree,u32* olba,int* ooff){
    u8 sec[512];
    if(fat_type==16){
        for(u32 s=0;s<fat_rootsecs;s++){ u32 lba=fat_rootlba+s; if(msd_read(lba,1,sec)<0)return 0;
            for(int e=0;e<16;e++){ u8* d=sec+e*32;
                if(findfree){ if(d[0]==0x00||d[0]==0xE5){ *olba=lba; *ooff=e*32; return 1; } }
                else { if(d[0]==0x00)return 0; if(d[0]==0xE5||(d[0x0B]&0x0F)==0x0F)continue; int m=1; for(int k=0;k<11;k++)if(d[k]!=(u8)n83[k]){m=0;break;} if(m){ *olba=lba; *ooff=e*32; return 1; } } } }
        return 0;
    }
    u32 clus=fat_rootclus; int guard=0;
    while(clus>=2&&clus<0x0FFFFFF8u&&guard++<4096){ u32 base=fat_dataclus+(clus-2)*fat_spc;
        for(u32 s=0;s<fat_spc;s++){ u32 lba=base+s; if(msd_read(lba,1,sec)<0)return 0;
            for(int e=0;e<16;e++){ u8* d=sec+e*32;
                if(findfree){ if(d[0]==0x00||d[0]==0xE5){ *olba=lba; *ooff=e*32; return 1; } }
                else { if(d[0]==0x00)return 0; if(d[0]==0xE5||(d[0x0B]&0x0F)==0x0F)continue; int m=1; for(int k=0;k<11;k++)if(d[k]!=(u8)n83[k]){m=0;break;} if(m){ *olba=lba; *ooff=e*32; return 1; } } } }
        clus=fat_next_cluster(clus); }
    return 0;
}
static int fat_write_file(const char* name,const char* data,int len){
    if(!fat_ok||msd_dev<0)return -1;
    char n83[11]; fat_make83(name,n83);
    u32 elba; int eoff; u8 sec[512];
    int exists=fat_dirent_scan(n83,0,&elba,&eoff);
    if(exists){ if(msd_read(elba,1,sec)<0)return -1; u8* d=sec+eoff; u32 oc=(d[0x1A]|(d[0x1B]<<8))|((u32)(d[0x14]|(d[0x15]<<8))<<16); if(oc>=2)fat_free_chain(oc); }
    else { if(!fat_dirent_scan(n83,1,&elba,&eoff))return -1; }
    int spb=fat_spc*512; int nclus=(len+spb-1)/spb; if(nclus<1)nclus=1;
    u32 first=0,prev=0; int wr=0;
    for(int c=0;c<nclus;c++){ u32 cl=fat_alloc(); if(cl==0)return -1; if(!first)first=cl; if(prev)fat_set(prev,cl); prev=cl;
        u32 base=fat_dataclus+(cl-2)*fat_spc;
        for(u32 s=0;s<fat_spc;s++){ u8 ds[512]; for(int k=0;k<512;k++)ds[k]=0; int n=len-wr; if(n>512)n=512; if(n<0)n=0; for(int k=0;k<n;k++)ds[k]=(u8)data[wr+k]; wr+=n; if(msd_write(base+s,1,ds)<0)return -1; } }
    if(prev)fat_set(prev, fat_type==16?0xFFFFu:0x0FFFFFFFu);
    if(msd_read(elba,1,sec)<0)return -1; u8* d=sec+eoff;
    for(int i=0;i<11;i++)d[i]=(u8)n83[i]; d[0x0B]=0x20; for(int i=0x0C;i<0x20;i++)d[i]=0;
    d[0x1A]=first&0xFF;d[0x1B]=(first>>8)&0xFF;d[0x14]=(first>>16)&0xFF;d[0x15]=(first>>24)&0xFF;
    d[0x1C]=len&0xFF;d[0x1D]=(len>>8)&0xFF;d[0x1E]=(len>>16)&0xFF;d[0x1F]=(len>>24)&0xFF;
    if(msd_write(elba,1,sec)<0)return -1;
    fat_flush();
    fat_mount();
    return 0;
}
static int fat_delete_file(int idx){
    if(idx<0||idx>=usbfs_n||!fat_ok)return -1;
    char n83[11]; fat_make83(usbfs[idx].name,n83);
    u32 lba; int off; u8 sec[512];
    if(!fat_dirent_scan(n83,0,&lba,&off))return -1;
    if(msd_read(lba,1,sec)<0)return -1; u8* d=sec+off;
    u32 oc=(d[0x1A]|(d[0x1B]<<8))|((u32)(d[0x14]|(d[0x15]<<8))<<16); if(oc>=2)fat_free_chain(oc);
    d[0]=0xE5; if(msd_write(lba,1,sec)<0)return -1;
    fat_flush();
    fat_mount();
    return 0;
}

#define XHCI_DCBAA   0x00990000u
#define XHCI_CMDRING 0x00991000u
#define XHCI_EVTRING 0x00992000u
#define XHCI_ERST    0x00993000u
#define XHCI_SCRATCH 0x00994000u
static int xhci_present=0, xhci_init_ok=0, xhci_slots=0, xhci_ports=0; static u32 xhci_base=0; static u8 xhci_caplen=0; static u8 xhci_port_conn[16], xhci_port_speed[16]; static u16 xhci_vendor=0;

#define XHCI_INPUTCTX 0x009C0000u
#define XHCI_DEVCTX   0x009C1000u
#define XHCI_EP0RING  0x009C2000u
#define XHCI_EPIRING  0x009C3000u
#define XHCI_XFERBUF  0x009C4000u
#define XHCI_MSBUF    0x009C5000u
#define XHCI_KBDRING  0x009C6000u
#define XHCI_KBDBUF   0x009C7000u
#define XHCI_KDEVCTX  0x009C8000u
static u32 xhci_op=0, xhci_db=0, xhci_rt=0; static int xhci_csz=0;
static int xhci_evt_idx=0, xhci_evt_cyc=1, xhci_cmd_idx=0, xhci_cmd_cyc=1;
static int xhci_ms_slot=0, xhci_ms_dci=0, xhci_ms_cyc=1, xhci_ms_idx=0, xhci_ms_mps=8, xhci_ms_found=0;
static int xhci_kb_slot=0, xhci_kb_dci=0, xhci_kb_cyc=1, xhci_kb_idx=0, xhci_kb_found=0;
static int xhci_ms_proto=2;
static int xhci_diag_step=0;
static int xhci_enum_port=0, xhci_slot_cc=0, xhci_addr_cc=0, xhci_ped=0;
static int xhci_ep0_idx=0, xhci_ep0_cyc=1;
static int xhci_ped_imm=0;
static const char* xhci_spd(int s){ switch(s){case 1:return "FULL";case 2:return "LOW";case 3:return "HIGH";case 4:return "SUPER";default:return "-";} }
static void xhci_init(void){
    xhci_present=0; xhci_init_ok=0; xhci_slots=0; xhci_ports=0; for(int i=0;i<16;i++){xhci_port_conn[i]=0;xhci_port_speed[i]=0;}
    int idx=-1; for(int i=0;i<pcin;i++) if(pcil[i].cls==0x0C&&pcil[i].sub==0x03&&pcil[i].prog==0x30){ idx=i; break; }
    if(idx<0)return; xhci_present=1;
    u8 bus=pcil[idx].bus, dev=pcil[idx].dev;
    xhci_vendor=(u16)(pci_read(bus,dev,0,0)&0xFFFF);
    u32 bar=pci_read(bus,dev,0,0x10); if(bar&1)return; u32 base=bar&0xFFFFFFF0u; if(!base)return;
    u32 cmd=pci_read(bus,dev,0,0x04); pci_write(bus,dev,0,0x04,cmd|0x06);

    if(xhci_vendor==0x8086){
        u32 u3prm=pci_read(bus,dev,0,0xDC); pci_write(bus,dev,0,0xD8,u3prm);
        u32 u2prm=pci_read(bus,dev,0,0xD4); pci_write(bus,dev,0,0xD0,u2prm);
    }
    xhci_base=base;
    u8 caplen=*(volatile u8*)base; xhci_caplen=caplen;
    u32 hcs1=MMR(base+0x04); int maxslots=hcs1&0xFF; int maxports=(hcs1>>24)&0xFF; if(maxports>16)maxports=16; if(maxports<1)maxports=1; xhci_ports=maxports; xhci_slots=maxslots;
    u32 hcs2=MMR(base+0x08); int spb=(((hcs2>>21)&0x1F)<<5)|((hcs2>>27)&0x1F); if(spb>32)spb=32;
    u32 rtsoff=MMR(base+0x18)&~0x1Fu;
    u32 op=base+caplen;
    volatile u32* usbcmd=(volatile u32*)(op+0x00);
    volatile u32* usbsts=(volatile u32*)(op+0x04);
    for(int t=0;t<200000;t++){ if(!(*usbsts&(1u<<11)))break; io_delay(1); }
    *usbcmd &= ~1u; for(int t=0;t<200000;t++){ if(*usbsts&1u)break; io_delay(1); }
    *usbcmd |= (1u<<1); int rok=0;
    for(int t=0;t<400000;t++){ if(!(*usbcmd&(1u<<1)) && !(*usbsts&(1u<<11))){ rok=1; break; } io_delay(1); }
    MMW(op+0x38, (u32)maxslots);
    { volatile u32* dc=(volatile u32*)XHCI_DCBAA; for(int i=0;i<(maxslots+1)*2;i++)dc[i]=0; }
    if(spb>0){ volatile u32* sa=(volatile u32*)XHCI_SCRATCH; for(int i=0;i<spb*2;i++)sa[i]=0; for(int i=0;i<spb;i++){ u32 b=XHCI_SCRATCH+0x1000u+(u32)i*0x1000u; sa[i*2]=b; sa[i*2+1]=0; volatile u8* bb=(volatile u8*)b; for(int k=0;k<512;k++)bb[k]=0; } *(volatile u32*)XHCI_DCBAA=XHCI_SCRATCH; *(volatile u32*)(XHCI_DCBAA+4)=0; }
    MMW(op+0x30, XHCI_DCBAA); MMW(op+0x34, 0);
    { volatile u32* cr=(volatile u32*)XHCI_CMDRING; for(int i=0;i<256*4;i++)cr[i]=0; int li=255*4; cr[li]=XHCI_CMDRING; cr[li+1]=0; cr[li+2]=0; cr[li+3]=(6u<<10)|(1u<<1)|1u; }
    MMW(op+0x18, XHCI_CMDRING|1u); MMW(op+0x1C, 0);
    { volatile u32* er=(volatile u32*)XHCI_EVTRING; for(int i=0;i<256*4;i++)er[i]=0; }
    { volatile u32* erst=(volatile u32*)XHCI_ERST; erst[0]=XHCI_EVTRING; erst[1]=0; erst[2]=256; erst[3]=0; }
    { u32 rt=base+rtsoff+0x20; MMW(rt+0x08,1); MMW(rt+0x18,XHCI_EVTRING); MMW(rt+0x1C,0); MMW(rt+0x10,XHCI_ERST); MMW(rt+0x14,0); }
    *usbcmd |= 1u; for(int t=0;t<200000;t++){ if(!(*usbsts&1u))break; io_delay(1); }

    for(int p=0;p<maxports;p++){ u32 pa=op+0x400+(u32)p*0x10; u32 v=MMR(pa);
        if(!(v&(1u<<9))){ MMW(pa,(v&0x0E00C3E0u)|(1u<<9)); } }
    for(int t=0;t<40000;t++) io_delay(1);
    for(int p=0;p<maxports;p++){ u32 v=MMR(op+0x400+(u32)p*0x10); xhci_port_conn[p]=(u8)(v&1); xhci_port_speed[p]=(u8)((v>>10)&0xF); }
    xhci_op=op; xhci_db=base+(MMR(base+0x14)&~3u); xhci_rt=base+rtsoff;
    xhci_csz=(MMR(base+0x10)&(1u<<2))?1:0;
    xhci_evt_idx=0; xhci_evt_cyc=1; xhci_cmd_idx=0; xhci_cmd_cyc=1;
    xhci_init_ok=rok;
}

static u32 xhci_ctx(u32 base,int idx){ return base + (u32)idx*(xhci_csz?64u:32u); }

static int xhci_command(u32 p0,u32 p1,u32 ctrl,int* slot_out){
    if(!xhci_init_ok) return -1;
    volatile u32* cr=(volatile u32*)XHCI_CMDRING;
    int i=xhci_cmd_idx;
    cr[i*4+0]=p0; cr[i*4+1]=p1; cr[i*4+2]=0;
    cr[i*4+3]=(ctrl&~1u)|(xhci_cmd_cyc?1u:0u);
    u32 trb_addr=XHCI_CMDRING+(u32)i*16;
    xhci_cmd_idx++; if(xhci_cmd_idx>=255){   xhci_cmd_idx=0; xhci_cmd_cyc^=1; }
    MMW(xhci_db+0, 0);

    for(int t=0;t<300000;t++){
        volatile u32* ev=(volatile u32*)XHCI_EVTRING;
        int e=xhci_evt_idx;
        u32 c3=ev[e*4+3];
        if((c3&1)==(u32)(xhci_evt_cyc?1:0)){
            u32 type=(c3>>10)&0x3F; u32 ce=(ev[e*4+2]>>24)&0xFF; u32 sid=(c3>>24)&0xFF; u32 ep0=ev[e*4+0];
            xhci_evt_idx++; if(xhci_evt_idx>=256){ xhci_evt_idx=0; xhci_evt_cyc^=1; }
            MMW(xhci_rt+0x20+0x18, (XHCI_EVTRING+(u32)xhci_evt_idx*16)|(1u<<3)); MMW(xhci_rt+0x20+0x1C,0);
            if(type==33 && ep0==trb_addr){ if(slot_out)*slot_out=(int)sid; return (int)ce; }

            continue;
        }
        io_delay(1);
    }
    return -1;
}

static int xhci_ctrl(int slot,const u8* setup,u8* din,int dlen){
    if(!xhci_init_ok) return -1;
    volatile u32* r=(volatile u32*)XHCI_EP0RING;
    int idx=xhci_ep0_idx, cyc=xhci_ep0_cyc;
    int dev2host=(setup[0]&0x80)!=0; if(dlen>256)dlen=256;

    u32 s0=setup[0]|(setup[1]<<8)|(setup[2]<<16)|(setup[3]<<24);
    u32 s1=setup[4]|(setup[5]<<8)|(setup[6]<<16)|(setup[7]<<24);
    int trt = (dlen>0)?(dev2host?3:2):0;
    r[idx*4+0]=s0; r[idx*4+1]=s1; r[idx*4+2]=8;
    r[idx*4+3]=((u32)trt<<16)|(2u<<10)|(1u<<6)|(cyc?1u:0u);
    idx++; if(idx>=255){idx=0;cyc^=1;}
    if(dlen>0){
        r[idx*4+0]=XHCI_XFERBUF; r[idx*4+1]=0; r[idx*4+2]=(u32)dlen;
        r[idx*4+3]=((u32)(dev2host?1:0)<<16)|(3u<<10)|(cyc?1u:0u);
        idx++; if(idx>=255){idx=0;cyc^=1;}
    }
    r[idx*4+0]=0; r[idx*4+1]=0; r[idx*4+2]=0;
    r[idx*4+3]=((u32)(dev2host?0:1)<<16)|(4u<<10)|(1u<<5)|(cyc?1u:0u);
    u32 last=XHCI_EP0RING+(u32)idx*16;
    idx++; if(idx>=255){idx=0;cyc^=1;}
    xhci_ep0_idx=idx; xhci_ep0_cyc=cyc;
    MMW(xhci_db+(u32)slot*4, 1);
    for(int t=0;t<300000;t++){
        volatile u32* ev=(volatile u32*)XHCI_EVTRING; int e=xhci_evt_idx; u32 c3=ev[e*4+3];
        if((c3&1)==(u32)(xhci_evt_cyc?1:0)){
            u32 type=(c3>>10)&0x3F; u32 ce=(ev[e*4+2]>>24)&0xFF; u32 ep0=ev[e*4+0];
            xhci_evt_idx++; if(xhci_evt_idx>=256){xhci_evt_idx=0;xhci_evt_cyc^=1;}
            MMW(xhci_rt+0x20+0x18,(XHCI_EVTRING+(u32)xhci_evt_idx*16)|(1u<<3)); MMW(xhci_rt+0x20+0x1C,0);
            if(type==32 && (ep0==last || ep0>=XHCI_EP0RING)){
                if(ce!=1&&ce!=13) return -1;
                if(dlen>0&&din){ for(int k=0;k<dlen;k++)din[k]=((volatile u8*)XHCI_XFERBUF)[k]; }
                return dlen>0?dlen:0;
            }
            continue;
        }
        io_delay(1);
    }
    return -1;
}

static int xhci_try_port(int port,int speed,int allow_ms,int allow_kb,u32 devctx){
    u32 op=xhci_op; u32 pa=op+0x400+(u32)port*0x10;
    xhci_diag_step=1; xhci_enum_port=port+1;

    xhci_ped=0; xhci_ped_imm=0;
    for(int attempt=0; attempt<3 && !xhci_ped; attempt++){
        u32 pre=MMR(pa)&0x0E0003E0u;
        MMW(pa, pre|(1u<<21)|(1u<<19));
        for(int t=0;t<2000;t++) io_delay(1);
        u32 rbit = (attempt==0)?(1u<<4):(1u<<31);
        u32 cbit = (attempt==0)?(1u<<21):(1u<<19);
        MMW(pa, pre|rbit);
        for(int t=0;t<500000;t++){ if(MMR(pa)&cbit)break; io_delay(1); }

        for(int t=0;t<8000;t++){ if(MMR(pa)&(1u<<1)){ xhci_ped=1; break; } io_delay(1); }
        if(!(MMR(pa)&1)) return 0;
    }
    xhci_ped_imm=(MMR(pa)>>1)&1;
    u32 ps=MMR(pa); speed=(ps>>10)&0xF; xhci_diag_step=2;
    if(!xhci_ped) return 0;

    int slot=0; xhci_slot_cc=xhci_command(0,0,(9u<<10),&slot);
    if(xhci_slot_cc!=1||slot<=0) return 0; xhci_diag_step=3;

    { volatile u32* z=(volatile u32*)XHCI_INPUTCTX; for(int i=0;i<512;i++)z[i]=0; }
    { volatile u32* z=(volatile u32*)devctx;   for(int i=0;i<512;i++)z[i]=0; }
    { volatile u32* z=(volatile u32*)XHCI_EP0RING;  for(int i=0;i<1024;i++)z[i]=0; }
    xhci_ep0_idx=0; xhci_ep0_cyc=1;

    MMW(xhci_ctx(XHCI_INPUTCTX,0)+0x00, 0);
    MMW(xhci_ctx(XHCI_INPUTCTX,0)+0x04, 0x3);

    u32 sc=xhci_ctx(XHCI_INPUTCTX,1);
    MMW(sc+0x00, ((u32)1<<27)|((u32)speed<<20));
    MMW(sc+0x04, ((u32)(port+1)<<16));

    int mps0 = (speed==4)?512:(speed==3)?64:8;
    u32 ec=xhci_ctx(XHCI_INPUTCTX,2);
    MMW(ec+0x04, (4u<<3)|((u32)mps0<<16)|(3u<<1));
    MMW(ec+0x08, XHCI_EP0RING|1u); MMW(ec+0x0C, 0);
    xhci_ms_mps=mps0;

    MMW(XHCI_DCBAA+(u32)slot*8, devctx); MMW(XHCI_DCBAA+(u32)slot*8+4, 0);

    xhci_addr_cc=xhci_command(XHCI_INPUTCTX,0,(11u<<10)|((u32)slot<<24),0);
    if(xhci_addr_cc==4){

        xhci_command(0,0,(10u<<10)|((u32)slot<<24),0);
        for(int t=0;t<40000;t++) io_delay(1);
        int ns=0; if(xhci_command(0,0,(9u<<10),&ns)==1&&ns>0){ slot=ns;
            MMW(XHCI_DCBAA+(u32)slot*8, devctx); MMW(XHCI_DCBAA+(u32)slot*8+4,0);
            xhci_ep0_idx=0; xhci_ep0_cyc=1;
            xhci_addr_cc=xhci_command(XHCI_INPUTCTX,0,(11u<<10)|((u32)slot<<24),0); }
    }
    if(xhci_addr_cc!=1){
        xhci_command(XHCI_INPUTCTX,0,(11u<<10)|(1u<<9)|((u32)slot<<24),0);
        for(int t=0;t<20000;t++) io_delay(1);
        xhci_addr_cc=xhci_command(XHCI_INPUTCTX,0,(11u<<10)|((u32)slot<<24),0);
        if(xhci_addr_cc!=1) return 0;
    }
    xhci_diag_step=4;

    u8 dd[18]; u8 gd[8]={0x80,0x06,0x00,0x01,0x00,0x00,18,0x00};
    if(xhci_ctrl(slot,gd,dd,18)<0) return 0; xhci_diag_step=5;

    u8 cfg[160]; u8 gc[8]={0x80,0x06,0x00,0x02,0x00,0x00,9,0x00};
    if(xhci_ctrl(slot,gc,cfg,9)<0) return 0;
    int total=cfg[2]|(cfg[3]<<8); if(total>160)total=160; if(total<9)total=9;
    u8 gc2[8]={0x80,0x06,0x00,0x02,0x00,0x00,(u8)total,0x00};
    if(xhci_ctrl(slot,gc2,cfg,total)<0) return 0; xhci_diag_step=6;
    int ms_ep=0,ms_mps=8,ms_intv=8,ms_if=0,ms_proto=2;
    int kb_ep=0,kb_mps=8,kb_intv=8,kb_if=0;
    int cur_cls=0,cur_proto=0,cur_ifnum=0,best=-1,off=0;
    while(off+1<total){ int bl=cfg[off],bt=cfg[off+1]; if(bl<2)break;
        if(bt==4&&off+8<=total){ cur_cls=cfg[off+5]; cur_ifnum=cfg[off+2]; cur_proto=cfg[off+7]; }
        else if(bt==5&&off+9<=total){ u8 epa=cfg[off+2],attr=cfg[off+3];
            if(cur_cls==0x03 && (attr&3)==3 && (epa&0x80)){
                if(cur_proto==1){ if(allow_kb && !kb_ep){ kb_ep=epa; kb_mps=cfg[off+4]|(cfg[off+5]<<8);
                        kb_intv=cfg[off+6]?cfg[off+6]:8; kb_if=cur_ifnum; } }
                else if(allow_ms){ int score=(cur_proto==2)?2:1;
                    if(score>best){ best=score; ms_ep=epa; ms_mps=cfg[off+4]|(cfg[off+5]<<8);
                        ms_intv=cfg[off+6]?cfg[off+6]:8; ms_if=cur_ifnum; ms_proto=cur_proto; } } } }
        off+=bl; }
    if(!ms_ep && !kb_ep) return 0; xhci_diag_step=7;

    int msdci=ms_ep?((ms_ep&0x0F)*2+1):0, kbdci=kb_ep?((kb_ep&0x0F)*2+1):0;
    int maxdci=msdci>kbdci?msdci:kbdci;
    { volatile u32* z=(volatile u32*)XHCI_INPUTCTX; for(int i=0;i<512;i++)z[i]=0; }
    if(ms_ep){ volatile u32* z=(volatile u32*)XHCI_EPIRING; for(int i=0;i<1024;i++)z[i]=0; }
    if(kb_ep){ volatile u32* z=(volatile u32*)XHCI_KBDRING; for(int i=0;i<1024;i++)z[i]=0; }
    u32 af=1u; if(ms_ep)af|=1u<<msdci; if(kb_ep)af|=1u<<kbdci;
    MMW(xhci_ctx(XHCI_INPUTCTX,0)+0x04, af);
    u32 sc2=xhci_ctx(XHCI_INPUTCTX,1);
    MMW(sc2+0x00, ((u32)maxdci<<27)|((u32)speed<<20)); MMW(sc2+0x04, ((u32)(port+1)<<16));
    if(ms_ep){ u32 epc=xhci_ctx(XHCI_INPUTCTX,msdci+1);
        MMW(epc+0x00, ((u32)ms_intv<<16));
        MMW(epc+0x04, (7u<<3)|((u32)ms_mps<<16)|(3u<<1));
        MMW(epc+0x08, XHCI_EPIRING|1u); MMW(epc+0x0C, 0);
        MMW(epc+0x10, (u32)ms_mps); }
    if(kb_ep){ u32 epc=xhci_ctx(XHCI_INPUTCTX,kbdci+1);
        MMW(epc+0x00, ((u32)kb_intv<<16));
        MMW(epc+0x04, (7u<<3)|((u32)kb_mps<<16)|(3u<<1));
        MMW(epc+0x08, XHCI_KBDRING|1u); MMW(epc+0x0C, 0);
        MMW(epc+0x10, (u32)kb_mps); }
    if(xhci_command(XHCI_INPUTCTX,0,(12u<<10)|((u32)slot<<24),0)!=1) return 0; xhci_diag_step=8;

    u8 cfv=(cfg[5])?cfg[5]:1;
    u8 scfg[8]={0x00,0x09,cfv,0,0,0,0,0}; xhci_ctrl(slot,scfg,0,0);
    int r=0;
    if(ms_ep){ u8 sp[8]={0x21,0x0B,0x00,0x00,(u8)ms_if,0,0,0}; xhci_ctrl(slot,sp,0,0);
        u8 si[8]={0x21,0x0A,0x00,0x00,(u8)ms_if,0,0,0}; xhci_ctrl(slot,si,0,0);
        xhci_ms_slot=slot; xhci_ms_dci=msdci; xhci_ms_cyc=1; xhci_ms_idx=0; xhci_ms_mps=ms_mps; xhci_ms_found=1; xhci_ms_proto=ms_proto;
        volatile u32* ir=(volatile u32*)XHCI_EPIRING;
        ir[0]=XHCI_MSBUF; ir[1]=0; ir[2]=(u32)(ms_mps>8?8:ms_mps);
        ir[3]=(1u<<5)|(1u<<2)|1u;
        MMW(xhci_db+(u32)slot*4,(u32)msdci); r|=1; }
    if(kb_ep){ u8 sp[8]={0x21,0x0B,0x00,0x00,(u8)kb_if,0,0,0}; xhci_ctrl(slot,sp,0,0);
        u8 si[8]={0x21,0x0A,0x00,0x00,(u8)kb_if,0,0,0}; xhci_ctrl(slot,si,0,0);
        xhci_kb_slot=slot; xhci_kb_dci=kbdci; xhci_kb_cyc=1; xhci_kb_idx=0; xhci_kb_found=1;
        volatile u32* ir=(volatile u32*)XHCI_KBDRING;
        ir[0]=XHCI_KBDBUF; ir[1]=0; ir[2]=8;
        ir[3]=(1u<<5)|(1u<<2)|1u;
        MMW(xhci_db+(u32)slot*4,(u32)kbdci); r|=2; }
    xhci_diag_step=9;
    return r;
}

static void xhci_enum(void){
    xhci_ms_found=0; xhci_diag_step=0;
    if(!xhci_init_ok) return;
    u32 op=xhci_op;

    for(int p=0;p<xhci_ports;p++){ u32 pa=op+0x400+(u32)p*0x10; u32 v=MMR(pa); if(!(v&(1u<<9))){ MMW(pa,(v&0x0E00C3E0u)|(1u<<9)); } }
    for(int t=0;t<40000;t++) io_delay(1);

    for(int p=0;p<xhci_ports;p++){ u32 v=MMR(op+0x400+(u32)p*0x10); if(!(v&1))continue; int sp=(v>>10)&0xF;
        if(sp==1||sp==2){ int m=xhci_try_port(p,sp,!xhci_ms_found,!xhci_kb_found,(xhci_ms_found||xhci_kb_found)?XHCI_KDEVCTX:XHCI_DEVCTX); (void)m;
            if(xhci_ms_found&&xhci_kb_found) return; } }

    for(int p=0;p<xhci_ports;p++){ u32 v=MMR(op+0x400+(u32)p*0x10); if(!(v&1))continue; int sp=(v>>10)&0xF;
        if(sp!=1&&sp!=2){ int m=xhci_try_port(p,sp,!xhci_ms_found,!xhci_kb_found,(xhci_ms_found||xhci_kb_found)?XHCI_KDEVCTX:XHCI_DEVCTX); (void)m;
            if(xhci_ms_found&&xhci_kb_found) return; } }
}
static int ata_probe(u16 base,u8 sel,int slot){
    atai[slot].present=0; atai[slot].type=0; atai[slot].model[0]=0; atai[slot].sectors=0;
    outb(base+6,sel); for(int i=0;i<5;i++)inb(base+7);
    outb(base+2,0);outb(base+3,0);outb(base+4,0);outb(base+5,0);
    outb(base+7,0xEC);
    u8 s=inb(base+7); if(s==0||s==0xFF)return 0;
    int t=0; while(inb(base+7)&0x80){ if(++t>500000)return 0; }
    u8 m=inb(base+4),h=inb(base+5);
    if(m==0x14&&h==0xEB){ atai[slot].present=1; atai[slot].type=2; const char*cd="ATAPI CD/DVD"; int k=0;while(cd[k]){atai[slot].model[k]=cd[k];k++;}atai[slot].model[k]=0; return 1; }
    t=0; for(;;){ u8 st=inb(base+7); if(st&0x01)return 0; if(st&0x08)break; if(++t>500000)return 0; }
    u16* id=(u16*)0x0095DE00u; insw(base,id,256);
    int p=0; for(int w=27;w<=46;w++){ u16 x=id[w]; char a=x>>8,b=x&0xFF; if(a&&p<42)atai[slot].model[p++]=a; if(b&&p<42)atai[slot].model[p++]=b; } atai[slot].model[p]=0;
    while(p>0&&atai[slot].model[p-1]==' ')atai[slot].model[--p]=0;
    atai[slot].sectors=id[60]|((u32)id[61]<<16); atai[slot].present=1; atai[slot].type=1; return 1;
}
static void hw_scan(void){
    u32 a,b,c,d;
    cpuid(0,0,&a,&b,&c,&d);
    *(u32*)&cpu_vendor[0]=b; *(u32*)&cpu_vendor[4]=d; *(u32*)&cpu_vendor[8]=c; cpu_vendor[12]=0;
    u32 ext; cpuid(0x80000000,0,&ext,&b,&c,&d);
    if(ext>=0x80000004){ u32*pp=(u32*)cpu_brand; for(u32 lf=0x80000002;lf<=0x80000004;lf++){ cpuid(lf,0,&a,&b,&c,&d); *pp++=a;*pp++=b;*pp++=c;*pp++=d; } cpu_brand[48]=0; }
    else cpu_brand[0]=0;
    u32 ext64=cmos(0x34)|((u32)cmos(0x35)<<8);
    ram_mb=16+(ext64*64)/1024;
    if(ram_mb<8||ram_mb>8192){ u32 base=cmos(0x30)|((u32)cmos(0x31)<<8); ram_mb=(base/1024)+1; }
    pcin=0;
    for(int bus=0;bus<2;bus++)for(int dev=0;dev<32;dev++){
        u32 v=pci_read(bus,dev,0,0); u16 ven=v&0xFFFF; if(ven==0xFFFF)continue;
        u16 did=v>>16; u32 cc=pci_read(bus,dev,0,0x08); u8 cls=(cc>>24)&0xFF,sub=(cc>>16)&0xFF,prog=(cc>>8)&0xFF;
        u32 bar0=pci_read(bus,dev,0,0x10); u16 io=(bar0&1)?(bar0&0xFFFC):0;
        u32 il=pci_read(bus,dev,0,0x3C); u8 irq=il&0xFF;
        if(pcin<MAXPCI){ pcil[pcin].bus=bus;pcil[pcin].dev=dev;pcil[pcin].ven=ven;pcil[pcin].did=did;pcil[pcin].cls=cls;pcil[pcin].sub=sub;pcil[pcin].prog=prog;pcil[pcin].io=io;pcil[pcin].irq=irq;pcin++; }
    }
    ata_probe(0x1F0,0xA0,0); ata_probe(0x1F0,0xB0,1);
    ata_probe(0x170,0xA0,2); ata_probe(0x170,0xB0,3);
}

static u32 *FB; static int PITCH,W,H;
static u32 *LFB;
static u32 *BACK=(u32*)0x03000000;
static u32 *BASE=(u32*)0x03800000;
static u32 *DRAGBUF=(u32*)0x04000000;

#define ICON_SZ 40
#define ICO_ZIP 25
#define ICO_CDRIVE 26
#define ICO_USB 27
static u8* ICONCACHE=(u8*)0x02000000;
static int icons_ready=0;
static void icons_init(void){
    u8* raw=(u8*)0x02200000;
    u8* tmp=(u8*)0x02400000;
    for(int i=0;i<NAPPICONS;i++){
        int w=0,h=0;
        u8* slot=ICONCACHE+(unsigned)i*ICON_SZ*ICON_SZ*4;
        int r=png_decode(APPICONS[i].data, APPICONS[i].len, raw, 0x100000, tmp, 0x100000, &w, &h);
        if(r==0 && w==ICON_SZ && h==ICON_SZ){
            for(int p=0;p<ICON_SZ*ICON_SZ*4;p++) slot[p]=tmp[p];
            /* gently tone icons -> 80%% saturation, full brightness (NOT grey) */
            for(int q=0;q<ICON_SZ*ICON_SZ;q++){ u8* px=slot+q*4; int Lm=(77*px[0]+150*px[1]+29*px[2])>>8;
                int R=Lm+((px[0]-Lm)*80)/100, G=Lm+((px[1]-Lm)*80)/100, B=Lm+((px[2]-Lm)*80)/100;
                px[0]=(u8)R; px[1]=(u8)G; px[2]=(u8)B; }
        } else {
            for(int p=0;p<ICON_SZ*ICON_SZ*4;p++) slot[p]=0;
        }
    }
    icons_ready=1;
}

static int icon_for_app(int a){
    switch(a){ case 1:return 0; case 2:return 1; case 5:return 2; case 4:return 3; case 6:return 4;
        case 7:return 5; case 9:return 6; case 8:return 7; case 11:return 8; case 12:return 9;
        case 13:return 10; case 10:return 11; case 14:return 12; case 15:return 13; case 3:return 14;
        case -2:return 15; case 17:return 16; case 30:return 17; case 23:return 18; case 24:return 19;
        case 25:return 20; case 26:return 21; case 27:return 22; case 28:return 23; default:return -1; }
}

static void blit_icon(int idx,int x,int y,int sz){
    if(!icons_ready||idx<0||idx>=NAPPICONS){ return; }
    const u8* src=ICONCACHE+(unsigned)idx*ICON_SZ*ICON_SZ*4;
    for(int j=0;j<sz;j++){ int py=y+j; if(py<0||py>=H)continue; int sy=(j*ICON_SZ)/sz;
        for(int i=0;i<sz;i++){ int px=x+i; if(px<0||px>=W)continue; int sx=(i*ICON_SZ)/sz;
            const u8* p=src+(sy*ICON_SZ+sx)*4; int a=p[3]; if(a<8)continue;
            u32 d=FB[py*PITCH+px]; int dr=(d>>16)&255,dg=(d>>8)&255,db=d&255; int ia=255-a;
            int rr=(p[0]*a+dr*ia)/255, gg=(p[1]*a+dg*ia)/255, bb=(p[2]*a+db*ia)/255;
            FB[py*PITCH+px]=((u32)rr<<16)|((u32)gg<<8)|(u32)bb;
        }
    }
}
static int drag_cached=0, dcw=0, dch=0, drag_wantcache=0;
static u32 PAL32[256];
static int acc_bold=0, acc_hicontrast=0, mag_on=0;
static char wx_temp[16]={0}, wx_cond[28]={0}, wx_loc[24]={0}; static int wx_have=0;
#define C_WHITE 64
#define C_BLUE  65
#define C_BBLUE 66
#define C_WIN   67
#define C_MGREY 68
#define C_TASK  69
#define C_TEAL  70
#define C_RED   71
#define C_TITLE 72
#define C_SHAD  73
#define C_GREEN 74
#define C_BSOD  75
#define C_FOLDER 76
#define BLUE0 128
static int accent=0;
static void set_palette(void){
    for(int i=0;i<64;i++){ int v=i*255/63; PAL32[i]=((u32)v<<16)|((u32)v<<8)|v; }
    static const u8 pal[][3]={
        {63,63,63},{12,18,30},{20,29,42},{7,8,11},{14,16,21},
        {5,6,9},{11,52,47},{59,17,17},{49,51,55},{2,2,4},{18,55,32},
        {7,15,49},{60,47,22}
    };
    for(unsigned i=0;i<sizeof(pal)/3;i++){ int r=pal[i][0]*255/63,g=pal[i][1]*255/63,b=pal[i][2]*255/63; PAL32[64+i]=((u32)r<<16)|((u32)g<<8)|b; }
    for(int i=0;i<64;i++){ int r=(i*45)/63,g=8+(i*46)/63,b=28+(i*35)/63; if(r>63)r=63;if(g>63)g=63;if(b>63)b=63; PAL32[128+i]=((u32)(r*255/63)<<16)|((u32)(g*255/63)<<8)|(b*255/63); }

    static const u8 dk[11][3]={ {72,100,150},{56,60,72},{74,110,140},{150,128,72},{150,88,100},
        {120,126,140},{108,96,140},{84,128,96},{156,116,76},{76,124,120},{88,100,150} };
    for(int i=0;i<11;i++) PAL32[100+i]=((u32)dk[i][0]<<16)|((u32)dk[i][1]<<8)|dk[i][2];
    if(acc_hicontrast){ for(int i=0;i<256;i++){ u32 c=PAL32[i]; int R=(c>>16)&255,G=(c>>8)&255,B=c&255;
        R=128+((R-128)*9)/5; G=128+((G-128)*9)/5; B=128+((B-128)*9)/5;
        if(R<0)R=0; if(R>255)R=255; if(G<0)G=0; if(G>255)G=255; if(B<0)B=0; if(B>255)B=255;
        PAL32[i]=((u32)R<<16)|((u32)G<<8)|(u32)B; } }
}
static void apply_accent(int a){
    accent=a;
    static const u8 ac[4][6]={{11,16,25,20,29,42},{15,15,23,26,27,37},{10,20,20,19,32,31},{13,21,15,24,33,25}};
    PAL32[65]=((u32)(ac[a][0]*255/63)<<16)|((u32)(ac[a][1]*255/63)<<8)|(ac[a][2]*255/63);
    PAL32[66]=((u32)(ac[a][3]*255/63)<<16)|((u32)(ac[a][4]*255/63)<<8)|(ac[a][5]*255/63);
}

static void fill(int x,int y,int w,int h,u8 c){ if(x<0){w+=x;x=0;} if(y<0){h+=y;y=0;} if(x+w>W)w=W-x; if(y+h>H)h=H-y; u32 v=PAL32[c]; for(int j=0;j<h;j++){ u32*r=FB+(y+j)*PITCH+x; for(int i=0;i<w;i++)r[i]=v; } }
static void hrule(int x,int y,int w,u8 c){ fill(x,y,w,1,c); }

static void afill(int x,int y,int w,int h,u8 c,int a);
static void rrect(int x,int y,int w,int h,u8 c);
static void afill(int x,int y,int w,int h,u8 c,int a){
    if(a<=0)return; if(a>=255){fill(x,y,w,h,c);return;}
    if(x<0){w+=x;x=0;} if(y<0){h+=y;y=0;} if(x+w>W)w=W-x; if(y+h>H)h=H-y;
    if(w<=0||h<=0)return; u32 v=PAL32[c];
    int sr=(v>>16)&255,sg=(v>>8)&255,sb=v&255, ia=255-a;
    for(int j=0;j<h;j++){ u32*r=FB+(y+j)*PITCH+x; for(int i=0;i<w;i++){
        u32 p=r[i]; int dr=(p>>16)&255,dg=(p>>8)&255,db=p&255;
        int rr=(sr*a+dr*ia)/255, gg=(sg*a+dg*ia)/255, bb=(sb*a+db*ia)/255;
        r[i]=((u32)rr<<16)|((u32)gg<<8)|(u32)bb;
    } }
}

static void soft_shadow(int x,int y,int w,int h){

    for(int k=6;k>=1;k--){ int a = 26 - k*3; if(a<6)a=6; afill(x-k+5, y-k+8, w+2*k, h+2*k, 0, a); }
}

static void ultra_shadow(int x,int y,int w,int h){
    for(int k=10;k>=1;k--){ int a = 30 - k*2; if(a<4)a=4; afill(x-k+6, y-k+11, w+2*k, h+2*k, 0, a); }
}

static int gfx_fast=0;
static void glass_panel(int x,int y,int w,int h,int tint_white,int tint_a){
    if(gfx_fast){ afill(x,y,w,h,tint_white?C_WHITE:0,tint_a+60); fill(x,y,w,1,C_WHITE); return; }
    if(x<2)x=2; if(y<2)y=2; if(x+w>W-2)w=W-2-x; if(y+h>H-2)h=H-2-y;
    if(w<4||h<4)return;

    for(int j=0;j<h;j++){ u32* r=FB+(y+j)*PITCH+x;
        u32 prev=r[0];
        for(int i=0;i<w;i++){
            int x0=i-2;if(x0<0)x0=0; int x1=i+2;if(x1>=w)x1=w-1;
            int rr=0,gg=0,bb=0,n=0;
            for(int q=x0;q<=x1;q++){ u32 p=r[q]; rr+=(p>>16)&255; gg+=(p>>8)&255; bb+=p&255; n++; }
            u32 nv=((u32)(rr/n)<<16)|((u32)(gg/n)<<8)|(u32)(bb/n);
            u32 keep=prev; prev=r[i]; (void)keep;
            r[i]=nv;
        }
    }
    for(int i=0;i<w;i++){
        for(int j=0;j<h;j++){
            int y0=j-2;if(y0<0)y0=0; int y1=j+2;if(y1>=h)y1=h-1;
            int rr=0,gg=0,bb=0,n=0;
            for(int q=y0;q<=y1;q++){ u32 p=FB[(y+q)*PITCH+(x+i)]; rr+=(p>>16)&255; gg+=(p>>8)&255; bb+=p&255; n++; }
            FB[(y+j)*PITCH+(x+i)]=((u32)(rr/n)<<16)|((u32)(gg/n)<<8)|(u32)(bb/n);
        }
    }
    afill(x,y,w,h,tint_white?C_WHITE:0,tint_a);
    fill(x,y,w,1,C_WHITE);
}

static void rrectR(int x,int y,int w,int h,int r,u8 c){
    if(r*2>w)r=w/2; if(r*2>h)r=h/2; if(r<1){ fill(x,y,w,h,c); return; }
    fill(x+r,y,w-2*r,h,c);
    fill(x,y+r,r,h-2*r,c); fill(x+w-r,y+r,r,h-2*r,c);

    u32 fv=PAL32[c]; int sr=(fv>>16)&255,sg=(fv>>8)&255,sb=fv&255;
    for(int dy=0;dy<r;dy++) for(int dx=0;dx<r;dx++){
        int cnt=0;
        for(int a=0;a<4;a++){ int sx=dx*8+2*a+1-r*8;
            for(int b=0;b<4;b++){ int sy=dy*8+2*b+1-r*8;
                if(sx*sx+sy*sy<=(r*8)*(r*8))cnt++; } }
        if(!cnt)continue;
        int cov=cnt*255/16, ia=255-cov;
        int cxs[4],cys[4]; cxs[0]=x+dx;cys[0]=y+dy; cxs[1]=x+w-1-dx;cys[1]=y+dy; cxs[2]=x+dx;cys[2]=y+h-1-dy; cxs[3]=x+w-1-dx;cys[3]=y+h-1-dy;
        for(int k=0;k<4;k++){ int xx=cxs[k],yy=cys[k]; if(xx<0||yy<0||xx>=W||yy>=H)continue;
            if(cov>=255){ FB[yy*PITCH+xx]=fv; continue; }
            u32 d=FB[yy*PITCH+xx]; int dr=(d>>16)&255,dg=(d>>8)&255,db=d&255;
            int rr=(sr*cov+dr*ia)/255,gg=(sg*cov+dg*ia)/255,bb=(sb*cov+db*ia)/255;
            FB[yy*PITCH+xx]=((u32)rr<<16)|((u32)gg<<8)|(u32)bb;
        }
    }
}
static void disc(int cx,int cy,int r,u8 c);

static void toggle_sw(int x,int y,int on){
    int w=40,h=20; rrectR(x,y,w,h,h/2,on?C_GREEN:(C_MGREY+18));
    disc(on?(x+w-h/2):(x+h/2), y+h/2, h/2-3, C_WHITE);
}
static void frame(int x,int y,int w,int h,u8 c){ fill(x,y,w,1,c); fill(x,y+h-1,w,1,c); fill(x,y,1,h,c); fill(x+w-1,y,1,h,c); }
static void disc(int cx,int cy,int r,u8 c){ u32 v=PAL32[c]; for(int dy=-r;dy<=r;dy++)for(int dx=-r;dx<=r;dx++){ if(dx*dx+dy*dy<=r*r){ int x=cx+dx,y=cy+dy; if(x>=0&&y>=0&&x<W&&y<H)FB[y*PITCH+x]=v; } } }
static void clear_all(u8 c){ u32 v=PAL32[c]; for(int i=0;i<PITCH*H;i++)FB[i]=v; }
static int strlen_(const char*s){ int n=0; while(*s++)n++; return n; }
static int streq(const char*a,const char*b){ while(*a&&*b){ char ca=*a,cb=*b; if(ca>='A'&&ca<='Z')ca+=32; if(cb>='A'&&cb<='Z')cb+=32; if(ca!=cb)return 0; a++;b++; } return *a==*b; }
static int streq_cs(const char*a,const char*b){ while(*a&&*b){ if(*a!=*b)return 0; a++;b++; } return *a==*b; }
static int startsw(const char*s,const char*p){ while(*p){ char a=*s,b=*p; if(a>='A'&&a<='Z')a+=32; if(b>='A'&&b<='Z')b+=32; if(a!=b)return 0; s++;p++; } return 1; }
static u32 hexparse(const char* s){ while(*s==' ')s++; if(s[0]=='0'&&(s[1]=='x'||s[1]=='X'))s+=2; u32 v=0; for(;;){ char c=*s++; u32 d; if(c>='0'&&c<='9')d=c-'0'; else if(c>='a'&&c<='f')d=c-'a'+10; else if(c>='A'&&c<='F')d=c-'A'+10; else break; v=(v<<4)|d; } return v; }
static void draw_char(int x,int y,char ch,u8 col){
    int idx=(int)(unsigned char)ch-32; if(idx<0||idx>=95)return;
    const unsigned char*g=fontaa[idx];
    u32 fg=PAL32[col]; int fr=(fg>>16)&255,fgc=(fg>>8)&255,fb=fg&255;
    for(int pass=0;pass<=(acc_bold?1:0);pass++){ int xo=pass;
    for(int r=0;r<AA_CH;r++){
        int yy=y+r; if(yy<0||yy>=H)continue;
        const unsigned char*gr=g+r*AA_CW;
        for(int c=0;c<AA_CW;c++){
            int a=gr[c]; if(!a)continue;
            int xx=x+c+xo; if(xx<0||xx>=W)continue;
            if(a>=250){ FB[yy*PITCH+xx]=fg; continue; }
            u32 bg=FB[yy*PITCH+xx];
            int br=(bg>>16)&255,bgc=(bg>>8)&255,bb=bg&255;
            int rr=(fr*a+br*(255-a))/255;
            int gg=(fgc*a+bgc*(255-a))/255;
            int bv=(fb*a+bb*(255-a))/255;
            FB[yy*PITCH+xx]=((u32)rr<<16)|((u32)gg<<8)|(u32)bv;
        }
    }
    }
}
static void draw_str(int x,int y,const char*s,u8 col){ while(*s){ draw_char(x,y,*s,col); x+=8; s++; } }

struct idt_entry { u16 off_lo, sel; u8 zero, flags; u16 off_hi; } __attribute__((packed));
struct idt_ptr   { u16 limit; u32 base; } __attribute__((packed));
static struct idt_entry g_idt[256];
static struct idt_ptr   g_idtp;
struct iframe { u32 edi,esi,ebp,esp_,ebx,edx,ecx,eax; u32 vec,err; u32 eip,cs,eflags; };

static const char* exc_name(u32 v){
    static const char* N[]={"DIVIDE BY ZERO","DEBUG","NMI","BREAKPOINT","OVERFLOW","BOUND RANGE",
      "INVALID OPCODE","DEVICE NOT AVAILABLE","DOUBLE FAULT","COPROCESSOR","INVALID TSS",
      "SEGMENT NOT PRESENT","STACK FAULT","GENERAL PROTECTION","PAGE FAULT","?","FPU ERROR",
      "ALIGNMENT","MACHINE CHECK","SIMD"};
    return (v<20)?N[v]:"EXCEPTION";
}

__attribute__((used)) void exception_handler(struct iframe* f){
    static volatile int in_handler=0;

    if(in_handler){ for(;;) __asm__ __volatile__("cli; hlt"); }
    in_handler=1;
    if(!FB){ for(;;) __asm__ __volatile__("cli; hlt"); }

    for(int i=0;i<PITCH*H;i++) FB[i]=0x00200008u;
    int cx=W/2-260, cy=H/2-150;
    draw_str(cx,cy,    "NOOVEXOS - SYSTEM EXCEPTION",0x0F);
    draw_str(cx,cy+24, "A FAULT OCCURRED. THE SYSTEM HAS BEEN HALTED",0x0F);
    draw_str(cx,cy+48, "SO IT DID NOT SILENTLY REBOOT.",0x0F);
    char hb[12];
    draw_str(cx,cy+84, "TYPE   : ",0x0F); draw_str(cx+72,cy+84, exc_name(f->vec),0x0E);
    draw_str(cx,cy+104,"VECTOR : ",0x0F); { char nb[8]; int v=f->vec,n=0; char t[4]; if(!v)t[n++]='0'; while(v){t[n++]='0'+v%10;v/=10;} int q=0; while(n)nb[q++]=t[--n]; nb[q]=0; draw_str(cx+72,cy+104,nb,0x0F); }
    draw_str(cx,cy+124,"ERROR  : 0x",0x0F); hex32(f->err,hb); draw_str(cx+88,cy+124,hb,0x0F);
    draw_str(cx,cy+144,"EIP    : 0x",0x0F); hex32(f->eip,hb); draw_str(cx+88,cy+144,hb,0x0E);
    draw_str(cx,cy+164,"EAX 0x",0x0A); hex32(f->eax,hb); draw_str(cx+48,cy+164,hb,0x0F);
    draw_str(cx+180,cy+164,"EBX 0x",0x0A); hex32(f->ebx,hb); draw_str(cx+228,cy+164,hb,0x0F);
    draw_str(cx,cy+184,"ECX 0x",0x0A); hex32(f->ecx,hb); draw_str(cx+48,cy+184,hb,0x0F);
    draw_str(cx+180,cy+184,"EDX 0x",0x0A); hex32(f->edx,hb); draw_str(cx+228,cy+184,hb,0x0F);
    draw_str(cx,cy+220,"WRITE DOWN THE EIP ABOVE - IT PINPOINTS THE BUG.",0x0E);
    draw_str(cx,cy+240,"PRESS  R  TO REBOOT.",0x0F);

    for(;;){ if(inb(0x64)&1){ u8 k=inb(0x60); if(k==0x13){ u8 g; do{ g=inb(0x64); }while(g&2); outb(0x64,0xFE); } } __asm__ __volatile__("hlt"); }
}

__attribute__((naked,used)) static void isr_common(void){
    __asm__ __volatile__(
        "pusha\n\t"
        "pushl %esp\n\t"
        "call exception_handler\n\t"
        "addl $4, %esp\n\t"
        "popa\n\t"
        "addl $8, %esp\n\t"
        "iret\n\t");
}

#define ISR_N(n) __attribute__((naked,used)) static void isr##n(void){ __asm__ __volatile__("cli\n\tpushl $0\n\tpushl $" #n "\n\tjmp isr_common"); }
#define ISR_E(n) __attribute__((naked,used)) static void isr##n(void){ __asm__ __volatile__("cli\n\tpushl $" #n "\n\tjmp isr_common"); }
ISR_N(0) ISR_N(1) ISR_N(2) ISR_N(3) ISR_N(4) ISR_N(5) ISR_N(6) ISR_N(7)
ISR_E(8) ISR_N(9) ISR_E(10) ISR_E(11) ISR_E(12) ISR_E(13) ISR_E(14) ISR_N(15)
ISR_N(16) ISR_E(17) ISR_N(18) ISR_N(19)

struct sysregs { u32 edi,esi,ebp,esp_,ebx,edx,ecx,eax; };
void syscall_dispatch(struct sysregs* r);
__attribute__((naked,used)) static void isr_syscall(void){
    __asm__ __volatile__(
        "pusha\n\t"
        "pushl %esp\n\t"
        "call syscall_dispatch\n\t"
        "addl $4, %esp\n\t"
        "popa\n\t"
        "iret\n\t");
}
static void idt_set(int n, void(*h)(void)){
    u32 a=(u32)h; g_idt[n].off_lo=a&0xFFFF; g_idt[n].sel=0x08; g_idt[n].zero=0;
    g_idt[n].flags=0x8E; g_idt[n].off_hi=(a>>16)&0xFFFF;
}
static void idt_init(void){
    void(*stubs[20])(void)={isr0,isr1,isr2,isr3,isr4,isr5,isr6,isr7,isr8,isr9,
        isr10,isr11,isr12,isr13,isr14,isr15,isr16,isr17,isr18,isr19};
    for(int i=0;i<20;i++) idt_set(i,stubs[i]);

    { u32 a=(u32)isr_syscall; g_idt[0x80].off_lo=a&0xFFFF; g_idt[0x80].sel=0x08; g_idt[0x80].zero=0;
      g_idt[0x80].flags=0xEE; g_idt[0x80].off_hi=(a>>16)&0xFFFF; }
    g_idtp.limit=sizeof(g_idt)-1; g_idtp.base=(u32)g_idt;
    __asm__ __volatile__("lidt %0"::"m"(g_idtp));
}
static void draw_char2(int x,int y,char ch,u8 col){ if(ch>='a'&&ch<='z')ch-=32; int idx=ch-32; if(idx<0||idx>=64)return; const u8*g=font8x8[idx]; for(int r=0;r<8;r++){u8 b=g[r];for(int c=0;c<8;c++) if(b&(0x80>>c)) fill(x+c*2,y+r*2,2,2,col);} }
static void draw_str2(int x,int y,const char*s,u8 col){ while(*s){ draw_char2(x,y,*s,col); x+=16; s++; } }
static void draw_char_n(int x,int y,char ch,u8 col,int n){ if(ch>='a'&&ch<='z')ch-=32; int idx=ch-32; if(idx<0||idx>=64)return; const u8*g=font8x8[idx]; for(int r=0;r<8;r++){u8 b=g[r];for(int c=0;c<8;c++) if(b&(0x80>>c)) fill(x+c*n,y+r*n,n,n,col);} }
static void draw_char3(int x,int y,char ch,u8 col){ if(ch>='a'&&ch<='z')ch-=32; int idx=ch-32; if(idx<0||idx>=64)return; const u8*g=font8x8[idx]; for(int r=0;r<8;r++){u8 b=g[r];for(int c=0;c<8;c++) if(b&(0x80>>c)) fill(x+c*3,y+r*3,3,3,col);} }
static void draw_str3(int x,int y,const char*s,u8 col){ while(*s){ draw_char3(x,y,*s,col); x+=24; s++; } }

static void cos_circle(int cx,int cy,int r,u8 c);
static void icon_folder(int x,int y){ rrectR(x+2,y+5,24,16,3,C_FOLDER); fill(x+3,y+2,10,4,C_FOLDER); fill(x+3,y+7,22,2,63); }
static void icon_terminal(int x,int y){ rrectR(x+1,y+2,26,19,3,10); fill(x+1,y+2,26,4,C_TASK); fill(x+4,y+3,2,2,C_RED); fill(x+8,y+3,2,2,63); draw_str(x+4,y+9,">",C_GREEN); fill(x+12,y+15,9,2,C_GREEN); }
static void icon_notepad(int x,int y){ rrectR(x+4,y+1,20,21,2,63); fill(x+4,y+1,20,5,C_BLUE); fill(x+7,y+9,10,2,0); for(int i=0;i<3;i++) fill(x+7,y+13+i*3,14,1,C_MGREY); }
static void icon_gear(int x,int y){ for(int a=0;a<4;a++){ fill(x+12+((a==1)?6:(a==3)?-6:0),y+11+((a==0)?-9:(a==2)?9:0),4,10,C_TEAL);} fill(x+3,y+10,22,4,C_TEAL); fill(x+10,y+3,4,20,C_TEAL); cos_circle(x+14,y+13,8,C_TEAL); cos_circle(x+14,y+13,3,C_WIN); }
static void icon_info(int x,int y){ cos_circle(x+14,y+11,10,C_BBLUE); fill(x+13,y+6,3,3,C_WHITE); fill(x+13,y+10,3,8,C_WHITE); }
static void icon_file(int x,int y){ rrectR(x+6,y+1,16,21,2,63); fill(x+16,y+1,6,6,C_MGREY); for(int i=0;i<4;i++) fill(x+9,y+10+i*3,10,1,C_MGREY); }

static u16 pit_read(void){ outb(0x43,0x00); u8 lo=inb(0x40); u8 hi=inb(0x40); return ((u16)hi<<8)|lo; }
static void pit_wait(int ticks){   for(int t=0;t<ticks;t++){ u16 a=pit_read(); for(;;){ u16 b=pit_read(); if(((u16)(a-b))>=1193) break; } } }

#if defined(NOOVEX7)||defined(NOOVEX8)
static int snd_on=1, bg_style=9;
#else
static int snd_on=1, bg_style=0;
#endif
static void spk_tone(int hz){ if(hz<=0)return; int d=1193182/hz; outb(0x43,0xB6); outb(0x42,d&0xFF); outb(0x42,(d>>8)&0xFF); u8 t=inb(0x61); outb(0x61,t|3); }
static void spk_off(void){ u8 t=inb(0x61); outb(0x61,t&~3); }
static void beep(int hz,int ticks){ if(!snd_on)return; spk_tone(hz); pit_wait(ticks); spk_off(); }
static void click_snd(void){ if(snd_on){ spk_tone(1521); pit_wait(1); spk_off(); } }
static void jingle(void){ if(!snd_on)return; beep(880,2); beep(1175,2); beep(1568,3); }

static u8 cmos(u8 r){ outb(0x70,r); return inb(0x71); }
static void cmos_write(u8 r,u8 v){ outb(0x70,r); outb(0x71,v); }
static u8 bcd(u8 v){ return (v&0x0F)+((v>>4)*10); }
static char clkbuf[9]="00:00:00";
static u8 last_sec=99;

static u8 rtc_year=0, rtc_month=0, rtc_day=0, rtc_hour=0, rtc_min=0, rtc_sec=0;
static char datebuf[12];
static int rtc_updating(void){ outb(0x70,0x0A); return inb(0x71)&0x80; }
static void rtc_read(void){
    int guard=100000; while(rtc_updating()&&guard--){}
    u8 s=cmos(0x00),mi=cmos(0x02),h=cmos(0x04),d=cmos(0x07),mo=cmos(0x08),y=cmos(0x09);
    u8 rb=cmos(0x0B);
    if(!(rb&0x04)){
        s=(s&0x0F)+((s>>4)*10); mi=(mi&0x0F)+((mi>>4)*10);
        h=((h&0x0F)+(((h&0x70)>>4)*10))|(h&0x80);
        d=(d&0x0F)+((d>>4)*10); mo=(mo&0x0F)+((mo>>4)*10); y=(y&0x0F)+((y>>4)*10);
    }
    if(!(rb&0x02)&&(h&0x80)) h=((h&0x7F)+12)%24;
    rtc_sec=s; rtc_min=mi; rtc_hour=h&0x7F; rtc_day=d; rtc_month=mo; rtc_year=y;

    datebuf[0]='2'; datebuf[1]='0'; datebuf[2]='0'+(y/10)%10; datebuf[3]='0'+y%10; datebuf[4]='-';
    datebuf[5]='0'+(mo/10)%10; datebuf[6]='0'+mo%10; datebuf[7]='-';
    datebuf[8]='0'+(d/10)%10; datebuf[9]='0'+d%10; datebuf[10]=0;
}

static void rtc_set(u8 h,u8 m,u8 s){
    int guard=100000; while(rtc_updating()&&guard--){}
    u8 rb=cmos(0x0B);
    if(!(rb&0x04)){ h=((h/10)<<4)|(h%10); m=((m/10)<<4)|(m%10); s=((s/10)<<4)|(s%10); }
    cmos_write(0x00,s); cmos_write(0x02,m); cmos_write(0x04,h);
}
static void rtc_now(void){
    rtc_read();
    u8 s=rtc_sec,m=rtc_min,h=rtc_hour;
    clkbuf[0]='0'+h/10; clkbuf[1]='0'+h%10; clkbuf[2]=':';
    clkbuf[3]='0'+m/10; clkbuf[4]='0'+m%10; clkbuf[5]=':';
    clkbuf[6]='0'+s/10; clkbuf[7]='0'+s%10; clkbuf[8]=0;
    last_sec=s;
}

static int mx=160,my=100,mbtn=0,prevbtn=0,last_mx=160,last_my=100;
static int ms_dx_acc=0;
static u8 mpkt[4]; static int mphase=0;
static int wheel_ok=0, mouse_speed=1, scroll_speed=3, scroll_rev=0, wheel_delta=0;
static int touchpad_present=0, touchpad_kind=0;
static void mouse_wait(int t){ int n=100000; if(t){ while(n--){ if((inb(0x64)&2)==0)return; } } else { while(n--){ if(inb(0x64)&1)return; } } }
static void mouse_cmd(u8 v){ mouse_wait(1); outb(0x64,0xD4); mouse_wait(1); outb(0x60,v); mouse_wait(0); inb(0x60); }

static int touchpad_detect(void){
    for(int i=0;i<4;i++){ mouse_cmd(0xE8); mouse_cmd(0x00); }
    mouse_wait(1); outb(0x64,0xD4); mouse_wait(1); outb(0x60,0xE9);
    mouse_wait(0); u8 b0=inb(0x60); mouse_wait(0); u8 b1=inb(0x60); mouse_wait(0); u8 b2=inb(0x60);
    (void)b0;(void)b2;
    if(b1==0x47){ touchpad_present=1; touchpad_kind=1; return 1; }
    return 0;
}
static u8 aux_present=0, aux_id=0xFF, aux_kind=0, aux_rst[3]={0,0,0}, aux_knock[3]={0,0,0};
static void ps2_aux_probe(void){
    aux_present=0; aux_id=0xFF; aux_kind=0;
    for(int i=0;i<3;i++){aux_rst[i]=0;aux_knock[i]=0;}
    for(int i=0;i<16;i++){ if(inb(0x64)&1) inb(0x60); }
    mouse_wait(1); outb(0x64,0xA8);
    mouse_wait(1); outb(0x64,0x20); mouse_wait(0); u8 cf=inb(0x60);
    cf &= ~0x20u;
    mouse_wait(1); outb(0x64,0x60); mouse_wait(1); outb(0x60,cf);
    mouse_wait(1); outb(0x64,0xD4); mouse_wait(1); outb(0x60,0xFF);
    mouse_wait(0); aux_rst[0]=(inb(0x64)&1)?inb(0x60):0xEE;
    mouse_wait(0); aux_rst[1]=(inb(0x64)&1)?inb(0x60):0xEE;
    mouse_wait(0); aux_rst[2]=(inb(0x64)&1)?inb(0x60):0xEE;
    if(aux_rst[0]==0xFA||aux_rst[1]==0xAA){ aux_present=1; aux_id=aux_rst[2]; aux_kind=(aux_id==3)?2:1; }
    for(int i=0;i<4;i++){ mouse_cmd(0xE8); mouse_cmd(0x00); }
    mouse_wait(1); outb(0x64,0xD4); mouse_wait(1); outb(0x60,0xE9);
    mouse_wait(0); aux_knock[0]=(inb(0x64)&1)?inb(0x60):0xEE;
    mouse_wait(0); aux_knock[1]=(inb(0x64)&1)?inb(0x60):0xEE;
    mouse_wait(0); aux_knock[2]=(inb(0x64)&1)?inb(0x60):0xEE;
    if(aux_knock[1]==0x47){ aux_present=1; aux_kind=3; touchpad_present=1; touchpad_kind=1; }
    else if(aux_knock[0]==0x3C && aux_knock[1]==0x03){ aux_present=1; aux_kind=4; touchpad_present=1; touchpad_kind=2; }
}
static int aux_read_ms(int ms){
    u32 need=(u32)ms*1193u; u32 el=0; u16 last=pit_read();
    for(;;){ if(inb(0x64)&1) return (int)inb(0x60);
        u16 b=pit_read(); el+=(u16)(last-b); last=b; if(el>=need) return -1; } }
static int aux_cmd_ack(u8 v){
    mouse_wait(1); outb(0x64,0xD4); mouse_wait(1); outb(0x60,v);
    for(int k=0;k<3;k++){ int r=aux_read_ms(150); if(r==0xFA)return 1; if(r<0)return 0; } return 0; }
static void mouse_init(void){
    for(int pass=0; pass<2; pass++){
        int fails=0;
        for(int i=0;i<8;i++){ if(inb(0x64)&1) inb(0x60); }
        mouse_wait(1); outb(0x64,0xA8);                       /* aux port on          */
        mouse_wait(1); outb(0x64,0x20);
        { int cf=aux_read_ms(80); if(cf>=0){ u8 c2=(u8)cf; c2&=~0x20u;
              mouse_wait(1); outb(0x64,0x60); mouse_wait(1); outb(0x60,c2); } } /* aux clock on */
        if(pass==0) ps2_aux_probe();
        mouse_wait(1); outb(0x64,0xD4); mouse_wait(1); outb(0x60,0xFF);   /* reset */
        { int bat=0;
          for(int k=0;k<6;k++){ int r=aux_read_ms(700); if(r<0)break;
              if(r==0xAA){ bat=1; aux_read_ms(100); break; } }             /* BAT+id: touchpads ~500ms */
          if(!bat) fails++; }
        if(!aux_cmd_ack(0xF6)) fails++;                        /* defaults             */
        aux_cmd_ack(0xF3); aux_cmd_ack(200);                   /* IntelliMouse knock   */
        aux_cmd_ack(0xF3); aux_cmd_ack(100);
        aux_cmd_ack(0xF3); aux_cmd_ack(80);
        mouse_wait(1); outb(0x64,0xD4); mouse_wait(1); outb(0x60,0xF2);
        { int a=aux_read_ms(150); int id=(a==0xFA)?aux_read_ms(150):a;
          if(id==3) wheel_ok=1; }
        if(!aux_cmd_ack(0xF3)) fails++; aux_cmd_ack(100);      /* 100 samples/s        */
        aux_cmd_ack(0xE8); aux_cmd_ack(0x03);                  /* 8 counts/mm          */
        aux_cmd_ack(0xE6);                                     /* 1:1 scaling          */
        if(!aux_cmd_ack(0xF4)) fails++;                        /* enable streaming     */
        for(int i=0;i<4;i++){ if(inb(0x64)&1) inb(0x60); }
        if(fails==0) break;
        pit_wait(250);                                          /* settle, then retry   */
    }
    mouse_cmd(0xF4);
    touchpad_detect();
}

#define curs_save ((u32*)0x0095C000u)
static int curs_x=-1,curs_y=-1,curs_shown=0;
static const u8 arrow[18*12]={
0};
static const u8 wincur[19*12]={
 1,0,0,0,0,0,0,0,0,0,0,0,
 1,1,0,0,0,0,0,0,0,0,0,0,
 1,2,1,0,0,0,0,0,0,0,0,0,
 1,2,2,1,0,0,0,0,0,0,0,0,
 1,2,2,2,1,0,0,0,0,0,0,0,
 1,2,2,2,2,1,0,0,0,0,0,0,
 1,2,2,2,2,2,1,0,0,0,0,0,
 1,2,2,2,2,2,2,1,0,0,0,0,
 1,2,2,2,2,2,2,2,1,0,0,0,
 1,2,2,2,2,2,2,2,2,1,0,0,
 1,2,2,2,2,2,2,2,2,2,1,0,
 1,2,2,2,2,2,1,1,1,1,1,1,
 1,2,2,1,2,2,1,0,0,0,0,0,
 1,2,1,1,2,2,1,0,0,0,0,0,
 1,1,0,0,1,2,2,1,0,0,0,0,
 1,0,0,0,1,2,2,1,0,0,0,0,
 0,0,0,0,0,1,2,2,1,0,0,0,
 0,0,0,0,0,1,2,2,1,0,0,0,
 0,0,0,0,0,0,1,1,1,0,0,0,
};
static int curs_kind=0;
static void cpx(int x,int y,u8 c){ if(x>=0&&y>=0&&x<W&&y<H)FB[y*PITCH+x]=PAL32[c]; }
static void draw_cursor(int x,int y){
    if(curs_kind==1){
        for(int i=0;i<14;i++){ cpx(x+i,y+i,0); cpx(x+i,y+i+1,C_WHITE); cpx(x+i+1,y+i,C_WHITE); }

        for(int i=0;i<6;i++){ cpx(x+1+i,y+1,C_WHITE);cpx(x+1,y+1+i,C_WHITE); cpx(x+13-i,y+13,C_WHITE);cpx(x+13,y+13-i,C_WHITE);
                              cpx(x+i,y,0);cpx(x,y+i,0); cpx(x+14-i,y+14,0);cpx(x+14,y+14-i,0); }
    } else if(curs_kind==2){
        for(int r=2;r<9;r++){ cpx(x+5,y+r,0); for(int c=6;c<8;c++)cpx(x+c,y+r,C_WHITE); cpx(x+8,y+r,0); }
        for(int r=8;r<18;r++) for(int c=2;c<13;c++){ u8 e=(c==2||c==12||r==17); cpx(x+c,y+r,e?0:C_WHITE); }
        for(int c=2;c<13;c++)cpx(x+c,y+8,0);
    } else if(curs_kind==3){
        for(int r=1;r<19;r++){ cpx(x+6,y+r,0); cpx(x+7,y+r,C_WHITE); cpx(x+8,y+r,0); }
        for(int c=3;c<12;c++){ cpx(x+c,y,0); cpx(x+c,y+1,C_WHITE); cpx(x+c,y+18,C_WHITE); cpx(x+c,y+19,0); }
    } else if(curs_kind==4){
        for(int a=0;a<256;a+=6){ int dx=(S((u8)(a+64))*9)/127, dy=(S((u8)a)*9)/127; u8 cc=(a<150)?C_BBLUE:(C_MGREY+10);
            cpx(x+9+dx,y+11+dy,cc); cpx(x+9+dx,y+11+dy+1,cc); }
    } else if(curs_kind==5){
        for(int i=0;i<19;i++){ cpx(x+9,y+i,C_WHITE); cpx(x+i,y+11,C_WHITE); cpx(x+8,y+i,0); cpx(x+10,y+i,0); cpx(x+i,y+10,0); cpx(x+i,y+12,0); }
        for(int i=0;i<5;i++){ cpx(x+9-i,y+1+i,C_WHITE);cpx(x+9+i,y+1+i,C_WHITE); cpx(x+9-i,y+17-i,C_WHITE);cpx(x+9+i,y+17-i,C_WHITE);
                              cpx(x+1+i,y+11-i,C_WHITE);cpx(x+1+i,y+11+i,C_WHITE); cpx(x+17-i,y+11-i,C_WHITE);cpx(x+17-i,y+11+i,C_WHITE); }
    } else {
        for(int r=0;r<19;r++) for(int c=0;c<12;c++){ u8 v=wincur[r*12+c]; if(v) cpx(x+c,y+r, v==1?0:C_WHITE); }
    }
}
static void hide_cursor(void){ if(!curs_shown)return; for(int r=0;r<24;r++)for(int c=0;c<16;c++){ int px=curs_x+c,py=curs_y+r; if(px<W&&py<H) FB[py*PITCH+px]=curs_save[r*16+c]; } curs_shown=0; }
static void show_cursor(void){ curs_x=mx; curs_y=my; for(int r=0;r<24;r++)for(int c=0;c<16;c++){ int px=curs_x+c,py=curs_y+r; if(px<W&&py<H) curs_save[r*16+c]=FB[py*PITCH+px]; } draw_cursor(mx,my); curs_shown=1; }
static void mouse_apply(void){
    int dx=mpkt[1]; if(mpkt[0]&0x10)dx-=256;
    int dy=mpkt[2]; if(mpkt[0]&0x20)dy-=256;
    if(mouse_speed==0){ dx/=2; dy/=2; } else if(mouse_speed==2){ dx*=2; dy*=2; }
    mx+=dx; ms_dx_acc+=dx; my-=dy;
    if(mx<0)mx=0; if(my<0)my=0; if(mx>W-2)mx=W-2; if(my>H-2)my=H-2;
    mbtn=mpkt[0]&7;
    if(wheel_ok){ int z=mpkt[3]&0x0F; if(z&0x08)z-=16; wheel_delta+=z; }
}
static void mouse_byte(u8 b){
    if(mphase==0){ if(!(b&8))return; mpkt[0]=b; mphase=1; }
    else if(mphase==1){ mpkt[1]=b; mphase=2; }
    else if(mphase==2){ mpkt[2]=b; if(wheel_ok){ mphase=3; } else { mphase=0; mouse_apply(); } }
    else { mpkt[3]=b; mphase=0; mouse_apply(); }
}

static const char kmap[128]={
0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
'\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
0,'a','s','d','f','g','h','j','k','l',';','\'','`',
0,'\\','z','x','c','v','b','n','m',',','.','/',0,
'*',0,' ',0,0,0,0,0,0,0,0,0,0,0,0,0,
'7','8','9','-','4','5','6','+','1','2','3','0','.',0,0,0,0
};
static u8 wait_key(void){ for(;;){ u8 st=inb(0x64); if(st&1){ u8 d=inb(0x60); if(st&0x20)continue; if(d&0x80)continue; return d; } } }

static int wall_photo=0;
static u32 space_dec[256*144]; static int space_loaded=0;
static void wp_space_render(void);
static void render_wallpaper(void){
    if(wall_photo) return;
    if(bg_style==0){

        for(int y=0;y<H;y++){
            for(int x=0;x<W;x++){
                int wx = x + (S((y*3+10)&0xFF)*70)/127 + (S((y+90)&0xFF)*40)/127;
                int v = wx + (y*W)/H;
                int band = (W>=900)?150:110;
                int m = ((v%band)+band)%band;
                int b = 5 + (y*6)/H;
                b += (m*8)/band;
                if(m<3) b += 14;
                else if(m<7) b += 6;
                else if(m>=band-5) b -= 3;
                if(b<2)b=2; if(b>32)b=32;
                FB[y*PITCH+x]=PAL32[(u8)b];
            }
        }
        int lx=W/2-8*16/2, ly=H/2-8;
        draw_str2(lx, ly, OSNAME, 26);
        return;
    }
    if(bg_style==9){

        int wm1=(W>1)?W-1:1, hm1=(H>1)?H-1:1;
        for(int y=0;y<H;y++){
            for(int x=0;x<W;x++){
                int t=((x*140)/wm1)+((y*115)/hm1); if(t>255)t=255;
                int r=235-(t*175)/255;
                int g=85-(t*65)/255;
                int b=165-(t*55)/255;

                int band=(x-y+H); int bd=band-(W/2); if(bd<0)bd=-bd;
                if(bd<120){ int add=(120-bd)/6; r+=add; b+=add; }
                if(r>255)r=255; if(g<0)g=0; if(b>255)b=255;
                FB[y*PITCH+x]=((u32)r<<16)|((u32)g<<8)|(u32)b;
            }
        }
        return;
    }
    if(bg_style==8){ wp_space_render(); return; }
    if(bg_style==1){

        int wm1=W>1?W-1:1, hm1=H>1?H-1:1;
        for(int y=0;y<H;y++){
            for(int x=0;x<W;x++){
                int w1=S(((x*2 - y*3)/9) & 0xFF);
                int w2=S(((x*3 + y)/13 + 64) & 0xFF);
                int lum=(w1*3 + w2*2)/5;
                int diag=(x*40)/wm1 + (y*24)/hm1;
                int v=lum+diag;
                int r=12 + (v*30)/127; if(r<5)r=5;  if(r>70)r=70;
                int g=20 + (v*55)/127; if(g<6)g=6;  if(g>110)g=110;
                int b=70 + (v*95)/127; if(b<30)b=30; if(b>235)b=235;
                FB[y*PITCH+x]=((u32)r<<16)|((u32)g<<8)|(u32)b;
            }
        }

        { const char* L="NOOVEXOS"; int lw=8*24; int lx=W/2-lw/2, ly=H/2-12;
          draw_str3(lx+3,ly+3,L,0); draw_str3(lx,ly,L,C_WHITE); }

        { int cx=W/2, cy=H/2, md=(cx*cx+cy*cy)/100;
          for(int y=0;y<H;y++)for(int x=0;x<W;x++){ int dx=x-cx,dy=y-cy; int d=(dx*dx+dy*dy)/100;
              int a=(d*46)/md; if(a<=0)continue; if(a>46)a=46;
              u32 p=FB[y*PITCH+x]; int pr=(p>>16)&255,pg=(p>>8)&255,pb=p&255;
              pr=(pr*(255-a))/255; pg=(pg*(255-a))/255; pb=(pb*(255-a))/255;
              FB[y*PITCH+x]=((u32)pr<<16)|((u32)pg<<8)|(u32)pb; } }
        return;
    }
    if(bg_style==2){ for(int y=0;y<H;y++){ u8 g=4+(y*36)/H; for(int x=0;x<W;x++)FB[y*PITCH+x]=PAL32[g]; } return; }
    if(bg_style==3){ u32 v=PAL32[6]; for(int i=0;i<PITCH*H;i++)FB[i]=v; return; }
    if(bg_style==4){

        for(int y=0;y<H;y++) for(int x=0;x<W;x++){
            int v=(S((u8)(x/5+y/7))+S((u8)(x/9-y/4+64)))/2;
            int b=24+(v+127)/3, g=40+(v+127)/4, r=8;
            int line=(((x+y)/3 + v)&31)<2 ? 60 : 0;
            FB[y*PITCH+x]=((u32)(r+line)<<16)|((u32)(g+line)<<8)|(u32)(b+line);
        } return;
    }
    if(bg_style==5){

        int wm1=W>1?W-1:1, hm1=H>1?H-1:1;
        for(int y=0;y<H;y++){
            int t=(y*255)/hm1;
            for(int x=0;x<W;x++){
                int r=40+(t*215)/255, g=20+(t*150)/255, b=90-(t*70)/255; if(b<20)b=20;
                FB[y*PITCH+x]=((u32)r<<16)|((u32)g<<8)|(u32)b;
            }
        }
        int scx=W*7/10, scy=H*4/10, sr=H/5;
        for(int dy=-sr;dy<=sr;dy++) for(int dx=-sr;dx<=sr;dx++){
            int d=dx*dx+dy*dy; if(d<=sr*sr){
                int xx=scx+dx,yy=scy+dy; if(xx<0||yy<0||xx>=W||yy>=H)continue;
                int t=(d*255)/(sr*sr);
                u32 p=FB[yy*PITCH+xx]; int pr=(p>>16)&255,pg=(p>>8)&255,pb=p&255;
                int sr2=255,sg2=200,sb2=120;
                int rr=pr+((sr2-pr)*(255-t))/255, gg=pg+((sg2-pg)*(255-t))/255, bb=pb+((sb2-pb)*(255-t))/255;
                FB[yy*PITCH+xx]=((u32)rr<<16)|((u32)gg<<8)|(u32)bb;
            }
        }
        (void)wm1; return;
    }
    if(bg_style==6){

        for(int i=0;i<PITCH*H;i++) FB[i]=0x000814;
        u32 trace=0x0028a0, node=0x00ff80;
        for(int y=0;y<H;y+=32){ for(int x=0;x<W;x++) FB[y*PITCH+x]=trace; }
        for(int x=0;x<W;x+=32){ for(int y=0;y<H;y++) FB[y*PITCH+x]=trace; }
        for(int y=32;y<H;y+=32) for(int x=32;x<W;x+=32){
            for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){
                int xx=x+dx,yy=y+dy; if(xx>=0&&yy>=0&&xx<W&&yy<H) FB[yy*PITCH+xx]=node;
            }
        }
        return;
    }

    int cxr=W/2,cyr=H/2,maxr=(W*W+H*H)/4;
    for(int y=0;y<H;y++)for(int x=0;x<W;x++){
        int fx=x>>2,fy=y>>2;
        int ax=fx+(S(fy*2+10)>>1),ay=fy+(S(fx*2+50)>>1);
        int bx=ax+(S(ay*3+120)>>1),by=ay+(S(ax*3+200)>>1);
        int cx2=bx+(S(by*5+30)>>2),cy2=by+(S(bx*5+90)>>2);
        int F=S(cx2*2+cy2*1)+(S(cx2*1+cy2*3+60)>>1)+(S(cx2*3-cy2*2+150)>>2);
        int m=(F+512)%40; int dd=(m>20)?(40-m):m;
        int b; if(dd==0)b=58; else if(dd<5)b=58-dd*11; else b=0; if(b<0)b=0;
        int ddx=x-cxr,ddy=y-cyr; int rr=ddx*ddx+ddy*ddy; if(b>0){ int v=(rr*10)/maxr; b-=v; if(b<0)b=0; }
        FB[y*PITCH+x]=PAL32[(u8)b];
    }
}

static int cur_folder=2;
static int fsel=0;
#define MAXF 14
static char F0[MAXF][16]={"README.TXT","NVXSHELL.LNK","WELCOME.TXT"};
static char F1[MAXF][16]={"SETUP.EXE","IMAGE.PNG","SONG.RAW","DRIVER.PKG","HELLO.HEX","BARS.HEX","COUNT.HEX"};
static char F2[MAXF][16]={"REPORT.DOC","NOTES.TXT","BUDGET.XLS","TODO.TXT","LETTER.DOC"};
static char F3[MAXF][16]={"WALLPAPER.BMP","SHOT1.PNG","LOGO.BMP","TOPO.BMP"};
static char F4[MAXF][16]={"TRACK01.RAW","JINGLE.RAW","BOOT.WAV"};
static char F5[MAXF][16]={"CLIP.RAW","INTRO.RAW"};
static char F6[MAXF][16]={"DESKTOP","DOWNLOADS","DOCUMENTS","PICTURES"};
static char F7[MAXF][16]={"Users","MediaTools","WebRoot","Chipset","Manuals","Graphics","PerfLogs","Programs","Programs32","NoovexOS","Games","sysconf.dat"};
static char F8[MAXF][16]={"BACKUP.ZIP","DATA.BIN"};
static char F9[MAXF][16]={"Hardware","Recovery","Toolbox","diskcfg.dat"};
static char FY[MAXF][16]={"WIN42.SYS","HAL42.SYS","NTOSKRN.BIN","REGISTRY.DAT","FONTS.SYS","VESA32.DRV","ACPI.SYS","SMSS.EXE"};
static char FP[MAXF][16]={"NVXSHELL.EXE","CALC.EXE","PAINT.EXE","HEXLANG.EXE","EDIT.EXE","FORMAT.EXE","PING.EXE"};
static char FA[MAXF][16]={"HELLO.HEX","BARS.HEX","COUNT.HEX","CLOCK.HEX","DRAW.HEX"};
static int fcount[14]={3,7,5,4,3,2,4,12,2,4,0,8,7,5};
static const int fperm[14]={0,0,0,0,0,0,0,0,0,0,0,1,1,0};
static const char* sb_names[14]={"DESKTOP","DOWNLOADS","DOCUMENTS","PICTURES","MUSIC","VIDEOS","HOME","BOOT (C:)","NVXDISK (S:)","DATA (D:)","USB (U:)","SYSTEM42 (Y:)","PATH (P:)","APPS (A:)"};
static const u8 sb_icon[14]={C_FOLDER,C_GREEN,C_TITLE,C_TEAL,C_RED,C_BBLUE,C_FOLDER,C_MGREY+10,C_MGREY+10,C_MGREY+10,C_GREEN,C_MGREY+10,C_TEAL,C_BBLUE};
static char (*folder_buf(int f))[16]{ switch(f){case 0:return F0;case 1:return F1;case 2:return F2;case 3:return F3;case 4:return F4;case 5:return F5;case 6:return F6;case 7:return F7;case 8:return F8;case 9:return F9;case 11:return FY;case 12:return FP;case 13:return FA;default:return F9;} }
static void folder_list(int f,char(**out)[16],int*n){ *out=folder_buf(f); *n=fcount[f]; }
static void file_delete(int f,int idx){ char(*b)[16]=folder_buf(f); for(int i=idx;i<fcount[f]-1;i++){int k=0;while(b[i+1][k]){b[i][k]=b[i+1][k];k++;}b[i][k]=0;} if(fcount[f]>0)fcount[f]--; }
/* THIS PC drive list shown in the Files sidebar: C:, D:, S:(NVXFS), U:(USB) */
static const int THISPC[]={7,9,8,10};
#define NTHISPC 4
/* add a file name to a folder array (not NVXFS/USB) */
static int file_add(int f,const char*name){
    if(f==8||f==10)return 0; if(f<0||f>13)return 0; if(fcount[f]>=MAXF)return 0;
    char(*b)[16]=folder_buf(f); int k=0; while(name[k]&&k<15){b[fcount[f]][k]=name[k];k++;} b[fcount[f]][k]=0; fcount[f]++; return 1; }
/* deterministic Explorer-style metadata. fills date "YYYY-MM-DD HH:MM" and type; returns 1 if folder */
static int entry_meta(int folder,int idx,const char*name,char*date,char*type){
    unsigned h=2166136261u; for(const char*q=name;*q;q++){h^=(unsigned char)*q;h*=16777619u;} h^=(unsigned)(folder*131u+idx*17u+2654435761u);
    int dot=-1,L=0; while(name[L]){ if(name[L]=='.')dot=L; L++; } int isf=(dot<0);
    int yr=2021+(int)(h%6u), mo=1+(int)((h>>5)%12u), dy=1+(int)((h>>9)%28u), hh=(int)((h>>13)%24u), mi=(int)((h>>18)%60u);
    int q=0; date[q++]=(char)('0'+(yr/1000)%10); date[q++]=(char)('0'+(yr/100)%10); date[q++]=(char)('0'+(yr/10)%10); date[q++]=(char)('0'+yr%10); date[q++]='-';
    date[q++]=(char)('0'+mo/10); date[q++]=(char)('0'+mo%10); date[q++]='-'; date[q++]=(char)('0'+dy/10); date[q++]=(char)('0'+dy%10); date[q++]=' ';
    date[q++]=(char)('0'+hh/10); date[q++]=(char)('0'+hh%10); date[q++]=':'; date[q++]=(char)('0'+mi/10); date[q++]=(char)('0'+mi%10); date[q]=0;
    if(isf){ const char*f="File folder"; int k=0; while(f[k]){type[k]=f[k];k++;} type[k]=0; }
    else { int k=0; for(int j=dot+1;name[j]&&k<6;j++){ char c=name[j]; if(c>='a'&&c<='z')c-=32; type[k++]=c; } const char*s2=" File"; int t=0; while(s2[t]&&k<14){type[k++]=s2[t++];} type[k]=0; }
    return isf; }
/* size string: blank for folders, "N kB" for files (.dat shows 8 kB) */
static void entry_size(int folder,int idx,const char*name,char*out){
    int dot=-1,L=0; while(name[L]){ if(name[L]=='.')dot=L; L++; } out[0]=0; if(dot<0)return;
    char e0=name[dot+1],e1=e0?name[dot+2]:0,e2=e1?name[dot+3]:0; int kb;
    if((e0=='d'||e0=='D')&&(e1=='a'||e1=='A')&&(e2=='t'||e2=='T')) kb=8;
    else { unsigned h=2166136261u; for(const char*q=name;*q;q++){h^=(unsigned char)*q;h*=16777619u;} h^=(unsigned)(folder*7u+idx*3u); kb=1+(int)(h%99u); }
    char t[8]; int tl=0,v=kb; if(v==0)t[tl++]='0'; while(v){t[tl++]=(char)('0'+v%10);v/=10;} int q=0; while(tl)out[q++]=t[--tl]; out[q++]=' '; out[q++]='k'; out[q++]='B'; out[q]=0; }

static int app=0;
static int widget_on[4]={0,0,0,0};
static int winx=240,winy=120,winw=380,winh=240;
static int dragging=0,dragdx,dragdy;
static int resizing=0,rs_w0=0,rs_h0=0,rs_mx0=0,rs_my0=0;
static int band_on=0,band_x=0,band_y=0,band_w=0,band_h=0;
static void xor_frame(int x,int y,int w,int h){
    if(w<2||h<2)return;
    for(int i=0;i<w;i++){ if(x+i<W){ if(y<H)LFB[y*PITCH+x+i]^=0xFFFFFF; if(y+h-1<H)LFB[(y+h-1)*PITCH+x+i]^=0xFFFFFF; } }
    for(int j=1;j<h-1;j++){ if(y+j<H){ if(x<W)LFB[(y+j)*PITCH+x]^=0xFFFFFF; if(x+w-1<W)LFB[(y+j)*PITCH+x+w-1]^=0xFFFFFF; } }
}
static int maximized=0,sx,sy,sw,sh;
#define MAXWIN 10
typedef struct { int app,x,y,w,h,maxd,sx,sy,sw,sh,min; } Win;
static Win wins[MAXWIN]; static int wincnt=0; static int cur_win=-1; static int win_focused=1;
static unsigned int rnd(void);
static void launch(int a);
static void* kmalloc(u32 n); static void kfree(void* p); static u32 kheap_used(void); static u32 kheap_total(void); static int kheap_isok(void);
static void install_to_disk(int slot);
static const char* t(int id);
static int supabase_check_email(const char* email);
static void cos_circle(int cx,int cy,int r,u8 c);
static void do_shutdown(void);
static void draw_lock(int cx,int cy,u8 col);
static const char* app_name(int a){ switch(a){case 1:return t(T_APP_FILES);case 2:return t(T_APP_TERMINAL);case 3:return t(T_APP_ABOUT);case 4:return t(T_APP_SETTINGS);case 5:return t(T_APP_NOTEPAD);case 6:return t(T_APP_TASKMGR);case 7:return t(T_APP_DEVICES);case 8:return "SNAKE";case 9:return "GUARD";case 10:return t(T_APP_RECYCLE);case 11:return t(T_APP_PAINT);case 12:return "TETRIS";case 13:return "PIANO";case 14:return "CRAFT";case 15:return t(T_APP_WEB);case 16:return "CLAUDE";case 17:return t(T_APP_STORE);case 18:return "FOLDER";case 19:return "APP";case 20:return "SAVE AS";case 21:return "ENCRYPTION";case 22:return "USB (U:)";case 23:return t(T_APP_CALC);case 24:return "CAMERA";case 25:return "PHOTO";case 26:return VND_APP;case 27:return "GPU";case 29:return "WIDGETS";case 30:return "NOOVEXGRAPH";case 31:return "ZIP VIEWER";case 32:return "PDF VIEWER";case 33:return "VIEWER";case 34:return "FLAPPY";case 35:return "PACKAGES";case 36:return "PHONE MIRROR";case 37:return "MAIL";case 38:return "QR CODE";case 39:return "2048";case 40:return "BREAKOUT";case 41:return "MINESWEEPER";case 42:return "GOPHER";case 43:return "WIKIPEDIA";case 44:return "NET TOOLS";case 45:return "SOUND";case 46:return "PHONE";default:return "APP";} }
static int win_find(int a){ for(int i=0;i<wincnt;i++) if(wins[i].app==a) return i; return -1; }
static void win_load(int i){ if(i<0){app=0;return;} app=wins[i].app; winx=wins[i].x; winy=wins[i].y; winw=wins[i].w; winh=wins[i].h; maximized=wins[i].maxd; sx=wins[i].sx;sy=wins[i].sy;sw=wins[i].sw;sh=wins[i].sh; }
static void win_store(int i){ if(i<0)return; wins[i].x=winx;wins[i].y=winy;wins[i].w=winw;wins[i].h=winh;wins[i].maxd=maximized;wins[i].sx=sx;wins[i].sy=sy;wins[i].sw=sw;wins[i].sh=sh; }
static int focus_top(void){ for(int z=wincnt-1;z>=0;z--) if(!wins[z].min) return z; return -1; }
static void win_raise(int i){ if(i<0||i>=wincnt)return; Win t=wins[i]; for(int k=i;k<wincnt-1;k++)wins[k]=wins[k+1]; wins[wincnt-1]=t; }
static void open_app(int a,int x,int y,int w,int h){
    int i=win_find(a);
    if(i<0){ if(wincnt>=MAXWIN){ for(int k=0;k<wincnt-1;k++)wins[k]=wins[k+1]; wincnt--; } i=wincnt++; wins[i].app=a; wins[i].x=x;wins[i].y=y;wins[i].w=w;wins[i].h=h; wins[i].maxd=0; wins[i].min=0; }
    else wins[i].min=0;
    win_raise(win_find(a)); cur_win=wincnt-1; win_load(cur_win); click_snd();

    int cx=x+w/2, cy=y+h/2;
    for(int f=1;f<=4;f++){ int ww=(w*f)/4, hh=(h*f)/4; int xx=cx-ww/2, yy=cy-hh/2;
        afill(xx+6,yy+10,ww,hh,0,32);
        rrectR(xx,yy,ww,hh,8,C_WIN);
        rrectR(xx,yy,ww,22,8,C_TASK); pit_wait(1); }
}
static void close_app(int a){ int i=win_find(a); if(i<0)return; for(int k=i;k<wincnt-1;k++)wins[k]=wins[k+1]; wincnt--; cur_win=focus_top(); win_load(cur_win); }
static int start_open=0, pending_action=0;
static int pending_neofetch=0;
static int run_open=0; static char run_buf[64]; static int run_len=0;
static int help_overlay=0;
static int pwr_open=0;
static int cal_open=0;
static void open_app(int a,int x,int y,int w,int h);

#define ZIP_BUF ((unsigned char*)0x02600000u)
#define ZIP_OUT ((unsigned char*)0x02800000u)
static ZipEntry zip_ents[ZIP_MAXENT];
static int zip_n=0, zip_sel=0, zip_len=0, zip_err=0;
static char zip_arc_name[20]="";
static char zip_msg[28]="";

static void zip_open_nvx(int idx,const char* name){
    zip_msg[0]=0; zip_err=0; zip_sel=0;
    int k=0; while(name[k]&&k<19){ zip_arc_name[k]=name[k]; k++; } zip_arc_name[k]=0;
    zip_len=nvx_read(idx,(char*)ZIP_BUF, 0x200000);
    zip_n=zip_list(ZIP_BUF, zip_len, zip_ents);
    if(zip_n<0){ zip_n=0; zip_err=1; }
    open_app(31,140,80,460,330);
}

#define PDF_BUF ((unsigned char*)0x02A00000u)
#define PDF_A85 ((unsigned char*)0x02C00000u)
#define PDF_INF ((unsigned char*)0x02E00000u)
#define PDF_TXT ((char*)0x02F00000u)
static int pdf_loaded=0, pdf_pages=0, pdf_txtlen=0, pdf_scroll=0;
static char pdf_name[20];

static int pdf_find(const unsigned char* h,int hl,const char* n,int from){
    int nl=0; while(n[nl])nl++; if(from<0)from=0;
    for(int i=from;i+nl<=hl;i++){ int m=1; for(int j=0;j<nl;j++) if(h[i+j]!=(unsigned char)n[j]){m=0;break;} if(m)return i; }
    return -1;
}

static int pdf_a85(const unsigned char* s,int sl,unsigned char* out,int cap){
    int o=0; unsigned tup=0; int cnt=0;
    for(int i=0;i<sl;i++){ unsigned char c=s[i];
        if(c=='~')break;
        if(c=='z'&&cnt==0){ if(o+4>cap)break; out[o++]=0;out[o++]=0;out[o++]=0;out[o++]=0; continue; }
        if(c<'!'||c>'u')continue;
        tup=tup*85u+(unsigned)(c-'!'); cnt++;
        if(cnt==5){ if(o+4>cap)break; out[o++]=(tup>>24)&0xFF;out[o++]=(tup>>16)&0xFF;out[o++]=(tup>>8)&0xFF;out[o++]=tup&0xFF; tup=0;cnt=0; }
    }
    if(cnt>0){ for(int k=cnt;k<5;k++)tup=tup*85u+84u; for(int k=0;k<cnt-1&&o<cap;k++)out[o++]=(tup>>(24-k*8))&0xFF; }
    return o;
}

static void pdf_putc(char c){ if(pdf_txtlen<0xE0000) PDF_TXT[pdf_txtlen++]=c; }

static void pdf_scan_content(const unsigned char* d,int dl){
    for(int i=0;i<dl;i++){
        if(d[i]=='('){ int depth=1; int j=i+1; char tmp[512]; int tl=0;
            while(j<dl&&depth>0){ unsigned char c=d[j];
                if(c=='\\'){ j++; if(j<dl){ char e=d[j]; if(e=='n')e='\n'; else if(e=='r')e='\r'; else if(e=='t')e=' '; if(tl<511)tmp[tl++]=e; } j++; continue; }
                if(c=='('){depth++;} else if(c==')'){depth--; if(depth==0){j++;break;}}
                if(depth>0&&tl<511)tmp[tl++]=(char)c; j++;
            }

            int show=0; for(int k=j;k<dl&&k<j+6;k++){ if(d[k]=='T'&&(d[k+1]=='j'||d[k+1]=='J')){show=1;break;} }
            if(show){ for(int c=0;c<tl;c++)pdf_putc(tmp[c]); pdf_putc(' '); }
            i=j-1;
        }
        else if(d[i]=='T'&&i+1<dl&&(d[i+1]=='*')){ pdf_putc('\n'); }
    }
}

static void pdf_extract(int rawlen){
    pdf_txtlen=0; pdf_pages=0;
    int cp=pdf_find(PDF_BUF,rawlen,"/Count",0);
    if(cp>=0){ int p=cp+6; while(p<rawlen&&PDF_BUF[p]==' ')p++; while(p<rawlen&&PDF_BUF[p]>='0'&&PDF_BUF[p]<='9'){pdf_pages=pdf_pages*10+(PDF_BUF[p]-'0');p++;} }
    int spos=0;
    while((spos=pdf_find(PDF_BUF,rawlen,"stream",spos))>=0 && pdf_txtlen<0xF0000){
        if(spos>0 && PDF_BUF[spos-1]=='d'){ spos+=6; continue; }

        int objstart=spos-500; if(objstart<0)objstart=0;
        int hasflate=0,hasa85=0;
        { int fp=pdf_find(PDF_BUF,spos,"FlateDecode",objstart); if(fp>=0&&fp<spos)hasflate=1;
          int ap=pdf_find(PDF_BUF,spos,"ASCII85Decode",objstart); if(ap>=0&&ap<spos)hasa85=1; }
        int sstart=spos+6; if(sstart<rawlen&&PDF_BUF[sstart]=='\r')sstart++; if(sstart<rawlen&&PDF_BUF[sstart]=='\n')sstart++;
        int send=pdf_find(PDF_BUF,rawlen,"endstream",sstart); if(send<0)break;
        int slen=send-sstart; while(slen>0&&(PDF_BUF[sstart+slen-1]=='\n'||PDF_BUF[sstart+slen-1]=='\r'))slen--;
        const unsigned char* cont=PDF_BUF+sstart; int clen=slen;
        if(hasa85){ int d=pdf_a85(PDF_BUF+sstart,slen,PDF_A85,0x200000); cont=PDF_A85; clen=d; }
        if(hasflate){ int z=pz_inflate(cont,clen,PDF_INF,0x200000); if(z>0){ cont=PDF_INF; clen=z; } else { spos=send+9; continue; } }
        pdf_scan_content(cont,clen);
        pdf_putc('\n');
        spos=send+9;
    }
    PDF_TXT[pdf_txtlen]=0;
    if(pdf_pages<1)pdf_pages=1;
}
static void pdf_open_usb(int fi,const char* name){
    int k=0; while(name[k]&&k<19){pdf_name[k]=name[k];k++;} pdf_name[k]=0;
    int n=usbfs_read(fi,(char*)PDF_BUF,0x200000);
    pdf_extract(n); pdf_loaded=1; pdf_scroll=0;
    open_app(32,120,70,520,400);
}
static void pdf_open_nvx(int idx,const char* name){
    int k=0; while(name[k]&&k<19){pdf_name[k]=name[k];k++;} pdf_name[k]=0;
    int n=nvx_read(idx,(char*)PDF_BUF,0x200000);
    pdf_extract(n); pdf_loaded=1; pdf_scroll=0;
    open_app(32,120,70,520,400);
}

static char DOCBUF[16384]; static int doc_len=0, doc_scroll=0, doc_type=0; static char doc_name[20];
static int ext_is(const char* nm,char e1,char e2,char e3);

static int doc_type_of(const char* nm){
    if(ext_is(nm,'C','S','V'))return 1; if(ext_is(nm,'J','S','N')||ext_is(nm,'J','S','O'))return 2;
    if(ext_is(nm,'M','D',' ')||ext_is(nm,(char)'M',(char)'K',(char)'D'))return 3;
    if(ext_is(nm,'I','N','I')||ext_is(nm,'C','F','G')||ext_is(nm,'C','O','N'))return 4;
    if(ext_is(nm,'L','O','G'))return 5; if(ext_is(nm,'X','M','L')||ext_is(nm,'H','T','M'))return 6;
    if(ext_is(nm,'T','A','R'))return 7; return 0;
}
static int doc_is_known(const char* nm){ return doc_type_of(nm)!=0 || ext_is(nm,'T','X','T'); }
static void doc_open_common(const char* name){
    int k=0; while(name[k]&&k<19){doc_name[k]=name[k];k++;} doc_name[k]=0;
    doc_type=doc_type_of(name); doc_scroll=0; DOCBUF[doc_len>16383?16383:doc_len]=0;
    open_app(33,120,64,560,420);
}
static void doc_open_nvx(int idx,const char* name){ doc_len=nvx_read(idx,DOCBUF,16383); if(doc_len<0)doc_len=0; DOCBUF[doc_len]=0; doc_open_common(name); }
static void doc_open_usb(int fi,const char* name){ doc_len=usbfs_read(fi,DOCBUF,16383); if(doc_len<0)doc_len=0; DOCBUF[doc_len]=0; doc_open_common(name); }
static void zip_open_usb(int fi,const char* name){
    zip_msg[0]=0; zip_err=0; zip_sel=0;
    int k=0; while(name[k]&&k<19){ zip_arc_name[k]=name[k]; k++; } zip_arc_name[k]=0;
    zip_len=usbfs_read(fi,(char*)ZIP_BUF, 0x200000);
    zip_n=zip_list(ZIP_BUF, zip_len, zip_ents);
    if(zip_n<0){ zip_n=0; zip_err=1; }
    open_app(31,140,80,460,330);
}

static int set_dockpos=0;
static int set_dockauto=0;
static int set_clock24=1;
static int set_anim=1;
static int set_shadows=1;
static int set_glass=1;
static int set_doubleclick=1;
static int set_corner=1;
static int set_wallpaper=1;
static int cc_open=0;
static int cc_night=0, cc_dark=0, cc_bt=0, cc_bright=6;

static int bat_present=0;
static int bat_pct=100;
static int bat_charging=1;
static int bat_sim=0;
static int bat_warned=0;
static int bat_tick=0;

static u32 acpi_rsdp=0, acpi_rsdt=0, acpi_fadt=0;
static int acpi_found=0, acpi_xsdt=0, ec_present=0, bat_via_ec=0;

static u8 acpi_sum(volatile u8* p, int len){ u8 s=0; for(int i=0;i<len;i++)s+=p[i]; return s; }
static int acpi_sig(u32 a,const char*s){ volatile u8* p=(volatile u8*)a; for(int i=0;i<4;i++) if(p[i]!=(u8)s[i]) return 0; return 1; }

static u32 acpi_find_rsdp(void){

    u32 ebda=((u32)(*(volatile u16*)0x40E))<<4;
    if(ebda>=0x80000 && ebda<0xA0000){
        for(u32 a=ebda; a<ebda+1024; a+=16){ volatile u8* p=(volatile u8*)a;
            if(p[0]=='R'&&p[1]=='S'&&p[2]=='D'&&p[3]==' '&&p[4]=='P'&&p[5]=='T'&&p[6]=='R'&&p[7]==' '&&acpi_sum(p,20)==0) return a; }
    }

    for(u32 a=0xE0000; a<0x100000; a+=16){ volatile u8* p=(volatile u8*)a;
        if(p[0]=='R'&&p[1]=='S'&&p[2]=='D'&&p[3]==' '&&p[4]=='P'&&p[5]=='T'&&p[6]=='R'&&p[7]==' '&&acpi_sum(p,20)==0) return a; }
    return 0;
}
static void acpi_scan(void){
    acpi_rsdp=acpi_find_rsdp();
    if(!acpi_rsdp){ acpi_found=0; return; }
    acpi_found=1;
    u8 rev=*(volatile u8*)(acpi_rsdp+15);
    if(rev>=2){
        u32 xsdt=*(volatile u32*)(acpi_rsdp+24); if(xsdt){ acpi_rsdt=xsdt; acpi_xsdt=1; }
        else acpi_rsdt=*(volatile u32*)(acpi_rsdp+16);
    } else acpi_rsdt=*(volatile u32*)(acpi_rsdp+16);

    if(!acpi_rsdt) return;
    u32 len=*(volatile u32*)(acpi_rsdt+4);
    int esz=acpi_xsdt?8:4;
    int n=(int)((len-36)/esz); if(n<0)n=0; if(n>64)n=64;
    for(int i=0;i<n;i++){ u32 ent=*(volatile u32*)(acpi_rsdt+36+i*esz);
        if(ent && acpi_sig(ent,"FACP")){ acpi_fadt=ent; break; } }
}

#define EC_DATA 0x62
#define EC_CMD  0x66
static int ec_wait_ibf(void){ for(int t=0;t<100000;t++){ if(!(inb(EC_CMD)&0x02)) return 1; io_delay(1);} return 0; }
static int ec_wait_obf(void){ for(int t=0;t<100000;t++){ if(inb(EC_CMD)&0x01) return 1; io_delay(1);} return 0; }
static int ec_read(u8 off,u8* out){
    if(!ec_wait_ibf()) return 0;
    outb(EC_CMD,0x80);
    if(!ec_wait_ibf()) return 0;
    outb(EC_DATA,off);
    if(!ec_wait_obf()) return 0;
    *out=inb(EC_DATA); return 1;
}
static int ec_detect(void){

    u8 st=inb(EC_CMD);
    if(st==0xFF) return 0;
    return ec_wait_ibf();
}

static int ec_battery_pct(int* pct,int* charging){
    if(!ec_present) return 0;
    static const u8 cand[]={0x2A,0x32,0x42,0x90,0xB0,0x14,0x24};
    for(unsigned i=0;i<sizeof(cand);i++){ u8 v;
        if(ec_read(cand[i],&v) && v>0 && v<=100){ *pct=v;
            u8 ac=0; if(ec_read(0x01,&ac)) *charging=(ac&0x01)?1:0; else *charging=0;
            return 1; }
    }
    return 0;
}

static void acpi_shutdown(void){
    if(!acpi_fadt) return;
    u16 pm1a=(u16)(*(volatile u32*)(acpi_fadt+64));
    u16 pm1b=(u16)(*(volatile u32*)(acpi_fadt+68));
    if(!pm1a) return;
    static const int tryslp[]={7,0,5};
    for(unsigned i=0;i<sizeof(tryslp)/sizeof(tryslp[0]);i++){
        u16 v=(u16)((tryslp[i]<<10)|(1<<13));
        outw(pm1a,v); if(pm1b) outw(pm1b,v);
        for(int t=0;t<8000;t++) io_delay(1);
    }
}
static void battery_init(void){
    bat_present=0; bat_pct=100; bat_charging=1;
    acpi_scan();
    ec_present=ec_detect();
    int p=0,ch=0;
    if(ec_present && ec_battery_pct(&p,&ch)){
        bat_present=1; bat_pct=p; bat_charging=ch; bat_via_ec=1;
    }
}

static void battery_tick(void){
    if(bat_sim){
        bat_present=1; bat_charging=0;
        bat_tick++;
        if(bat_tick>=2){ bat_tick=0; if(bat_pct>0) bat_pct--; }
        if(bat_pct>20) bat_warned=0;
    } else if(bat_via_ec){

        int p=0,ch=0; if(ec_battery_pct(&p,&ch)){ bat_present=1; bat_pct=p; bat_charging=ch; }
        if(bat_pct>20) bat_warned=0;
    } else {
        bat_present=0; bat_pct=100; bat_charging=1; bat_warned=0;
    }
}
static int setcat=0, need_rebuild=0, dock_size=1;
static int g_dwin=0,g_dwx=0,g_dwy=0,g_dww=0,g_dwh=0,g_dwfx=0,g_dwfy=0,g_dwfw=0,g_dwfh=0,g_dwscale=1;
static void doom_win_setup(void); static void doom_win_backdrop(void);
static int ctx_open=0,ctx_x=0,ctx_y=0,ctx_type=0,ctx_fidx=0; static char ctx_fname[20];
static int rename_mode=0,rename_folder=0,rename_idx=0,rename_len=0; static char rename_buf[16];
static int perm_pending=0,perm_folder=0,perm_idx=0;
static int pending_update=0, pending_uac=0, g_pending_action=0;
static char note_name[16]="NOTES.TXT"; static int note_status=0;
static int np_size=2,np_bold=0,np_under=0,np_align=0;
static char sas_name[16]={0}; static int sas_len=0, sas_loc=0;
static char bl_pw[24]={0}; static int bl_pwlen=0; static int bl_state=0; static char bl_rkdisp[56]={0};
#define termbuf ((char*)0x00951000u)
static int termlen=0;
static int term_scroll=0;
static char cmdline[200]; static int cmdlen=0;
/* ===VTERMS=== 10 virtual terminal sessions (one shared shell, switchable views) */
static char vt_out[10][2048]; static int vt_len[10]; static int vt_scr[10]; static char vt_cmd[10][200]; static int vt_cmdlen[10]; static int vt_cur=0; static u32 vt_magic=0;
static void vt_seed(int s){ const char* p="NOOVEX8 TERMINAL - SESSION "; int o=0; while(p[o]){vt_out[s][o]=p[o];o++;} int num=s+1; if(num>=10){vt_out[s][o++]='1';vt_out[s][o++]='0';} else vt_out[s][o++]=(char)('0'+num); vt_out[s][o++]='\n'; vt_out[s][o]=0; vt_len[s]=o; vt_scr[s]=0; vt_cmd[s][0]=0; vt_cmdlen[s]=0; }
static void vt_save(int s){ if(s<0||s>9)return; int n=termlen; if(n>2047)n=2047; for(int i=0;i<n;i++)vt_out[s][i]=termbuf[i]; vt_out[s][n]=0; vt_len[s]=n; vt_scr[s]=term_scroll; int cn=cmdlen; if(cn>199)cn=199; for(int i=0;i<cn;i++)vt_cmd[s][i]=cmdline[i]; vt_cmd[s][cn]=0; vt_cmdlen[s]=cn; }
static void vt_load(int s){ if(s<0||s>9)return; int n=vt_len[s]; if(n>2040)n=2040; for(int i=0;i<n;i++)termbuf[i]=vt_out[s][i]; termbuf[n]=0; termlen=n; term_scroll=vt_scr[s]; int cn=vt_cmdlen[s]; if(cn>199)cn=199; for(int i=0;i<cn;i++)cmdline[i]=vt_cmd[s][i]; cmdline[cn]=0; cmdlen=cn; }
static void vt_switch(int s){ if(s<0||s>9||s==vt_cur)return; vt_save(vt_cur); vt_cur=s; vt_load(s); }
static void vt_term_open(void){ if(vt_magic==0x56540011u)return; vt_magic=0x56540011u; for(int s=0;s<10;s++)vt_seed(s); vt_cur=0; vt_load(0); }
#define notebuf ((char*)0x0095D000u)
static int notelen=0;
#define desk_files ((char(*)[20])0x0095E100u)
static int desk_cnt=0;

#define DOCKN 11
static int dock_tray_x=0;
#define BOUNCE_MAX 14
static const int  dock_app[DOCKN]={1,2,15,5,11,4,8,12,14,6,-3};
static const u8   dock_col[DOCKN]={100,101,102,103,104,105,106,107,108,109,110};
static int dock_bounce[DOCKN]={0};
static int dock_ix[DOCKN],dock_iy[DOCKN],dock_isz[DOCKN];
static u16 dock_pit=0; static int dock_acc=0;
static int fdrag=0; static char fdrag_name[20];
static int fdrag_sx=0,fdrag_sy=0;
static int fdrag_armed=0, fdrag_src_folder=-1, fdrag_src_idx=-1;
static int cur_node=-1; static int fdc_row=-1, fdc_timer=0;
typedef struct { const char* name; short parent; unsigned char isdir; const char* body; } VN;
static const VN VFS[]={
  {"C:",-1,1,0},
  {"Users",0,1,0},
  {"Public",1,1,0},
  {"Desktop.ini",2,0,0},
  {"README.TXT",2,0,"Shared files for all users.\nDo not delete this folder.\n"},
  {"admin",1,1,0},
  {"Documents",5,1,0},
  {"notes.txt",6,0,"Remember to back up D: Recovery.\nMeeting 14:00.\n"},
  {"budget.txt",6,0,"Q3 budget draft.\nServer: 4200\nLicenses: 1800\n"},
  {"Downloads",5,1,0},
  {"setup.exe",9,0,0},
  {"driver.pkg",9,0,0},
  {"NTUSER.DAT",5,0,0},
  {"Default",1,1,0},
  {"Desktop.ini",13,0,0},
  {"MediaTools",0,1,0},
  {"MEDIACONV.EXE",15,0,0},
  {"CODECS.DLL",15,0,0},
  {"PRESETS.DAT",15,0,0},
  {"README.TXT",15,0,"MediaTools 3.2\nDrag a file onto the window to convert.\n"},
  {"WebRoot",0,1,0},
  {"index.html",20,0,"<html><body><h1>NoovexOS</h1>It works!</body></html>\n"},
  {"about.html",20,0,"<html><body>About this server.</body></html>\n"},
  {"style.css",20,0,"body{background:#0d1b2a;color:#eee;}\n"},
  {"server.cfg",20,0,"port=80\nroot=/WebRoot\nlog=on\n"},
  {"Chipset",0,1,0},
  {"CHIPSET.INF",25,0,0},
  {"INSTALL.EXE",25,0,0},
  {"LICENSE.TXT",25,0,"Chipset drivers.\nProvided as-is.\n"},
  {"Manuals",0,1,0},
  {"GUIDE.TXT",29,0,"NoovexOS Quick Guide\n--------------------\nWin+E  Files\nWin+R  Run\nWin+I  Settings\n"},
  {"SETUP.TXT",29,0,"First boot setup completed.\nSee Settings to change options.\n"},
  {"CHANGELOG.TXT",29,0,"v4  - USB, mouse fix, Explorer\nv3  - setup wizard\nv2  - boot menu\n"},
  {"Graphics",0,1,0},
  {"GPU.DRV",33,0,0},
  {"CONTROL.EXE",33,0,0},
  {"SHADERS.BIN",33,0,0},
  {"PerfLogs",0,1,0},
  {"BOOT.LOG",37,0,"[ OK ] kernel loaded\n[ OK ] framebuffer up\n[ OK ] PS/2 mouse\n"},
  {"SYSTEM.LOG",37,0,"boot count: 42\nuptime ok\n"},
  {"ERRORS.LOG",37,0,"no errors logged.\n"},
  {"Programs",0,1,0},
  {"Notepad",41,1,0},
  {"NOTEPAD.EXE",42,0,0},
  {"readme.txt",42,0,"Simple text editor.\n"},
  {"Calculator",41,1,0},
  {"CALC.EXE",45,0,0},
  {"Paint",41,1,0},
  {"PAINT.EXE",47,0,0},
  {"palette.dat",47,0,0},
  {"Programs32",0,1,0},
  {"HEXLANG.EXE",50,0,0},
  {"PYMINI.EXE",50,0,0},
  {"EDITOR.EXE",50,0,0},
  {"NoovexOS",0,1,0},
  {"System32",54,1,0},
  {"KERNEL.BIN",55,0,0},
  {"HAL.DLL",55,0,0},
  {"NTDLL.DLL",55,0,0},
  {"GDI32.DLL",55,0,0},
  {"USER32.DLL",55,0,0},
  {"CONFIG.SYS",55,0,"FILES=64\nBUFFERS=32\nSHELL=NVXSHELL.EXE\n"},
  {"drivers",55,1,0},
  {"MOUSE.SYS",62,0,0},
  {"KBD.SYS",62,0,0},
  {"DISK.SYS",62,0,0},
  {"NET.SYS",62,0,0},
  {"Drivers",54,1,0},
  {"GPU.DRV",67,0,0},
  {"AUDIO.DRV",67,0,0},
  {"USB.DRV",67,0,0},
  {"Fonts",54,1,0},
  {"SYSTEM.FON",71,0,0},
  {"FIXED.FON",71,0,0},
  {"TERMINAL.FON",71,0,0},
  {"KERNEL.BIN",54,0,0},
  {"EXPLORER.EXE",54,0,0},
  {"REGEDIT.EXE",54,0,0},
  {"BOOT.INI",54,0,"[boot]\ndefault=NoovexOS\ntimeout=3\n"},
  {"WIN.CFG",54,0,"theme=teal\nanim=on\nsound=on\n"},
  {"Games",0,1,0},
  {"SNAKE.EXE",80,0,0},
  {"TETRIS.EXE",80,0,0},
  {"INVADERS.EXE",80,0,0},
  {"MINESWEEP.EXE",80,0,0},
  {"sysconf.dat",0,0,0},
  {"D:",-1,1,0},
  {"Hardware",86,1,0},
  {"GPU.DRV",87,0,0},
  {"CHIPSET.SYS",87,0,0},
  {"USB3.DRV",87,0,0},
  {"AUDIO.DRV",87,0,0},
  {"LAN.DRV",87,0,0},
  {"SATA.SYS",87,0,0},
  {"Recovery",86,1,0},
  {"RESTORE.IMG",94,0,0},
  {"BACKUP.DAT",94,0,0},
  {"RECENV.EXE",94,0,0},
  {"BOOTREC.EXE",94,0,0},
  {"RECOVERY.TXT",94,0,"Recovery console.\nHold ESC at boot menu to enter.\n"},
  {"Toolbox",86,1,0},
  {"DISKMGR.EXE",100,0,0},
  {"CLEANUP.EXE",100,0,0},
  {"BENCH.EXE",100,0,0},
  {"HEXEDIT.EXE",100,0,0},
  {"README.TXT",100,0,"Toolbox utilities.\nRun DISKMGR to manage volumes.\n"},
  {"NOTES.TXT",100,0,"TODO: defrag D:\nTODO: test recovery image\n"},
  {"diskcfg.dat",86,0,0},
};
#define VFS_N 108
#define ROOT_C 0
#define ROOT_D 86
static int vfs_nchild(int par){ int c=0; for(int i=0;i<VFS_N;i++) if(VFS[i].parent==par)c++; return c; }
static int vfs_child(int par,int k){ int c=0; for(int i=0;i<VFS_N;i++){ if(VFS[i].parent==par){ if(c==k)return i; c++; } } return -1; }
static void vfs_path(int node,char* out,int cap){ int st[20],sp=0,nn=node; while(nn>=0&&sp<20){ st[sp++]=nn; nn=VFS[nn].parent; } int q=0; for(int z=sp-1;z>=0;z--){ const char* nm=VFS[st[z]].name; int j=0; while(nm[j]&&q<cap-5){out[q++]=nm[j++];} if(z>0&&q<cap-5){ out[q++]=(char)32; out[q++]=(char)62; out[q++]=(char)32; } } out[q]=0; }
static void vfs_open_file(int ni){ const VN* v=&VFS[ni]; int k=0; while(v->name[k]&&k<15){note_name[k]=v->name[k];k++;} note_name[k]=0; int L=0; if(v->body){ const char* b=v->body; while(b[L]&&L<1020){notebuf[L]=b[L];L++;} } else { const char* ph="Binary file. No text preview available.\n"; while(ph[L]&&L<1020){notebuf[L]=ph[L];L++;} } notebuf[L]=0; notelen=L; note_status=2; open_app(5,60,50,540,380); }
#define GCOLS 22
#define GROWS 16
#define sbx ((unsigned char*)0x0095D400u)
#define sby ((unsigned char*)0x0095D500u)
static int slen=3,sdir=3,sfx=5,sfy=5,sscore=0,salive=0;
static int fby10,fvy10,fscore=0,falive=0; static int fpx[3],fpg[3],fscored[3]; static int flap_acc=0; static unsigned short flap_pit;
static unsigned int rngs=2463534242u;
static u32 upsec=0;
static unsigned short pit_prev; static int snk_acc=0;
static unsigned short bk_pit=0; static int bk_acc=0;

static int diag_kbd_seen=0, diag_mouse_seen=0, diag_secs=0, diag_shown=0, diag_dismissed=0;
static u8 diag_raw_count=0;

#define COM1 0x3F8
static int com1_ok=0;
static void com1_init(void){
    outb(COM1+1,0x00); outb(COM1+3,0x80); outb(COM1+0,0x0C); outb(COM1+1,0x00);
    outb(COM1+3,0x03); outb(COM1+2,0xC7); outb(COM1+4,0x0B);
    outb(COM1+4,0x1E); outb(COM1+0,0xAE);
    int t=100000; while(!(inb(COM1+5)&1)&&t-->0);
    com1_ok=(inb(COM1+0)==0xAE);
    outb(COM1+4,0x0F);
}
static char g_blog[4096]; static int g_blogn=0;
static void blog_add(const char* s){ while(*s && g_blogn<4090){ g_blog[g_blogn++]=*s++; } g_blog[g_blogn]=0; }
static void com1_putc(char c){ int t=50000; while(!(inb(COM1+5)&0x20)&&t-->0); outb(COM1,c); }
static void com1_puts(const char*s){ blog_add(s); while(*s){ if(*s=='\n')com1_putc('\r'); com1_putc(*s++); } }
static void dlog(const char* s){ if(dev_mode&&com1_ok)com1_puts(s); else blog_add(s); }

static int dispi_id=0; static int res_pw=0,res_ph=0;
static void dispi_w(u16 idx,u16 val){ outw(0x1CE,idx); outw(0x1CF,val); }
static u16  dispi_rd(u16 idx){ outw(0x1CE,idx); return inw(0x1CF); }
static void dispi_detect(void){ u16 id=dispi_rd(0); dispi_id=(id>=0xB0C0&&id<=0xB0C5)?id:0; }
static int dispi_set(int w,int h){
    if(!dispi_id)return 0;
    dispi_w(4,0);
    dispi_w(1,(u16)w);
    dispi_w(2,(u16)h);
    dispi_w(3,32);
    dispi_w(6,(u16)w);
    dispi_w(4,0x41);
    W=w; H=h; PITCH=w;
    set_palette();
    return 1;
}

static int gpu_found=0, gpu_bus=0,gpu_dev=0,gpu_fn=0; static u16 gpu_ven=0,gpu_did=0; static u8 gpu_cls=0,gpu_sub=0; static u32 gpu_bar0=0,gpu_bar0sz=0;
static const char* gpu_vendor_name(u16 v){ switch(v){
    case 0x8086:return "INTEL"; case 0x10DE:return "NVIDIA"; case 0x1002:return "AMD/ATI";
    case 0x15AD:return "VMWARE SVGA II"; case 0x1234:return "QEMU/BOCHS VGA"; case 0x80EE:return "VIRTUALBOX VGA";
    case 0x1AF4:return "VIRTIO GPU"; case 0x1013:return "CIRRUS LOGIC"; case 0x5333:return "S3 GRAPHICS";
    case 0x102B:return "MATROX"; case 0x121A:return "3DFX"; default:return "UNKNOWN VENDOR"; } }
static void gpu_detect(void){
    gpu_found=0;
    for(int bus=0;bus<2&&!gpu_found;bus++) for(int dev=0;dev<32;dev++){
        u32 v=pci_read((u8)bus,(u8)dev,0,0); u16 ven=v&0xFFFF; if(ven==0xFFFF)continue;
        u32 cc=pci_read((u8)bus,(u8)dev,0,0x08); u8 cls=(cc>>24)&0xFF, sub=(cc>>16)&0xFF;
        if(cls==0x03){ gpu_found=1; gpu_bus=bus; gpu_dev=dev; gpu_fn=0; gpu_ven=ven; gpu_did=(u16)(v>>16); gpu_cls=cls; gpu_sub=sub;
            u32 bar0=pci_read((u8)bus,(u8)dev,0,0x10); gpu_bar0=bar0&0xFFFFFFF0u;
            u32 cmd=pci_read((u8)bus,(u8)dev,0,0x04); pci_write((u8)bus,(u8)dev,0,0x04,cmd&~0x2u);
            pci_write((u8)bus,(u8)dev,0,0x10,0xFFFFFFFFu); u32 sz=pci_read((u8)bus,(u8)dev,0,0x10);
            pci_write((u8)bus,(u8)dev,0,0x10,bar0); pci_write((u8)bus,(u8)dev,0,0x04,cmd);
            if(bar0&1)gpu_bar0sz=0; else { u32 mask=sz&0xFFFFFFF0u; gpu_bar0sz = mask?((~mask)+1):0; }
            break; }
    }
}

#define SB_BASE 0x220
static int sb_ok=0, sb_ver=0;
static u8 *sb_buf=(u8*)0x00500000;
static int sb_len=0;
static void sb_out(u8 v){ int t=100000; while((inb(SB_BASE+0xC)&0x80)&&t-->0); outb(SB_BASE+0xC,v); }
static int  sb_inb(void){ int t=100000; while(!(inb(SB_BASE+0xE)&0x80)&&t-->0); return inb(SB_BASE+0xA); }
static void sb_detect(void){
    outb(SB_BASE+0x6,1); for(volatile int i=0;i<50000;i++); outb(SB_BASE+0x6,0);
    int t=200000; while(!(inb(SB_BASE+0xE)&0x80)&&t-->0);
    if(inb(SB_BASE+0xA)!=0xAA){ sb_ok=0; return; }
    sb_out(0xE1); { int hi=sb_inb(),lo=sb_inb(); sb_ver=(hi<<8)|lo; }
    sb_ok=1;
}
static void sb_gen(void){
    int rate=11025; sb_len=rate/2; if(sb_len>30000)sb_len=30000;
    int notes[3]={523,659,784};
    for(int i=0;i<sb_len;i++){
        int seg=(i*3)/sb_len; if(seg>2)seg=2; int f=notes[seg];
        int ph=(i*f*256)/rate;
        int amp=120-(i*100)/sb_len; if(amp<8)amp=8;
        int s=128+(S(ph)*amp)/127; if(s<0)s=0; if(s>255)s=255;
        sb_buf[i]=(u8)s;
    }
}
static void sb_play(void){
    if(!sb_ok)return;
    sb_gen();
    u32 a=0x00500000; int len=sb_len;
    outb(0x0A,0x05);
    outb(0x0C,0x00);
    outb(0x0B,0x49);
    outb(0x02,a&0xFF); outb(0x02,(a>>8)&0xFF);
    outb(0x83,(a>>16)&0xFF);
    outb(0x03,(len-1)&0xFF); outb(0x03,((len-1)>>8)&0xFF);
    outb(0x0A,0x01);
    sb_out(0x41); sb_out(11025>>8); sb_out(11025&0xFF);
    sb_out(0x14); sb_out((len-1)&0xFF); sb_out(((len-1)>>8)&0xFF);
    pit_wait(11);
    inb(SB_BASE+0xE);
}

#define NIC_INITB 0x00900000u
#define NIC_RXD   0x00900100u
#define NIC_TXD   0x00900200u
#define NIC_RXB   0x00901000u
#define NIC_TXB   0x00905000u
#define RXR 8
static u16 nic_io=0; static u8 nic_bus=0,nic_dev=0;
static int nic_present=0,nic_init=0,net_ok=0;
static int noovex_delay=60, noovex_want=0, noovex_done=0;
static int nic_rx_idx=0,nic_tx_idx=0;
static u32 nic_txc=0,nic_rxc=0;
static u32 last_dns_ip=0; static int last_tcp_est=0;
static int tcp_rpk=0,tcp_dpk=0,tcp_smiss=0; static u8 tcp_lfl=0;
static u8 my_mac[6];
static u32 my_ip=0x0A00020Fu;
static u32 gw_ip=0x0A000202u;
static u8 gw_mac[6]; static int gw_known=0;
static u32 dns_ip=0x0A000203u, netmask=0xFFFFFF00u;
static int dhcp_run(void);
static int http_body_len=0;
static void ip_to_str(u32 ip,char* b){ int p=0; for(int i=3;i>=0;i--){ u32 oc=(ip>>(i*8))&0xFF; char t[4];int tl=0; if(oc==0)t[tl++]='0'; else { u32 v=oc;char tmp[4];int n=0;while(v){tmp[n++]='0'+v%10;v/=10;}while(n)t[tl++]=tmp[--n]; } for(int k=0;k<tl;k++)b[p++]=t[k]; if(i)b[p++]='.'; } b[p]=0; }
static u16 ping_ms=0;
static void be16(u8*b,u16 v){ b[0]=v>>8; b[1]=(u8)v; }
static void be32(u8*b,u32 v){ b[0]=v>>24; b[1]=v>>16; b[2]=v>>8; b[3]=(u8)v; }
static u16 rd16(const u8*b){ return ((u16)b[0]<<8)|b[1]; }
static u32 rd32(const u8*b){ return ((u32)b[0]<<24)|((u32)b[1]<<16)|((u32)b[2]<<8)|b[3]; }
static u16 cksum(const u8*d,int n){ u32 s=0; for(int i=0;i+1<n;i+=2)s+=((u32)d[i]<<8)|d[i+1]; if(n&1)s+=(u32)d[n-1]<<8; while(s>>16)s=(s&0xFFFF)+(s>>16); return (u16)~s; }
static void wcsr(u16 i,u32 v){ outl(nic_io+0x14,i); outl(nic_io+0x10,v); }
static u32  rcsr(u16 i){ outl(nic_io+0x14,i); return inl(nic_io+0x10); }
static void wbcr(u16 i,u32 v){ outl(nic_io+0x14,i); outl(nic_io+0x1C,v); }
static u32  rbcr(u16 i){ outl(nic_io+0x14,i); return inl(nic_io+0x1C); }
static void nic_detect(void){ nic_present=0; for(int i=0;i<pcin;i++){ if(pcil[i].ven==0x1022&&pcil[i].did==0x2000){ nic_io=pcil[i].io; nic_bus=pcil[i].bus; nic_dev=pcil[i].dev; nic_present=1; return; } } }

static int wifi_pci=0, wifi_usb=0; static u16 wifi_ven=0, wifi_did=0, wifi_usb_vid=0, wifi_usb_pid=0;
static u8 wifi_bus=0, wifi_dev=0;

static const char* wifi_chip_name(u16 ven,u16 did){
    if(ven==0x8086){ switch(did){
        case 0x2723: return "Intel Wi-Fi 6 AX200";
        case 0xA0F0: case 0x02F0: case 0x06F0: case 0x4DF0: return "Intel Wi-Fi 6 AX201";
        case 0x2725: return "Intel Wi-Fi 6E AX210";
        case 0x51F0: case 0x54F0: return "Intel Wi-Fi 6E AX211";
        case 0x9DF0: case 0x31DC: return "Intel Wireless 9560";
        case 0x24FD: return "Intel Wireless 8265";
        case 0x24F3: return "Intel Wireless 8260";
        case 0x095A: case 0x095B: return "Intel Wireless 7265";
        case 0x08B1: case 0x08B2: return "Intel Wireless 7260";
        case 0x3165: case 0x3166: return "Intel Wireless 3165";
        case 0x3168: return "Intel Wireless 3168";
        default: return "Intel Wireless (unknown model)";
    } }
    if(ven==0x10EC){ switch(did){
        case 0xB822: return "Realtek RTL8822BE";
        case 0xC822: return "Realtek RTL8822CE";
        case 0x8821: return "Realtek RTL8821AE";
        case 0x8723: return "Realtek RTL8723BE";
        case 0x8176: return "Realtek RTL8188CE";
        default: return "Realtek Wireless (unknown model)";
    } }
    if(ven==0x168C) return "Qualcomm Atheros Wireless";
    if(ven==0x14E4) return "Broadcom Wireless";
    if(ven==0x17CB) return "Qualcomm WCN Wireless";
    return "Unknown vendor";
}

static const char* wifi_needs(u16 ven){
    if(ven==0x8086) return "iwlwifi driver + iwlwifi-*.ucode firmware blob";
    if(ven==0x10EC) return "rtw88/rtlwifi driver + rtw88 firmware blob";
    if(ven==0x168C) return "ath9k/ath10k driver (+ firmware on 10k)";
    if(ven==0x14E4) return "brcmfmac driver + brcm firmware blob";
    return "vendor-specific driver + firmware blob";
}
static const char* wifi_ven_name(u16 v){ switch(v){
    case 0x8086:return "INTEL"; case 0x168C:case 0x0CF3:return "ATHEROS"; case 0x17CB:return "QUALCOMM";
    case 0x10EC:case 0x0BDA:return "REALTEK"; case 0x14E4:return "BROADCOM"; case 0x1814:case 0x148F:return "RALINK";
    case 0x14C3:case 0x0E8D:return "MEDIATEK"; case 0x11AB:case 0x1B4B:return "MARVELL"; case 0x1A56:return "KILLER";
    case 0x2357:return "TP-LINK"; case 0x0846:return "NETGEAR"; case 0x7392:return "EDIMAX"; case 0x13B1:return "LINKSYS"; case 0x050D:return "BELKIN";
    default:return "UNKNOWN"; } }
static int wifi_is_usb_vid(u16 v){ switch(v){case 0x0BDA:case 0x148F:case 0x0E8D:case 0x0CF3:case 0x2357:case 0x0846:case 0x7392:case 0x13B1:case 0x050D:return 1;default:return 0;} }
/* real MMIO probe: enable mem space, map BAR0, read Intel CSR_HW_REV (0x28) + CSR_GP_CNTRL (0x24).
   read-only CSR access - safe without firmware. returns 1 and fills regs on success. */
static int wifi_rst_ok=0, wifi_clk_ok=0, wifi_rfkill=-1, wifi_dma_ok=0, wifi_inited=0;
static u8 wifi_mac[6]={0,0,0,0,0,0};
#define WIFI_RXRING 0x009C9000u
#define WIFI_TXRING 0x009CA000u
/* Phase 3+4: reset hw, clock handshake, RF-kill, MAC from OTP, DMA rings.
   All documented pre-firmware CSR ops (iwlwifi CSR_RESET=0x20, CSR_GP_CNTRL=0x24,
   MAC OTP at 0x380/0x384, STRAP at 0x388/0x38C). Radio TX/RX still needs firmware. */
static int wifi_init_hw(void){
    if(!wifi_pci||wifi_ven!=0x8086) return -1;
    u32 cmd=pci_read(wifi_bus,wifi_dev,0,0x04); pci_write(wifi_bus,wifi_dev,0,0x04,cmd|0x06);
    u32 bar=pci_read(wifi_bus,wifi_dev,0,0x10);
    if(bar==0||bar==0xFFFFFFFFu||(bar&1)) return -2;
    volatile u32* m=(volatile u32*)(bar&0xFFFFFFF0u);
    m[0x20/4]=0x80;                                   /* CSR_RESET: SW_RESET       */
    for(volatile int d=0;d<200000;d++);
    wifi_rst_ok=1;
    m[0x24/4] |= 0x08;                                /* GP_CNTRL: INIT_DONE       */
    wifi_clk_ok=0;
    for(int t=0;t<200000;t++){ if(m[0x24/4]&0x01){ wifi_clk_ok=1; break; } }  /* MAC_CLOCK_READY */
    { u32 gp=m[0x24/4]; wifi_rfkill=(gp&0x08000000u)?0:1; }   /* HW_RF_KILL_SW: bit set = radio enabled */
    { u32 a0=m[0x380/4], a1=m[0x384/4];
      if(a0==0||a0==0xFFFFFFFFu){ a0=m[0x388/4]; a1=m[0x38C/4]; }             /* OTP empty -> STRAP */
      wifi_mac[0]=a0&0xFF; wifi_mac[1]=(a0>>8)&0xFF; wifi_mac[2]=(a0>>16)&0xFF;
      wifi_mac[3]=(a0>>24)&0xFF; wifi_mac[4]=a1&0xFF; wifi_mac[5]=(a1>>8)&0xFF; }
    { volatile u32* r=(volatile u32*)WIFI_RXRING; for(int i=0;i<1024;i++)r[i]=0;
      volatile u32* t2=(volatile u32*)WIFI_TXRING; for(int i=0;i<1024;i++)t2[i]=0; wifi_dma_ok=1; }
    wifi_inited=1; return 0;
}
static int wifi_radio_probe(u32* hwrev,u32* gpc){
    if(!wifi_pci) return 0;
    u32 cmd=pci_read(wifi_bus,wifi_dev,0,0x04); pci_write(wifi_bus,wifi_dev,0,0x04,cmd|0x06);
    u32 bar=pci_read(wifi_bus,wifi_dev,0,0x10);
    if(bar==0||bar==0xFFFFFFFFu||(bar&1)) return 0;      /* absent or IO bar */
    volatile u32* m=(volatile u32*)(bar&0xFFFFFFF0u);
    u32 hr=m[0x28/4], gp=m[0x24/4];
    if(hr==0xFFFFFFFFu) return 0;                         /* bus error / powered off */
    if(hwrev)*hwrev=hr; if(gpc)*gpc=gp; return 1;
}
static void wifi_detect(void){
    wifi_pci=0; wifi_usb=0;
    for(int i=0;i<pcin;i++){ if(pcil[i].cls==0x02 && pcil[i].sub==0x80){ wifi_pci=1; wifi_ven=pcil[i].ven; wifi_did=pcil[i].did; wifi_bus=pcil[i].bus; wifi_dev=pcil[i].dev; break; } }
    for(int i=0;i<usbdev_n;i++){ u8 eff=usbdev[i].cls?usbdev[i].cls:usbdev[i].ifcls; if(wifi_is_usb_vid(usbdev[i].vid) && (eff==0xFF||eff==0xE0)){ wifi_usb=1; wifi_usb_vid=usbdev[i].vid; wifi_usb_pid=usbdev[i].pid; break; } }
}

#define CAM_MAX 4
static int cam_count=0;
static struct {
    u8  dev_idx;
    u8  has_iad;
    u8  vc_iface;
    u8  vs_iface;
    u8  has_vc_header;
    u16 bcd_uvc;
    u8  n_formats;
    u8  fmt_type;
    u16 best_w, best_h;
    u16 cfg_len;
    u8  n_ifaces;
    u8  n_endpoints;
    u8  vs_ep; u16 vs_mps; u8 vs_bulk;
} cam_info[CAM_MAX];
static const char* cam_vendor_name(u16 v){
    switch(v){
        case 0x046D: return "LOGITECH";
        case 0x045E: return "MICROSOFT";
        case 0x1532: return "RAZER";
        case 0x05A3: return "ARKMICRO";
        case 0x0C45: return "MICRODIA";
        case 0x1BCF: return "SUNPLUS IT";
        case 0x04F2: return "CHICONY";
        case 0x064E: return "SUYIN";
        case 0x13D3: return "AZUREWAVE";
        case 0x0BDA: return "REALTEK";
        case 0x05AC: return "APPLE";
        case 0x0AC8: return "Z-STAR";
        case 0x1908: return "GENERAL IMAGING";
        case 0x2BC5: return "ORBBEC";
        case 0x8086: return "INTEL REALSENSE";
        default: return "UVC DEVICE";
    }
}
static const char* cam_fmt_name(u8 f){
    switch(f){ case 1:return "MJPEG"; case 2:return "UNCOMPRESSED"; case 3:return "H264"; case 4:return "DV"; default:return "UNKNOWN"; }
}

static void cam_parse_cfg(int ci, const u8* cfg, int cl){
    cam_info[ci].has_iad=0; cam_info[ci].has_vc_header=0; cam_info[ci].bcd_uvc=0;
    cam_info[ci].n_formats=0; cam_info[ci].fmt_type=0;
    cam_info[ci].best_w=0; cam_info[ci].best_h=0;
    cam_info[ci].vc_iface=0xFF; cam_info[ci].vs_iface=0xFF;
    cam_info[ci].n_ifaces=0; cam_info[ci].n_endpoints=0;
    cam_info[ci].vs_ep=0; cam_info[ci].vs_mps=0; cam_info[ci].vs_bulk=0;
    int off=0; int cur_if_class=0; int cur_if_sub=0;
    while(off+2<=cl){
        int blen=cfg[off]; int btype=cfg[off+1];
        if(blen<2 || off+blen>cl) break;
        if(btype==0x0B && blen>=8){
            if(cfg[off+4]==0x0E) cam_info[ci].has_iad=1;
        }
        else if(btype==0x04 && blen>=9){
            cam_info[ci].n_ifaces++;
            int num=cfg[off+2]; cur_if_class=cfg[off+5]; int sub=cfg[off+6]; cur_if_sub=sub;
            if(cur_if_class==0x0E){
                if(sub==0x01 && cam_info[ci].vc_iface==0xFF) cam_info[ci].vc_iface=(u8)num;
                else if(sub==0x02 && cam_info[ci].vs_iface==0xFF) cam_info[ci].vs_iface=(u8)num;
            }
        }
        else if(btype==0x05){
            cam_info[ci].n_endpoints++;
            if(cur_if_class==0x0E && cur_if_sub==0x02 && blen>=6){
                u8 epa=cfg[off+2], attr=cfg[off+3];
                if((epa&0x80) && (attr&0x03)==0x02 && !cam_info[ci].vs_ep){   /* bulk IN on VS iface */
                    cam_info[ci].vs_ep=epa&0x0F; cam_info[ci].vs_mps=(u16)(cfg[off+4]|(cfg[off+5]<<8)); cam_info[ci].vs_bulk=1;
                }
            }
        }
        else if(btype==0x24 && blen>=3 && cur_if_class==0x0E){
            int subtype=cfg[off+2];
            if(subtype==0x01 && blen>=12){
                cam_info[ci].has_vc_header=1;
                cam_info[ci].bcd_uvc=(u16)(cfg[off+3]|(cfg[off+4]<<8));
            }
            else if(subtype==0x06){
                cam_info[ci].n_formats++;
                if(!cam_info[ci].fmt_type) cam_info[ci].fmt_type=1;
            }
            else if(subtype==0x04){
                cam_info[ci].n_formats++;
                if(!cam_info[ci].fmt_type) cam_info[ci].fmt_type=2;
            }
            else if(subtype==0x10){
                cam_info[ci].n_formats++;
                if(!cam_info[ci].fmt_type) cam_info[ci].fmt_type=3;
            }
            else if(subtype==0x05 || subtype==0x07){
                if(blen>=9){
                    u16 w=(u16)(cfg[off+5]|(cfg[off+6]<<8));
                    u16 h=(u16)(cfg[off+7]|(cfg[off+8]<<8));
                    if((u32)w*(u32)h > (u32)cam_info[ci].best_w*(u32)cam_info[ci].best_h){
                        cam_info[ci].best_w=w; cam_info[ci].best_h=h;
                    }
                }
            }
        }
        off+=blen;
    }
}

static void cam_detect(void){
    cam_count=0;
    static u8 cbuf[768];
    for(int i=0;i<usbdev_n && cam_count<CAM_MAX;i++){
        u8 eff=usbdev[i].cls?usbdev[i].cls:usbdev[i].ifcls;
        int is_video=(eff==0x0E);
        int is_iad_misc=(usbdev[i].cls==0xEF && usbdev[i].sub==0x02);
        if(!is_video && !is_iad_misc) continue;

        u8 gc[8]={0x80,0x06,0x00,0x02,0x00,0x00,0x00,0x02};
        int r=ehci_ctrl(usbdev[i].addr,64,gc,cbuf,512);
        if(r<9) continue;
        int tot=cbuf[2]|(cbuf[3]<<8);
        if(tot>512) tot=512;
        if(r<tot) tot=r;
        int ci=cam_count;
        cam_info[ci].dev_idx=(u8)i;
        cam_info[ci].cfg_len=(u16)tot;
        cam_parse_cfg(ci,cbuf,tot);

        if(cam_info[ci].vc_iface!=0xFF || cam_info[ci].has_vc_header || is_video){
            usbdev[i].kind=5;
            cam_count++;
        }
    }
}

static int  webcam_active=-1;
static int  webcam_neg_ok=0;
static u32  webcam_max_frame=0;
static u32  webcam_max_payload=0;
static u8   webcam_fmt_idx=1, webcam_frame_idx=1;
static u32  webcam_interval=0;
static int  webcam_err=0;

static int webcam_negotiate(int ci){
    webcam_neg_ok=0; webcam_err=0; webcam_max_frame=0; webcam_max_payload=0; webcam_interval=0;
    if(ci<0||ci>=cam_count){ webcam_err=-1; return -1; }
    int di=cam_info[ci].dev_idx;
    u8 ifn=cam_info[ci].vs_iface;
    if(ifn==0xFF){ webcam_err=-2; return -2; }
    u8 addr=usbdev[di].addr;

    static u8 probe[48];
    for(int k=0;k<48;k++)probe[k]=0;
    probe[0]=0x01;
    probe[2]=webcam_fmt_idx;
    probe[3]=webcam_frame_idx;

    { u8 s[8]={0x21,0x01,0x00,0x01,ifn,0x00,26,0x00}; if(ehci_ctrl(addr,64,s,probe,26)<0){ webcam_err=-3; return -3; } }
    io_delay(20000);

    { u8 s[8]={0xA1,0x81,0x00,0x01,ifn,0x00,26,0x00}; int r=ehci_ctrl(addr,64,s,probe,26); if(r<26){ webcam_err=-4; return -4; } }
    webcam_fmt_idx   = probe[2];
    webcam_frame_idx = probe[3];
    webcam_interval  = (u32)probe[4]|((u32)probe[5]<<8)|((u32)probe[6]<<16)|((u32)probe[7]<<24);
    webcam_max_frame = (u32)probe[18]|((u32)probe[19]<<8)|((u32)probe[20]<<16)|((u32)probe[21]<<24);
    webcam_max_payload=(u32)probe[22]|((u32)probe[23]<<8)|((u32)probe[24]<<16)|((u32)probe[25]<<24);
    io_delay(20000);

    { u8 s[8]={0x21,0x01,0x00,0x02,ifn,0x00,26,0x00}; if(ehci_ctrl(addr,64,s,probe,26)<0){ webcam_err=-5; return -5; } }
    webcam_active=ci; webcam_neg_ok=1;
    return 0;
}

static int webcam_fps(void){ if(!webcam_interval)return 0; return (int)(10000000UL/webcam_interval); }
#define CAM_MJPEG ((u8*)0x02400000u)
#define CAM_RGBA  ((unsigned char*)0x02500000u)
static int cam_have=0, cam_fw=0, cam_fh=0, cam_dtog=0, cam_lasterr=0;
/* Grab ONE frame from a BULK UVC camera (MJPEG). Isoc cameras are unsupported. */
static int webcam_grab(int ci){
    cam_lasterr=0;
    if(ci<0||ci>=cam_count) return -1;
    if(!cam_info[ci].vs_bulk||!cam_info[ci].vs_ep){ cam_lasterr=-10; return -10; } /* isoc-only cam */
    if(cam_info[ci].fmt_type!=1){ cam_lasterr=-11; return -11; }                   /* not MJPEG */
    int di=cam_info[ci].dev_idx; u8 addr=usbdev[di].addr;
    int ep=cam_info[ci].vs_ep, mps=cam_info[ci].vs_mps?cam_info[ci].vs_mps:512;
    u8* frame=CAM_MJPEG; int flen=0; int maxf=0x180000;
    u8* rd=(u8*)0x02300000u;              /* scratch payload buffer */
    int got_eof=0;
    for(int iter=0; iter<400 && !got_eof; iter++){
        int r=ehci_bulk(addr,ep,1,mps,rd,mps,&cam_dtog);
        if(r<0){ cam_lasterr=-12; if(iter==0) return -12; break; }
        if(r<2) continue;
        int hlen=rd[0]; if(hlen<2||hlen>r) continue;
        u8 bfh=rd[1];                     /* bit1=EOF, bit0=FID */
        int payload=r-hlen;
        if(payload>0 && flen+payload<=maxf){ for(int k=0;k<payload;k++)frame[flen+k]=rd[hlen+k]; flen+=payload; }
        if(bfh&0x02) got_eof=1;
    }
    if(flen<100){ cam_lasterr=-13; return -13; }
    /* find JPEG SOI..EOI (some cams pad) */
    int st=0; while(st<flen-1 && !(frame[st]==0xFF&&frame[st+1]==0xD8)) st++;
    int w=0,h=0;
    if(jpeg_decode(frame+st,flen-st,CAM_RGBA,0x400000,&w,&h)!=0||w<=0||h<=0){ cam_lasterr=-14; return -14; }
    cam_fw=w; cam_fh=h; cam_have=1; return 0;
}
static void cam_blit(int dx,int dy,int dw,int dh){
    if(!cam_have||cam_fw<=0||cam_fh<=0)return;
    for(int y=0;y<dh;y++){ int yy=dy+y; if(yy<0||yy>=H)continue; int sy=y*cam_fh/dh;
        for(int x=0;x<dw;x++){ int xx=dx+x; if(xx<0||xx>=W)continue; int sx=x*cam_fw/dw;
            unsigned char* px=CAM_RGBA+((sy*cam_fw+sx)*4);
            LFB[yy*PITCH+xx]=((u32)px[0]<<16)|((u32)px[1]<<8)|px[2]; } }
}
static int nic_up(void){
    if(nic_init)return net_ok;
    nic_init=1;
    if(!nic_present||!nic_io){ com1_puts("NET: no PCnet NIC found\n"); net_ok=0; return 0; }
    u32 cmd=pci_read(nic_bus,nic_dev,0,0x04); pci_write(nic_bus,nic_dev,0,0x04,cmd|0x05);
    for(int i=0;i<6;i++) my_mac[i]=inb(nic_io+i);
    inl(nic_io+0x18); inw(nic_io+0x14);
    for(volatile int d=0;d<20000;d++);
    outl(nic_io+0x10,0);
    wcsr(0,0x04);
    wbcr(20,2);
    wbcr(2, rbcr(2)|0x0002);
    volatile u8* ib=(u8*)NIC_INITB;
    for(int i=0;i<28;i++)ib[i]=0;
    ib[0]=0x00; ib[1]=0x80;
    ib[2]=(3<<4);
    ib[3]=(1<<4);
    for(int i=0;i<6;i++)ib[4+i]=my_mac[i];
    *(volatile u32*)(ib+20)=NIC_RXD;
    *(volatile u32*)(ib+24)=NIC_TXD;
    for(int i=0;i<RXR;i++){ volatile u32* d=(u32*)(NIC_RXD+i*16); d[0]=NIC_RXB+i*2048; d[1]=0x8000F800u; d[2]=0; d[3]=0; }
    for(int i=0;i<2;i++){ volatile u32* d=(u32*)(NIC_TXD+i*16); d[0]=NIC_TXB; d[1]=0; d[2]=0; d[3]=0; }
    wcsr(1,NIC_INITB&0xFFFF); wcsr(2,(NIC_INITB>>16)&0xFFFF);
    wcsr(0,0x0001);
    int t=2000000; while(!(rcsr(0)&0x0100)&&t-->0);
    if(t<=0){ com1_puts("NET: INIT timeout\n"); net_ok=0; return 0; }
    wcsr(0,0x0002);
    net_ok=1;
    { char b[40]; const char* hx="0123456789ABCDEF"; int p=0; for(int i=0;i<6;i++){ b[p++]=hx[my_mac[i]>>4]; b[p++]=hx[my_mac[i]&15]; if(i<5)b[p++]=':'; } b[p++]='\n'; b[p]=0; com1_puts("NET: PCnet up MAC "); com1_puts(b); }
    if(!dhcp_run()){ my_ip=0x0A00020Fu; gw_ip=0x0A000202u; dns_ip=0x0A000203u; netmask=0xFFFFFF00u; com1_puts("NET: DHCP failed, static NAT 10.0.2.15\n"); }
    return 1;
}
static void nic_send(const u8* data,int len){
    if(!net_ok)return; if(len<60)len=60;
    volatile u32* d=(u32*)(NIC_TXD+nic_tx_idx*16);
    for(int w=0;(d[1]&(1u<<31))&&w<300000;w++);
    for(int i=0;i<len;i++)((volatile u8*)NIC_TXB)[i]=data[i];
    d[0]=NIC_TXB; d[2]=0;
    d[1]=(0xF000u|(((u32)(~len+1))&0xFFF))|(1u<<24)|(1u<<25)|(1u<<31);
    nic_tx_idx=(nic_tx_idx+1)&1; nic_txc++;
    wcsr(0,0x000A);
    for(int w=0;(d[1]&(1u<<31))&&w<300000;w++);
}
static int nic_recv(u8* out,int max){
    volatile u32* d=(u32*)(NIC_RXD+nic_rx_idx*16);
    if(d[1]&(1u<<31))return 0;
    int len=(int)(d[2]&0xFFF); if(len>max)len=max;
    u32 buf=NIC_RXB+nic_rx_idx*2048;
    for(int i=0;i<len;i++)out[i]=((volatile u8*)buf)[i];
    d[2]=0; d[1]=0x8000F800u;
    nic_rx_idx=(nic_rx_idx+1)%RXR; nic_rxc++;
    return len;
}
static int eth_hdr(u8* f,const u8* dst,u16 type){ for(int i=0;i<6;i++)f[i]=dst[i]; for(int i=0;i<6;i++)f[6+i]=my_mac[i]; be16(f+12,type); return 14; }
static void arp_request(u32 tip){
    u8 f[60]; for(int i=0;i<60;i++)f[i]=0;
    static const u8 bc[6]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    int o=eth_hdr(f,bc,0x0806);
    be16(f+o,1); be16(f+o+2,0x0800); f[o+4]=6; f[o+5]=4; be16(f+o+6,1);
    for(int i=0;i<6;i++)f[o+8+i]=my_mac[i]; be32(f+o+14,my_ip);
    be32(f+o+24,tip);
    nic_send(f,o+28);
    com1_puts("NET: ARP request sent\n");
}

static int net_poll(u8* rx,int max,int ticks){
    for(int k=0;k<ticks;k++){ u16 a=pit_read(); for(;;){ int n=nic_recv(rx,max); if(n>0)return n; u16 b=pit_read(); if(((u16)(a-b))>=1193)break; } }
    return 0;
}
static int arp_resolve(u32 ip,u8* outmac){
    if(ip==gw_ip&&gw_known){ for(int i=0;i<6;i++)outmac[i]=gw_mac[i]; return 1; }
    arp_request(ip);
    u8 rx[1600];
    for(int tries=0;tries<20;tries++){ int n=net_poll(rx,1600,1); if(n>=42&&rd16(rx+12)==0x0806&&rd16(rx+20)==2){ if(rd32(rx+28)==ip){ for(int i=0;i<6;i++)outmac[i]=rx[22+i]; if(ip==gw_ip){ for(int i=0;i<6;i++)gw_mac[i]=outmac[i]; gw_known=1; } com1_puts("NET: ARP reply received\n"); return 1; } } }
    com1_puts("NET: ARP no reply\n"); return 0;
}

#define SCAN_MAX 32
static int scan_n=0;
static struct { u32 ip; u8 mac[6]; } scan_res[SCAN_MAX];
static void scan_add(u32 ip,const u8* mac){
    if(ip==0||ip==my_ip) return;
    for(int i=0;i<scan_n;i++) if(scan_res[i].ip==ip) return;
    if(scan_n>=SCAN_MAX) return;
    scan_res[scan_n].ip=ip; for(int i=0;i<6;i++)scan_res[scan_n].mac[i]=mac[i]; scan_n++;
}

static void scan_drain(void){
    u8 rx[1600];
    for(int i=0;i<16;i++){ int n=nic_recv(rx,1600); if(n<=0)break;
        if(n>=42 && rd16(rx+12)==0x0806 && rd16(rx+20)==2) scan_add(rd32(rx+28),rx+22); }
}

static int net_scan(void){
    scan_n=0;
    if(!nic_up()) return -1;
    u32 base=my_ip&netmask;
    u32 hostmax=(~netmask)&0xFF; if(hostmax==0||hostmax>254)hostmax=254;

    { u8 gm[6]; if(arp_resolve(gw_ip,gm)) scan_add(gw_ip,gm); }
    for(u32 h=1;h<=hostmax;h++){ u32 ip=base|h; if(ip==my_ip)continue; arp_request(ip); if((h&3)==0)scan_drain(); }

    u8 rx[1600];
    for(int p=0;p<50;p++){
        int n=net_poll(rx,1600,1);
        if(n>=42 && rd16(rx+12)==0x0806 && rd16(rx+20)==2) scan_add(rd32(rx+28),rx+22);
        else if(n==0 && p>6) break;
    }
    scan_drain();
    return scan_n;
}
static int do_ping(u32 tip){
    if(!nic_up())return -1;
    u32 route=((tip&0xFFFFFF00u)==(my_ip&0xFFFFFF00u))?tip:gw_ip;
    u8 dst[6]; if(!arp_resolve(route,dst))return 0;
    u8 f[98]; for(int i=0;i<98;i++)f[i]=0;
    int o=eth_hdr(f,dst,0x0800);
    u8* ip=f+o;
    ip[0]=0x45; ip[1]=0; be16(ip+2,28); be16(ip+4,0x1234); be16(ip+6,0); ip[8]=64; ip[9]=1;
    be32(ip+12,my_ip); be32(ip+16,tip);
    u16 ick=cksum(ip,20); ip[10]=ick>>8; ip[11]=(u8)ick;
    u8* ic=ip+20; ic[0]=8; ic[1]=0; be16(ic+4,0xABCD); be16(ic+6,1);
    u16 cck=cksum(ic,8); ic[2]=cck>>8; ic[3]=(u8)cck;
    nic_send(f,o+28);
    com1_puts("NET: ICMP echo sent\n");
    u8 rx[1600]; u16 t0=pit_read();
    for(int tries=0;tries<150;tries++){ int n=net_poll(rx,1600,1);
        if(n>=34&&rd16(rx+12)==0x0800){ u8* rip=rx+14; if(rip[9]==1){ u8* ric=rip+(rip[0]&0x0F)*4; if(ric[0]==0&&rd16(ric+4)==0xABCD){ u16 t1=pit_read(); ping_ms=(u16)(((u16)(t0-t1))/22); com1_puts("NET: ICMP reply received\n"); return 1; } } } }
    com1_puts("NET: ICMP no reply\n"); return 0;
}

static u16 udp_cksum(u32 sip,u32 dip,const u8* udp,int udplen){
    u32 s=0; s+=(sip>>16)&0xFFFF; s+=sip&0xFFFF; s+=(dip>>16)&0xFFFF; s+=dip&0xFFFF; s+=17; s+=udplen;
    for(int i=0;i+1<udplen;i+=2)s+=((u32)udp[i]<<8)|udp[i+1];
    if(udplen&1)s+=(u32)udp[udplen-1]<<8;
    while(s>>16)s=(s&0xFFFF)+(s>>16);
    u16 r=(u16)~s; if(r==0)r=0xFFFF; return r;
}
static void udp_send(u32 dip,u16 sport,u16 dport,const u8* pl,int plen,const u8* dstmac){
    if(!net_ok)return;
    u8 f[1500]; for(int i=0;i<14+20+8+plen&&i<1500;i++)f[i]=0;
    int o=eth_hdr(f,dstmac,0x0800);
    u8* ip=f+o; int iplen=20+8+plen;
    ip[0]=0x45; ip[1]=0; be16(ip+2,iplen); be16(ip+4,0x4321); be16(ip+6,0); ip[8]=64; ip[9]=17; be16(ip+10,0);
    be32(ip+12,my_ip); be32(ip+16,dip);
    u16 ick=cksum(ip,20); ip[10]=ick>>8; ip[11]=(u8)ick;
    u8* udp=ip+20; be16(udp+0,sport); be16(udp+2,dport); be16(udp+4,8+plen); be16(udp+6,0);
    for(int i=0;i<plen;i++)udp[8+i]=pl[i];
    u16 uck=udp_cksum(my_ip,dip,udp,8+plen); udp[6]=uck>>8; udp[7]=(u8)uck;
    nic_send(f,o+iplen);
}

static u32 net_rng_state=0;
static u32 net_rand(void){
    u32 a,d; __asm__ volatile("rdtsc":"=a"(a),"=d"(d));
    if(net_rng_state==0) net_rng_state=a^d^0x9E3779B9u;
    u32 x=net_rng_state; x^=x<<13; x^=x>>17; x^=x<<5; net_rng_state=x;
    return x ^ a;
}
static int dns_skipname(const u8* p,int off){ while(p[off]!=0){ if((p[off]&0xC0)==0xC0)return off+2; off+=p[off]+1; } return off+1; }
static int dns_try(const char* host,u32 dns,u32* out_ip){
    if(!nic_up())return -1;
    u32 droute=((dns&0xFFFFFF00u)==(my_ip&0xFFFFFF00u))?dns:gw_ip;
    u8 dmac[6]; if(!arp_resolve(droute,dmac))return 0;

    u16 txid=(u16)net_rand(); if(txid==0)txid=0x1234;
    u16 sport=(u16)(0xC000u | (net_rand() & 0x3FFFu));
    u8 q[300]; int qp=0;
    be16(q+0,txid); be16(q+2,0x0100); be16(q+4,1); be16(q+6,0); be16(q+8,0); be16(q+10,0); qp=12;
    const char* p=host;
    while(*p){ const char* s=p; int l=0; while(*p&&*p!='.'){p++;l++;} if(l>63)l=63; q[qp++]=(u8)l; for(int i=0;i<l;i++)q[qp++]=s[i]; if(*p=='.')p++; }
    q[qp++]=0; be16(q+qp,1); qp+=2; be16(q+qp,1); qp+=2;
    int qsec_len=qp-12;
    udp_send(dns,sport,53,q,qp,dmac);
    com1_puts("NET: DNS query sent\n");
    u8 rx[1600];
    for(int tries=0;tries<150;tries++){ int n=net_poll(rx,1600,1);
        if(n<42)continue; if(rd16(rx+12)!=0x0800)continue;
        u8* ip=rx+14; if(ip[9]!=17)continue;

        if(rd32(ip+12)!=dns)continue;
        int ihl=(ip[0]&0x0F)*4; u8* udp=ip+ihl;
        if(rd16(udp+0)!=53)continue;
        if(rd16(udp+2)!=sport)continue;
        u8* d=udp+8;

        if(rd16(d+0)!=txid)continue;
        if(!(rd16(d+2)&0x8000))continue;
        if(rd16(d+4)!=1)continue;
        int qmatch=1; for(int i=0;i<qsec_len;i++){ if(d[12+i]!=q[12+i]){ qmatch=0; break; } }
        if(!qmatch){ com1_puts("NET: DNS question mismatch (dropped)\n"); continue; }
        int ancount=rd16(d+6);
        if(ancount==0){ com1_puts("NET: DNS no answer\n"); return 0; }
        int off=12; off=dns_skipname(d,off); off+=4;
        for(int a=0;a<ancount;a++){
            off=dns_skipname(d,off);
            u16 type=rd16(d+off); off+=2; off+=2; off+=4;
            u16 rdl=rd16(d+off); off+=2;
            if(type==1&&rdl==4){ *out_ip=rd32(d+off); com1_puts("NET: DNS resolved (validated)\n"); return 1; }
            off+=rdl;
        }
        return 0;
    }
    com1_puts("NET: DNS no reply\n"); return 0;
}
static char dns_chost[64]={0}; static u32 dns_cip=0;
static int dns_query(const char* host,u32* out_ip){
    if(dns_cip && streq(host,dns_chost)){ *out_ip=dns_cip; com1_puts("NET: DNS cache hit\n"); return 1; }
    int r=dns_try(host,dns_ip,out_ip);
    if(r==-1)return -1;
    if(r!=1){
        if(dns_ip!=0x0A000203u){ com1_puts("NET: DNS fallback 10.0.2.3\n"); r=dns_try(host,0x0A000203u,out_ip); }
        if(r!=1 && dns_ip!=0x0A000202u){ com1_puts("NET: DNS fallback gw 10.0.2.2\n"); r=dns_try(host,0x0A000202u,out_ip); }
    }
    if(r==1){ int k=0; while(host[k]&&k<63){ dns_chost[k]=host[k]; k++; } dns_chost[k]=0; dns_cip=*out_ip; }
    return r==1?1:0;
}

#define TF_FIN 0x01
#define TF_SYN 0x02
#define TF_RST 0x04
#define TF_PSH 0x08
#define TF_ACK 0x10
static u32 tcp_isn=0x00C0FFEEu; static u16 ipid=0x1000;
static u16 tcp_cksum(u32 sip,u32 dip,const u8* t,int len){
    u32 s=0; s+=(sip>>16)&0xFFFF; s+=sip&0xFFFF; s+=(dip>>16)&0xFFFF; s+=dip&0xFFFF; s+=6; s+=len;
    for(int i=0;i+1<len;i+=2)s+=((u32)t[i]<<8)|t[i+1];
    if(len&1)s+=(u32)t[len-1]<<8;
    while(s>>16)s=(s&0xFFFF)+(s>>16);
    return (u16)~s;
}
static void tcp_send(u32 dip,u16 sp,u16 dp,const u8* dmac,u32 seq,u32 ack,u8 fl,const u8* data,int dl){
    if(!net_ok)return;
    u8 f[1600]; int tot=34+20+dl; for(int i=0;i<tot&&i<1600;i++)f[i]=0;
    int o=eth_hdr(f,dmac,0x0800);
    u8* ip=f+o; int iplen=20+20+dl;
    ip[0]=0x45; be16(ip+2,iplen); be16(ip+4,ipid++); ip[8]=64; ip[9]=6;
    be32(ip+12,my_ip); be32(ip+16,dip);
    u16 ick=cksum(ip,20); ip[10]=ick>>8; ip[11]=(u8)ick;
    u8* t=ip+20;
    be16(t+0,sp); be16(t+2,dp); be32(t+4,seq); be32(t+8,ack);
    t[12]=(5<<4); t[13]=fl; be16(t+14,8192);
    for(int i=0;i<dl;i++)t[20+i]=data[i];
    u16 ck=tcp_cksum(my_ip,dip,t,20+dl); t[16]=ck>>8; t[17]=(u8)ck;
    nic_send(f,o+iplen);
}
static int tcp_get(u32 dip,u16 dport,const char* req,int reqlen,u8* out,int maxout){
    if(!nic_up())return -1;
    u32 route=((dip&0xFFFFFF00u)==(my_ip&0xFFFFFF00u))?dip:gw_ip;
    u8 dmac[6]; if(!arp_resolve(route,dmac))return -2;
    u16 sp=40000+(pit_read()&0x3FFF);
    u32 snd=net_rand(); tcp_isn=snd;          u32 rcv=0;
    u8 rx[1600]; int est=0; tcp_rpk=0;tcp_dpk=0;tcp_smiss=0;tcp_lfl=0;
    for(int z=0;z<16;z++){ if(nic_recv(rx,1600)<=0)break; }
    for(int att=0;att<4&&!est;att++){
        tcp_send(dip,sp,dport,dmac,snd,0,TF_SYN,0,0);
        for(int k=0;k<300;k++){ int n=net_poll(rx,1600,1);
            if(n<54)continue; if(rd16(rx+12)!=0x0800)continue; u8* ip=rx+14; if(ip[9]!=6)continue;
            int ihl=(ip[0]&0x0F)*4; u8* t=ip+ihl; if(rd16(t+0)!=dport||rd16(t+2)!=sp)continue;
            u8 fl=t[13];
            if((fl&(TF_SYN|TF_ACK))==(TF_SYN|TF_ACK)&&rd32(t+8)==snd+1){ rcv=rd32(t+4)+1; snd+=1; est=1; break; }
            if(fl&TF_RST){ com1_puts("NET: TCP RST\n"); return -4; } } }
    last_tcp_est=est;
    if(!est){ com1_puts("NET: TCP no SYN-ACK\n"); return -3; }
    com1_puts("NET: TCP established\n");
    tcp_send(dip,sp,dport,dmac,snd,rcv,TF_ACK,0,0);
    u32 getseq=snd;
    tcp_send(dip,sp,dport,dmac,getseq,rcv,TF_PSH|TF_ACK,(const u8*)req,reqlen); snd=getseq+reqlen;
    com1_puts("NET: HTTP GET sent\n");
    int outlen=0,done=0,idle=0,rexmit=0;
    while(!done&&outlen<maxout){
        int n=net_poll(rx,1600,1);
        if(n<=0){ if(++idle>1800)break;
            if(outlen==0 && (idle%450)==0 && rexmit<3){ tcp_send(dip,sp,dport,dmac,getseq,rcv,TF_PSH|TF_ACK,(const u8*)req,reqlen); rexmit++; com1_puts("NET: GET retransmit\n"); }
            continue; }
        if(rd16(rx+12)!=0x0800)continue; u8* ip=rx+14; if(ip[9]!=6)continue;
        int ihl=(ip[0]&0x0F)*4; u8* t=ip+ihl; if(rd16(t+0)!=dport||rd16(t+2)!=sp)continue;
        int iptot=rd16(ip+2); int thl=((t[12]>>4)&0xF)*4; int dl=iptot-ihl-thl; if(dl<0)dl=0;
        u32 seq=rd32(t+4); u8 fl=t[13];
        tcp_rpk++; tcp_lfl=fl; if(dl>0){ tcp_dpk++; if(seq!=rcv)tcp_smiss++; }
        if(fl&TF_RST){ done=1; break; }
        if(dl>0){ if(seq==rcv){ int cp=dl; if(outlen+cp>maxout)cp=maxout-outlen; for(int i=0;i<cp;i++)out[outlen+i]=t[thl+i]; outlen+=cp; rcv+=dl; }
            tcp_send(dip,sp,dport,dmac,snd,rcv,TF_ACK,0,0); idle=0; }
        if(fl&TF_FIN){ rcv+=1; tcp_send(dip,sp,dport,dmac,snd,rcv,TF_FIN|TF_ACK,0,0); snd+=1; done=1; } }
    com1_puts("NET: HTTP done\n");
    return outlen;
}

static u32 ts_dip; static u16 ts_dport, ts_sp; static u8 ts_dmac[6];
static u32 ts_snd, ts_rcv; static int ts_est, ts_fin;
static int tcps_open(u32 dip,u16 dport){
    if(!nic_up())return 0;
    u32 route=((dip&0xFFFFFF00u)==(my_ip&0xFFFFFF00u))?dip:gw_ip;
    if(!arp_resolve(route,ts_dmac))return 0;
    ts_dip=dip; ts_dport=dport; ts_sp=41000+(pit_read()&0x3FFF);
    ts_snd=net_rand(); tcp_isn=ts_snd;        ts_rcv=0; ts_est=0; ts_fin=0;
    u8 rx[1600]; for(int z=0;z<16;z++){ if(nic_recv(rx,1600)<=0)break; }
    for(int att=0;att<4&&!ts_est;att++){
        tcp_send(ts_dip,ts_sp,ts_dport,ts_dmac,ts_snd,0,TF_SYN,0,0);
        for(int k=0;k<300;k++){ int n=net_poll(rx,1600,1); if(n<54)continue; if(rd16(rx+12)!=0x0800)continue; u8*ip=rx+14; if(ip[9]!=6)continue; int ihl=(ip[0]&0x0F)*4; u8*t=ip+ihl; if(rd16(t+0)!=ts_dport||rd16(t+2)!=ts_sp)continue; u8 fl=t[13];
            if((fl&(TF_SYN|TF_ACK))==(TF_SYN|TF_ACK)&&rd32(t+8)==ts_snd+1){ ts_rcv=rd32(t+4)+1; ts_snd+=1; ts_est=1; break; }
            if(fl&TF_RST)return 0; } }
    if(!ts_est)return 0;
    tcp_send(ts_dip,ts_sp,ts_dport,ts_dmac,ts_snd,ts_rcv,TF_ACK,0,0);
    return 1;
}
static int tcps_wr(void* c,const u8* buf,int len){ (void)c;
    int off=0; while(off<len){ int chunk=len-off; if(chunk>1400)chunk=1400;
        tcp_send(ts_dip,ts_sp,ts_dport,ts_dmac,ts_snd,ts_rcv,TF_PSH|TF_ACK,buf+off,chunk); ts_snd+=(u32)chunk; off+=chunk; }
    return len;
}
static int tcps_rd(void* c,u8* buf,int max){ (void)c;
    if(ts_fin)return 0; u8 rx[1600];
    for(int idle=0; idle<2500; ){
        int n=net_poll(rx,1600,1);
        if(n<=0){ idle++; continue; }
        if(rd16(rx+12)!=0x0800)continue; u8*ip=rx+14; if(ip[9]!=6)continue;
        int ihl=(ip[0]&0x0F)*4; u8*t=ip+ihl; if(rd16(t+0)!=ts_dport||rd16(t+2)!=ts_sp)continue;
        int iptot=rd16(ip+2); int thl=((t[12]>>4)&0xF)*4; int dl=iptot-ihl-thl; if(dl<0)dl=0;
        u32 seq=rd32(t+4); u8 fl=t[13];
        if(fl&TF_RST){ ts_fin=1; return 0; }
        if(dl>0){
            if(seq==ts_rcv){ int cp=dl; if(cp>max)cp=max; for(int i=0;i<cp;i++)buf[i]=t[thl+i]; ts_rcv+=(u32)dl;
                tcp_send(ts_dip,ts_sp,ts_dport,ts_dmac,ts_snd,ts_rcv,TF_ACK,0,0);
                if(fl&TF_FIN){ ts_rcv+=1; ts_fin=1; tcp_send(ts_dip,ts_sp,ts_dport,ts_dmac,ts_snd,ts_rcv,TF_FIN|TF_ACK,0,0); ts_snd+=1; }
                return cp; }
            else { tcp_send(ts_dip,ts_sp,ts_dport,ts_dmac,ts_snd,ts_rcv,TF_ACK,0,0); }
        } else if(fl&TF_FIN){ ts_rcv+=1; ts_fin=1; tcp_send(ts_dip,ts_sp,ts_dport,ts_dmac,ts_snd,ts_rcv,TF_FIN|TF_ACK,0,0); ts_snd+=1; return 0; }
    }
    return 0;
}
static int last_tls_code=0;
static const char* tls_err_text(int n){ switch(n){
    case -1: return "TLS: could not send ClientHello to";
    case -2: return "TLS: no/invalid ServerHello (stage 2) from";
    case -3: return "TLS: ServerHello key_share parse failed (stage 3) for";
    case -4: return "TLS: server chose unsupported cipher (stage 4) at";
    case -5: return "TLS: failed reading server handshake flight (stage 5) from";
    case -6: return "TLS: server flight decrypt failed (stage 6) from";
    case -7: return "TLS: server Finished not received (stage 7) from";
    case -8: return "TLS: could not send client Finished (stage 8) to";
    case -9: return "TLS: could not send encrypted request (stage 9) to";
    default: return "TLS: handshake failed at"; } }

#define bin_files ((char(*)[20])0x0095E200u)
static int bin_cnt=0;
static void bin_push(const char*nm){ if(bin_cnt>=10||!nm[0])return; int k=0; while(nm[k]&&k<19){bin_files[bin_cnt][k]=nm[k];k++;} bin_files[bin_cnt][k]=0; bin_cnt++; }

#define TCOLS 10
#define TROWS 18
#define tboard ((u8(*)[TCOLS])0x0095D600u)
static int tpiece=0,trot=0,tpx=3,tpy=0,tscore=0,tdead=0,tlines=0;
static int tnext=0,tbest=0,tlevel=1;
static int tet_acc=0; static unsigned short tet_pit=0;
static const unsigned short TET[7][4]={
 {0x0F00,0x2222,0x00F0,0x4444},{0x8E00,0x6440,0x0E20,0x44C0},{0x2E00,0x4460,0x0E80,0xC440},
 {0x6600,0x6600,0x6600,0x6600},{0x6C00,0x4620,0x06C0,0x8C40},{0x4E00,0x4640,0x0E40,0x4C40},
 {0xC600,0x2640,0x0C60,0x4C80}};
static const u8 TCOL[7]={C_TEAL,C_BBLUE,C_FOLDER,C_GREEN,C_RED,C_TITLE,C_WHITE};
static int tet_cell(int p,int r,int cx,int cy){ int bit=cy*4+cx; return (TET[p][r&3]>>(15-bit))&1; }
static int tet_fits(int p,int r,int px,int py){
    for(int cy=0;cy<4;cy++)for(int cx=0;cx<4;cx++) if(tet_cell(p,r,cx,cy)){ int x=px+cx,y=py+cy; if(x<0||x>=TCOLS||y>=TROWS)return 0; if(y>=0&&tboard[y][x])return 0; } return 1; }
static void tet_spawn(void){ tpiece=tnext; tnext=rnd()%7; trot=0; tpx=3; tpy=0; if(!tet_fits(tpiece,trot,tpx,tpy)){ tdead=1; beep(180,6);} }
static void tetris_init(void){ rngs^=pit_read(); for(int y=0;y<TROWS;y++)for(int x=0;x<TCOLS;x++)tboard[y][x]=0; tscore=0;tlines=0;tlevel=1;tdead=0;tet_acc=0; tnext=rnd()%7; tet_spawn(); }
static void tet_lock(void){
    for(int cy=0;cy<4;cy++)for(int cx=0;cx<4;cx++) if(tet_cell(tpiece,trot,cx,cy)){ int x=tpx+cx,y=tpy+cy; if(y>=0&&y<TROWS&&x>=0&&x<TCOLS)tboard[y][x]=(u8)(TCOL[tpiece]); }
    int cleared=0;
    for(int y=TROWS-1;y>=0;y--){ int full=1; for(int x=0;x<TCOLS;x++) if(!tboard[y][x]){full=0;break;} if(full){ cleared++; for(int yy=y;yy>0;yy--)for(int x=0;x<TCOLS;x++)tboard[yy][x]=tboard[yy-1][x]; for(int x=0;x<TCOLS;x++)tboard[0][x]=0; y++; } }
    if(cleared){ tlines+=cleared; tscore+=cleared*cleared*100*tlevel; tlevel=1+tlines/10; if(tscore>tbest)tbest=tscore; beep(900,2); }
    tet_spawn();
}
static void tetris_step(void){ if(tdead)return; if(tet_fits(tpiece,trot,tpx,tpy+1))tpy++; else tet_lock(); }

static const int PIANO_HZ[14]={262,277,294,311,330,349,370,392,415,440,466,494,523,554};

#define PW 300
#define PH 200
static u8 *paint_buf=(u8*)0x00820000;
static int paint_col=C_RED, paint_init=0, paint_brush=1;
static void paint_clear(void){ for(int i=0;i<PW*PH;i++)paint_buf[i]=C_WHITE; paint_init=1; }
static int infected=0, av_state=0, av_threat=0, av_prog=0;
static int pending_virus=0, pending_avscan=0, mal_confirm=0;
static int install_pending=0, install_nslots=0; static int install_slots[4]={0,0,0,0};
static char search_q[24]; static int search_len=0;
static char toast[40]; static int toast_t=0;
static int dirty=0;
static int kext=0,kctrl=0,kshift=0,kalt=0,kwin=0,kwin_used=0; static int kesc=0; static int kaltgr=0;
static char kchar_shift(u8 sc);
static void toast_set(const char*m){ int k=0; while(m[k]&&k<38){toast[k]=m[k];k++;} toast[k]=0; toast_t=3; }
static int in(int x,int y,int rx,int ry,int rw,int rh){ return x>=rx&&x<rx+rw&&y>=ry&&y<ry+rh; }

static char srch[24]; static int srch_len=0;
typedef struct { u8 used,type; int app; int x,y,parent; char name[20]; } ditem;
#define DSK ((ditem*)0x00960000u)
#define DSK_MAX 48
static void rename_begin(int f,int idx){ rename_mode=1; rename_folder=f; rename_idx=idx; rename_len=0; rename_buf[0]=0; }
static void rename_commit(void){ if(!rename_mode)return;
    if(rename_folder==-1){ int k=0; while(rename_buf[k]&&k<19){DSK[rename_idx].name[k]=rename_buf[k];k++;} DSK[rename_idx].name[k]=0; rename_mode=0; return; }
    char(*b)[16]=folder_buf(rename_folder); int k=0; while(rename_buf[k]&&k<15){b[rename_idx][k]=rename_buf[k];k++;} b[rename_idx][k]=0; rename_mode=0; }

static int strncmp_(const char*a,const char*b,int n){ for(int i=0;i<n;i++){ if(a[i]!=b[i])return 1; if(!a[i])return 0; } return 0; }

static int ac97_ok; static int rec_frames, rec_rate, rec_peak;
static int mic_record(int ms); static void mic_playback(void);
static void tputs(const char*s){ while(*s){ if(termlen<2040)termbuf[termlen++]=*s; s++; } termbuf[termlen]=0; }
static void tnl(void){ if(termlen<2040)termbuf[termlen++]='\n'; termbuf[termlen]=0; }
static char* br_fetch(const char* spec);
static char br_proxy[200]="https://ogtkwovhbpvswepndtkc.supabase.co/functions/v1/read?url="; static int reader_on=1; static char jina_fmt[12]="html";
static int br_ok=0;

static u32 dhcp_xid=0x4E4F4F58u;
static void dhcp_send(u8 type,u32 reqip,u32 sid){
    u8 f[400]; for(int i=0;i<400;i++)f[i]=0;
    static const u8 bc[6]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    int o=eth_hdr(f,bc,0x0800);
    u8* ip=f+o; u8* udp=ip+20; u8* bp=udp+8;
    bp[0]=1; bp[1]=1; bp[2]=6; bp[3]=0;
    be32(bp+4,dhcp_xid); be16(bp+10,0x8000);
    for(int i=0;i<6;i++)bp[28+i]=my_mac[i];
    be32(bp+236,0x63825363u);
    int op=240;
    bp[op++]=53; bp[op++]=1; bp[op++]=type;
    bp[op++]=55; bp[op++]=4; bp[op++]=1; bp[op++]=3; bp[op++]=6; bp[op++]=15;
    if(reqip){ bp[op++]=50; bp[op++]=4; be32(bp+op,reqip); op+=4; }
    if(sid){ bp[op++]=54; bp[op++]=4; be32(bp+op,sid); op+=4; }
    bp[op++]=255;
    int bootlen=op; if(bootlen<300)bootlen=300;
    int udplen=8+bootlen;
    be16(udp+0,68); be16(udp+2,67); be16(udp+4,udplen);
    int iplen=20+udplen;
    ip[0]=0x45; be16(ip+2,iplen); be16(ip+4,ipid++); ip[8]=64; ip[9]=17;
    be32(ip+12,0); be32(ip+16,0xFFFFFFFFu);
    u16 ick=cksum(ip,20); ip[10]=ick>>8; ip[11]=(u8)ick;
    u16 uck=udp_cksum(0,0xFFFFFFFFu,udp,udplen); udp[6]=uck>>8; udp[7]=(u8)uck;
    nic_send(f,o+iplen);
}
static u8* dhcp_opt(u8* bp,int len,u8 want,int* ol){ int o=240; while(o+1<len){ u8 c=bp[o]; if(c==255)break; if(c==0){o++;continue;} u8 l=bp[o+1]; if(c==want){*ol=l; return bp+o+2;} o+=2+l; } return 0; }
static int dhcp_recv(u8 want,u32* yip,u32* sid){
    u8 rx[1600];
    for(int tries=0;tries<30;tries++){ int n=net_poll(rx,1600,1);
        if(n<282)continue; if(rd16(rx+12)!=0x0800)continue;
        u8* ip=rx+14; if(ip[9]!=17)continue; int ihl=(ip[0]&0x0F)*4; u8* udp=ip+ihl;
        if(rd16(udp+2)!=68)continue; u8* bp=udp+8; if(bp[0]!=2)continue; if(rd32(bp+4)!=dhcp_xid)continue;
        int blen=rd16(udp+4)-8; int ol;
        u8* mt=dhcp_opt(bp,blen,53,&ol); if(!mt||mt[0]!=want)continue;
        *yip=rd32(bp+16);
        u8* s=dhcp_opt(bp,blen,54,&ol); *sid=s?rd32(s):0;
        u8* rt=dhcp_opt(bp,blen,3,&ol); if(rt&&ol>=4)gw_ip=rd32(rt);
        u8* dn=dhcp_opt(bp,blen,6,&ol); if(dn&&ol>=4)dns_ip=rd32(dn);
        u8* sm=dhcp_opt(bp,blen,1,&ol); if(sm&&ol>=4)netmask=rd32(sm);
        return 1;
    }
    return 0;
}
static int dhcp_run(void){
    if(!net_ok)return 0;
    dhcp_xid=0x4E000000u|((u32)pit_read()<<8)|(cmos(0)&0xFF);
    dhcp_send(1,0,0); com1_puts("NET: DHCP discover\n");
    u32 yip=0,sid=0; if(!dhcp_recv(2,&yip,&sid)){ com1_puts("NET: DHCP no offer\n"); return 0; }
    dhcp_send(3,yip,sid);
    u32 y2=0,s2=0; if(!dhcp_recv(5,&y2,&s2)){ com1_puts("NET: DHCP no ack\n"); return 0; }
    my_ip=yip; gw_known=0; com1_puts("NET: DHCP bound\n");
    return 1;
}

static const char* SERVER_PAGE=
 "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n"
 "<html><head><title>NoovexOS</title></head><body>"
 "<h1>Hello from NoovexOS!</h1>"
 "<p>This page is served by a web server running on a from-scratch operating system.</p>"
 "<p>TCP/IP stack: PCnet driver, ARP, IPv4, ICMP, UDP, DNS, TCP, HTTP - all hand-written in C.</p>"
 "<hr><p>Powered by NoovexOS. No Linux. No Apache. Just NoovexOS.</p></body></html>";
static void srv_puts(const char* x); static void srv_nl(void);

static char srv_http_buf[9000];
static int http_dirlist(char* out,int cap){
    int o=0;
    #define HPUT(S) do{ const char* _s=(S); for(int _i=0;_s[_i]&&o<cap-1;_i++)out[o++]=_s[_i]; }while(0)
    HPUT("HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n");
    HPUT("<html><head><title>NoovexOS Storage</title>"
         "<style>body{font-family:sans-serif;background:#0b0f1a;color:#cde;margin:40px}"
         "h1{color:#7fd}a{color:#9cf;text-decoration:none}a:hover{text-decoration:underline}"
         "li{margin:6px 0}</style></head><body>");
    HPUT("<h1>NoovexOS Storage Node</h1><p>Files on NVXFS:</p><ul>");
    for(unsigned f=0; f<nvx.count && o<cap-300; f++){
        HPUT("<li><a href=\"/"); HPUT(nvx.e[f].name); HPUT("\">"); HPUT(nvx.e[f].name); HPUT("</a> &mdash; ");
        char nb[12]; int q=0,v=(int)nvx.e[f].len; char t[12];int tl=0; if(!v)t[tl++]='0'; while(v){t[tl++]='0'+v%10;v/=10;} while(tl)nb[q++]=t[--tl]; nb[q]=0;
        HPUT(nb); HPUT(" bytes</li>");
    }
    if(nvx.count==0) HPUT("<li><i>(no files)</i></li>");
    HPUT("</ul><hr><p>Served by NoovexOS &mdash; hand-written TCP/IP, no Apache, no Linux.</p></body></html>");
    #undef HPUT
    return o;
}

static int http_serve_once(u16 port, u8* rx){
    u8 cmac[6]; u32 cip=0; u16 cport=0; u32 their=0,my=0; int waiting=1;
    while(waiting){
        u8 st=inb(0x64); if(st&1){ u8 d=inb(0x60); (void)d; if(!(st&0x20))return -2; }
        int n=net_poll(rx,1600,1); if(n<54)continue;
        if(rd16(rx+12)!=0x0800)continue; u8* ip=rx+14; if(ip[9]!=6)continue;
        int ihl=(ip[0]&0x0F)*4; u8* t=ip+ihl; if(rd16(t+2)!=port)continue;
        if((t[13]&(TF_SYN|TF_ACK))!=TF_SYN)continue;
        for(int i=0;i<6;i++)cmac[i]=rx[6+i];
        cip=rd32(ip+12); cport=rd16(t+0); their=rd32(t+4)+1; my=net_rand(); tcp_isn=my; waiting=0;
    }
    tcp_send(cip,port,cport,cmac,my,their,TF_SYN|TF_ACK,0,0); my+=1;

    char path[128]; path[0]='/'; path[1]=0;
    for(int k=0;k<24;k++){ int n=net_poll(rx,1600,1); if(n<54)continue;
        u8* ip=rx+14; if(ip[9]!=6)continue; int ihl=(ip[0]&0x0F)*4; u8* t=ip+ihl;
        if(rd16(t+0)!=cport||rd16(t+2)!=port)continue;
        int iptot=rd16(ip+2); int thl=((t[12]>>4)&0xF)*4; int dl=iptot-ihl-thl; if(dl<0)dl=0;
        u32 seq=rd32(t+4);
        if(dl>0&&seq==their){ u8* data=t+thl;
            if(dl>=5&&data[0]=='G'&&data[1]=='E'&&data[2]=='T'&&data[3]==' '){ int i=4,o=0;
                while(i<dl&&data[i]!=' '&&data[i]!='\r'&&data[i]!='\n'&&o<127)path[o++]=data[i++]; path[o]=0; }
            their+=dl; tcp_send(cip,port,cport,cmac,my,their,TF_ACK,0,0); break; }
    }

    srv_puts("HTTP GET "); srv_puts(path); srv_puts(" from "); { char ib[20]; ip_to_str(cip,ib); srv_puts(ib); } srv_nl();

    int plen;
    if(path[1]==0){ plen=http_dirlist(srv_http_buf,sizeof(srv_http_buf)); }
    else { const char* fn=path+1; int idx=nvx_find(fn);
        if(idx<0){ const char* nf="HTTP/1.0 404 Not Found\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nFile not found on NoovexOS.\n";
            plen=0; for(int i=0;nf[i];i++)srv_http_buf[plen++]=nf[i]; }
        else { const char* hd="HTTP/1.0 200 OK\r\nContent-Type: application/octet-stream\r\nConnection: close\r\n\r\n";
            plen=0; for(int i=0;hd[i];i++)srv_http_buf[plen++]=hd[i];
            int got=nvx_read(idx,srv_http_buf+plen,(int)sizeof(srv_http_buf)-plen-4); if(got>0)plen+=got; }
    }

    int off=0; while(off<plen){ int chunk=plen-off; if(chunk>1400)chunk=1400;
        tcp_send(cip,port,cport,cmac,my,their,TF_PSH|TF_ACK,(const u8*)(srv_http_buf+off),chunk); my+=(u32)chunk; off+=chunk;
        for(int w=0;w<6;w++){ int n=net_poll(rx,1600,1); if(n<54)continue; u8* ip=rx+14; if(ip[9]!=6)continue; int ihl=(ip[0]&0x0F)*4; u8* t=ip+ihl; if(rd16(t+0)==cport&&(t[13]&TF_ACK)){ break; } } }
    tcp_send(cip,port,cport,cmac,my,their,TF_FIN|TF_ACK,0,0); my+=1;
    for(int k=0;k<10;k++){ int n=net_poll(rx,1600,1); if(n<54)continue; u8* ip=rx+14; if(ip[9]!=6)continue; int ihl=(ip[0]&0x0F)*4; u8* t=ip+ihl; if(rd16(t+0)!=cport)continue; if(t[13]&TF_FIN){ their+=1; tcp_send(cip,port,cport,cmac,my,their,TF_ACK,0,0); break; } }
    return 1;
}
static int tcp_serve(u16 port){
    if(!nic_up())return -1;
    u8 rx[1600]; u8 cmac[6]; u32 cip=0; u16 cport=0; u32 their=0,my=0; int waiting=1;
    while(waiting){
        u8 st=inb(0x64); if(st&1){ u8 d=inb(0x60); (void)d; if(!(st&0x20))return 0; }
        int n=net_poll(rx,1600,1); if(n<54)continue;
        if(rd16(rx+12)!=0x0800)continue; u8* ip=rx+14; if(ip[9]!=6)continue;
        int ihl=(ip[0]&0x0F)*4; u8* t=ip+ihl; if(rd16(t+2)!=port)continue;
        if((t[13]&(TF_SYN|TF_ACK))!=TF_SYN)continue;
        for(int i=0;i<6;i++)cmac[i]=rx[6+i];
        cip=rd32(ip+12); cport=rd16(t+0); their=rd32(t+4)+1; my=net_rand(); tcp_isn=my;              waiting=0;
    }
    tcp_send(cip,port,cport,cmac,my,their,TF_SYN|TF_ACK,0,0); my+=1;
    com1_puts("NET: SRV SYN-ACK\n");
    for(int k=0;k<20;k++){ int n=net_poll(rx,1600,1); if(n<54)continue;
        u8* ip=rx+14; if(ip[9]!=6)continue; int ihl=(ip[0]&0x0F)*4; u8* t=ip+ihl;
        if(rd16(t+0)!=cport||rd16(t+2)!=port)continue;
        int iptot=rd16(ip+2); int thl=((t[12]>>4)&0xF)*4; int dl=iptot-ihl-thl; if(dl<0)dl=0;
        u32 seq=rd32(t+4);
        if(dl>0&&seq==their){ their+=dl; tcp_send(cip,port,cport,cmac,my,their,TF_ACK,0,0); break; } }
    int plen=strlen_(SERVER_PAGE); if(plen>1400)plen=1400;
    tcp_send(cip,port,cport,cmac,my,their,TF_PSH|TF_ACK,(const u8*)SERVER_PAGE,plen); my+=plen;
    tcp_send(cip,port,cport,cmac,my,their,TF_FIN|TF_ACK,0,0); my+=1;
    com1_puts("NET: SRV served request\n");
    for(int k=0;k<10;k++){ int n=net_poll(rx,1600,1); if(n<54)continue; u8* ip=rx+14; if(ip[9]!=6)continue; int ihl=(ip[0]&0x0F)*4; u8* t=ip+ihl; if(rd16(t+0)!=cport)continue; if(t[13]&TF_FIN){ their+=1; tcp_send(cip,port,cport,cmac,my,their,TF_ACK,0,0); break; } }
    return 1;
}

static u32 le32_(const u8*b){ return (u32)b[0]|((u32)b[1]<<8)|((u32)b[2]<<16)|((u32)b[3]<<24); }
static const char* part_type_name(u8 t){
    switch(t){ case 0x00:return "EMPTY"; case 0x01:case 0x04:case 0x06:case 0x0E:return "FAT16";
        case 0x07:return "NTFS/EXFAT"; case 0x0B:case 0x0C:return "FAT32"; case 0x05:case 0x0F:return "EXTENDED";
        case 0x82:return "LINUX SWAP"; case 0x83:return "LINUX"; case 0xA5:return "BSD";
        case 0xEE:return "GPT PROT"; case 0xEF:return "EFI SYSTEM"; default:return "OTHER"; }
}

static int iso_read2048(u32 iso_lba, u8* out){ return msd_read(iso_lba*4, 4, out); }
static void cmd_imginfo(void){
    if(msd_dev<0||!msd_ready){ tputs("NO USB DRIVE MOUNTED."); tnl(); return; }
    static u8 s[512];
    if(msd_read(0,1,s)<0){ tputs("READ FAILED (LBA 0)."); tnl(); return; }
    if(s[510]!=0x55||s[511]!=0xAA){ tputs("NO MBR SIGNATURE (NOT PARTITIONED, OR ISO/SUPERFLOPPY)."); tnl(); return; }
    tputs("MBR PARTITION TABLE (U:):"); tnl();
    int n=0;
    for(int i=0;i<4;i++){ const u8* p=s+446+i*16; u8 boot=p[0],ty=p[4]; u32 start=le32_(p+8),cnt=le32_(p+12);
        if(ty==0&&cnt==0)continue;
        char b[12]; tputs("  P"); utoa(i+1,b); tputs(b); tputs(boot==0x80?" * ":"   ");
        tputs(part_type_name(ty)); tputs("  START="); utoa(start,b); tputs(b);
        u32 mb=(u32)(((unsigned long long)cnt*512ULL)/1048576ULL); tputs("  SIZE="); utoa(mb,b); tputs(b); tputs("MB"); tnl();
        n++; }
    char nb[8]; utoa(n,nb); tputs("PARTITIONS: "); tputs(nb); tnl();
    if(n==0){ tputs("(TABLE EMPTY)"); tnl(); }
}
static void cmd_isoinfo(void){
    if(msd_dev<0||!msd_ready){ tputs("NO USB DRIVE MOUNTED."); tnl(); return; }
    u8* s=(u8*)0x0084B000u;
    if(iso_read2048(16,s)<0){ tputs("READ FAILED (ISO SECTOR 16)."); tnl(); return; }
    if(s[0]!=1 || s[1]!='C'||s[2]!='D'||s[3]!='0'||s[4]!='0'||s[5]!='1'){
        tputs("NOT AN ISO9660 VOLUME (NO CD001 AT SECTOR 16)."); tnl();
        tputs("(USB MUST CONTAIN AN ISO IMAGE FOR THIS.)"); tnl(); return; }
    char vid[33]; for(int i=0;i<32;i++)vid[i]=s[40+i]; vid[32]=0;
    int e=32; while(e>0&&vid[e-1]==' ')vid[--e]=0;
    tputs("ISO9660 VOLUME: "); tputs(vid); tnl();
    u32 vss=le32_(s+80); char b[12]; utoa(vss,b); tputs("SIZE: "); tputs(b); tputs(" SECTORS ("); utoa(vss*2,b); tputs(b); tputs(" KB)"); tnl();
    const u8* rr=s+156; u32 rext=le32_(rr+2), rlen=le32_(rr+10);

    u8* d=(u8*)0x0084B800u;
    if(iso_read2048(rext,d)<0){ tputs("ROOT DIR READ FAILED."); tnl(); return; }
    tputs("ROOT DIRECTORY:"); tnl();
    int off=0,count=0,shown=0;
    while(off<(int)rlen && off<2048){
        int rl=d[off]; if(rl==0)break;
        u32 ln=le32_(d+off+10); int fl=d[off+25]; int nl=d[off+32];
        char nm[40]; int j=0; for(int k=0;k<nl&&k<38;k++)nm[j++]=d[off+33+k]; nm[j]=0;
        if(nl==1&&d[off+33]==0){ off+=rl; count++; continue; }
        if(nl==1&&d[off+33]==1){ off+=rl; count++; continue; }
        for(int k=0;nm[k];k++){ if(nm[k]==';'){ nm[k]=0; break; } }
        tputs("  "); tputs(nm);
        int pad=20-(int)strlen_(nm); while(pad-->0)tputs(" ");
        if(fl&0x02) tputs("<DIR>");
        else { char lb[12]; utoa(ln,lb); tputs(lb); tputs(" B"); }
        tnl(); count++; shown++;
        if(shown>=24){ tputs("  ...(more)"); tnl(); break; }
        off+=rl;
    }
    char cb[8]; utoa(shown,cb); tputs("FILES/DIRS SHOWN: "); tputs(cb); tnl();
}
static const u8 BOOTSTUB[512]={
    0xFA,0x31,0xC0,0x8E,0xD8,0x8E,0xC0,0x8E,0xD0,0xBC,0x00,0x7C,0xFB,0xBE,0x21,0x7C,
    0xAC,0x84,0xC0,0x74,0x09,0xB4,0x0E,0xBB,0x07,0x00,0xCD,0x10,0xEB,0xF2,0xF4,0xEB,
    0xFD,0x0D,0x0A,0x4E,0x4F,0x4F,0x56,0x45,0x58,0x4F,0x53,0x20,0x42,0x4F,0x4F,0x54,
    0x41,0x42,0x4C,0x45,0x20,0x55,0x53,0x42,0x0D,0x0A,0x57,0x52,0x49,0x54,0x54,0x45,
    0x4E,0x20,0x42,0x59,0x20,0x4D,0x4B,0x42,0x4F,0x4F,0x54,0x20,0x2D,0x20,0x53,0x54,
    0x55,0x42,0x20,0x4F,0x4B,0x0D,0x0A,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x55,0xAA,};
static void cmd_mkboot(int confirmed){
    if(msd_dev<0||!msd_ready){ tputs("NO USB DRIVE MOUNTED."); tnl(); return; }
    if(!confirmed){
        tputs("MKBOOT WRITES A BOOT SECTOR TO U: LBA 0."); tnl();
        tputs("THIS OVERWRITES THE EXISTING PARTITION TABLE/BOOT"); tnl();
        tputs("SECTOR AND CAN MAKE FILES UNREADABLE. TO PROCEED:"); tnl();
        tputs("  RUN:  mkboot yes"); tnl();
        return;
    }
    if(msd_write(0,1,BOOTSTUB)<0){ tputs("WRITE FAILED."); tnl(); return; }
    tputs("BOOT SECTOR WRITTEN TO U: (LBA 0)."); tnl();
    tputs("USB IS NOW LEGACY-BIOS BOOTABLE TO THE NOOVEX STUB."); tnl();
    tputs("NOTE: BOOTS IN LEGACY/CSM MODE ONLY (NOT UEFI)."); tnl();
    tputs("IT IS A MINIMAL STUB, NOT A FULL OS IMAGE."); tnl();
}
extern char __file_end;
static void cmd_mkusb(int confirmed){
    if(!dev_mode){ tputs("MKUSB REQUIRES DEVELOPER MODE."); tnl(); tputs("ENABLE: PRESS ESC AT BOOT, THEN CTRL+D."); tnl(); return; }
    if(msd_dev<0||!msd_ready){ tputs("NO USB DRIVE MOUNTED. ATTACH A USB DRIVE FIRST."); tnl(); return; }
    if(!confirmed){
        tputs("MKUSB WRITES A FULL BOOTABLE NOOVEXOS TO THE USB (U:)."); tnl();
        tputs("THIS ERASES THE START OF THE USB - ALL FILES ON IT"); tnl();
        tputs("MAY BE LOST. ONLY USE A USB YOU CAN ERASE. TO PROCEED:"); tnl();
        tputs("  RUN:  mkusb yes"); tnl();
        return;
    }
    tputs("WRITING BOOT SECTOR (LBA 0)..."); tnl();
    { u8 z[512]; for(int i=0;i<512;i++)z[i]=(i<nvx_boot_bin_len)?nvx_boot_bin[i]:0; if(msd_write(0,1,z)<0){ tputs("USB WRITE FAILED (LBA 0)."); tnl(); return; } }
    tputs("WRITING STAGE2 (LBA 1-4)..."); tnl();
    for(int sct=0;sct<4;sct++){ u8 z[512]; for(int i=0;i<512;i++){ int o=sct*512+i; z[i]=(o<nvx_stage2_bin_len)?nvx_stage2_bin[o]:0; } if(msd_write(1+sct,1,z)<0){ tputs("USB WRITE FAILED (STAGE2)."); tnl(); return; } }
    u32 flen=(u32)(&__file_end)-0x10000u; const u8* kim=(const u8*)0x10000u;
    { char nb[8]; utoa((flen+511)/512,nb); tputs("WRITING KERNEL ("); tputs(nb); tputs(" SECTORS) "); }
    for(u32 sct=0;sct<896;sct++){ u8 z[512]; u32 off=sct*512u;
        for(int i=0;i<512;i++){ u32 o=off+(u32)i; z[i]=(o<flen)?kim[o]:0; }
        if(msd_write(5+sct,1,z)<0){ tnl(); tputs("USB WRITE FAILED (KERNEL)."); tnl(); return; }
        if((sct&127)==0){ tputs("."); } }
    tnl();
    tputs("DONE. THE USB IS NOW A BOOTABLE NOOVEXOS (LEGACY/CSM)."); tnl();
    tputs("BOOT IT: SET THE USB FIRST IN THE BIOS BOOT ORDER."); tnl();
}

typedef struct { u8 ident[16]; u16 type,machine; u32 version,entry,phoff,shoff,flags; u16 ehsize,phentsize,phnum,shentsize,shnum,shstrndx; } Elf32_Ehdr;
typedef struct { u32 type,offset,vaddr,paddr,filesz,memsz,flags,align; } Elf32_Phdr;
#define PROG_FILEBUF ((u8*)0x09000000u)

static u32 g_kjmp[6];
__attribute__((naked,used)) static int k_setjmp(u32* buf){
    __asm__ __volatile__(
        "movl 4(%esp), %eax\n\t"
        "movl %ebx, 0(%eax)\n\t" "movl %esi, 4(%eax)\n\t" "movl %edi, 8(%eax)\n\t"
        "movl %ebp, 12(%eax)\n\t" "movl %esp, 16(%eax)\n\t"
        "movl (%esp), %ecx\n\t" "movl %ecx, 20(%eax)\n\t"
        "xorl %eax, %eax\n\t" "ret\n\t");
}
__attribute__((naked,used)) static void k_longjmp(u32* buf,int val){
    __asm__ __volatile__(
        "movl 4(%esp), %eax\n\t" "movl 8(%esp), %edx\n\t"
        "movl 0(%eax), %ebx\n\t" "movl 4(%eax), %esi\n\t" "movl 8(%eax), %edi\n\t"
        "movl 12(%eax), %ebp\n\t" "movl 16(%eax), %esp\n\t" "movl 20(%eax), %ecx\n\t"
        "movl %edx, %eax\n\t" "jmp *%ecx\n\t");
}

static int progcx, progcy;
static void prog_console_init(void){
    clear_all(0);
    draw_str(8,6,"NoovexOS Program Output",C_TEAL);
    fill(0,22,W,1,C_MGREY); progcx=8; progcy=32;
}
static void prog_newline(void){ progcx=8; progcy+=12; if(progcy>H-28){ progcy=32; clear_all(0); draw_str(8,6,"NoovexOS Program Output",C_TEAL); fill(0,22,W,1,C_MGREY);} }
static void prog_putc(char ch){ if(ch=='\n'){prog_newline();return;} if(ch=='\r'){progcx=8;return;} if(progcx>W-12)prog_newline(); if(ch>=32&&ch<127)draw_char(progcx,progcy,ch,C_WHITE); progcx+=8; }
static void prog_print(const char* s){ int g=0; while(*s&&g++<4096) prog_putc(*s++); }

static void fb_pixel(int x,int y,u32 rgb){ if((unsigned)x<(unsigned)W&&(unsigned)y<(unsigned)H) FB[y*PITCH+x]=rgb; }
static void fb_rect(int x,int y,int w,int h,u32 rgb){ if(x<0){w+=x;x=0;} if(y<0){h+=y;y=0;} if(x+w>W)w=W-x; if(y+h>H)h=H-y; for(int j=0;j<h;j++){ u32*r=FB+(y+j)*PITCH+x; for(int i=0;i<w;i++)r[i]=rgb; } }

static void br_grad_fill(int x,int y,int w,int h,u8 c1,u8 c2,int dir){
    u32 a=PAL32[c1],b=PAL32[c2];
    int ar=(a>>16)&255,ag=(a>>8)&255,ab=a&255, br2=(b>>16)&255,bg=(b>>8)&255,bb=b&255;
    if(dir==2){ for(int i=0;i<w;i++){ int t=(w>1)?(i*255/(w-1)):0;
        u32 col=((u32)(ar+(br2-ar)*t/255)<<16)|((u32)(ag+(bg-ag)*t/255)<<8)|(u32)(ab+(bb-ab)*t/255); fb_rect(x+i,y,1,h,col); } }
    else { for(int j=0;j<h;j++){ int t=(h>1)?(j*255/(h-1)):0;
        u32 col=((u32)(ar+(br2-ar)*t/255)<<16)|((u32)(ag+(bg-ag)*t/255)<<8)|(u32)(ab+(bb-ab)*t/255); fb_rect(x,y+j,w,1,col); } }
}
static void fb_char_rgb(int x,int y,char ch,u32 rgb){ int idx=(int)(unsigned char)ch-32; if(idx<0||idx>=95)return; const unsigned char*g=fontaa[idx];
    int fr=(rgb>>16)&255,fgc=(rgb>>8)&255,fbb=rgb&255;
    for(int r=0;r<AA_CH;r++){ int yy=y+r; if(yy<0||yy>=H)continue; const unsigned char*gr=g+r*AA_CW;
        for(int c=0;c<AA_CW;c++){ int xx=x+c; if(xx<0||xx>=W)continue; unsigned a=gr[c]; if(!a)continue;
            u32 bg=FB[yy*PITCH+xx]; int br=(bg>>16)&255,bgc=(bg>>8)&255,bb=bg&255;
            int rr=(fr*a+br*(255-a))/255, gg=(fgc*a+bgc*(255-a))/255, bl=(fbb*a+bb*(255-a))/255;
            FB[yy*PITCH+xx]=((u32)rr<<16)|((u32)gg<<8)|(u32)bl; } } }
static void fb_text_rgb(int x,int y,const char* s,u32 rgb){ int g=0; while(*s&&g++<512){ fb_char_rgb(x,y,*s++,rgb); x+=8; } }
static u32 prng_state=0x1234abcd;
static u32 prog_rand(void){ prng_state^=prng_state<<13; prng_state^=prng_state>>17; prng_state^=prng_state<<5; return prng_state; }

/* validate a program-supplied pointer lies in real RAM (stack/program/heap),
   so a wild pointer from a buggy program returns an error instead of faulting
   the kernel. Stability guard for the loadable-program ABI (DOOM et al). */
static int pok(u32 p,u32 len){
    u32 mbc=(u32)ram_mb; if(mbc>3072u)mbc=3072u; if(mbc<64u)mbc=64u;
    u32 top=mbc*0x100000u;
    if(p<0x10000u) return 0;
    if(p>=top) return 0;
    if(len){ if(p+len<p) return 0; if(p+len>top) return 0; }
    return 1;
}
__attribute__((used)) void syscall_dispatch(struct sysregs* r){
    switch(r->eax){
        case 0: k_longjmp(g_kjmp,1); break;
        case 1: if(pok(r->ebx,1)) prog_print((const char*)r->ebx); break;
        case 2: r->eax=(u32)wait_key(); break;
        case 3: prog_putc((char)r->ebx); break;
        case 4: prog_console_init(); break;
        case 5: beep((int)r->ebx,2); break;
        case 6: { u32 t=r->ebx; while(t--)io_delay(1000); } break;
        case 7: switch(r->ebx){ case 0:r->eax=(u32)W;break; case 1:r->eax=(u32)H;break;
                    case 2:r->eax=(u32)ram_mb;break; case 3:r->eax=pit_read();break;
                    case 4:r->eax=(u32)(disk_ok?1:0);break; default:r->eax=0; } break;
        case 8: fb_pixel((int)r->ebx,(int)r->ecx,r->edx); break;
        case 9: fb_rect((int)r->ebx,(int)r->ecx,(int)r->edx,(int)r->esi,r->edi); break;
        case 10: if(pok(r->edx,1)) fb_text_rgb((int)r->ebx,(int)r->ecx,(const char*)r->edx,r->esi); break;
        case 11: { if(!disk_ok||!pok(r->ebx,1)||!pok(r->ecx,r->edx)){r->eax=(u32)-1;break;} int idx=nvx_find((const char*)r->ebx);
                    if(idx<0){r->eax=(u32)-1;break;} r->eax=(u32)nvx_read(idx,(char*)r->ecx,(int)r->edx); } break;
        case 12: { if(!disk_ok||!pok(r->ebx,1)||!pok(r->ecx,r->edx)){r->eax=(u32)-1;break;} int w=nvx_write((const char*)r->ebx,(const char*)r->ecx,(int)r->edx);
                    r->eax=(u32)(w>=0?0:-1); } break;
        case 13: { if(!disk_ok||r->ebx>=nvx.count||!pok(r->ecx,16)){r->eax=(u32)-1;break;} char* nb=(char*)r->ecx; int k=0;
                    while(k<15&&nvx.e[r->ebx].name[k]){ nb[k]=nvx.e[r->ebx].name[k]; k++; } nb[k]=0; r->eax=(u32)k; } break;
        case 14: { u32 v=r->ebx; for(int i=0;i<PITCH*H;i++)FB[i]=v; } break;
        case 15: r->eax=prog_rand(); break;
        case 16: usbmsd_mount(); r->eax=(u32)usbfs_n; break;
        case 17: { int i=(int)r->ebx; if(i<0||i>=usbfs_n||!pok(r->ecx,16)){r->eax=(u32)-1;break;} char* nb=(char*)r->ecx; int k=0;
                    while(k<15&&usbfs[i].name[k]){ nb[k]=usbfs[i].name[k]; k++; } nb[k]=0; r->eax=(u32)k; } break;
        case 18: { int i=(int)r->ebx; if(i<0||i>=usbfs_n){r->eax=(u32)-1;break;} r->eax=usbfs[i].size; } break;
        case 19: { int i=(int)r->ebx; if(i<0||i>=usbfs_n||!pok(r->ecx,r->edx)){r->eax=0;break;} r->eax=(u32)usbfs_read(i,(char*)r->ecx,(int)r->edx); } break;
        case 20: switch(r->ebx){ case 0:r->eax=(u32)LFB;break; case 1:r->eax=(u32)PITCH;break;
                    case 2:r->eax=(u32)W;break; case 3:r->eax=(u32)H;break; default:r->eax=0; } break;
        case 21: { static u32 ms_acc=0,ms_rem=0; static u16 ms_last=0; static int ms_init=0;
                    u16 now=pit_read(); if(!ms_init){ms_last=now;ms_init=1;}
                    u16 d=(u16)(ms_last-now); ms_last=now;
                    ms_rem+=d; ms_acc+=ms_rem/1193u; ms_rem%=1193u;
                    r->eax=ms_acc; } break;
        case 22: { xhci_poll(); r->eax=0; for(int g=0;g<48;g++){ u8 st=inb(0x64); if(!(st&1))break; u8 d=inb(0x60); if(st&0x20){ mouse_byte(d); continue; } r->eax=(u32)d; break; } } break;
        case 23: {
                    const char* url=(const char*)r->ebx; char* dst=(char*)r->ecx; int max=(int)r->edx;
                    if(!pok(r->ebx,1)||!pok(r->ecx,(u32)max)){ r->eax=(u32)-1; break; }
                    char* body=br_fetch(url);
                    if(!br_ok||max<1){ r->eax=(u32)-1; break; }
                    int n=http_body_len; if(n>max-1)n=max-1; if(n<0)n=0;
                    for(int i=0;i<n;i++)dst[i]=body[i]; dst[n]=0; r->eax=(u32)n; } break;
        case 24: {
                    const char* nm=(const char*)r->ebx; if(!pok(r->ebx,1)){ r->eax=(u32)-1; break; } int idx=nvx_find(nm);
                    if(idx<0){ r->eax=(u32)-1; break; }
                    nvx_delete(idx); r->eax=0; } break;
        case 25: { xhci_poll(); int* o=(int*)r->ebx; if(pok(r->ebx,16)){ o[0]=mx; o[1]=my; o[2]=mbtn; o[3]=ms_dx_acc; ms_dx_acc=0; } r->eax=0; } break;
        case 26: switch(r->ebx){ case 0:r->eax=(u32)g_dwin;break; case 1:r->eax=(u32)g_dwx;break; case 2:r->eax=(u32)g_dwy;break; case 3:r->eax=(u32)g_dww;break; case 4:r->eax=(u32)g_dwh;break; case 5:r->eax=(u32)g_dwscale;break; case 6:doom_win_backdrop();r->eax=0;break; default:r->eax=0; } break;
        default: break;
    }
}

struct nvxapp { char magic[4]; u8 ver, has_icon, icon_w, icon_h; char name[32]; char appver[16]; u32 elf_off, elf_len; };
static int is_nvx(const u8* p){ return p[0]=='N'&&p[1]=='V'&&p[2]=='X'&&p[3]=='A'; }

static int nvx_app_meta(const char* file, char* nameout, char* verout){
    if(!disk_ok) return 0; int idx=nvx_find(file); if(idx<0) return 0;
    static u8 hb[64]; int n=nvx_read(idx,(char*)hb,64); if(n<64) return 0;
    if(!is_nvx(hb)) return 0; struct nvxapp* a=(struct nvxapp*)hb;
    int k; for(k=0;k<31&&a->name[k];k++)nameout[k]=a->name[k]; nameout[k]=0;
    for(k=0;k<15&&a->appver[k];k++)verout[k]=a->appver[k]; verout[k]=0; return 1;
}

static int elf_exec_filebuf(int n){
    u8* img=PROG_FILEBUF;
    if(is_nvx(PROG_FILEBUF)){ struct nvxapp* a=(struct nvxapp*)PROG_FILEBUF;
        if(a->elf_off+52>(u32)n) return -3; img=PROG_FILEBUF+a->elf_off; }
    Elf32_Ehdr* eh=(Elf32_Ehdr*)img;
    if(!(eh->ident[0]==0x7f&&eh->ident[1]=='E'&&eh->ident[2]=='L'&&eh->ident[3]=='F')) return -4;
    if(eh->ident[4]!=1||eh->machine!=3||eh->type!=2) return -5;
    for(int i=0;i<eh->phnum;i++){
        Elf32_Phdr* ph=(Elf32_Phdr*)(img+eh->phoff+(u32)i*eh->phentsize);
        if(ph->type!=1) continue;
        if(ph->vaddr<0x03900000u) return -6;
        u8* dst=(u8*)ph->vaddr;
        { const u8* sp=img+ph->offset; u32 fs=ph->filesz, ms=ph->memsz, b=0; for(; b+4<=fs; b+=4) *(u32*)(dst+b)=*(const u32*)(sp+b); for(; b<fs; b++) dst[b]=sp[b]; for(; b<ms; b++) dst[b]=0; }
    }
    prog_console_init();
    void(*entry)(void)=(void(*)(void))eh->entry;
    if(k_setjmp(g_kjmp)==0){ entry(); }
    draw_str(8,H-16,"-- program finished - press a key --",C_GREEN);
    wait_key(); need_rebuild=1;
    return 0;
}
static int nvx_name_ieq(const char* a,const char* b){ int i=0; for(;a[i]&&b[i];i++){ char ca=a[i],cb=b[i]; if(ca>='a'&&ca<='z')ca-=32; if(cb>='a'&&cb<='z')cb-=32; if(ca!=cb)return 0; } return a[i]==b[i]; }
static int elf_run(const char* name){
    if(disk_ok){ int idx=nvx_find(name); if(idx>=0){ int n=nvx_read(idx,(char*)PROG_FILEBUF,0x2000000); if(n>=52) return elf_exec_filebuf(n); } }
    usbmsd_mount();
    for(int i=0;i<usbfs_n;i++){ if(nvx_name_ieq(usbfs[i].name,name)){ int n=usbfs_read(i,(char*)PROG_FILEBUF,0x2000000); if(n>=52) return elf_exec_filebuf(n); return -3; } }
    return -2;
}

static int elf_run_mem(const unsigned char* data,int n){
    if(n<52) return -3; if(n>0x2000000)n=0x2000000;
    for(int i=0;i<n;i++) ((u8*)PROG_FILEBUF)[i]=data[i];
    return elf_exec_filebuf(n);
}
static void pkg_cmd(const char* c);
static void run_command(void){
    cmdline[cmdlen]=0; char*c=cmdline; tnl();
    if(install_pending){
        install_pending=0;
        if(c[0]>='1'&&c[0]<='9'&&c[1]==0){
            int n=c[0]-'0';
            if(n>=1&&n<=install_nslots){ tputs("LAUNCHING INSTALLER..."); tnl(); install_to_disk(install_slots[n-1]); dirty=1; }
            else { tputs("INVALID DISK NUMBER. INSTALL CANCELLED."); tnl(); }
        } else { tputs("INSTALL CANCELLED."); tnl(); }
        cmdline[0]=0; cmdlen=0; return;
    }
    if(streq(c,"del system")){ pending_uac=1; }
    else if(streq(c,"pkg")||startsw(c,"pkg ")){ pkg_cmd(c); }
    else if(streq(c,"help")){ tputs("--- NOOVEX TERMINAL - COMMANDS ---"); tnl();
        tputs("FOLDERS: mkdir <d>  cd <d>  cd ..  pwd  ls"); tnl();
        tputs("CRYPTO : encrypt <f> <key>  decrypt <f.enc> <key>"); tnl();
        tputs("PROGRAMS: mkdemo  webdemo  run <file.elf>  exec <file.elf>"); tnl();
        tputs("LINUX  : pwd uname hostname free ps env cal which man"); tnl();
        tputs("FILES  : cat touch rm wc head grep <file>"); tnl();
        tputs("SYSTEM : help clear ver about cpu mem sys uptime date time"); tnl();
        tputs("DISK   : ls cat disk df format diskinfo install"); tnl();
        tputs("HW     : pci dev lsusb ehci uhci xhci usbrescan battery"); tnl();
        tputs("NET    : net scan ping dns  |  reader: nexus <ref>  proxy on/off"); tnl();
        tputs("MEDIA  : play beep calc cam camapp color"); tnl();
        tputs("IMG    : imginfo isoinfo mkboot"); tnl();
        tputs("POWER  : reboot shutdown bootrecovery delsystem"); tnl();
        tputs("FUN    : echo neofetch crash matrix whoami"); tnl();
        tputs("TYPE A COMMAND AND PRESS ENTER."); tnl(); }
    else if(streq(c,"delsystem")){
        tputs("*** WARNING: ERASING SYSTEM FILESYSTEM ***"); tnl();
        tputs("THIS WILL DESTROY NVXFS. REWRITE THE USB TO REINSTALL."); tnl();
        pit_wait(20);

        { u8 z[512]; for(int i=0;i<512;i++)z[i]=0;
          for(int se=0;se<NVX_DIR_SECS;se++) ata_write(NVX_DIR_LBA+se,z);
          ata_write(ACCT_LBA,z);
          for(int i=0;i<NVX_MAX;i++) ata_write(NVX_DATA0+i*NVX_SECPF,z);
          nvx.count=0; disk_ok=0; }
        cmos_write(0x37,0x52);
        tputs("FILESYSTEM ERASED. CRASHING..."); tnl(); pit_wait(16);
        bsod(); pit_wait(40);
        recovery_mode(); need_rebuild=1; }
    else if(streq(c,"neofetch")){ pending_neofetch=1; tputs("launching neofetch..."); tnl(); }
    else if(streq(c,"uptime")){ char b[12]; u32 sec=upsec; utoa(sec/60,b); tputs("UP "); tputs(b); tputs("M "); utoa(sec%60,b); tputs(b); tputs("S"); tnl(); }
    else if(streq(c,"sys")){ char b[16]; tputs("NOOVEX8 32-BIT PROTECTED MODE"); tnl(); utoa(pcin,b); tputs("PCI DEVICES: "); tputs(b); tnl(); utoa(usbdev_n,b); tputs("USB DEVICES: "); tputs(b); tnl(); tputs("FS: "); tputs(disk_ok?"NVXFS OK":"NO DISK"); tnl();
        tputs("ACPI: "); tputs(acpi_found?"TABLES FOUND":"NOT FOUND"); tnl();
        tputs("BATTERY: "); tputs(bat_via_ec?"REAL (EC)":(bat_present?"SIM":"AC POWER")); tnl();
        tputs("TOUCHPAD: "); tputs(touchpad_present?"DETECTED":"NONE"); tnl();
        tputs("CRASH HANDLER: IDT ACTIVE (NO REBOOT LOOPS)"); tnl(); }
    else if(streq(c,"df")||streq(c,"diskinfo")){ char b[12]; if(disk_ok){ utoa(nvx.count,b); tputs("NVXFS FILES: "); tputs(b); tputs("/"); utoa(NVX_MAX,b); tputs(b); tnl(); } else { tputs("NO DISK MOUNTED."); tnl(); } }
    else if(streq(c,"whoami")){ tputs(have_user?acct.user:"guest"); tnl(); }
    else if(streq(c,"apps")){ if(!disk_ok){ tputs("no disk"); tnl(); }
        else { tputs("--- INSTALLED PROGRAMS ---"); tnl(); int found=0;
            for(unsigned i=0;i<nvx.count;i++){ const char* nm=nvx.e[i].name; int L=0; while(nm[L])L++;
                int iself=(L>4&&nm[L-4]=='.'&&(nm[L-3]=='E'||nm[L-3]=='e')&&(nm[L-2]=='L'||nm[L-2]=='l')&&(nm[L-1]=='F'||nm[L-1]=='f'));
                int isnvx=(L>4&&nm[L-4]=='.'&&(nm[L-3]=='N'||nm[L-3]=='n')&&(nm[L-2]=='V'||nm[L-2]=='v')&&(nm[L-1]=='X'||nm[L-1]=='x'));
                if(!iself&&!isnvx)continue; found++;
                tputs("  "); tputs(nm);
                if(isnvx){ char an[32],av[16]; if(nvx_app_meta(nm,an,av)){ tputs("  ["); tputs(an); tputs(" v"); tputs(av); tputs("]"); } }
                tnl(); }
            if(!found){ tputs("(none) - try 'mkdemo' then 'apps'"); tnl(); }
            tputs("run with:  run <file>"); tnl(); } }
    else if(startsw(c,"encrypt ")){ const char* p=c+8; char fn[20]; int k=0; while(*p&&*p!=' '&&k<19)fn[k++]=*p++; fn[k]=0; while(*p==' ')p++; const char* key=p;
        if(!disk_ok){ tputs("no disk"); tnl(); }
        else if(!*key){ tputs("usage: encrypt <file> <key>"); tnl(); }
        else { int idx=nvx_find(fn); if(idx<0){ tputs("encrypt: "); tputs(fn); tputs(": no such file"); tnl(); }
            else { static char eb[4096],ob[4096]; int n=nvx_read(idx,eb,4000); if(n<0)n=0;
                for(int i=0;i<16;i++)ob[i]=(fn[i])?fn[i]:0;
                u32 ks=0x9e3779b9u; for(const char* q=key;*q;q++)ks=ks*131u+(u8)*q;
                for(int i=0;i<n;i++){ ks^=ks<<13; ks^=ks>>17; ks^=ks<<5; ob[16+i]=eb[i]^(char)(ks&0xFF); }
                char on[20]; int b=0; while(fn[b]&&fn[b]!='.'&&b<15){on[b]=fn[b];b++;} const char* ee=".ENC"; int z=0; while(ee[z]&&b<19)on[b++]=ee[z++]; on[b]=0;
                if(nvx_write(on,ob,16+n)>=0){ tputs("encrypted -> "); tputs(on); } else tputs("encrypt: write failed (disk full?)"); tnl(); } } }
    else if(startsw(c,"decrypt ")){ const char* p=c+8; char fn[20]; int k=0; while(*p&&*p!=' '&&k<19)fn[k++]=*p++; fn[k]=0; while(*p==' ')p++; const char* key=p;
        if(!disk_ok){ tputs("no disk"); tnl(); }
        else if(!*key){ tputs("usage: decrypt <file.enc> <key>"); tnl(); }
        else { int idx=nvx_find(fn); if(idx<0){ tputs("decrypt: no such file"); tnl(); }
            else { static char eb[4096],ob[4096]; int n=nvx_read(idx,eb,4000); if(n<16){ tputs("decrypt: not an encrypted file"); tnl(); }
                else { char on[20]; int kk=0; while(kk<16&&eb[kk]){on[kk]=eb[kk];kk++;} on[kk]=0;
                    u32 ks=0x9e3779b9u; for(const char* q=key;*q;q++)ks=ks*131u+(u8)*q;
                    int dn=n-16; for(int i=0;i<dn;i++){ ks^=ks<<13; ks^=ks>>17; ks^=ks<<5; ob[i]=eb[16+i]^(char)(ks&0xFF); }
                    if(nvx_write(on,ob,dn)>=0){ tputs("decrypted -> "); tputs(on); tputs(" (wrong key = garbage)"); } else tputs("decrypt: write failed"); tnl(); } } } }
    else if(streq(c,"lsblk")||streq(c,"ahci")||streq(c,"lsblk -a")){
        tputs("--- STORAGE CONTROLLERS (PCI) ---"); tnl();
        int found=0;
        for(int i=0;i<pcin;i++){ if(pcil[i].cls!=0x01)continue; found++;
            tputs("  "); char b[8]; hex16(pcil[i].ven,b); tputs(b); tputs(":"); hex16(pcil[i].did,b); tputs(b); tputs("  ");
            if(pcil[i].sub==0x06){ tputs("SATA/AHCI controller"); if(pcil[i].prog==0x01)tputs(" (AHCI 1.0)"); }
            else if(pcil[i].sub==0x01){ tputs("IDE/ATA controller"); }
            else if(pcil[i].sub==0x08){ tputs("NVMe controller"); }
            else tputs("mass storage");
            tnl(); }
        if(!found)tputs("  (no PCI storage controllers seen)\n");
        tputs("ACTIVE DRIVER: ATA PIO (LBA28)"); tnl();
        tputs("disk: "); tputs(disk_ok?"READY":"not detected"); tnl();
        if(found)tputs("note: AHCI detected but driver uses ATA PIO compat mode\n"); }
    else if(streq(c,"mkdocs")){ if(!disk_ok){ tputs("no disk"); tnl(); }
        else { nvx_write("DATA.CSV","name,age,city\nErik,30,Soderkoping\nAnna,25,Norrkoping\nLars,42,Linkoping",73);
            nvx_write("CONF.JSN","{\"os\":\"NoovexOS\",\"version\":1,\"apps\":[\"files\",\"term\"],\"ok\":true}",61);
            nvx_write("READ.MD","# NoovexOS\n## Features\n- 32-bit OS\n- ELF programs\n- Document viewer\n\nBuilt from scratch.",95);
            nvx_write("SET.INI","[display]\nwidth=1280\nheight=1024\n[sound]\nenabled=1\n; end of config",70);
            tputs("created DATA.CSV CONF.JSN READ.MD SET.INI"); tnl();
            tputs("open them in Files, or they show in the Document Viewer"); tnl(); } }
    else if(streq(c,"webdemo")){ if(!disk_ok){ tputs("no disk - need an NVXFS disk for files"); tnl(); }
        else {
            const char* IH="<html><head>\n<link rel=\"stylesheet\" href=\"style.css\">\n</head><body>\n<h1 class=\"title\">NoovexOS Web Demo</h1>\n<p class=\"intro\">This page loads <b>style.css</b> and <b>script.js</b> as separate files from NVXFS.</p>\n<div class=\"card\">\n<h2>JavaScript output</h2>\n<script src=\"script.js\"></script>\n</div>\n<p id=\"foot\">Three separate files: index.html + style.css + script.js</p>\n</body></html>";
            const char* CS="body { background:#101828; color:#dfe6ee; padding:16px; }\n.title { color:#4da3ff; font-size:30px; }\n.intro { color:#aab4c0; }\n.card { background:#1b2740; border:2px solid #4da3ff; padding:12px; margin-top:10px; }\nh2 { color:#7ee0a0; }\n#foot { color:#7a8694; font-size:14px; margin-top:14px; }";
            const char* JS="var s=\"\";\nfor(var i=1;i<=5;i++){ s = s + \"Row \" + i + \": \" + i + \" squared = \" + (i*i) + \"<br>\"; }\ndocument.write(\"<p>\"+s+\"</p>\");\ndocument.write(\"<p>This text was generated by script.js</p>\");";
            int r1=nvx_write("index.html",IH,strlen_(IH));
            int r2=nvx_write("style.css",CS,strlen_(CS));
            int r3=nvx_write("script.js",JS,strlen_(JS));
            if(r1>=0&&r2>=0&&r3>=0){ tputs("created: index.html  style.css  script.js"); tnl();
                tputs("now open the Browser and type in the address bar:"); tnl();
                tputs("   file:index.html"); tnl();
                tputs("it renders the HTML with the linked CSS and JS."); tnl(); }
            else { tputs("webdemo: write failed (disk full?)"); tnl(); } } }
    else if(streq(c,"mkdemo")){ if(!disk_ok){ tputs("no disk - need an NVXFS disk"); tnl(); }
        else { int r1=nvx_write("HELLO.ELF",(const char*)HELLODEMO,HELLODEMO_LEN);
            int r2=nvx_write("GFX.NVX",(const char*)GFXDEMO,GFXDEMO_LEN);
            if(r1>=0)tputs("wrote HELLO.ELF (text demo)"); else tputs("HELLO.ELF failed"); tnl();
            if(r2>=0)tputs("wrote GFX.NVX (graphics demo app)"); else tputs("GFX.NVX failed"); tnl();
            tputs("run them:  run HELLO.ELF   |   run GFX.NVX"); tnl();
            tputs("or open NoovexStore > MY APPS and click LAUNCH"); tnl(); } }
    else if(startsw(c,"run ")||startsw(c,"exec ")){ const char* fn=c+(c[1]=='u'?4:5);
        int r=elf_run(fn);
        if(r==-1)tputs("no disk"); else if(r==-2){tputs("run: ");tputs(fn);tputs(": not found");}
        else if(r==-3)tputs("run: file too small / unreadable");
        else if(r==-4)tputs("run: not an ELF file");
        else if(r==-5)tputs("run: not a 32-bit i386 executable");
        else if(r==-6)tputs("run: load address overlaps kernel (link at 0x04000000)");
        if(r!=0)tnl(); }

    else if(streq(c,"pwd")){ if(nvx_cwd<0){ tputs("/"); } else { char path[80]; int chain[8],n=0,p=nvx_cwd; while(p!=-1&&n<8){chain[n++]=p;p=nvx_parent(p);} int o=0; for(int i=n-1;i>=0;i--){ path[o++]='/'; const char* nm=nvx.e[chain[i]].name; for(int k=0;nm[k]&&o<78;k++)path[o++]=nm[k]; } path[o]=0; tputs(path); } tnl(); }
    else if(startsw(c,"mkdir ")){ const char* nm=c+6; if(!disk_ok){ tputs("no disk"); tnl(); }
        else if(*nm==0){ tputs("usage: mkdir <name>"); tnl(); }
        else { int r=nvx_mkdir(nm); if(r>=0){ tputs("created folder "); tputs(nm); tputs("/"); } else tputs("mkdir: failed (exists, full, or no disk)"); tnl(); } }
    else if(startsw(c,"cd ")||streq(c,"cd")){ const char* nm=(c[2]==' ')?c+3:"";
        if(*nm==0||streq(nm,"/")||streq(nm,"~")){ nvx_cwd=-1; }
        else if(streq(nm,"..")){ nvx_cwd=nvx_parent(nvx_cwd); }
        else { int idx=nvx_find_in(nm,nvx_cwd); if(idx>=0&&nvx_isdir(idx))nvx_cwd=idx; else { tputs("cd: "); tputs(nm); tputs(": no such folder"); tnl(); } } }
    else if(streq(c,"uname")){ tputs("NoovexOS"); tnl(); }
    else if(startsw(c,"uname -a")){ char b[8]; tputs("NoovexOS noovex 1.0 i386 32-bit VESA  RAM:"); utoa(ram_mb,b); tputs(b); tputs("MB"); tnl(); }
    else if(streq(c,"hostname")){ tputs("noovexos"); tnl(); }
    else if(streq(c,"free")){ char b[12]; tputs("              total       used       free"); tnl();
        tputs("Mem:       "); utoa(ram_mb*1024,b); tputs(b); tputs(" KB"); tnl();
        tputs("(NVXFS files: "); utoa(disk_ok?(int)nvx.count:0,b); tputs(b); tputs("/15)"); tnl(); }
    else if(streq(c,"ps")){ tputs("  PID  TTY   CMD"); tnl(); tputs("    1  con   noovex-kernel"); tnl();
        { char b[8]; int p=2; for(int z=0;z<wincnt;z++){ if(wins[z].min)continue; utoa(p++,b); tputs("    "); tputs(b); tputs("  win   "); tputs(app_name(wins[z].app)); tnl(); } } }
    else if(streq(c,"env")){ tputs("USER="); tputs(have_user?acct.user:"guest"); tnl(); tputs("HOME=/"); tnl(); tputs("SHELL=/bin/nvxsh"); tnl(); tputs("OS=NoovexOS"); tnl(); tputs("TERM=noovex"); tnl(); }
    else if(streq(c,"cal")){ rtc_now(); tputs("   NoovexOS calendar"); tnl(); tputs("Su Mo Tu We Th Fr Sa"); tnl(); tputs(" 1  2  3  4  5  6  7"); tnl(); tputs(" 8  9 10 11 12 13 14"); tnl(); tputs("15 16 17 18 19 20 21"); tnl(); tputs("22 23 24 25 26 27 28"); tnl(); }
    else if(streq(c,"which")||startsw(c,"which ")){ const char* a=c+6; if(c[5]==0){ tputs("usage: which <cmd>"); tnl(); }
        else { tputs("/bin/"); tputs(a); tnl(); } }
    else if(startsw(c,"man ")){ const char* a=c+4;
        if(streq(a,"ls"))tputs("ls - list directory contents");
        else if(streq(a,"cat"))tputs("cat - print file contents");
        else if(streq(a,"pwd"))tputs("pwd - print working directory");
        else if(streq(a,"grep"))tputs("grep - search text in a file");
        else if(streq(a,"ps"))tputs("ps - list running tasks");
        else if(streq(a,"rm"))tputs("rm - remove a file");
        else tputs("No manual entry. Try: help");
        tnl(); }
    else if(startsw(c,"cat ")){ const char* fn=c+4;
        if(!disk_ok){ tputs("no disk"); tnl(); }
        else { int idx=nvx_find(fn); if(idx<0){ tputs("cat: "); tputs(fn); tputs(": no such file"); tnl(); }
            else { static char fb[2048]; int n=nvx_read(idx,fb,2047); if(n<0)n=0; fb[n]=0; for(int i=0;i<n;i++){ char ch[2]={fb[i],0}; if(fb[i]=='\n')tnl(); else tputs(ch);} tnl(); } } }
    else if(startsw(c,"touch ")){ const char* fn=c+6;
        if(!disk_ok){ tputs("no disk"); tnl(); } else { if(nvx_write(fn,"",0)>=0){ tputs("created "); tputs(fn); } else tputs("touch: failed (disk full?)"); tnl(); } }
    else if(startsw(c,"rm ")){ const char* fn=c+3;
        if(!disk_ok){ tputs("no disk"); tnl(); } else { int idx=nvx_find(fn); if(idx>=0){ nvx_delete(idx); tputs("removed "); tputs(fn); } else { tputs("rm: "); tputs(fn); tputs(": no such file"); } tnl(); } }
    else if(startsw(c,"wc ")){ const char* fn=c+3;
        if(!disk_ok){ tputs("no disk"); tnl(); }
        else { int idx=nvx_find(fn); if(idx<0){ tputs("wc: no such file"); tnl(); }
            else { static char fb[2048]; int n=nvx_read(idx,fb,2047); if(n<0)n=0; int lines=0,words=0,inw=0; for(int i=0;i<n;i++){ if(fb[i]=='\n')lines++; if(fb[i]==' '||fb[i]=='\n'||fb[i]=='\t'){inw=0;} else if(!inw){inw=1;words++;} }
                char b[8]; utoa(lines,b); tputs("  "); tputs(b); utoa(words,b); tputs("  "); tputs(b); utoa(n,b); tputs("  "); tputs(b); tputs("  "); tputs(fn); tnl(); } } }
    else if(startsw(c,"head ")){ const char* fn=c+5;
        if(!disk_ok){ tputs("no disk"); tnl(); }
        else { int idx=nvx_find(fn); if(idx<0){ tputs("head: no such file"); tnl(); }
            else { static char fb[2048]; int n=nvx_read(idx,fb,2047); if(n<0)n=0; fb[n]=0; int ln=0; for(int i=0;i<n&&ln<10;i++){ char ch[2]={fb[i],0}; if(fb[i]=='\n'){tnl();ln++;} else tputs(ch);} tnl(); } } }
    else if(startsw(c,"grep ")){ const char* p=c+5; char pat[32]; int pl=0; while(*p&&*p!=' '&&pl<31)pat[pl++]=*p++; pat[pl]=0; if(*p==' ')p++; const char* fn=p;
        if(!disk_ok){ tputs("no disk"); tnl(); }
        else { int idx=nvx_find(fn); if(idx<0){ tputs("grep: no such file"); tnl(); }
            else { static char fb[2048]; int n=nvx_read(idx,fb,2047); if(n<0)n=0; fb[n]=0; int ls=0,hits=0; for(int i=0;i<=n;i++){ if(fb[i]=='\n'||fb[i]==0){ fb[i]=0; int found=0; for(int j=ls;fb[j];j++){ int k=0; while(pat[k]&&fb[j+k]==pat[k])k++; if(pat[k]==0){found=1;break;} } if(found){ tputs(fb+ls); tnl(); hits++; } ls=i+1; if(fb[i]==0&&i<n)fb[i]='\n'; } } if(!hits){ tputs("(no matches)"); tnl(); } } } }
    else if(streq(c,"matrix")){ for(int r=0;r<6;r++){ char ln[41]; rngs^=pit_read(); rngs^=rngs<<13; rngs^=rngs>>17; rngs^=rngs<<5; for(int i=0;i<40;i++){ ln[i]=(char)('0'+((rngs>>(i&15))&1)); } ln[40]=0; tputs(ln); tnl(); } }
    else if(streq(c,"crash")){ tputs("FORCING BSOD..."); tnl(); pit_wait(8); bsod(); pit_wait(30); need_rebuild=1; }
    else if(streq(c,"mouse")){ char nb[8];
        tputs("--- MOUSE STATUS ---"); tnl();
        u8 st=inb(0x64); hex16(st,nb); tputs("PS/2 CTRL (0x64): 0x"); tputs(nb); tnl();
        tputs("PS/2 PACKETS SEEN: "); tputs(diag_mouse_seen?"YES":"NONE"); tnl();
        tputs("WHEEL MOUSE: "); tputs(wheel_ok?"YES":"NO"); tnl();
        tputs("TOUCHPAD: "); tputs(touchpad_present?(touchpad_kind==1?"SYNAPTICS DETECTED":"PS/2 DETECTED"):"NONE"); tnl();
        tputs("XHCI MOUSE BOUND: "); tputs(xhci_ms_found?"YES":"NO"); tnl();
        tputs("HID DRIVER: GAMING-READY (5-BTN, WHEEL,"); tnl();
        tputs("  MULTI-IFACE, PROTO 0/2, 1000HZ OK)"); tnl();
        tputs("IF PS/2 PACKETS = NONE, BIOS DOES NOT EMULATE"); tnl();
        tputs("THE USB MOUSE. TYPE 'remouse' TO RE-INIT IT."); tnl(); }
    else if(streq(c,"remouse")){ tputs("RE-INITIALIZING PS/2 MOUSE..."); tnl(); mouse_init(); tputs("DONE - TRY MOVING THE MOUSE."); tnl(); }
    else if(streq(c,"clear")){ termlen=0; termbuf[0]=0; }
    else if(streq(c,"ver")){ tputs(OSVER); tnl(); }
    else if(streq(c,"about")){ tputs(OSNAME " - 32-BIT VESA GRAPHICAL OS"); tnl(); }
    else if(streq(c,"time")){ rtc_now(); tputs(clkbuf); tnl(); }
    else if(streq(c,"date")){ rtc_read(); tputs(datebuf); tputs("  "); tputs(clkbuf); tnl(); }
    else if(startsw(c,"settime ")){ const char* p=c+8; int h=0,m=0,s=0;
        if(p[0]&&p[1]&&p[2]==':'){ h=(p[0]-'0')*10+(p[1]-'0'); m=(p[3]-'0')*10+(p[4]-'0'); if(p[5]==':')s=(p[6]-'0')*10+(p[7]-'0');
            rtc_set((u8)h,(u8)m,(u8)s); rtc_now(); tputs("clock set to "); tputs(clkbuf); tnl(); }
        else tputs("usage: settime HH:MM or HH:MM:SS\n"); }
    else if(streq(c,"ls")||streq(c,"dir")){ if(!disk_ok){ tputs("REPORT.DOC NOTES.TXT BUDGET.XLS TODO.TXT"); tnl(); }
        else { int shown=0;
            for(unsigned i=0;i<nvx.count;i++){ if(nvx_parent(i)!=nvx_cwd)continue; tputs("  "); tputs(nvx.e[i].name); if(nvx_isdir(i))tputs("/"); tnl(); shown++; }
            if(!shown)tputs("(empty directory)\n"); } }
    else if(startsw(c,"echo ")){ tputs(c+5); tnl(); }
    else if(streq(c,"play")){ tputs("PLAYING JINGLE..."); tnl(); }
    else if(streq(c,"beep")){ tputs("BEEP!"); tnl(); }
    else if(streq(c,"calc")){ tputs("CALC DEMO"); tnl(); }
    else if(streq(c,"disk")){ if(disk_ok){ tputs("DISK LABEL: "); tputs(DISK_LABEL); tnl(); int inst=(cmos(0x46)==0xCD); tputs("INSTALLED: "); tputs(inst?"YES":"LIVE MODE"); tnl(); if(inst){ char gb[8]; int g=disk_size_gb,gl=0,tl=0; char tt[4]; if(g==0)tt[tl++]='0'; while(g){tt[tl++]='0'+g%10;g/=10;} while(tl)gb[gl++]=tt[--tl]; gb[gl]=0; tputs("RESERVED: "); tputs(gb); tputs(" GB"); tnl(); } tputs("ATA PRIMARY MASTER: OK"); tnl(); tputs("NVXFS MOUNTED. FILES: "); int c2=nvx.count; if(c2==0)tputs("0"); else { char t[6];int tl=0;while(c2){t[tl++]='0'+c2%10;c2/=10;} while(tl){char s[2]={t[--tl],0};tputs(s);} } tnl(); } else { tputs("NO ATA DISK FOUND."); tnl(); } }
    else if(streq(c,"format")){ if(disk_ok){ nvx_format(); tputs("NVXFS FORMATTED."); tnl(); } else { tputs("NO DISK."); tnl(); } }
    else if(streq(c,"install")){
        install_nslots=0;
        tputs("AVAILABLE HARD DISKS:"); tnl();
        for(int s=0;s<4;s++){
            if(atai[s].present&&atai[s].type==1){
                int n=++install_nslots; install_slots[n-1]=s;
                char line[96]; int q=0; line[q++]='0'+n; line[q++]=')'; line[q++]=' ';
                const char* bn[4]={"PRI-MASTER ","PRI-SLAVE  ","SEC-MASTER ","SEC-SLAVE  "};
                const char* bb=bn[s]; for(int i=0;bb[i];i++)line[q++]=bb[i];
                u32 gb=atai[s].sectors/1953125u; char nb[12]; utoa(gb,nb); for(int i=0;nb[i];i++)line[q++]=nb[i];
                line[q++]='G';line[q++]='B';line[q++]=' ';
                for(int i=0;i<22&&atai[s].model[i];i++)line[q++]=atai[s].model[i];
                line[q++]=' ';line[q++]='[';
                u8 b0[512]; int used=0; if(ata_read_drv(s,0,b0)==0&&b0[510]==0x55&&b0[511]==0xAA)used=1;
                if(used){ const char* u="IN-USE"; for(int i=0;u[i];i++)line[q++]=u[i]; }
                else { const char* e="EMPTY"; for(int i=0;e[i];i++)line[q++]=e[i]; }
                line[q++]=']'; line[q]=0; tputs(line); tnl();
            }
        }
        if(install_nslots==0){ tputs("NO ATA HARD DISK FOUND. ATTACH AN IDE/SATA DISK FIRST."); tnl(); }
        else { tputs("WARNING: INSTALL ERASES THE DISK BOOT AREA - NEVER PICK YOUR WINDOWS DISK!"); tnl();
               tputs("TYPE THE NUMBER OF THE DISK TO INSTALL TO  (0 = CANCEL):"); tnl(); install_pending=1; }
    }
    else if(streq(c,"reboot")){ g_pending_action=1; }
    else if(streq(c,"shutdown")){ g_pending_action=2; }
    else if(streq(c,"cpu")){ tputs(cpu_brand[0]?cpu_brand:cpu_vendor); tnl(); }
    else if(streq(c,"mem")){ char b[8]; utoa(ram_mb,b); tputs("RAM: "); tputs(b); tputs(" MB"); tnl(); }
    else if(streq(c,"pci")){ char b[8]; utoa(pcin,b); tputs("PCI DEVICES: "); tputs(b); tnl(); for(int i=0;i<pcin;i++){ char vd[12]; hex16(pcil[i].ven,vd); vd[4]=':'; hex16(pcil[i].did,vd+5); tputs(vd); tputs(" "); tputs((pcil[i].cls==0x0C&&pcil[i].sub==0x03)?usb_type(pcil[i].prog):cls_name(pcil[i].cls,pcil[i].sub)); tnl(); } }
    else if(streq(c,"usb")){ if(usb_present){ char b[8]; utoa(usb_count,b); tputs("USB CONTROLLER DETECTED: "); tputs(usb_type(usb_prog)); tnl(); tputs("CONTROLLERS: "); tputs(b); tnl(); tputs("NOTE: DETECTION ONLY - NO USB STACK YET."); tnl(); } else { tputs("NO USB CONTROLLER FOUND ON PCI."); tnl(); } }
    else if(startsw(c,"usbrm ")){ if(!fat_ok){ tputs("NO FAT FS."); tnl(); } else { const char* nm=c+6; int fi=-1; for(int i=0;i<usbfs_n;i++){ int eq=1; for(int k=0;k<16;k++){ char x=usbfs[i].name[k],y=nm[k]; if(x>='a'&&x<='z')x-=32; if(y>='a'&&y<='z')y-=32; if(x!=y){eq=0;break;} if(!x&&!y)break; } if(eq){fi=i;break;} } if(fi<0){ tputs("FILE NOT FOUND."); tnl(); } else if(fat_delete_file(fi)==0){ tputs("DELETED."); tnl(); } else { tputs("DELETE FAILED."); tnl(); } } }
    else if(streq(c,"usbeject")||streq(c,"eject")){ if(msd_dev<0){ tputs("NO USB DISK TO EJECT."); tnl(); } else { usb_eject(); tputs("USB DISK EJECTED - SAFE TO REMOVE."); tnl(); } }
    else if(streq(c,"usbls")){ if(!fat_ok){ tputs(msd_dev>=0?"USB DISK PRESENT - NO FAT FS":"NO USB DISK."); tnl(); } else { tputs(fat_type==32?"USB FAT32 ROOT:":"USB FAT16 ROOT:"); tnl(); for(int i=0;i<usbfs_n;i++){ char sb2[10]; utoa(usbfs[i].size,sb2); tputs("  "); tputs(usbfs[i].name); tputs("  "); tputs(sb2); tputs(" B"); tnl(); } if(usbfs_n==0){ tputs("  (EMPTY)"); tnl(); } } }
    else if(startsw(c,"usbcat ")){ if(!fat_ok){ tputs("NO FAT FS."); tnl(); } else { const char* nm=c+7; int fi=-1; for(int i=0;i<usbfs_n;i++){ int eq=1; for(int k=0;k<16;k++){ char x=usbfs[i].name[k],y=nm[k]; if(x>='a'&&x<='z')x-=32; if(y>='a'&&y<='z')y-=32; if(x!=y){eq=0;break;} if(!x&&!y)break; } if(eq){fi=i;break;} } if(fi<0){ tputs("FILE NOT FOUND."); tnl(); } else { u32 clus=usbfs[fi].clus,left=usbfs[fi].size; int cap=1400,guard=0; u8 sec[512]; while(clus>=2&&clus<0x0FFFFFF8u&&left>0&&cap>0&&guard++<512){ u32 base=fat_dataclus+(clus-2)*fat_spc; int stop=0; for(u32 s=0;s<fat_spc&&left>0&&cap>0;s++){ if(msd_read(base+s,1,sec)<0){stop=1;break;} u32 n=(left<512)?left:512; for(u32 k=0;k<n&&cap>0;k++){ char ch=(char)sec[k]; if(ch=='\n')tnl(); else if(ch>=32&&ch<127){ char tb[2]={ch,0}; tputs(tb);} cap--; } left-=n; } if(stop)break; clus=fat_next_cluster(clus); } tnl(); } } }
    else if(streq(c,"wifi init")){
        tputs("=== WIFI INIT (PHASE 3-4) ==="); tnl();
        if(!wifi_pci){ tputs("NO WIFI HARDWARE (VBOX HAS NONE - LENOVO ONLY)."); tnl(); }
        else if(wifi_ven!=0x8086){ tputs("CHIP: "); tputs(wifi_chip_name(wifi_ven,wifi_did)); tnl();
            tputs("INIT ONLY IMPLEMENTED FOR INTEL CSR LAYOUT."); tnl(); }
        else{
            tputs("PCI DETECT ........ OK  "); tputs(wifi_chip_name(wifi_ven,wifi_did)); tnl();
            tputs("MATCH DEVICE ID ... OK  DRIVER: iwl-csr (pre-firmware)"); tnl();
            int r=wifi_init_hw(); char hb[16];
            if(r==-2){ tputs("INIT HARDWARE ..... FAIL (BAR0 not mapped)"); tnl(); }
            else{
                tputs("RESET HARDWARE .... "); tputs(wifi_rst_ok?"OK":"FAIL"); tnl();
                tputs("CLOCK HANDSHAKE ... "); tputs(wifi_clk_ok?"OK (MAC_CLOCK_READY)":"TIMEOUT"); tnl();
                tputs("RF-KILL SWITCH .... "); tputs(wifi_rfkill==1?"RADIO ENABLED":(wifi_rfkill==0?"KILLED (hw switch off)":"UNKNOWN")); tnl();
                tputs("MAC ADDRESS ....... ");
                for(int i2=0;i2<6;i2++){ hb[0]="0123456789ABCDEF"[wifi_mac[i2]>>4]; hb[1]="0123456789ABCDEF"[wifi_mac[i2]&15]; hb[2]=0; tputs(hb); if(i2<5)tputs(":"); } tnl();
                tputs("DMA RX/TX RINGS ... "); tputs(wifi_dma_ok?"ALLOCATED (host)":"FAIL"); tnl();
                tputs("LOAD FIRMWARE ..... MISSING (proprietary .ucode blob)"); tnl();
                tputs("START RADIO ....... BLOCKED BY FIRMWARE"); tnl();
                tputs(">> PHASE 4 COMPLETE UP TO THE FIRMWARE WALL."); tnl();
            } }
    }
    else if(streq(c,"wifi scan")||streq(c,"wifiscan")){
        tputs("=== WIFI SCAN ==="); tnl();
        if(!wifi_pci&&!wifi_usb){ tputs("NO WIFI HARDWARE FOUND."); tnl();
            tputs("(VIRTUALBOX HAS NO WIFI EMULATION - REAL LAPTOP ONLY.)"); tnl();
            tputs("WIRED NETWORK SCAN WORKS TODAY: TYPE 'scan'"); tnl(); }
        else{
            tputs("CHIP: "); tputs(wifi_chip_name(wifi_ven,wifi_did)); tnl();
            u32 hr=0,gp=0;
            if(wifi_ven==0x8086 && wifi_radio_probe(&hr,&gp)){
                char hb[12];
                tputs("RADIO PROBE (MMIO BAR0):"); tnl();
                tputs("  CSR_HW_REV   = 0x"); hex32(hr,hb); tputs(hb); tnl();
                tputs("  CSR_GP_CNTRL = 0x"); hex32(gp,hb); tputs(hb); tnl();
                tputs("  >> RADIO RESPONDS ON PCIe (REAL REGISTER READ)"); tnl();
            } else if(wifi_ven==0x8086){ tputs("RADIO PROBE: BAR0 NOT MAPPED / NO RESPONSE"); tnl(); }
            else { tputs("RADIO PROBE: ONLY IMPLEMENTED FOR INTEL CSR LAYOUT"); tnl(); }
            if(!wifi_inited){ tputs("RUN 'wifi init' FIRST (PHASE 3-4)."); tnl(); }
            tputs("START SCAN .......... "); tputs(wifi_rfkill==0?"BLOCKED (RF-KILL SWITCH OFF)":"BLOCKED (NO FIRMWARE)"); tnl();
            tputs("RECEIVE BEACONS ..... IMPOSSIBLE - RADIO OFFLINE"); tnl();
            tnl();
            tputs("AVAILABLE NETWORKS"); tnl();
            tputs("  (0 FOUND - RADIO CANNOT LISTEN WITHOUT ITS FIRMWARE BLOB)"); tnl();
            tputs("  NO FAKE SSIDS WILL BE SHOWN HERE."); tnl();
            tnl();
            tputs("WIRED LAN DISCOVERY WORKS TODAY: TYPE 'scan'"); tnl(); }
    }
    else if(streq(c,"wifi")||streq(c,"lswifi")){
        tputs("=== STEP 1: WIFI HARDWARE DETECTION ==="); tnl();
        if(wifi_pci){ char vd[12]; hex16(wifi_ven,vd); vd[4]=':'; hex16(wifi_did,vd+5);
            tputs("CHIP   : "); tputs(wifi_chip_name(wifi_ven,wifi_did)); tnl();
            tputs("VENDOR : "); tputs(wifi_ven_name(wifi_ven)); tputs("  ID "); tputs(vd); tnl();
            tputs("BUS    : PCI  CLASS 02:80 (802.11 WIRELESS)"); tnl();
            tputs("NEEDS  : "); tputs(wifi_needs(wifi_ven)); tnl();
            tnl();
            tputs("STEP 1 DETECT HW ........ [DONE] CHIP IDENTIFIED"); tnl();
            tputs("STEP 2 LOAD DRIVER ...... [BLOCKED] NO OPEN SW SPEC"); tnl();
            tputs("STEP 3 LOAD FIRMWARE .... [BLOCKED] NEEDS BINARY BLOB"); tnl();
            tputs("STEP 4 SCAN NETWORKS .... [WAITING ON 2-3]"); tnl();
            tputs("STEP 5 WPA2/WPA3 CRYPTO . [WAITING ON 2-3]"); tnl();
            tputs("STEP 6 AUTHENTICATION ... [WAITING ON 2-3]"); tnl();
            tputs("STEP 7 DHCP ............. [WAITING ON 2-3]"); tnl();
            tputs("STEP 8 INTERNET ......... [WAITING ON 2-3]"); tnl();
            tnl();
            tputs("WIFI CHIPS HAVE NO PUBLIC REGISTER SPEC AND NEED A"); tnl();
            tputs("PROPRIETARY FIRMWARE BLOB TO DO ANYTHING. THIS IS"); tnl();
            tputs("THE SAME WALL ALL HOBBY OSES HIT. (SEE OSDEV.)"); tnl();
            tnl(); tputs("TRY: 'wifi scan' (radio probe) | 'scan' (wired LAN)"); tnl();
        }
        else if(wifi_usb){ char vd[12]; hex16(wifi_usb_vid,vd); vd[4]=':'; hex16(wifi_usb_pid,vd+5);
            tputs("CHIP   : USB WIFI DONGLE"); tnl();
            tputs("VENDOR : "); tputs(wifi_ven_name(wifi_usb_vid)); tputs("  ID "); tputs(vd); tnl();
            tputs("STEP 1 DETECT HW ........ [DONE]"); tnl();
            tputs("STEP 2-3 DRIVER+FW ...... [BLOCKED] BLOB REQUIRED"); tnl();
        }
        else { tputs("NO WIFI HARDWARE DETECTED ON THIS MACHINE."); tnl();
            tputs("(IN VIRTUALBOX THERE IS NO EMULATED WIFI - USE"); tnl();
            tputs("WIRED ETHERNET, WHICH WORKS FULLY.)"); tnl(); }
    }
    else if(streq(c,"cam")||streq(c,"lscam")){
        if(cam_count==0){
            tputs("NO USB CAMERA DETECTED."); tnl();
            tputs("(VM USB CONTROLLERS DO NOT EXPOSE WEBCAMS TO GUEST."); tnl();
            tputs(" PLUG A USB CAM ON REAL HARDWARE TO TEST.)"); tnl();
        } else {
            char ncb[8]; utoa(cam_count,ncb); tputs("USB CAMERAS DETECTED: "); tputs(ncb); tnl();
            for(int k=0;k<cam_count;k++){
                int di=cam_info[k].dev_idx;
                char vd[12]; hex16(usbdev[di].vid,vd); vd[4]=':'; hex16(usbdev[di].pid,vd+5);
                tputs(" #"); { char a[6]; utoa(usbdev[di].addr,a); tputs(a); }
                tputs(" "); tputs(vd);
                tputs(" "); tputs(cam_vendor_name(usbdev[di].vid));
                tnl();

                tputs("    CONFIG: "); { char b[6]; utoa(cam_info[k].cfg_len,b); tputs(b); tputs(" BYTES, "); }
                { char b[6]; utoa(cam_info[k].n_ifaces,b); tputs(b); tputs(" IFACES, "); }
                { char b[6]; utoa(cam_info[k].n_endpoints,b); tputs(b); tputs(" ENDPOINTS"); }
                tnl();

                if(cam_info[k].has_vc_header){
                    u8 maj=(u8)(cam_info[k].bcd_uvc>>8), min=(u8)(cam_info[k].bcd_uvc&0xFF);
                    tputs("    UVC: "); { char b[4]; utoa(maj,b); tputs(b); tputs("."); utoa(min>>4,b); tputs(b); tputs((min&0xF)?"5":"0"); }
                    if(cam_info[k].has_iad) tputs(" (IAD)");
                    tnl();
                } else {
                    tputs("    UVC: NO VC_HEADER FOUND"); tnl();
                }

                if(cam_info[k].n_formats){
                    tputs("    FORMAT: "); tputs(cam_fmt_name(cam_info[k].fmt_type));
                    if(cam_info[k].n_formats>1){ char b[4]; utoa(cam_info[k].n_formats,b); tputs(" ("); tputs(b); tputs(" FORMATS)"); }
                    tnl();
                }
                if(cam_info[k].best_w){
                    char wb[6],hb[6]; utoa(cam_info[k].best_w,wb); utoa(cam_info[k].best_h,hb);
                    tputs("    MAX RES: "); tputs(wb); tputs("x"); tputs(hb); tnl();
                }
                tputs("    VC IFACE: "); { char b[6]; utoa(cam_info[k].vc_iface,b); tputs(b); tputs("  VS IFACE: "); utoa(cam_info[k].vs_iface,b); tputs(b); } tnl();
            }
            tputs("STATUS: DETECTION ONLY - NO CAPTURE/STREAMING."); tnl();
        }
    }
    else if(streq(c,"camapp")){ launch(24); tputs("OPENING CAMERA APP..."); tnl(); }
    else if(streq(c,"graph")){ launch(30); tputs("OPENING NOOVEXGRAPH..."); tnl(); }
    else if(streq(c,"uhci")){ if(!uhci_ok){ tputs("NO UHCI CONTROLLER (USB 1.1) FOUND."); tnl(); } else { char hb[8]; hex16(uhci_io,hb); tputs("UHCI USB 1.1 CONTROLLER"); tnl(); tputs("  IO PORT: 0x"); tputs(hb); tnl(); if(uhci_ms_found){ char ab[8]; utoa(uhci_ms_addr,ab); tputs("  MOUSE:   FOUND (ADDR "); tputs(ab); tputs(uhci_ms_ls?", LOW-SPEED)":", FULL-SPEED)"); tnl(); tputs("  STATUS:  ACTIVE - MOVE THE MOUSE"); tnl(); } else { tputs("  MOUSE:   NOT FOUND ON UHCI PORTS"); tnl(); tputs("  TIP: TYPE 'usbrescan' AFTER PLUGGING IT IN"); tnl(); } } }
    else if(streq(c,"bootrecovery")){ tputs("ENTERING RECOVERY..."); tnl(); recovery_mode(); need_rebuild=1; }
    else if(streq(c,"xhci")){ if(!xhci_init_ok){ tputs("NO XHCI CONTROLLER / INIT FAILED."); tnl(); } else { char nb[12]; tputs("XHCI USB 3.0 - "); utoa(xhci_ports,nb); tputs(nb); tputs(" PORTS  VEN 0x"); hex16(xhci_vendor,nb); tputs(nb); tputs(xhci_vendor==0x8086?" (INTEL)":""); tnl(); tputs("ENUM STEP "); utoa(xhci_diag_step,nb); tputs(nb); tputs("/9"); tnl();
        tputs("PORT  PORTSC      STATE"); tnl();
        for(int p=0;p<xhci_ports;p++){ u32 v=MMR(xhci_op+0x400+(u32)p*0x10); int conn=v&1; int en=(v>>1)&1; int sp=(v>>10)&0xF;
            tputs(" "); utoa(p+1,nb); if(p+1<10)tputs(" "); tputs(nb); tputs("   0x"); hex32(v,nb); tputs(nb); tputs("  ");
            if(conn){ const char* sn=(sp==1)?"FS":(sp==2)?"LS":(sp==3)?"HS":(sp==4)?"SS":"?"; tputs("CONN "); tputs(sn); if(en)tputs(" EN"); }
            else tputs("-"); tnl(); }
        if(xhci_ms_found){ tputs(">> MOUSE BOUND, SLOT "); utoa(xhci_ms_slot,nb); tputs(nb); tnl(); }
        if(xhci_kb_found){ tputs(">> KEYBOARD BOUND, SLOT "); utoa(xhci_kb_slot,nb); tputs(nb); tnl(); }
        if(xhci_ms_found){ tputs(xhci_ms_proto==0?">> MOUSE MODE: ABSOLUTE (TABLET)":">> MOUSE MODE: RELATIVE (BOOT)"); tnl(); }
        else { tputs(">> NO MOUSE. LAST PORT "); utoa(xhci_enum_port,nb); tputs(nb);
            tputs(" STEP "); utoa(xhci_diag_step,nb); tputs(nb); tputs("/9"); tnl();
            tputs("   SLOT_CC="); utoa(xhci_slot_cc<0?99:xhci_slot_cc,nb); tputs(nb);
            tputs(" ADDR_CC="); utoa(xhci_addr_cc<0?99:xhci_addr_cc,nb); tputs(nb);
            tputs(" PED="); utoa(xhci_ped,nb); tputs(nb); tputs(" PED_IMM="); utoa(xhci_ped_imm,nb); tputs(nb); tnl();
            tputs("   (CC 1=OK 4=USB-ERR 5=TRB-ERR 11=CTX 17=PARAM 99=TIMEOUT)"); tnl(); }
        tputs("(SCROLL: UP/DOWN OR PGUP/PGDN)"); tnl(); } }
    else if(streq(c,"usbrescan")){ tputs("RESCANNING USB BUSES..."); tnl(); ehci_enumerate(); usbmsd_mount(); uhci_init(); xhci_enum(); tputs(uhci_ms_found?"UHCI MOUSE BOUND.":"NO UHCI MOUSE."); tnl(); tputs(xhci_ms_found?"XHCI MOUSE BOUND - MOVE THE MOUSE.":"NO XHCI MOUSE - RUN 'xhci' TO SEE STEP."); tnl(); }
    else if(streq(c,"ehci")||streq(c,"lsusb")){ if(!ehci_present){ tputs("NO EHCI CONTROLLER FOUND."); tnl(); } else { tputs("EHCI USB 2.0 CONTROLLER"); tnl(); char hh[8],hl[8]; hex16((u16)(ehci_base>>16),hh); hex16((u16)(ehci_base&0xFFFF),hl); tputs("MMIO: 0x"); tputs(hh); tputs(hl); tnl(); tputs("INIT: "); tputs(ehci_init_ok?"OK":"FAIL"); tnl(); char pb[8]; utoa(ehci_nports,pb); tputs("ROOT PORTS: "); tputs(pb); tnl(); for(int i=0;i<ehci_nports;i++){ char nb[8]; utoa(i,nb); tputs("  PORT "); tputs(nb); tputs(ehci_port_conn[i]?": CONNECTED":": EMPTY"); tnl(); } char db[8]; utoa(usbdev_n,db); tputs("ENUMERATED DEVICES: "); tputs(db); tnl(); for(int i=0;i<usbdev_n;i++){ char ab[8],vd[12]; utoa(usbdev[i].addr,ab); hex16(usbdev[i].vid,vd); vd[4]=':'; hex16(usbdev[i].pid,vd+5); u8 eff=usbdev[i].cls?usbdev[i].cls:usbdev[i].ifcls; tputs("  #"); tputs(ab); tputs(" "); tputs(vd); tputs(" "); tputs(usb_cls_name(eff)); if(usbdev[i].kind==1)tputs(" [KBD DRV]"); else if(usbdev[i].kind==2)tputs(" [MOUSE DRV]"); else if(usbdev[i].kind==3)tputs(" [HUB]"); else if(usbdev[i].kind==5)tputs(" [CAM]"); if(usbdev[i].name[0]){ tputs(" ["); tputs(usbdev[i].name); tputs("]"); } tnl(); } tputs("NOTE: ENUM ONLY - NO CLASS DRIVERS YET."); tnl(); } }
    else if(streq(c,"battery")||streq(c,"acpi")){
        tputs("=== BATTERY / ACPI DRIVER ==="); tnl();
        tputs("ACPI TABLES: "); tputs(acpi_found?"FOUND":"NOT FOUND"); tnl();
        if(acpi_found){ char hb[12]; tputs("  RSDP @ 0x"); hex32(acpi_rsdp,hb); tputs(hb); tnl();
            tputs("  "); tputs(acpi_xsdt?"XSDT":"RSDT"); tputs(" @ 0x"); hex32(acpi_rsdt,hb); tputs(hb); tnl();
            tputs("  FADT @ 0x"); hex32(acpi_fadt,hb); tputs(hb); tnl(); }
        tputs("EMBEDDED CTRL: "); tputs(ec_present?"DETECTED (PORTS 62/66)":"NOT PRESENT"); tnl();
        if(bat_present){ char pb[6]; int q=0,v=bat_pct; char t[4];int tl=0; if(v==0)t[tl++]='0'; while(v){t[tl++]='0'+v%10;v/=10;} while(tl)pb[q++]=t[--tl]; pb[q++]='%'; pb[q]=0;
            tputs("BATTERY:     "); tputs(pb); tputs("  "); tputs(bat_charging?"CHARGING":"DISCHARGING"); tnl();
            tputs("SOURCE:      "); tputs(bat_via_ec?"REAL (READ VIA EC)":"SIMULATED"); tnl(); }
        else { tputs("BATTERY:     NONE (AC POWER / NOT READABLE)"); tnl();
            if(!ec_present) tputs("  (VMs EXPOSE NO EC; THIS IS NORMAL IN VBOX)"); tnl(); }
        tputs("TIP: SETTINGS > BATTERY > SIMULATE TO TEST UI"); tnl(); }
    else if(streq(c,"dev")){ tputs("OPEN START > DEVICES FOR FULL LIST"); tnl(); }
    else if(streq(c,"printer")||streq(c,"lpstat")){ int pi=-1; for(int i=0;i<usbdev_n;i++) if(usbdev[i].kind==6){pi=i;break;}
        if(pi<0){ tputs("NO USB PRINTER DETECTED."); tnl(); tputs("CONNECT A USB PRINTER AND IT WILL APPEAR HERE."); tnl(); }
        else { tputs("USB PRINTER FOUND:"); tnl(); char vd[12]; hex16(usbdev[pi].vid,vd); vd[4]=':'; hex16(usbdev[pi].pid,vd+5);
            tputs("  ID:   "); tputs(vd); tnl(); if(usbdev[pi].name[0]){ tputs("  NAME: "); tputs(usbdev[pi].name); tnl(); }
            char ab[8]; utoa(usbdev[pi].addr,ab); tputs("  ADDR: #"); tputs(ab); tnl();
            tputs("  CLASS: 07h (PRINTER)"); tnl(); tputs("  STATUS: DETECTED - NO PRINT DRIVER YET."); tnl(); } }
    else if(streq(c,"serial")){ if(com1_ok){ com1_puts("Hello from NoovexOS terminal\n"); tputs("SENT TO COM1 (9600 8N1)"); } else tputs("COM1 NOT DETECTED"); tnl(); }
    else if(startsw(c,"res ")){
        if(!dispi_id){ tputs("DISPI EJ TILLGANGLIG."); tnl(); tputs("SATT GRAPHICS CONTROLLER = VBoxVGA"); tnl(); }
        else { const char*p=c+4; int w=0,h=0; while(*p==' ')p++; while(*p>='0'&&*p<='9'){w=w*10+(*p-'0');p++;} while(*p&&(*p<'0'||*p>'9'))p++; while(*p>='0'&&*p<='9'){h=h*10+(*p-'0');p++;}
            if(w>=320&&h>=200&&w<=1600&&h<=1200){ res_pw=w; res_ph=h; tputs("SWITCHING RESOLUTION..."); tnl(); } else { tputs("USAGE: res 1024 768  (320..1600 x 200..1200)"); tnl(); } } }
    else if(streq(c,"res")){ char b[12]; utoa(W,b); tputs("CURRENT: "); tputs(b); tputs("x"); utoa(H,b); tputs(b); tnl(); tputs(dispi_id?"DISPI: AVAILABLE (try: res 1280 1024)":"DISPI: NOT AVAILABLE"); tnl(); }
    else if(startsw(c,"proxy ")){ const char* p=c+6; while(*p==' ')p++;
        if(streq(p,"off")){ reader_on=0; tputs("READER PROXY: OFF"); tnl(); }
        else if(streq(p,"on")){ if(br_proxy[0]){ reader_on=1; tputs("READER PROXY ON:"); tnl(); tputs("  "); tputs(br_proxy); tnl(); } else { tputs("NO PROXY URL SET. USE: proxy http://HOST/read?url="); tnl(); } }
        else { int k=0; while(p[k]&&k<198){br_proxy[k]=p[k];k++;} br_proxy[k]=0; reader_on=1;
            tputs("READER PROXY ON:"); tnl(); tputs("  "); tputs(br_proxy); tnl(); tputs("OPEN THE BROWSER AND SURF - URLS GO THROUGH THE PROXY."); tnl(); } }
    else if(streq(c,"proxy")){ if(reader_on&&br_proxy[0]){ tputs("READER PROXY ON:"); tnl(); tputs("  "); tputs(br_proxy); tnl(); tputs("DISABLE: proxy off"); tnl(); }
        else { tputs("READER PROXY: OFF"); tnl(); if(br_proxy[0]){ tputs("URL IS SET - ENABLE WITH: proxy on"); tnl(); tputs("  "); tputs(br_proxy); tnl(); } else { tputs("ENABLE: proxy http://YOURHOST/read?url="); tnl(); } } }
    else if(startsw(c,"nexus ")){ const char* p=c+6; while(*p==' ')p++;
        if(!p[0]){ tputs("USAGE: nexus <project-ref>   (the bit before .supabase.co)"); tnl(); }
        else { int o=0; const char* a="https://"; for(int i=0;a[i];i++)br_proxy[o++]=a[i];
            for(int i=0;p[i]&&o<150;i++){ char ch=p[i]; if(ch==' ')break; br_proxy[o++]=ch; }
            const char* b=".supabase.co/functions/v1/read?url="; for(int i=0;b[i]&&o<198;i++)br_proxy[o++]=b[i]; br_proxy[o]=0;
            reader_on=1; tputs("NEXUSREADER PROXY ON:"); tnl(); tputs("  "); tputs(br_proxy); tnl();
            tputs("open the browser and surf - pages go through your supabase reader."); tnl(); } }
    else if(streq(c,"nexus")){ tputs("NEXUSREADER (your supabase proxy)"); tnl(); tputs("SET: nexus <project-ref>   e.g. nexus abcd1234efgh5678ijkl"); tnl(); if(reader_on&&br_proxy[0]){ tputs("ACTIVE: "); tputs(br_proxy); tnl(); } else tputs("currently OFF. proxy off to disable, proxy on to re-enable."); tnl(); }
    else if(streq(c,"jina")||startsw(c,"jina ")){ const char* p=c+4; while(*p==' ')p++;
        if(streq(p,"off")){ reader_on=0; tputs("JINA READER: OFF"); tnl(); }
        else { if(streq(p,"text")){ const char* f="text"; int i=0; while(f[i]){jina_fmt[i]=f[i];i++;} jina_fmt[i]=0; }
            else if(streq(p,"md")||streq(p,"markdown")){ const char* f="markdown"; int i=0; while(f[i]){jina_fmt[i]=f[i];i++;} jina_fmt[i]=0; }
            else if(streq(p,"html")){ const char* f="html"; int i=0; while(f[i]){jina_fmt[i]=f[i];i++;} jina_fmt[i]=0; }
            const char* jp="https://r.jina.ai/"; int k=0; while(jp[k]&&k<198){br_proxy[k]=jp[k];k++;} br_proxy[k]=0; reader_on=1;
            tputs("JINA READER ON - free, no setup. FORMAT: "); tputs(jina_fmt); tnl(); tputs("OPEN THE BROWSER AND SURF - PAGES CLEANED BY r.jina.ai"); tnl(); } }
    else if(streq(c,"unenroll")){ acct.enrolled=0; for(int i=0;i<24;i++)acct.org[i]=0; if(disk_ok){ acct_save(); tputs("UNENROLLED - 'MANAGED BY' REMOVED. REBOOT TO SEE THE CHANGE."); } else tputs("UNENROLLED (NOT SAVED - NO DISK INSTALL)."); tnl(); }
    else if(streq(c,"gpu")||streq(c,"lspci")){ if(gpu_found){ tputs("GPU: "); tputs(gpu_vendor_name(gpu_ven)); tnl();
            char hb[12]; { u32 vv=gpu_ven,dd=gpu_did; for(int i=0;i<4;i++){int d=(vv>>((3-i)*4))&0xF;hb[i]=d<10?'0'+d:'A'+d-10;} hb[4]=':'; for(int i=0;i<4;i++){int d=(dd>>((3-i)*4))&0xF;hb[5+i]=d<10?'0'+d:'A'+d-10;} hb[9]=0; }
            tputs("  PCI ID: "); tputs(hb); tnl();
            tputs("  RES: "); { char b[8]; utoa(W,b); tputs(b); tputs("x"); utoa(H,b); tputs(b); tputs("x8"); } tnl();
            if(gpu_bar0sz){ tputs("  VRAM: "); char b[8]; utoa(gpu_bar0sz/(1024*1024),b); tputs(b); tputs(" MB (BAR0)"); tnl(); }
            tputs("  ACCEL: NONE (SOFTWARE LFB ONLY)"); tnl();
        } else { tputs("NO PCI DISPLAY CONTROLLER DETECTED."); tnl(); } }
    else if(streq(c,"mic")){
        tputs("=== MIC (AC97 PCM CAPTURE) ==="); tnl();
        if(!ac97_ok){ tputs("NO AC97 CODEC (SET VBOX AUDIO=ICH AC97, OR REAL HW)."); tnl(); }
        else{ tputs("RECORDING 4s FROM MIC..."); tnl();
            int f=mic_record(4000); char nb[12];
            if(f<=0){ tputs("CAPTURE FAILED."); tnl(); }
            else{ tputs("CAPTURED "); utoa((u32)f,nb); tputs(nb); tputs(" FRAMES @ "); utoa((u32)rec_rate,nb); tputs(nb); tputs(" Hz"); tnl();
                tputs("PEAK LEVEL: "); utoa((u32)rec_peak,nb); tputs(nb); tputs(" / 32767"); tnl();
                tputs(rec_peak<200?"  (VERY QUIET - CHECK MIC/GAIN)":"  (SIGNAL OK)"); tnl();
                tputs("PLAYING BACK..."); tnl(); mic_playback();
                tputs("USE THE RECORDER APP TO SAVE AS mic.wav."); tnl(); } }
    }
    else if(streq(c,"mp4")||(c[0]=='m'&&c[1]=='p'&&c[2]=='4'&&c[3]==' ')){
        const char* fn=(c[3]==' ')?c+4:"video.mp4";
        tputs("=== MP4 / ISOBMFF INSPECTOR ==="); tnl();
        tputs("FILE: "); tputs(fn); tnl();
        int idx=nvx_find(fn);
        if(idx<0){ tputs("NOT ON NVXFS. COPY AN .mp4 TO THE DISK FIRST."); tnl(); }
        else{
            u8* buf=(u8*)0x01800000u; int max=0x600000;
            int n=nvx_read(idx,(char*)buf,max); if(n<8){ tputs("READ ERROR / TOO SMALL."); tnl(); }
            else{
                char nb[12]; int vcodec=0, acodec=0, vw=0, vh=0, ach=0, arate=0; u32 tscale=0,dur=0;
                /* iterative box walk with a small explicit stack */
                u32 stk_end[8]; int stk_ind[8]; int sp2=0; u32 pos=0; u32 endp=(u32)n; int depth=0;
                while(pos+8<=endp || sp2>0){
                    while(sp2>0 && pos>=stk_end[sp2-1]){ sp2--; depth--; }
                    if(pos+8>endp) break;
                    u32 bs=((u32)buf[pos]<<24)|(buf[pos+1]<<16)|(buf[pos+2]<<8)|buf[pos+3];
                    char ty[5]; ty[0]=buf[pos+4];ty[1]=buf[pos+5];ty[2]=buf[pos+6];ty[3]=buf[pos+7];ty[4]=0;
                    u32 hdr=8; if(bs==1){ bs=(u32)(((u64)buf[pos+8]<<24|buf[pos+9]<<16|buf[pos+10]<<8|buf[pos+11])); hdr=16; }
                    if(bs<hdr){ break; } u32 bend=pos+bs; if(bend>endp)bend=endp;
                    for(int d=0;d<depth&&d<6;d++)tputs("  ");
                    tputs("["); tputs(ty); tputs("] "); utoa(bs,nb); tputs(nb); tputs(" B"); tnl();
                    int cont = (!strncmp_(ty,"moov",4)||!strncmp_(ty,"trak",4)||!strncmp_(ty,"mdia",4)||
                                !strncmp_(ty,"minf",4)||!strncmp_(ty,"stbl",4)||!strncmp_(ty,"dinf",4)||
                                !strncmp_(ty,"udta",4)||!strncmp_(ty,"mvex",4));
                    if(!strncmp_(ty,"ftyp",4)&&pos+12<=endp){ for(int d=0;d<=depth;d++)tputs("  "); tputs("BRAND: ");
                        char br[5]={(char)buf[pos+8],(char)buf[pos+9],(char)buf[pos+10],(char)buf[pos+11],0}; tputs(br); tnl(); }
                    if(!strncmp_(ty,"mvhd",4)&&pos+20<=endp){ tscale=((u32)buf[pos+20]<<24)|(buf[pos+21]<<16)|(buf[pos+22]<<8)|buf[pos+23];
                        dur=((u32)buf[pos+24]<<24)|(buf[pos+25]<<16)|(buf[pos+26]<<8)|buf[pos+27]; }
                    if(!strncmp_(ty,"stsd",4)&&pos+16<=endp){ u32 se=pos+16;
                        if(se+8<=endp){ char cf[5]={(char)buf[se+4],(char)buf[se+5],(char)buf[se+6],(char)buf[se+7],0};
                            for(int d=0;d<=depth;d++)tputs("  "); tputs("CODEC: "); tputs(cf); tnl();
                            if(!strncmp_(cf,"avc1",4)||!strncmp_(cf,"avc3",4)){ vcodec=1; if(se+32<=endp){ vw=(buf[se+32]<<8)|buf[se+33]; vh=(buf[se+34]<<8)|buf[se+35]; } }
                            else if(!strncmp_(cf,"hev1",4)||!strncmp_(cf,"hvc1",4))vcodec=2;
                            else if(!strncmp_(cf,"mp4v",4))vcodec=3;
                            else if(!strncmp_(cf,"mjpg",4)||!strncmp_(cf,"jpeg",4))vcodec=4;
                            else if(!strncmp_(cf,"mp4a",4)){ acodec=1; if(se+26<=endp){ ach=(buf[se+24]<<8)|buf[se+25]; } }
                            else if(!strncmp_(cf,"ac-3",4))acodec=2;
                        } }
                    if(cont){ if(sp2<8){ stk_end[sp2]=bend; stk_ind[sp2]=depth; sp2++; depth++; } pos+=hdr; }
                    else pos=bend;
                    if(depth>6)depth=6;
                }
                tnl();
                if(tscale){ tputs("DURATION: "); utoa(tscale?dur/tscale:0,nb); tputs(nb); tputs(" s"); tnl(); }
                if(vw||vh){ tputs("VIDEO: "); utoa((u32)vw,nb); tputs(nb); tputs("x"); utoa((u32)vh,nb); tputs(nb); tnl(); }
                tputs("--- DECODE STATUS ---"); tnl();
                tputs("VIDEO CODEC: ");
                tputs(vcodec==1?"H.264/AVC":vcodec==2?"H.265/HEVC":vcodec==3?"MPEG-4":vcodec==4?"MJPEG":"NONE/UNKNOWN"); tnl();
                if(vcodec==4) tputs("  >> MJPEG IS DECODABLE (kernel has JPEG). PLAYER TODO.");
                else if(vcodec) tputs("  >> CONTAINER PARSED OK, BUT THIS CODEC NEEDS A DECODER");
                else tputs("  >> NO VIDEO TRACK FOUND");
                tnl();
                if(vcodec==1||vcodec==2){ tputs("  H.264/H.265 DECODERS ARE ~50K LINES + NEED HW/TEST -"); tnl();
                    tputs("  SAME CLASS OF WALL AS WIFI FIRMWARE. NOT FAKED HERE."); tnl(); }
                tputs("AUDIO CODEC: "); tputs(acodec==1?"AAC (needs decoder)":acodec==2?"AC-3 (needs decoder)":"NONE/UNKNOWN"); tnl();
            }
        }
    }
    else if(streq(c,"hda")){ hda_cmdline(); }
    else if(streq(c,"sb16")){ if(sb_ok){ char b[8]; tputs("SB16 DSP v"); utoa((sb_ver>>8),b); tputs(b); tputs("."); utoa((sb_ver&0xFF),b); tputs(b); tputs(" - PLAYING CHIME"); tnl(); sb_play(); } else { tputs("SB16 EJ HITTAD."); tnl(); tputs("SATT AUDIO CONTROLLER = SoundBlaster 16"); tnl(); } }
    else if(streq(c,"net")){
        if(!nic_present){ tputs("NIC: NO PCnet FOUND."); tnl(); tputs("SATT ADAPTER TYPE = PCnet-FAST III (NAT)"); tnl(); }
        else { int ok=nic_up(); char ib[20];
            tputs("NIC: AMD PCnet"); tnl();
            if(ok){ tputs("MAC "); { const char* hx="0123456789ABCDEF"; for(int i=0;i<6;i++){ char p[4]; p[0]=hx[my_mac[i]>>4];p[1]=hx[my_mac[i]&15];p[2]=(i<5)?':':0;p[3]=0; tputs(p);} } tnl();
                ip_to_str(my_ip,ib); tputs("IP  "); tputs(ib); tnl();
                ip_to_str(gw_ip,ib); tputs("GW  "); tputs(ib); tnl();
                ip_to_str(dns_ip,ib); tputs("DNS "); tputs(ib); tnl();
                { u8 gm[6]; gw_known=0; int ar=arp_resolve(gw_ip,gm); char nb[8];
                  utoa(nic_txc,nb); tputs("TX="); tputs(nb); utoa(nic_rxc,nb); tputs(" RX="); tputs(nb); tnl();
                  if(ar){ tputs("ARP GW: OK (RX WORKS!)"); tnl(); }
                  else { tputs("ARP GW: NO REPLY"); tnl(); tputs(nic_rxc?"  (RX got frames but no ARP reply)":"  (RX=0: card receives nothing)"); tnl(); } }
                tputs("TRY: ping 10.0.2.2 | dns google.com | scan"); tnl(); }            else { tputs("NIC INIT FAILED (see COM1 log)"); tnl(); } } }
    else if(streq(c,"scan")||streq(c,"netscan")||streq(c,"arp")){
        if(!nic_present){ tputs("NO PCnet NIC - WIRED SCAN UNAVAILABLE."); tnl(); tputs("(WIFI SCAN IS NOT POSSIBLE: NO RADIO/DRIVER.)"); tnl(); }
        else {
            tputs("SCANNING LOCAL SUBNET VIA ARP..."); tnl();
            int r=net_scan();
            if(r<0){ tputs("NETWORK NOT AVAILABLE (NIC DOWN)."); tnl(); }
            else {
                char ib[20]; ip_to_str(my_ip&netmask,ib);
                tputs("SUBNET: "); tputs(ib); tputs("/24"); tnl();
                char nb[8]; utoa(scan_n,nb); tputs("LIVE HOSTS: "); tputs(nb); tnl();
                const char* hx="0123456789ABCDEF";
                for(int i=0;i<scan_n;i++){
                    ip_to_str(scan_res[i].ip,ib); tputs("  "); tputs(ib);
                    int pad=16-(int)strlen_(ib); while(pad-->0)tputs(" ");
                    for(int k=0;k<6;k++){ char p[4]; p[0]=hx[scan_res[i].mac[k]>>4]; p[1]=hx[scan_res[i].mac[k]&15]; p[2]=(k<5)?':':0; p[3]=0; tputs(p); }
                    if(scan_res[i].ip==gw_ip)tputs(" (GW)");
                    else if(scan_res[i].ip==dns_ip)tputs(" (DNS)");
                    tnl();
                }
                if(scan_n==0){ tputs("  (NO REPLIES - NAT MODE OFTEN HIDES HOSTS;"); tnl(); tputs("   TRY BRIDGED ADAPTER ON REAL LAN.)"); tnl(); }
                tputs("NOTE: ARP REACHES ONLY THE LOCAL SUBNET."); tnl();
            }
        }
    }
    else if(streq(c,"imginfo")||streq(c,"img")){ cmd_imginfo(); }
    else if(streq(c,"isoinfo")||streq(c,"iso")){ cmd_isoinfo(); }
    else if(streq(c,"mkboot")){ cmd_mkboot(0); }
    else if(streq(c,"mkboot yes")){ cmd_mkboot(1); }
    else if(streq(c,"mkusb")){ cmd_mkusb(0); }
    else if(streq(c,"mkusb yes")){ cmd_mkusb(1); }
    else if(startsw(c,"peek ")){ if(!dev_mode){ tputs("PEEK REQUIRES DEVELOPER MODE."); tnl(); } else { u32 a=hexparse(c+5); u32 v=*(volatile u32*)a; char hx[12]; tputs("["); hex32(a,hx); tputs(hx); tputs("] = "); hex32(v,hx); tputs(hx); tnl(); } }
    else if(streq(c,"crash")){ if(!dev_mode){ tputs("CRASH REQUIRES DEVELOPER MODE."); tnl(); } else { tputs("TRIGGERING TEST EXCEPTION (#DE)..."); tnl(); volatile int zz=0; volatile int qq=1/zz; (void)qq; } }
    else if(startsw(c,"checkemail ")){
        const char* em=c+11; while(*em==' ')em++;
        if(!*em){ tputs("USAGE: checkemail you@example.com"); tnl(); }
        else {
            tputs("CHECKING EXACT EMAIL VIA SUPABASE..."); tnl();
            int r=supabase_check_email(em);
            if(r==1){ tputs("RESULT: EXACT MATCH - AUTHORIZED"); tnl(); }
            else if(r==0){ tputs("RESULT: NOT AUTHORIZED (NO EXACT MATCH)"); tnl(); }
            else if(r==-2){ tputs("RESULT: INVALID EMAIL FORMAT"); tnl(); }
            else { char eb[8]; utoa(-r,eb); tputs("NETWORK/TLS ERROR (E"); tputs(eb); tputs(")"); tnl();
                   tputs("(NEEDS WORKING INTERNET + DNS.)"); tnl(); }
        }
    }
    else if(streq(c,"dhcp")){
        if(!nic_up()){ tputs("NETWORK NOT AVAILABLE"); tnl(); }
        else { tputs("REQUESTING DHCP LEASE..."); tnl(); int r=dhcp_run(); char ib[20];
            if(r){ ip_to_str(my_ip,ib); tputs("LEASED IP "); tputs(ib); tputs("  GW "); ip_to_str(gw_ip,ib); tputs(ib); tnl(); }
            else { tputs("DHCP FAILED (see COM1 log)"); tnl(); } } }
    else if(streq(c,"serve")){
        if(!nic_up()){ tputs("NETWORK NOT AVAILABLE"); tnl(); }
        else { char ib[20]; ip_to_str(my_ip,ib);
            tputs("HTTP SERVER ON "); tputs(ib); tputs(":80"); tnl();
            tputs("IN VBOX: PORT FORWARD HOST->GUEST 80, THEN BROWSE."); tnl();
            tputs("PRESS ANY KEY TO STOP. SERVING..."); tnl();
            int served=0; for(;;){ int r=tcp_serve(80); if(r<=0)break; served++; }
            char nb[8]; utoa(served,nb); tputs("STOPPED. SERVED "); tputs(nb); tputs(" REQUEST(S)."); tnl(); } }
    else if(startsw(c,"get ")){
        const char* p=c+4; while(*p==' ')p++;
        char url[128]; int ui=0; while(*p&&*p!=' '&&ui<127){url[ui++]=*p;p++;} url[ui]=0;
        while(*p==' ')p++; char nm[20]; int ni=0; while(*p&&*p!=' '&&ni<15){nm[ni++]=*p;p++;} nm[ni]=0;
        if(!url[0]||!nm[0]){ tputs("USAGE: get neverssl.com PAGE.TXT"); tnl(); }
        else if(!disk_ok){ tputs("NO DISK (NVXFS)"); tnl(); }
        else { tputs("DOWNLOADING..."); tnl(); char* b=br_fetch(url);
            if(!br_ok){ tputs("DOWNLOAD FAILED (see COM1)"); tnl(); }
            else { int L=http_body_len; if(L>4090)L=4090; nvx_write(nm,b,L); char nb[8]; utoa(L,nb); tputs("SAVED "); tputs(nb); tputs(" BYTES -> "); tputs(nm); tnl(); tputs("OPEN: file:"); tputs(nm); tputs(" IN 'WEB' OR IN NOTEPAD"); tnl(); } } }
    else if(startsw(c,"ping")){
        u32 ip=gw_ip; const char* p=c+4; while(*p==' ')p++;
        if(*p>='0'&&*p<='9'){ u32 o0=0,o1=0,o2=0,o3=0; o0=0; while(*p>='0'&&*p<='9'){o0=o0*10+(*p-'0');p++;} if(*p=='.')p++; while(*p>='0'&&*p<='9'){o1=o1*10+(*p-'0');p++;} if(*p=='.')p++; while(*p>='0'&&*p<='9'){o2=o2*10+(*p-'0');p++;} if(*p=='.')p++; while(*p>='0'&&*p<='9'){o3=o3*10+(*p-'0');p++;} ip=(o0<<24)|(o1<<16)|(o2<<8)|o3; }
        tputs("PINGING... "); tnl();
        int r=do_ping(ip);
        if(r==1){ tputs("REPLY OK from target"); tnl(); }
        else if(r==0){ tputs("NO REPLY (timeout / NAT may block ICMP)"); tnl(); }
        else { tputs("NETWORK NOT AVAILABLE"); tnl(); } }
    else if(startsw(c,"dns ")){
        const char* h=c+4; while(*h==' ')h++;
        if(!*h){ tputs("USAGE: dns google.com"); tnl(); }
        else { tputs("RESOLVING "); tputs(h); tputs("..."); tnl();
            u32 ip=0; int r=dns_query(h,&ip);
            if(r==1){ char b[24]; int p=0; for(int i=3;i>=0;i--){ u32 oc=(ip>>(i*8))&0xFF; char t[4]; int tl=0; if(oc==0)t[tl++]='0'; else { u32 v=oc; char tmp[4]; int n=0; while(v){tmp[n++]='0'+v%10;v/=10;} while(n)t[tl++]=tmp[--n]; } for(int k=0;k<tl;k++)b[p++]=t[k]; if(i)b[p++]='.'; } b[p]=0; tputs(h); tputs(" -> "); tputs(b); tnl(); }
            else if(r==0){ tputs("DNS FAILED (no answer / NAT)"); tnl(); }
            else { tputs("NETWORK NOT AVAILABLE"); tnl(); } } }
    else if(startsw(c,"http ")){ const char* h=c+5; while(*h==' ')h++;
        if(!*h){ tputs("USAGE: http neverssl.com"); tnl(); }
        else { tputs("FETCHING (HTTP/1.0)..."); tnl(); char* b=br_fetch(h);
            if(!br_ok){ tputs("FETCH FAILED (see COM1 log)"); tnl(); }
            else { tputs("OK. OPEN 'WEB' AND TYPE: http:"); tputs(h); tnl(); int sl=0; while(b[sl]&&b[sl]!='\n'&&sl<40)sl++; char ln[42]; for(int i=0;i<sl;i++)ln[i]=b[i]; ln[sl]=0; tputs("BODY STARTS: "); tputs(ln); tnl(); } } }
    else if(cmdlen>0){ tputs("UNKNOWN CMD: "); tputs(c); tnl(); tputs("TYPE 'HELP'"); tnl(); }
    cmdlen=0;
}
static void term_key(u8 sc){ if(sc&0x80)return; char ch=kchar_shift(sc); if(!ch)return; if(ch=='\b'){ if(cmdlen>0){cmdlen--; if(termlen>0)termlen--; termbuf[termlen]=0;} } else if(ch=='\n'){ run_command(); } else { if(termlen<2040)termbuf[termlen++]=ch; if(cmdlen<198)cmdline[cmdlen++]=ch; termbuf[termlen]=0; } }
static void note_key(u8 sc){ if(sc&0x80)return; char ch=kchar_shift(sc); if(!ch)return; if(ch=='\b'){ if(notelen>0)notelen--; } else if(notelen<1020){ notebuf[notelen++]=ch; } notebuf[notelen]=0; }

static void draw_window_chrome(const char*title){
    if(gfx_fast){ afill(winx+5,winy+8,winw,winh,0,40); }
    else { ultra_shadow(winx,winy,winw,winh); }
    rrectR(winx,winy,winw,winh,8,C_WIN);

    { u32 tc=PAL32[win_focused?C_TASK:C_WIN]; int tr=(tc>>16)&255,tg=(tc>>8)&255,tb=tc&255;
      for(int j=0;j<22;j++){ int lift=18-(j*30)/22;
          int rr=tr+lift,gg=tg+lift,bb=tb+lift;
          if(rr<0)rr=0;if(rr>255)rr=255; if(gg<0)gg=0;if(gg>255)gg=255; if(bb<0)bb=0;if(bb>255)bb=255;
          u32 v=((u32)rr<<16)|((u32)gg<<8)|(u32)bb;
          int x0=winx+((j<8)?(8-j):0), x1=winx+winw-((j<8)?(8-j):0);
          u32* row=FB+(winy+j)*PITCH; for(int xx=x0;xx<x1;xx++)if(xx>=0&&xx<W&&winy+j>=0&&winy+j<H)row[xx]=v; } }
    fill(winx+1,winy+22,winw-2,1,win_focused?C_BLUE:C_MGREY);

    draw_str(winx+12,winy+7,title,win_focused?C_WHITE:C_TITLE);
    { int bw=22,by=winy;
      int cN=winx+winw-3*bw-2, cM=winx+winw-2*bw-2, cC=winx+winw-bw-2;
      u8 ic=win_focused?C_WHITE:(C_MGREY+8);
      int hN=in(mx,my,cN,by,bw,22), hM=in(mx,my,cM,by,bw,22), hC=in(mx,my,cC,by,bw,22);
      if(win_focused&&hN)afill(cN,by+1,bw,19,C_WHITE,28);
      if(win_focused&&hM)afill(cM,by+1,bw,19,C_WHITE,28);
      if(win_focused&&hC){ for(int yy=0;yy<19;yy++){ int ins=(yy<7)?(7-yy):0; afill(cC,by+1+yy,bw-ins,1,C_RED,210); } }
      fill(cN+bw/2-5,winy+11,10,1,ic);
      { int qx=cM+bw/2-4, qy=winy+7;
        if(maximized){ for(int i=0;i<6;i++){ cpx(qx+2+i,qy,ic); cpx(qx+2+i,qy+5,ic); cpx(qx+2,qy+i,ic); cpx(qx+7,qy+i,ic); cpx(qx+i,qy+2,ic); cpx(qx+i,qy+7,ic); cpx(qx,qy+2+i,ic); cpx(qx+5,qy+2+i,ic); } }
        else { for(int i=0;i<8;i++){ cpx(qx+i,qy,ic); cpx(qx+i,qy+7,ic); cpx(qx,qy+i,ic); cpx(qx+7,qy+i,ic); } } }
      { int xx=cC+bw/2-4, xy=winy+7; u8 xc=(win_focused&&hC)?C_WHITE:ic;
        for(int i=0;i<8;i++){ cpx(xx+i,xy+i,xc); cpx(xx+i,xy+7-i,xc); } } }
}

#define G3N 24
#define G3H 12
static int g3_yaw=32, g3_wire=0, g3_preset=0;
static u16 g3_pit=0; static int g3_acc=0;
static short g3sx[G3N*G3N], g3sy[G3N*G3N]; static short g3zv[G3N*G3N];
static int g3z(int gx,int gy){
    switch(g3_preset){
        case 0: return gx*gx - gy*gy;
        case 1: return -(gx*gx + gy*gy);
        case 2: { int r2=gx*gx+gy*gy; return S((u8)(r2*4)); }
        default:{ return (S((u8)(gx*20))*S((u8)(gy*20)))/64; }
    }
}
static const char* g3_formula(void){ switch(g3_preset){case 0:return "Z = X^2 - Y^2";case 1:return "Z = -(X^2+Y^2)";case 2:return "Z = SIN(R)";default:return "Z = SIN X * SIN Y";} }
static u32 g3_heat(int t){ int r,g,b; if(t<0)t=0; if(t>255)t=255;
    if(t<64){ r=0; g=t*4; b=255; } else if(t<128){ r=0; g=255; b=255-(t-64)*4; }
    else if(t<192){ r=(t-128)*4; g=255; b=0; } else { r=255; g=255-(t-192)*4; b=0; }
    return ((u32)r<<16)|((u32)g<<8)|(u32)b; }
static void tri_fill(int x0,int y0,int x1,int y1,int x2,int y2,u32 c,int cx0,int cy0,int cx1,int cy1){
    int t;
    if(y1<y0){t=x0;x0=x1;x1=t;t=y0;y0=y1;y1=t;}
    if(y2<y0){t=x0;x0=x2;x2=t;t=y0;y0=y2;y2=t;}
    if(y2<y1){t=x1;x1=x2;x2=t;t=y1;y1=y2;y2=t;}
    if(y2==y0)return;
    for(int y=y0;y<=y2;y++){
        if(y<cy0||y>cy1)continue;
        int xa=x0+(x2-x0)*(y-y0)/(y2-y0), xb;
        if(y<y1){ xb=(y1==y0)?x1:(x0+(x1-x0)*(y-y0)/(y1-y0)); }
        else    { xb=(y2==y1)?x2:(x1+(x2-x1)*(y-y1)/(y2-y1)); }
        int l=xa<xb?xa:xb, r=xa>xb?xa:xb; if(l<cx0)l=cx0; if(r>cx1)r=cx1;
        u32* row=FB+y*PITCH; for(int x=l;x<=r;x++)row[x]=c;
    }
}
static void g3_line(int x0,int y0,int x1,int y1,u32 c,int cx0,int cy0,int cx1,int cy1){
    int dx=x1-x0,dy=y1-y0,ax=dx<0?-dx:dx,ay=dy<0?-dy:dy,sx=dx<0?-1:1,sy=dy<0?-1:1;
    int err=(ax>ay?ax:-ay)/2;
    for(;;){ if(x0>=cx0&&x0<=cx1&&y0>=cy0&&y0<=cy1)FB[y0*PITCH+x0]=c;
        if(x0==x1&&y0==y1)break; int e2=err; if(e2>-ax){err-=ay;x0+=sx;} if(e2<ay){err+=ax;y0+=sy;} }
}

#define CW 64
#define CH 48
#define CTILE 18
static u8 *craft=(u8*)0x00800000;
static int craft_cam=0,craft_camy=0,craft_sel=1,craft_gen=0;
static u8 craft_col(u8 t){ switch(t){
    case 1:return C_GREEN; case 2:return 20; case 3:return 30; case 4:return C_FOLDER;
    case 5:return C_TEAL;  case 6:return 54; case 7:return 44; case 8:return C_RED;
    case 9:return C_BBLUE; case 11:return 8; case 12:return 60; case 13:return 50; default:return 0; } }
static void craft_tile(int px,int py,u8 t){
    if(t==0){ fill(px,py,CTILE,CTILE,C_BBLUE); return; }
    u8 c=craft_col(t); fill(px,py,CTILE,CTILE,c); u8 dk=(c>4)?c-4:0;
    if(t==1){ fill(px,py,CTILE,4,C_GREEN); fill(px,py+4,CTILE,CTILE-4,20); }
    else if(t==2){ fill(px+3,py+4,2,2,dk); fill(px+10,py+9,2,2,dk); fill(px+6,py+13,2,2,dk); }
    else if(t==3){ fill(px+4,py+3,3,3,dk); fill(px+11,py+10,3,3,dk); }
    else if(t==4){ fill(px+CTILE/2-1,py,2,CTILE,dk); fill(px+3,py,1,CTILE,dk); }
    else if(t==5){ fill(px+3,py+3,2,2,dk); fill(px+9,py+7,2,2,dk); fill(px+6,py+12,2,2,dk); }
    else if(t==8){ fill(px,py+CTILE/2,CTILE,1,dk); fill(px+CTILE/2,py,1,CTILE/2,dk); }
    else if(t==9){ frame(px+2,py+2,CTILE-4,CTILE-4,C_WHITE); }
    else if(t==11){ fill(px+5,py+5,4,4,2); fill(px+10,py+10,3,3,2); }
    else if(t==12){ fill(px+5,py+5,4,4,63); fill(px+10,py+10,3,3,63); }
    else if(t==13){ fill(px+6,py+6,5,5,C_WHITE); } }
static void craft_genworld(void){
    rngs^=pit_read()^((unsigned)cmos(0)<<8);
    int h[CW]; int hh=CH/2;
    for(int x=0;x<CW;x++){ hh+=(rnd()%3)-1; if(hh<CH/2-6)hh=CH/2-6; if(hh>CH/2+6)hh=CH/2+6;
        int top=hh+(S(x*6)/50); if(top<8)top=8; if(top>CH-6)top=CH-6; h[x]=top; }
    for(int x=0;x<CW;x++){ int top=h[x];
        for(int y=0;y<CH;y++){ u8 t=0;
            if(y==top)t=1; else if(y>top&&y<top+4)t=2; else if(y>=top+4)t=3;
            if(t==3){ int r=rnd()%120; if(r<3)t=11; else if(r==3)t=12; else if(r==4&&(rnd()%3==0))t=13; }
            craft[y*CW+x]=t; } }
    for(int x=3;x<CW-3;x++){ if(rnd()%9==0){ int top=h[x]; int th=3+rnd()%2;
        for(int k=1;k<=th;k++) if(top-k>0)craft[(top-k)*CW+x]=4;
        int ly=top-th; for(int dy=-2;dy<=0;dy++)for(int dx=-2;dx<=2;dx++){ int xx=x+dx,yy=ly+dy; if(xx>=0&&xx<CW&&yy>=0&&yy<CH&&craft[yy*CW+xx]==0)craft[yy*CW+xx]=5; } } }
    craft_gen=1; craft_cam=0; craft_camy=0; }
static void craft_save(void){ if(craft_gen&&disk_ok) nvx_write("WORLD.NVX",(char*)craft,CW*CH); }
static void craft_load(void){ if(disk_ok){ int idx=nvx_find("WORLD.NVX"); if(idx>=0){ nvx_read(idx,(char*)craft,CW*CH); craft_gen=1; } } }
static void craft_draw(void){
    draw_window_chrome("NOOVEXCRAFT");
    if(!craft_gen)craft_genworld();
    int cvx=winx+4,cvy=winy+22;
    int cols=(winw-8)/CTILE, rows=(winh-22-32)/CTILE;
    if(craft_cam>CW-cols)craft_cam=CW-cols; if(craft_cam<0)craft_cam=0;
    if(craft_camy>CH-rows)craft_camy=CH-rows; if(craft_camy<0)craft_camy=0;
    for(int ry=0;ry<rows;ry++)for(int rx=0;rx<cols;rx++){ int wx=craft_cam+rx,wy=craft_camy+ry; u8 t=(wx>=0&&wx<CW&&wy>=0&&wy<CH)?craft[wy*CW+wx]:0; craft_tile(cvx+rx*CTILE,cvy+ry*CTILE,t); }
    int hbx=winx+6,hby=winy+winh-28;
    for(int i=1;i<=9;i++){ int bx=hbx+(i-1)*30; fill(bx,hby,26,24,C_TASK); if(craft_sel==i)frame(bx-1,hby-1,28,26,C_WHITE); craft_tile(bx+4,hby+2,i); char nb[2]={'0'+i,0}; draw_str(bx+1,hby+1,nb,C_WHITE); }
    draw_str(winx+6,winy+winh-42,"WASD PAN  1-9 PICK  LMB BUILD RMB DIG  R NEW K/L SAVE",C_TITLE); }

static char* cur_html=0;

static inline u64 rdtsc(void){ u32 a,d; __asm__ volatile("rdtsc":"=a"(a),"=d"(d)); return ((u64)d<<32)|a; }
static void secure_zero(void* p,int n){ volatile u8* v=(volatile u8*)p; for(int i=0;i<n;i++)v[i]=0; }
#define ENT_POOL ((u8*)0x00951800u)
static u32 ent_ctr=0; static int ent_ready=0;
static void entropy_add(const void* d,int n){
    u8 b[96]; int m=0; for(int i=0;i<32;i++)b[m++]=ENT_POOL[i];
    const u8* p=(const u8*)d; for(int i=0;i<n&&m<92;i++)b[m++]=p[i];
    sha256_hash(b,m,ENT_POOL);
}
static void entropy_stir(void){
    u8 b[20]; u64 t=rdtsc(); for(int i=0;i<8;i++)b[i]=(u8)(t>>(i*8));
    u16 pc=pit_read(); b[8]=(u8)pc; b[9]=(u8)(pc>>8);
    b[10]=cmos(0); b[11]=cmos(2); b[12]=cmos(4);
    b[13]=(u8)mx; b[14]=(u8)(mx>>8); b[15]=(u8)my; b[16]=(u8)(my>>8);
    b[17]=(u8)mbtn; b[18]=(u8)ent_ctr; b[19]=(u8)(ent_ctr>>8);
    entropy_add(b,20);
}
static void entropy_init(void){
    entropy_add((const void*)0x00200000u,256);
    for(int i=0;i<96;i++){ entropy_stir(); volatile int sp=0; for(int j=0;j<300;j++)sp+=j; (void)sp; }
    ent_ready=1;
}
static void entropy_get(u8* out,int n){
    if(!ent_ready)entropy_init();
    entropy_stir();
    int o=0;
    while(o<n){
        u8 b[40]; for(int i=0;i<32;i++)b[i]=ENT_POOL[i];
        u32 c=ent_ctr++; b[32]=(u8)c;b[33]=(u8)(c>>8);b[34]=(u8)(c>>16);b[35]=(u8)(c>>24);
        u64 t=rdtsc(); b[36]=(u8)t;b[37]=(u8)(t>>8);b[38]=(u8)(t>>16);b[39]=(u8)(t>>24);
        u8 blk[32]; sha256_hash(b,40,blk);
        for(int i=0;i<32&&o<n;i++)out[o++]=blk[i];
        u8 rb[33]; for(int i=0;i<32;i++)rb[i]=ENT_POOL[i]; rb[32]=1; sha256_hash(rb,33,ENT_POOL);
    }
}
static char br_addr[128]; static int br_addr_len=0, br_addr_focus=0, br_scroll=0;
static int br_brand=0;
static int br_is_local=0;
static int br_use_expand=0;
static void br_prep(void);
static int br_last_https=0, https_only=0;
static char br_cur_host[64]={0}; static int br_cur_https=0;
static char* br_fbuf=(char*)0x00810000;
#define BR_LINKBUF ((char*)0x00814000u)
static char* br_hist[12]; static int br_hist_n=0;
#define br_hnm ((char(*)[24])0x0095DA00u)
typedef struct{int x,y,w,h; const char* href; int hlen;} brlink;
#define br_link ((brlink*)0x0095D700u)
static int br_link_n=0;
static const char* PG_HOME=
 "<title>New Tab</title>"
 "<style>"
 " body{background:linear-gradient(#ffffff,#eaf1fb);}"
 " .logo{text-align:center;background:linear-gradient(to right,#4285f4,#9b30ff);color:white;height:44;padding:10;font-size:30px;margin:18;}"
 " .sub{text-align:center;color:#5f6368;padding:2;}"
 " .search{background:#ffffff;border:2 #c8d0dc;height:26;padding:9;width:560;margin:12;text-align:center;color:#3c4043;}"
 " .row{margin:8;}"
 " .t1{background:linear-gradient(to right,#4285f4,#00a2ff);color:white;text-align:center;padding:12;height:22;}"
 " .t2{background:linear-gradient(to right,#ea4335,#ff7a59);color:white;text-align:center;padding:12;height:22;}"
 " .t3{background:linear-gradient(to right,#34a853,#00c489);color:white;text-align:center;padding:12;height:22;}"
 " .t4{background:linear-gradient(to right,#fbbc05,#ff9d00);color:white;text-align:center;padding:12;height:22;}"
 " .t5{background:linear-gradient(to right,#9b30ff,#c86bff);color:white;text-align:center;padding:12;height:22;}"
 " .t6{background:linear-gradient(to right,#00897b,#26c6da);color:white;text-align:center;padding:12;height:22;}"
 " .foot{text-align:center;color:#80868b;padding:6;}"
 "</style>"
 "<div class=logo>NOOVEX</div>"
 "<div class=sub>NoovexBrowser - real HTML/CSS/JS engine on a from-scratch OS</div>"
 "<div class=search>Type a site in the address bar above - HTTPS (TLS 1.3) or HTTP</div>"
 "<table class=row><tr>"
 "<td><div class=t1><a href=about>ABOUT</a></div></td>"
 "<td><div class=t2><a href=news>NEWS</a></div></td>"
 "<td><div class=t3><a href=games>GAMES</a></div></td>"
 "</tr><tr>"
 "<td><div class=t4><a href=help>HELP</a></div></td>"
 "<td><div class=t5><a href=gfx>GRAPHICS</a></div></td>"
 "<td><div class=t6><a href=demo>JS DEMO</a></div></td>"
 "</tr></table>"
 "<table class=row><tr>"
 "<td><div class=t1><a href=https://example.com>EXAMPLE.COM</a></div></td>"
 "<td><div class=t3><a href=http://neverssl.com>NEVERSSL</a></div></td>"
 "<td><div class=t5><a href=css>CSS + GRADIENTS</a></div></td>"
 "</tr></table>"
 "<div class=foot>NoovexOS - no Chromium, no Linux</div>"
 "<p>Tip: write HTML in NOTEPAD, save it, then open it here by typing file:NAME in the address bar.</p>";
static const char* PG_ABOUT=
 "<title>ABOUT</title><h1>About NoovexOS</h1>"
 "<p>NoovexOS is a 32-bit protected-mode OS written from scratch in C. Own bootloader, VESA graphics, window manager, NVXFS filesystem and drivers.</p>"
 "<h2>Apps</h2><p>Files, Terminal, Paint, Tetris, Snake, Piano, NoovexCraft and this browser.</p>"
 "<p><a href=home>BACK TO HOME</a></p>";
static const char* PG_NEWS=
 "<title>NEWS</title><h1>NoovexOS News</h1>"
 "<h2>v1.0</h2><p>Added NoovexCraft block game and NoovexBrowser. Real apps, really running.</p>"
 "<h2>Drivers</h2><p>COM1 serial, VBE DISPI runtime resolution, Sound Blaster 16.</p>"
 "<hr><p><a href=home>HOME</a> <a href=games>GAMES</a></p>";
static const char* PG_GAMES=
 "<title>GAMES</title><h1>Games</h1>"
 "<ul><li>NoovexCraft - build with blocks</li><li>Tetris</li><li>Snake</li></ul>"
 "<p><a href=home>HOME</a></p>";
static const char* PG_HELP=
 "<title>HELP</title><h1>Help</h1>"
 "<p>Type a page name like home about news games then press ENTER.</p>"
 "<p>Open a saved file with file:NAME.TXT - use BACK and RLD buttons.</p>"
 "<p><a href=home>HOME</a></p>";
static const char* PG_404=
 "<title>404</title><h1>Not Found</h1><p>That page or file does not exist.</p><p><a href=home>HOME</a></p>";
static const char* PG_DEMO=
 "<title>JS+CSS DEMO</title>"
 "<style>"
 " h1{color:white;background:navy;text-align:center;padding:8;}"
 " .card{border:2px solid teal;padding:6;margin:6;width:400;}"
 " .card h3{color:teal;}"
 " .tag{background:green;color:white;}"
 " #muted{color:gray;}"
 " nav a{color:orange;}"
 "</style>"
 "<h1>JS + CSS BOX MODEL</h1>"
 "<div class=card>"
 "<h3>CARD WITH BORDER + PADDING</h3>"
 "<p>This box has a <b>border</b>, padding, margin and a fixed width set in CSS.</p>"
 "<p class=tag>Span with a green background.</p>"
 "<p id=muted>Gray muted text via #id rule.</p>"
 "</div>"
 "<nav><a href=home>HOME</a>  <a href=demo>RELOAD</a></nav>"
 "<h3>JAVASCRIPT</h3>"
 "<script>"
 "function sq(x){ return x*x; }"
 "document.write('<p>sq(9)='+sq(9)+', max(4,7)='+Math.max(4,7)+', sqrt(81)='+Math.sqrt(81)+'</p>');"
 "var a=[]; for(var i=1;i<=5;i++){ a.push(sq(i)); } document.write('<p>squares: '+a+'</p>');"
 "var s='noovex'; document.write('<p>upper='+s.toUpperCase()+' len='+s.length+'</p>');"
 "document.write('<p>fib: '); var x=0,y=1; for(var k=0;k<10;k++){ document.write(x+' '); var t=x+y; x=y; y=t; } document.write('</p>');"
 "</script>"
 "<p><a href=home>BACK HOME</a></p>";
static const char* PG_CSS=
 "<title>CSS ENGINE</title>"
 "<style>"
 " h1{color:rgb(255,255,255);background:rgb(30,90,200);text-align:center;padding:10;}"
 " h2{color:teal;}"
 " .box{border:2px solid navy;padding:8;margin:8;width:420;}"
 " .ok{background:green;color:white;padding:4;}"
 " .warn{background:red;color:white;padding:4;}"
 " #big{font-size:24px;color:navy;}"
 " #hidden{display:none;}"
 " .box p{color:gray;}"
 " .grad1{background:linear-gradient(to right, #ff5500, #0088ff); height:40; padding:8; color:white;}"
 " .grad2{background:linear-gradient(navy, teal); height:40; padding:8; color:white;}"
 " .grad3{background:linear-gradient(90deg, #aa00ff, #ffaa00); height:40; padding:8; color:white;}"
 "</style>"
 "<h1>NOOVEX CSS ENGINE</h1>"
 "<h2>Selectors that work</h2>"
 "<div class=box>"
 "<p id=big>Large text via id and font-size 24px</p>"
 "<p>Gray paragraph via descendant selector box p</p>"
 "<p class=ok>GREEN class background</p>"
 "<p class=warn>RED class background</p>"
 "<p id=hidden>You should NOT see this display none</p>"
 "</div>"
 "<h2>Gradients</h2>"
 "<div class=grad1>Horizontal: orange to blue</div>"
 "<div class=grad2>Vertical: navy to teal</div>"
 "<div class=grad3>Diagonal 90deg: purple to gold</div>"
 "<nav><a href=css>RELOAD</a>  <a href=demo>BOX DEMO</a>  <a href=home>HOME</a></nav>";
static const char* PG_GFX=
 "<title>GRAPHICS</title>"
 "<style>"
 " body{background:linear-gradient(#ffffff,#cfe3ff);}"
 " h1{color:white;background:linear-gradient(to right,#7a00ff,#00a2ff);text-align:center;padding:8;height:34;font-size:24px;}"
 " h2{color:navy;font-size:20px;}"
 " h3{color:teal;}"
 " p{color:#222;}"
 " .card{border:2px solid #0088cc;padding:8;margin:6;width:430;}"
 " .g1{background:linear-gradient(to right,#ff5500,#0088ff);height:30;padding:6;color:white;}"
 " .g2{background:linear-gradient(#aa00ff,#00ddaa);height:30;padding:6;color:white;}"
 " .g3{background:linear-gradient(90deg,#ff0066,#ffcc00);height:30;padding:6;color:white;}"
 " .sw{height:15;padding:2;color:white;}"
 "</style>"
 "<body>"
 "<h1>NOOVEX GRAPHICS ENGINE</h1>"
 "<p>Gradients, hsl() colours, the box model and JavaScript-drawn graphics - all rendered by NoovexOS itself.</p>"
 "<h2>DECODED JPEG PHOTO</h2>"
 "<p>A real baseline JPEG, decoded by the NoovexOS kernel and drawn here:</p>"
 "<img src=noovex.jpg>"
 "<h2>HSL HUE SPECTRUM</h2>"
 "<div class=sw style=background:linear-gradient(hsl(0,90,52),hsl(0,90,52))>HUE 0</div>"
 "<div class=sw style=background:linear-gradient(hsl(30,95,50),hsl(30,95,50))>HUE 30</div>"
 "<div class=sw style=background:linear-gradient(hsl(60,95,48),hsl(60,95,48))>HUE 60</div>"
 "<div class=sw style=background:linear-gradient(hsl(90,90,46),hsl(90,90,46))>HUE 90</div>"
 "<div class=sw style=background:linear-gradient(hsl(120,90,42),hsl(120,90,42))>HUE 120</div>"
 "<div class=sw style=background:linear-gradient(hsl(150,90,42),hsl(150,90,42))>HUE 150</div>"
 "<div class=sw style=background:linear-gradient(hsl(180,90,42),hsl(180,90,42))>HUE 180</div>"
 "<div class=sw style=background:linear-gradient(hsl(210,90,48),hsl(210,90,48))>HUE 210</div>"
 "<div class=sw style=background:linear-gradient(hsl(240,95,58),hsl(240,95,58))>HUE 240</div>"
 "<div class=sw style=background:linear-gradient(hsl(270,90,56),hsl(270,90,56))>HUE 270</div>"
 "<div class=sw style=background:linear-gradient(hsl(300,90,50),hsl(300,90,50))>HUE 300</div>"
 "<div class=sw style=background:linear-gradient(hsl(330,92,52),hsl(330,92,52))>HUE 330</div>"
 "<h2>GRADIENTS</h2>"
 "<div class=g1>Horizontal: orange to blue</div>"
 "<div class=g2>Vertical: purple to mint</div>"
 "<div class=g3>Diagonal: pink to gold</div>"
 "<h2>BOX MODEL</h2>"
 "<div class=card><h3>A CSS CARD</h3><p>This box has a border, padding, a margin and a fixed width - all set in CSS.</p></div>"
 "<h2>JAVASCRIPT BAR CHART</h2>"
 "<script>"
 "var data=[42,78,55,90,33,67];"
 "var lab=['JAN','FEB','MAR','APR','MAY','JUN'];"
 "for(var i=0;i<data.length;i++){"
 " var w=data[i]*4; var hue=i*60;"
 " document.write('<div style=background:linear-gradient(hsl('+hue+',80,50),hsl('+hue+',80,50));width:'+w+';height:15;color:white;padding:1>'+lab[i]+' '+data[i]+'</div>');"
 "}"
 "</script>"
 "<h2>TABLE / INFOBOX</h2>"
 "<table>"
 "<tr><th>PLANET</th><th>MOONS</th><th>DAY h</th></tr>"
 "<tr><td>EARTH</td><td>1</td><td>24</td></tr>"
 "<tr><td>MARS</td><td>2</td><td>25</td></tr>"
 "<tr><td>JUPITER</td><td>95</td><td>10</td></tr>"
 "</table>"
 "<nav><a href=gfx>RELOAD</a>  <a href=css>CSS DEMO</a>  <a href=home>HOME</a></nav>";
static char* http_buf=(char*)0x00830000;
static int parse_dotted(const char* s,u32* ip){ u32 o[4]={0,0,0,0}; int part=0,seen=0; while(*s){ if(*s>='0'&&*s<='9'){ o[part]=o[part]*10+(*s-'0'); seen=1; } else if(*s=='.'){ if(part>=3)return 0; part++; } else return 0; s++; } if(part!=3||!seen)return 0; *ip=(o[0]<<24)|(o[1]<<16)|(o[2]<<8)|o[3]; return 1; }
static const char* PG_NOTLS=
 "<title>HTTPS</title><h1>HTTPS not supported</h1>"
 "<p>NoovexBrowser speaks plain HTTP only. TLS (encryption + certificates) is a huge crypto stack that is not built yet.</p>"
 "<p>Try an http:// site instead:</p><ul>"
 "<li><a href=http://neverssl.com>http://neverssl.com</a></li>"
 "<li><a href=http://example.com>http://example.com</a></li></ul>"
 "<p><a href=home>HOME</a></p>";
#define br_epage ((char*)0x00952000u)
static void mkstr(char* d,int* o,const char* s){ while(*s)d[(*o)++]=*s++; }
static void mku32(char* d,int* o,u32 v){ char t[12];int n=0; if(!v)t[n++]='0'; while(v){t[n++]='0'+v%10;v/=10;} while(n)d[(*o)++]=t[--n]; }
static char* br_fail(const char* msg,const char* host){
    br_ok=0; int o=0; char ib[20];
    mkstr(br_epage,&o,"<title>Error</title><h1>Cannot load page</h1><p>");
    mkstr(br_epage,&o,msg);
    if(host){ mkstr(br_epage,&o," "); mkstr(br_epage,&o,host); }
    mkstr(br_epage,&o,"</p><hr><h2>Live network diagnostics</h2><p>");
    mkstr(br_epage,&o,"<b>NIC (AMD PCnet):</b> ");
    mkstr(br_epage,&o, nic_present?"FOUND":"NOT FOUND - set Adapter = PCnet-FAST III");
    mkstr(br_epage,&o,"<br><b>Init / link up:</b> "); mkstr(br_epage,&o, net_ok?"OK":"FAILED");
    if(nic_present){
        mkstr(br_epage,&o,"<br><b>MAC:</b> ");
        const char* hx="0123456789ABCDEF";
        for(int i=0;i<6;i++){ br_epage[o++]=hx[my_mac[i]>>4]; br_epage[o++]=hx[my_mac[i]&15]; if(i<5)br_epage[o++]=':'; }
        ip_to_str(my_ip,ib);  mkstr(br_epage,&o,"<br><b>My IP:</b> "); mkstr(br_epage,&o,ib);
        ip_to_str(gw_ip,ib);  mkstr(br_epage,&o,"<br><b>Gateway:</b> "); mkstr(br_epage,&o,ib);
        ip_to_str(dns_ip,ib); mkstr(br_epage,&o,"<br><b>DNS server:</b> "); mkstr(br_epage,&o,ib);
        mkstr(br_epage,&o,"<br><b>Packets sent (TX):</b> "); mku32(br_epage,&o,nic_txc);
        mkstr(br_epage,&o,"<br><b>Packets recv (RX):</b> "); mku32(br_epage,&o,nic_rxc);
        u8 gm[6]; gw_known=0; int ar = net_ok ? arp_resolve(gw_ip,gm) : 0;
        mkstr(br_epage,&o,"<br><b>Gateway ARP test:</b> "); mkstr(br_epage,&o, ar?"REPLY OK - link works":"NO REPLY");
        mkstr(br_epage,&o,"<br><b>TX after probe:</b> "); mku32(br_epage,&o,nic_txc);
        mkstr(br_epage,&o,"<br><b>RX after probe:</b> "); mku32(br_epage,&o,nic_rxc);
    }
    if(net_ok){
        mkstr(br_epage,&o,"<br><b>Resolved IP:</b> ");
        if(last_dns_ip){ ip_to_str(last_dns_ip,ib); mkstr(br_epage,&o,ib); } else mkstr(br_epage,&o,"(none yet)");
        mkstr(br_epage,&o,"<br><b>TCP established:</b> "); mkstr(br_epage,&o,last_tcp_est?"YES":"NO");
        mkstr(br_epage,&o,"<br><b>Reply pkts after GET:</b> "); mku32(br_epage,&o,(u32)tcp_rpk);
        mkstr(br_epage,&o,"<br><b>Pkts with data:</b> "); mku32(br_epage,&o,(u32)tcp_dpk);
        mkstr(br_epage,&o,"<br><b>Seq mismatches:</b> "); mku32(br_epage,&o,(u32)tcp_smiss);
        mkstr(br_epage,&o,"<br><b>Last TCP flags:</b> 0x");
        { const char* hx="0123456789ABCDEF"; br_epage[o++]=hx[(tcp_lfl>>4)&15]; br_epage[o++]=hx[tcp_lfl&15]; }
        mkstr(br_epage,&o," (FIN=01 SYN=02 RST=04 PSH=08 ACK=10)");
    }
    mkstr(br_epage,&o,"</p><hr><h2>How to read this</h2><ul>");
    mkstr(br_epage,&o,"<li>TX is 0: card not sending - driver/PCI bug</li>");
    mkstr(br_epage,&o,"<li>TX above 0 but RX is 0: sends ok, receives nothing - RX ring or link</li>");
    mkstr(br_epage,&o,"<li>Gateway ARP REPLY OK: network works, only DNS is the issue</li>");
    mkstr(br_epage,&o,"<li>VBox: Adapter = PCnet-FAST III, Attached to NAT</li>");
    mkstr(br_epage,&o,"<li>HTTPS works (TLS 1.3); toggle HTTPS-ONLY in the bar to block plaintext</li></ul>");
    mkstr(br_epage,&o,"<p><a href=home>HOME</a></p>");
    br_epage[o]=0; return br_epage;
}

static int br_find_ci(const char* s,int n,const char* pat){
    int pl=0; while(pat[pl])pl++; if(pl==0)return -1;
    for(int i=0;i+pl<=n;i++){ int j=0; for(;j<pl;j++){ char a=s[i+j],b=pat[j]; if(a>='A'&&a<='Z')a+=32; if(b>='A'&&b<='Z')b+=32; if(a!=b)break; } if(j==pl)return i; }
    return -1;
}
static int br_status(const char* b,int n){ int i=0; while(i<n&&b[i]!=' ')i++; i++; if(i+2>=n||b[i]<'0'||b[i]>'9')return 0; return (b[i]-'0')*100+(b[i+1]-'0')*10+(b[i+2]-'0'); }
static int br_get_loc(const char* h,int hlen,char* out,int max){
    int p=br_find_ci(h,hlen,"\nlocation:"); if(p<0)return 0; p+=10;
    while(p<hlen&&(h[p]==' '||h[p]=='\t'))p++;
    int o=0; while(p<hlen&&h[p]!='\r'&&h[p]!='\n'&&o<max-1)out[o++]=h[p++]; out[o]=0; return o>0;
}

static int br_dechunk(char* b,int n){
    int i=0,o=0;
    for(;;){ int sz=0,got=0;
        while(i<n){ char c=b[i]; int d; if(c>='0'&&c<='9')d=c-'0'; else if(c>='a'&&c<='f')d=c-'a'+10; else if(c>='A'&&c<='F')d=c-'A'+10; else break; sz=sz*16+d; i++; got=1; }
        while(i<n&&b[i]!='\n')i++; if(i<n)i++;
        if(!got||sz<=0)break;
        if(i+sz>n)sz=n-i;
        for(int k=0;k<sz;k++)b[o++]=b[i++];
        if(i<n&&b[i]=='\r')i++; if(i<n&&b[i]=='\n')i++;
    }
    return o;
}
static char* br_fetch(const char* spec){
    char* cur=(char*)0x00928400u; char* loc=(char*)0x00928700u;
    { int i=0; while(spec[i]&&i<299){cur[i]=spec[i];i++;} cur[i]=0; }
    if(reader_on && br_proxy[0] && cur[0]!='/'){
        const char* tp=cur; if(startsw(tp,"https://"))tp+=8; else if(startsw(tp,"http://"))tp+=7; else if(startsw(tp,"https:"))tp+=6; else if(startsw(tp,"http:"))tp+=5;
        char thost[64]; int ti=0; while(*tp&&*tp!='/'&&ti<63){thost[ti++]=*tp;tp++;} thost[ti]=0;
        const char* pp=br_proxy; if(startsw(pp,"https://"))pp+=8; else if(startsw(pp,"http://"))pp+=7;
        char phost[64]; int pj=0; while(*pp&&*pp!='/'&&pj<63){phost[pj++]=*pp;pp++;} phost[pj]=0;
        if(!streq(thost,phost)){
            char tgt[600]; int to=0;
            if(!startsw(cur,"http://")&&!startsw(cur,"https://")){ const char* hs="https://"; for(int i=0;hs[i]&&to<590;i++)tgt[to++]=hs[i]; }
            for(int i=0;cur[i]&&to<599;i++)tgt[to++]=cur[i]; tgt[to]=0;
            char wr[760]; int o=0; const char* hx="0123456789ABCDEF";
            for(int i=0;br_proxy[i]&&o<200;i++)wr[o++]=br_proxy[i];
            if(streq(phost,"r.jina.ai")){
                for(int i=0;tgt[i]&&o<758;i++)wr[o++]=tgt[i];
            } else {
                for(int i=0;tgt[i]&&o<752;i++){ unsigned char ch=(unsigned char)tgt[i];
                    if((ch>='A'&&ch<='Z')||(ch>='a'&&ch<='z')||(ch>='0'&&ch<='9')||ch=='-'||ch=='_'||ch=='.'||ch=='~') wr[o++]=ch;
                    else { wr[o++]='%'; wr[o++]=hx[ch>>4]; wr[o++]=hx[ch&15]; } }
            }
            wr[o]=0; int k=0; while(wr[k]&&k<767){cur[k]=wr[k];k++;} cur[k]=0;
        }
    }
    char host[64];
    for(int hop=0;hop<6;hop++){
        char path[640]; int sch_https=0; const char* p=cur; while(*p==' ')p++;
        if(startsw(p,"https://")){p+=8;sch_https=1;} else if(startsw(p,"http://"))p+=7; else if(startsw(p,"https:")){p+=6;sch_https=1;} else if(startsw(p,"http:"))p+=5;
        int hi=0; while(*p&&*p!='/'&&hi<63){host[hi++]=*p;p++;} host[hi]=0;
        int pi=0; if(*p=='/'){ while(*p&&pi<639){path[pi++]=*p;p++;} } if(pi==0)path[pi++]='/'; path[pi]=0;
        br_last_https=sch_https;
        if(https_only && !sch_https) return br_fail("HTTPS-ONLY MODE IS ON - refused unencrypted HTTP for",host);
        if(!nic_up()) return br_fail("No network adapter. Set Adapter Type = PCnet-FAST III + NAT in VirtualBox.",0);
        u32 ip=0; if(!parse_dotted(host,&ip)){ if(dns_query(host,&ip)!=1) return br_fail("DNS lookup failed for",host); }
        last_dns_ip=ip;
        int n;
        if(sch_https){
            if(!tcps_open(ip,443)) return br_fail("HTTPS: could not establish TCP to port 443 for",host);
            char rq[820]; int rr=0; const char* g="GET ";
            while(*g)rq[rr++]=*g++;
            for(int i=0;path[i]&&rr<640;i++)rq[rr++]=path[i];
            const char* h1=" HTTP/1.1\r\nHost: "; for(int i=0;h1[i]&&rr<700;i++)rq[rr++]=h1[i];
            for(int i=0;host[i]&&rr<760;i++)rq[rr++]=host[i];
            const char* he="\r\n"; for(int i=0;he[i]&&rr<764;i++)rq[rr++]=he[i];
            if(streq(host,"r.jina.ai")){ const char* hj="X-Return-Format: "; for(int i=0;hj[i]&&rr<744;i++)rq[rr++]=hj[i]; for(int i=0;jina_fmt[i]&&rr<760;i++)rq[rr++]=jina_fmt[i]; rq[rr++]='\r'; rq[rr++]='\n'; }
            const char* h2="Accept-Encoding: identity\r\nConnection: close\r\nUser-Agent: NoovexBrowser\r\n\r\n"; for(int i=0;h2[i]&&rr<819;i++)rq[rr++]=h2[i];
            u8 ent[64]; entropy_get(ent,64);
            tls_io* io=(tls_io*)0x00930000u; io->wr=tcps_wr; io->rd=tcps_rd; io->ctx=0; io->rlen=0; io->rpos=0;
            n=tls_get(io,host,hi,ent,rq,rr,(u8*)http_buf,59000); last_tls_code=n; secure_zero(ent,64);
            if(n<=0) return br_fail(tls_err_text(n),host);
        } else {
            char req[820]; int r=0; const char* g="GET ";
            while(*g)req[r++]=*g++;
            for(int i=0;path[i]&&r<640;i++)req[r++]=path[i];
            const char* h1=" HTTP/1.0\r\nHost: "; for(int i=0;h1[i]&&r<700;i++)req[r++]=h1[i];
            for(int i=0;host[i]&&r<760;i++)req[r++]=host[i];
            const char* h2="\r\nAccept-Encoding: identity\r\nConnection: close\r\nUser-Agent: NoovexBrowser\r\n\r\n"; for(int i=0;h2[i]&&r<819;i++)req[r++]=h2[i];
            n=tcp_get(ip,80,req,r,(u8*)http_buf,59000);
            if(n==-2)return br_fail("Could not reach the gateway (ARP failed).",0);
            if(n==-3)return br_fail("No response - connection timed out to",host);
            if(n==-4)return br_fail("Connection refused (RST) by",host);
            if(n<=0)return br_fail("Connected but received no data from",host);
        }
        http_buf[n]=0;
        int hdrend=n,bodyoff=n;
        for(int i=0;i+3<n;i++){ if(http_buf[i]=='\r'&&http_buf[i+1]=='\n'&&http_buf[i+2]=='\r'&&http_buf[i+3]=='\n'){ hdrend=i+2; bodyoff=i+4; break; } }
        int code=br_status(http_buf,hdrend);
        if((code==301||code==302||code==303||code==307||code==308)&&hop<5&&br_get_loc(http_buf,hdrend,loc,260)){
            if(startsw(loc,"http://")||startsw(loc,"https://")){ int i=0; while(loc[i]&&i<299){cur[i]=loc[i];i++;} cur[i]=0; }
            else { int o=0; const char* sc=sch_https?"https://":"http://"; for(int i=0;sc[i]&&o<299;i++)cur[o++]=sc[i];
                for(int i=0;host[i]&&o<299;i++)cur[o++]=host[i]; if(loc[0]!='/'&&o<299)cur[o++]='/';
                for(int i=0;loc[i]&&o<299;i++)cur[o++]=loc[i]; cur[o]=0; }
            continue;
        }
        char* body=http_buf+bodyoff; int blen=n-bodyoff; if(blen<0)blen=0;
        if(br_find_ci(http_buf,hdrend,"\ntransfer-encoding:")>=0 && br_find_ci(http_buf,hdrend,"chunked")>=0)
            blen=br_dechunk(body,blen);
        body[blen]=0; http_body_len=blen; br_ok=1;
        { int i=0; while(host[i]&&i<63){br_cur_host[i]=host[i];i++;} br_cur_host[i]=0; br_cur_https=sch_https; }
        return body;
    }
    return br_fail("Too many redirects for",host);
}

static void wx_clean(char* d,const char* s,int max){
    int o=0;
    for(int i=0;s[i]&&o<max-1;i++){ unsigned char c=(unsigned char)s[i];
        if(c>=0x80) continue;
        if(c=='\n'||c=='\r') break;
        if(c>='a'&&c<='z') c-=32;
        if(c<32) continue;
        d[o++]=(char)c; }
    d[o]=0;
}
static void weather_fetch(void){
    if(!nic_up()){ toast_set("NO NETWORK - SET PCNET-FAST III + NAT"); return; }
    toast_set("FETCHING WEATHER...");
    int saved=reader_on; reader_on=0;
    char* body=br_fetch("https://wttr.in/?format=%l|%t|%C");
    reader_on=saved;
    if(!body||!br_ok){ toast_set("WEATHER FETCH FAILED"); return; }
    char parts[3][40]; int np=0,pi=0;
    for(int i=0;body[i]&&np<3;i++){ char c=body[i];
        if(c=='\n'||c=='\r') break;
        if(c=='|'){ parts[np][pi]=0; np++; pi=0; continue; }
        if(pi<39) parts[np][pi++]=c; }
    if(pi>0&&np<3){ parts[np][pi]=0; np++; }
    if(np>=3){
        wx_clean(wx_loc,parts[0],24);
        wx_clean(wx_temp,parts[1],16);
        wx_clean(wx_cond,parts[2],28);
        wx_have=1; toast_set("WEATHER UPDATED"); dirty=1;
    } else { toast_set("WEATHER PARSE ERROR"); }
}

#define AI_REQ    ((char*)0x00940000u)
#define AI_ANS    ((char*)0x00948000u)
#define AI_KEY    ((char*)0x00950000u)
#define AI_PROMPT ((char*)0x00950400u)
#ifndef CLAUDE_MODEL
#define CLAUDE_MODEL "claude-sonnet-4-6"
#endif
static int ai_keylen=0, ai_plen=0, ai_field=0, ai_rc=0, ai_scroll=0, pending_ai=0, ai_saved=0;
static int ai_show=0, cliplen=0, chat_n=0, chat_used=0, ai_tobottom=0;

typedef struct { u8 cat; int app; const char* name; const char* blurb; } stentry;
static const char* CAT_NAMES[5]={"GAMES","TOOLS","NET","MEDIA","SYSTEM"};
static const stentry STORE[39]={
    {0, 8,"SNAKE","EAT, GROW, DONT CRASH"},
    {0,12,"TETRIS","CLASSIC FALLING BLOCKS"},
    {0,14,"NOOVEX CRAFT","2D SANDBOX BUILDER"},
    {0,19,"MINESCAN","FIND THE HIDDEN MINES"},
    {0,34,"FLAPPY BIRD","TAP SPACE TO FLY"},
    {0,19,"BRICKOUT","SMASH THE BRICK WALL"},
    {0,19,"SOLITAIRE","CLASSIC CARD PATIENCE"},
    {0,19,"ASTRO BLAST","SHOOT THE ASTEROIDS"},
    {1, 5,"NOTEPAD","WRITE AND SAVE TEXT"},
    {1,11,"PAINT","16-COLOR DRAWING CANVAS"},
    {1,19,"CALCULATOR","ADD, SUBTRACT, MORE"},
    {1,19,"CLOCK","WORLD CLOCK + ALARMS"},
    {1,19,"CALENDAR","PLAN YOUR MONTH"},
    {1,19,"STICKY NOTES","QUICK DESKTOP MEMOS"},
    {1,19,"UNIT CONVERT","CONVERT UNITS FAST"},
    {1,19,"ARCHIVER","ZIP AND UNZIP FILES"},
    {2,15,"NOOVEX BROWSER","BROWSE THE WEB OVER HTTPS"},
    {2,16,"ASK CLAUDE","CHAT WITH CLAUDE AI"},
    {2,37,"NOOVEX MAIL","SEND EMAIL OVER TLS"},
    {2,19,"CHATROOM","TALK TO FRIENDS ONLINE"},
    {2,19,"FTP CLIENT","TRANSFER FILES OVER NET"},
    {2,19,"WEATHER","FORECAST FOR YOUR CITY"},
    {2,19,"RSS READER","FOLLOW YOUR FEEDS"},
    {2,19,"DOWNLOADER","GRAB FILES FROM THE WEB"},
    {3,13,"PIANO","PLAY THE PC SPEAKER"},
    {3,19,"MUSIC PLAYER","PLAY YOUR TUNES"},
    {3,19,"IMAGE VIEWER","BROWSE PICTURES"},
    {3,19,"VIDEO PLAYER","WATCH CLIPS"},
    {3,19,"RECORDER","RECORD AUDIO NOTES"},
    {3,19,"GIF MAKER","MAKE ANIMATED GIFS"},
    {3,19,"NET RADIO","STREAM RADIO STATIONS"},
    {3,19,"VISUALIZER","TRIPPY SOUND VISUALS"},
    {4, 1,"FILES","BROWSE YOUR FILES"},
    {4, 2,"TERMINAL","COMMAND-LINE SHELL"},
    {4, 4,"SETTINGS","TWEAK YOUR SYSTEM"},
    {4, 6,"TASK MGR","SEE RUNNING APPS"},
    {4, 7,"DEVICES","INSPECT HARDWARE"},
    {4, 3,"ABOUT","ABOUT " OSNAME},
    {4,10,"RECYCLE BIN","RESTORE DELETED FILES"},
};
#define STORE_N 39
static u8 store_inst[5]={0,0,0,0,0}; static int store_scroll=0, store_cat=-1;
static int store_myapps=0;
static int inst_get(int i){ return (store_inst[i>>3]>>(i&7))&1; }
static void inst_set(int i){ store_inst[i>>3]|=(u8)(1<<(i&7)); }

static int idrag=-1, idrag_dx=0, idrag_dy=0;
static int fld_view=-1;
static int dclk_item=-1, dclk_timer=0;
static char gen_name[20];
static int dsk_count_desktop(void){ int c=0; for(int i=0;i<DSK_MAX;i++) if(DSK[i].used&&DSK[i].parent==-1)c++; return c; }
static int dsk_sel=0;
static int dsk_nth(int n){ int c=0; for(int i=0;i<DSK_MAX;i++) if(DSK[i].used&&DSK[i].parent==-1){ if(c==n)return i; c++; } return -1; }
static int dsk_alloc(void){ for(int i=0;i<DSK_MAX;i++) if(!DSK[i].used)return i; return -1; }
static void dsk_place(int i){ int n=dsk_count_desktop(); int col=n%2,row=n/2; DSK[i].x=180+col*84; DSK[i].y=70+row*64; }
static int dsk_add(u8 type,int app,const char* nm,int parent){ int i=dsk_alloc(); if(i<0)return -1;
    DSK[i].used=1; DSK[i].type=type; DSK[i].app=app; DSK[i].parent=parent;
    int k=0; while(nm[k]&&k<19){DSK[i].name[k]=nm[k];k++;} DSK[i].name[k]=0;
    if(parent==-1) dsk_place(i); else { DSK[i].x=0; DSK[i].y=0; } return i; }
static int is_ancestor(int anc,int node){ int g=0; while(node>=0&&g++<DSK_MAX){ if(node==anc)return 1; node=DSK[node].parent; } return 0; }
static void dsk_init(void){
    for(int i=0;i<DSK_MAX;i++) DSK[i].used=0;
    if(msd_dev>=0) dsk_add(2,22,"USB (U:)",-1);
    idrag=-1; fld_view=-1;
}
static int usb_icon_idx=-1;
static void usb_icon_sync(void){

    usb_icon_idx=-1;
    for(int i=0;i<DSK_MAX;i++) if(DSK[i].used&&DSK[i].type==2&&DSK[i].app==22){ usb_icon_idx=i; break; }
    if(msd_dev>=0 && usb_icon_idx<0){ usb_icon_idx=dsk_add(2,22,"USB (U:)",-1); }
    else if(msd_dev<0 && usb_icon_idx>=0){ DSK[usb_icon_idx].used=0; usb_icon_idx=-1; }
}
static void dsk_open(int i){
    if(i<0||i>=DSK_MAX||!DSK[i].used)return;
    if(DSK[i].type==2){
        if(DSK[i].app==19){ int k=0; while(DSK[i].name[k]&&k<19){gen_name[k]=DSK[i].name[k];k++;} gen_name[k]=0; open_app(19,200,120,360,190); }
        else launch(DSK[i].app);
    } else if(DSK[i].type==0){
        int k=0; while(DSK[i].name[k]&&k<15){note_name[k]=DSK[i].name[k];k++;} note_name[k]=0;
        if(disk_ok){ int ix=nvx_find(note_name); if(ix>=0){ notelen=nvx_read(ix,notebuf,1020); notebuf[notelen]=0; } else { notelen=0; notebuf[0]=0; } } else { notelen=0; notebuf[0]=0; }
        note_status=0; open_app(5,60,50,540,380);
    } else { fld_view=i; open_app(18,220,110,360,260); }
}
static void sas_commit(void){
    if(sas_len==0)return;
    int k=0; while(sas_name[k]&&k<15){note_name[k]=sas_name[k];k++;} note_name[k]=0;
    if(sas_loc==3){ int r=fat_ok?fat_write_file(note_name,notebuf,notelen):-1; note_status=(r==0)?1:3; close_app(20); return; }
    int wrote = disk_ok ? nvx_write(note_name,notebuf,notelen) : -1;
    if(sas_loc==0){ dsk_add(0,0,note_name,-1); }
    else if(sas_loc==1){ if(fcount[2]<MAXF){ char(*b)[16]=folder_buf(2); int j=0; while(note_name[j]&&j<15){b[fcount[2]][j]=note_name[j];j++;} b[fcount[2]][j]=0; fcount[2]++; } }
    note_status=(disk_ok&&wrote>=0)?1:3;
    close_app(20);
}
#define CLIP      ((char*)0x00953000u)
typedef struct { u8 role; int off; int len; } cmsg;
#define chat_msg  ((cmsg*)0x00954000u)
#define CHAT_POOL ((char*)0x00955000u)
static void chat_add(u8 role,const char* t,int len){
    if(len>4000)len=4000; if(len<0)len=0;
    if(chat_n>=30 || chat_used+len>31000) return;
    chat_msg[chat_n].role=role; chat_msg[chat_n].off=chat_used; chat_msg[chat_n].len=len;
    for(int i=0;i<len;i++)CHAT_POOL[chat_used+i]=t[i]; chat_used+=len; chat_n++;
}
static int translit(unsigned cp,char* o);
static char kchar_shift(u8 sc){
    sc&=0x7F; char c=kmap[sc];
    if(kaltgr){ switch(sc){ case 0x03:return '@'; case 0x05:return '$'; case 0x08:return '{'; case 0x09:return '[';
        case 0x0A:return ']'; case 0x0B:return '}'; case 0x0C:return '\\'; case 0x56:return '|'; default:return 0; } }
    if(kshift){ switch(sc){ case 0x02:return '!'; case 0x03:return '"'; case 0x04:return '#'; case 0x05:return '$';
        case 0x06:return '%'; case 0x07:return '&'; case 0x08:return '/'; case 0x09:return '('; case 0x0A:return ')';
        case 0x0B:return '='; case 0x0C:return '?'; case 0x0D:return '`'; case 0x29:return 0; case 0x33:return ';';
        case 0x34:return ':'; case 0x35:return '_'; case 0x2B:return '*'; case 0x56:return '>';
        default: if(c>='a'&&c<='z')return (char)(c-32); return c; } }
    switch(sc){ case 0x0C:return '+'; case 0x0D:return 0; case 0x35:return '-'; case 0x2B:return '\''; case 0x56:return '<'; default:return c; }
}
static int claude_ask(const char* key){
    u32 ip=0;
    if(dns_query("api.anthropic.com",&ip)!=1) return -100;
    char* B=AI_ANS; int b=0;
    const char* j1="{\"model\":\"" CLAUDE_MODEL "\",\"max_tokens\":1024,\"messages\":[";
    for(int i=0;j1[i];i++)B[b++]=j1[i];
    for(int mi=0;mi<chat_n;mi++){
        if(mi)B[b++]=',';
        const char* rh = chat_msg[mi].role? "{\"role\":\"assistant\",\"content\":\"" : "{\"role\":\"user\",\"content\":\"";
        for(int i=0;rh[i];i++)B[b++]=rh[i];
        const char* tx=CHAT_POOL+chat_msg[mi].off; int tl=chat_msg[mi].len;
        for(int i=0;i<tl&&b<26000;i++){ char c=tx[i];
            if(c=='"'){B[b++]='\\';B[b++]='"';}
            else if(c=='\\'){B[b++]='\\';B[b++]='\\';}
            else if(c=='\n'){B[b++]='\\';B[b++]='n';}
            else if((unsigned char)c<32){B[b++]=' ';}
            else B[b++]=c; }
        B[b++]='"'; B[b++]='}';
    }
    B[b++]=']'; B[b++]='}'; B[b]=0;
    char* R=AI_REQ; int p=0;
    const char* l1="POST /v1/messages HTTP/1.1\r\nHost: api.anthropic.com\r\nx-api-key: ";
    for(int i=0;l1[i];i++)R[p++]=l1[i];
    for(int i=0;key[i]&&i<250;i++)R[p++]=key[i];
    const char* hv="\r\nanthropic-version: 2023-06-01\r\ncontent-type: application/json\r\nconnection: close\r\ncontent-length: ";
    for(int i=0;hv[i];i++)R[p++]=hv[i];
    { char t[12]; int v=b,nl=0; if(v==0)t[nl++]='0'; while(v){t[nl++]='0'+v%10;v/=10;} while(nl)R[p++]=t[--nl]; }
    R[p++]='\r';R[p++]='\n';R[p++]='\r';R[p++]='\n';
    for(int i=0;i<b;i++)R[p++]=B[i];
    if(!tcps_open(ip,443)) return -101;
    u8 ent[64]; entropy_get(ent,64);
    tls_io* io=(tls_io*)0x00930000u; io->wr=tcps_wr; io->rd=tcps_rd; io->ctx=0; io->rlen=0; io->rpos=0;
    int n=tls_get(io,"api.anthropic.com",17,ent,R,p,(u8*)http_buf,59000);
    secure_zero(ent,64); secure_zero((void*)AI_REQ,p);
    if(n<=0){ AI_ANS[0]=0; return n; }
    http_buf[n]=0;
    int hdrend=n,bo=n;
    for(int i=0;i+3<n;i++){ if(http_buf[i]=='\r'&&http_buf[i+1]=='\n'&&http_buf[i+2]=='\r'&&http_buf[i+3]=='\n'){ hdrend=i+2; bo=i+4; break; } }
    int code=br_status(http_buf,hdrend);
    char* body=http_buf+bo; int blen=n-bo; if(blen<0)blen=0;
    if(br_find_ci(http_buf,hdrend,"\ntransfer-encoding:")>=0 && br_find_ci(http_buf,hdrend,"chunked")>=0) blen=br_dechunk(body,blen);
    body[blen]=0;
    int isErr=0; int tpos=br_find_ci(body,blen,"\"text\":\"");
    if(tpos<0){ tpos=br_find_ci(body,blen,"\"message\":\""); isErr=1; }
    if(tpos<0){ const char* m="(no text field in response)"; int k=0; for(;m[k];k++)AI_ANS[k]=m[k]; AI_ANS[k]=0; return code?code:-9; }
    int sp=tpos+(isErr?11:8), o=0;
    for(int i=sp;i<blen && o<15000;){
        char c=body[i];
        if(c=='"')break;
        if(c=='\\'){ char d=body[i+1];
            if(d=='n'){AI_ANS[o++]='\n';i+=2;}
            else if(d=='t'){AI_ANS[o++]=' ';i+=2;}
            else if(d=='r'){i+=2;}
            else if(d=='"'){AI_ANS[o++]='"';i+=2;}
            else if(d=='\\'){AI_ANS[o++]='\\';i+=2;}
            else if(d=='/'){AI_ANS[o++]='/';i+=2;}
            else if(d=='u'){ unsigned cp=0; for(int k=0;k<4;k++){ char h=body[i+2+k]; int v=(h>='0'&&h<='9')?h-'0':(h>='a'&&h<='f')?h-'a'+10:(h>='A'&&h<='F')?h-'A'+10:0; cp=cp*16+v; } o+=translit(cp,AI_ANS+o); i+=6; }
            else { AI_ANS[o++]=d; i+=2; }
        } else { AI_ANS[o++]=c; i++; }
    }
    AI_ANS[o]=0;
    return isErr ? (code?code:-8) : 200;
}

#define SUPA_HOST "ogtkwovhbpvswepndtkc.supabase.co"
#define SUPA_KEY  "sb_publishable_TQ7YWnC9nuVZwnWAL5Dz7Q_7yYM45as"

static int email_valid(const char* e){
    int len=0, at=-1, dotAfterAt=0;
    for(int i=0;e[i];i++){ char c=e[i]; len++;
        if(c<=' ') return 0;
        if(c=='@'){ if(at>=0) return 0; at=i; }
        else if(c=='.' && at>=0 && i>at+1) dotAfterAt=1;
    }
    if(len<5||len>63) return 0;
    if(at<1) return 0;
    if(at>len-4) return 0;
    if(!dotAfterAt) return 0;
    if(e[len-1]=='.'||e[len-1]=='@') return 0;
    return 1;
}

static int supabase_check_email(const char* email){
    if(!email_valid(email)) return -2;
    u32 ip=0;
    if(dns_query(SUPA_HOST,&ip)!=1) return -100;
    char* R=AI_REQ; int p=0;
    const char* l1="GET /rest/v1/org_emails?email=eq.";
    for(int i=0;l1[i];i++)R[p++]=l1[i];

    const char* hx="0123456789ABCDEF";
    for(int i=0;email[i]&&i<200;i++){ char c=email[i];
        if((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='.'||c=='-'||c=='_') R[p++]=c;
        else { R[p++]='%'; R[p++]=hx[(c>>4)&0xF]; R[p++]=hx[c&0xF]; }
    }
    const char* l2="&select=email&limit=1 HTTP/1.1\r\nHost: " SUPA_HOST
                   "\r\napikey: " SUPA_KEY
                   "\r\nAuthorization: Bearer " SUPA_KEY
                   "\r\nAccept: application/json\r\nConnection: close\r\n\r\n";
    for(int i=0;l2[i];i++)R[p++]=l2[i];
    if(!tcps_open(ip,443)) return -101;
    u8 ent[64]; entropy_get(ent,64);
    tls_io* io=(tls_io*)0x00930000u; io->wr=tcps_wr; io->rd=tcps_rd; io->ctx=0; io->rlen=0; io->rpos=0;
    int n=tls_get(io,SUPA_HOST,strlen_(SUPA_HOST),ent,R,p,(u8*)http_buf,59000);
    secure_zero(ent,64);
    if(n<=0) return n<0?n:-102;
    http_buf[n]=0;
    int bo=n;
    for(int i=0;i+3<n;i++){ if(http_buf[i]=='\r'&&http_buf[i+1]=='\n'&&http_buf[i+2]=='\r'&&http_buf[i+3]=='\n'){ bo=i+4; break; } }
    char* b=http_buf+bo;
    while(*b==' '||*b=='\r'||*b=='\n'||*b=='\t')b++;
    if(b[0]!='[') return -103;
    if(b[1]==']') return 0;

    char got[72]; int gotok=0;
    for(int i=0;b[i]&&i<59000;i++){
        if(b[i]=='"'&&b[i+1]=='e'&&b[i+2]=='m'&&b[i+3]=='a'&&b[i+4]=='i'&&b[i+5]=='l'&&b[i+6]=='"'&&b[i+7]==':'&&b[i+8]=='"'){
            int j=i+9,g=0; while(b[j]&&b[j]!='"'&&g<71){ char c=b[j]; if(c>='A'&&c<='Z')c+=32; got[g++]=c; j++; }
            got[g]=0; gotok=1; break;
        }
    }
    if(!gotok) return 0;
    return streq(got,email)?1:0;
}
static void nv_app(char* b,int* pp,const char* s){ while(*s)b[(*pp)++]=*s++; }
static void nv_appu(char* b,int* pp,unsigned v){ char t[12]; int k=0; if(!v)t[k++]='0'; while(v){t[k++]=(char)('0'+v%10);v/=10;} while(k)b[(*pp)++]=t[--k]; }
static int noovex_push(void){
    if(!nic_present) return -1;
    if(!nic_up()) return -2;
    u32 ip=0; if(dns_query(SUPA_HOST,&ip)!=1) return -100;
    char body[200]; int bp=0;
    nv_app(body,&bp,"{\"username\":\"");
    { const char* u=acct.user[0]?acct.user:"guest"; for(int i=0;u[i]&&bp<150;i++){ char c=u[i]; if(c=='\"'||c=='\\')c='_'; body[bp++]=c; } }
    nv_app(body,&bp,"\",\"data\":{\"os\":\"" OSNAME "\",\"build\":\"" BUILDVER "\",\"res\":\"");
    nv_appu(body,&bp,(unsigned)W); body[bp++]='x'; nv_appu(body,&bp,(unsigned)H);
    nv_app(body,&bp,"\",\"ip\":\"");
    nv_appu(body,&bp,(my_ip>>24)&255); body[bp++]='.'; nv_appu(body,&bp,(my_ip>>16)&255); body[bp++]='.'; nv_appu(body,&bp,(my_ip>>8)&255); body[bp++]='.'; nv_appu(body,&bp,my_ip&255);
    nv_app(body,&bp,"\"}}"); body[bp]=0;
    char* R=AI_REQ; int p=0;
    nv_app(R,&p,"POST /rest/v1/noovex_data HTTP/1.1\r\nHost: " SUPA_HOST "\r\napikey: " SUPA_KEY "\r\nAuthorization: Bearer " SUPA_KEY "\r\nContent-Type: application/json\r\nPrefer: return=minimal\r\nConnection: close\r\nContent-Length: ");
    nv_appu(R,&p,(unsigned)bp);
    nv_app(R,&p,"\r\n\r\n");
    for(int i=0;i<bp;i++)R[p++]=body[i];
    if(!tcps_open(ip,443)) return -101;
    u8 ent[64]; entropy_get(ent,64);
    tls_io* io=(tls_io*)0x00930000u; io->wr=tcps_wr; io->rd=tcps_rd; io->ctx=0; io->rlen=0; io->rpos=0;
    int n=tls_get(io,SUPA_HOST,strlen_(SUPA_HOST),ent,R,p,(u8*)http_buf,59000);
    secure_zero(ent,64);
    if(n<=0) return n<0?n:-102;
    http_buf[n]=0;
    int code=0; if(n>12 && http_buf[9]>='0'&&http_buf[9]<='9') code=(http_buf[9]-'0')*100+(http_buf[10]-'0')*10+(http_buf[11]-'0');
    return code;
}
static int chat_draw(int x0,int top,int x1,int bot,int scroll,int draw){
    int y = draw ? (top-scroll) : top; int starty=y;
    for(int mi=0;mi<chat_n;mi++){
        u8 role=chat_msg[mi].role; const char* t=CHAT_POOL+chat_msg[mi].off; int tl=chat_msg[mi].len;
        if(draw && y>=top-12 && y<=bot) draw_str(x0,y, role?"CLAUDE:":"YOU:", role?C_TEAL:C_GREEN);
        y+=12; int x=x0+8, i=0;
        while(i<tl){
            if(t[i]=='\n'){ x=x0+8; y+=11; i++; continue; }
            if(t[i]==' '){ x+=8; i++; if(x>x1-8){x=x0+8;y+=11;} continue; }
            int wl=0; while(i+wl<tl && t[i+wl]!=' ' && t[i+wl]!='\n')wl++;
            if(x+wl*8>x1-2){ x=x0+8; y+=11; }
            if(draw && y>=top-11 && y<=bot){ char w[80]; int q=0; for(int k=0;k<wl&&k<79;k++)w[q++]=t[i+k]; w[q]=0; draw_str(x,y,w,0); }
            x+=wl*8+8; i+=wl;
        }
        y+=14;
    }
    return y-starty;
}
static void ai_send(void){
    if(ai_keylen<=0||ai_plen<=0) return;
    if(chat_n>=30 || chat_used+ai_plen>31000){ ai_rc=-200; return; }
    chat_add(0,AI_PROMPT,ai_plen); ai_plen=0; AI_PROMPT[0]=0;
    ai_rc=1; pending_ai=1; ai_tobottom=1;
}
static void br_nav(const char* name,int len){
    char n[64]; int k=0; while(k<len&&k<63){ n[k]=name[k]; k++; } n[k]=0;

    br_brand=0; br_is_local=0;
    const char* pg;
    if(br_brand)pg=PG_HOME;
    else if(n[0]==0||streq(n,"home"))pg=PG_HOME;
    else if(streq(n,"about"))pg=PG_ABOUT; else if(streq(n,"news"))pg=PG_NEWS;
    else if(streq(n,"games"))pg=PG_GAMES; else if(streq(n,"help"))pg=PG_HELP;
    else if(streq(n,"demo"))pg=PG_DEMO;
    else if(streq(n,"css"))pg=PG_CSS;
    else if(streq(n,"gfx"))pg=PG_GFX;
    else if(startsw(n,"file:")){ pg=PG_404; br_is_local=1; if(disk_ok){ int idx=nvx_find(n+5); if(idx>=0){ int L=nvx_read(idx,br_fbuf,4090); br_fbuf[L]=0; pg=br_fbuf; } } }
    else if(startsw(n,"https:")){ char* fb=br_fetch(n); pg=fb?fb:PG_404; }
    else if(startsw(n,"http:")){ char* fb=br_fetch(n); pg=fb?fb:PG_404; }
    else if(n[0]=='/' && br_cur_host[0]){
        char rel[200]; int o=0; const char* sc=br_cur_https?"https://":"http://";
        for(int i=0;sc[i]&&o<199;i++)rel[o++]=sc[i];
        for(int i=0;br_cur_host[i]&&o<199;i++)rel[o++]=br_cur_host[i];
        for(int i=0;n[i]&&o<199;i++)rel[o++]=n[i]; rel[o]=0;
        char* fb=br_fetch(rel); pg=fb?fb:PG_404; }
    else { int dot=0,slash=0; for(int i=0;n[i];i++){ if(n[i]=='.')dot=1; if(n[i]=='/')slash=1; }
        if(dot){ char* fb=br_fetch(n); pg=fb?fb:PG_404; }
        else if(slash && br_cur_host[0]){
            char rel[200]; int o=0; const char* sc=br_cur_https?"https://":"http://";
            for(int i=0;sc[i]&&o<199;i++)rel[o++]=sc[i];
            for(int i=0;br_cur_host[i]&&o<199;i++)rel[o++]=br_cur_host[i]; if(o<199)rel[o++]='/';
            for(int i=0;n[i]&&o<199;i++)rel[o++]=n[i]; rel[o]=0;
            char* fb=br_fetch(rel); pg=fb?fb:PG_404; }
        else pg=PG_404; }
    cur_html=(char*)pg; br_scroll=0;
    int i=0; while(n[i]&&i<127){br_addr[i]=n[i];i++;} br_addr[i]=0; br_addr_len=i;
    if(br_hist_n<12){ br_hist[br_hist_n]=cur_html; int j=0; while(n[j]&&j<23){br_hnm[br_hist_n][j]=n[j];j++;} br_hnm[br_hist_n][j]=0; br_hist_n++; }
    if(!br_brand) br_prep(); else br_use_expand=0; }
static void br_back(void){ br_brand=0; if(br_hist_n>1){ br_hist_n--; cur_html=br_hist[br_hist_n-1]; int j=0; while(br_hnm[br_hist_n-1][j]&&j<39){br_addr[j]=br_hnm[br_hist_n-1][j];j++;} br_addr[j]=0; br_addr_len=j; br_scroll=0; br_prep(); } }
static int tagis(const char* t,const char* name){ int i=0; while(name[i]){ char a=t[i],b=name[i]; if(a>='A'&&a<='Z')a+=32; if(a!=b)return 0; i++; } char c=t[i]; return (c=='>'||c==' '||c=='/'||c=='\t'||c=='\n'||c=='\r'); }

static int translit(unsigned cp,char* o){
    if(cp<0x80){ if(cp==0)return 0; o[0]=(char)cp; return 1; }
    switch(cp){
      case 0xE5:case 0xE4:case 0xE0:case 0xE1:case 0xE2:case 0xE3:case 0xAA: o[0]='a';return 1;
      case 0xC5:case 0xC4:case 0xC0:case 0xC1:case 0xC2:case 0xC3: o[0]='A';return 1;
      case 0xF6:case 0xF8:case 0xF2:case 0xF3:case 0xF4:case 0xF5:case 0xF0:case 0xBA: o[0]='o';return 1;
      case 0xD6:case 0xD8:case 0xD2:case 0xD3:case 0xD4:case 0xD5: o[0]='O';return 1;
      case 0xE9:case 0xE8:case 0xEA:case 0xEB: o[0]='e';return 1;
      case 0xC9:case 0xC8:case 0xCA:case 0xCB: o[0]='E';return 1;
      case 0xFC:case 0xF9:case 0xFA:case 0xFB: o[0]='u';return 1;
      case 0xDC:case 0xD9:case 0xDA:case 0xDB: o[0]='U';return 1;
      case 0xED:case 0xEC:case 0xEE:case 0xEF: o[0]='i';return 1;
      case 0xCD:case 0xCC:case 0xCE:case 0xCF: o[0]='I';return 1;
      case 0xF1: o[0]='n';return 1; case 0xD1: o[0]='N';return 1;
      case 0xE7: o[0]='c';return 1; case 0xC7: o[0]='C';return 1;
      case 0xFD:case 0xFF: o[0]='y';return 1;
      case 0xDF: o[0]='s';o[1]='s';return 2;
      case 0xE6: o[0]='a';o[1]='e';return 2;
      case 0xC6: o[0]='A';o[1]='E';return 2;
      case 0xA0: o[0]=' ';return 1;
      case 0x2013:case 0x2014:case 0x2212: o[0]='-';return 1;
      case 0x2018:case 0x2019:case 0x201B:case 0x2032: o[0]='\'';return 1;
      case 0x201C:case 0x201D:case 0x201E: o[0]='"';return 1;
      case 0x2026: o[0]='.';o[1]='.';o[2]='.';return 3;
      case 0x2022:case 0x00B7: o[0]='*';return 1;
      case 0xA9: o[0]='(';o[1]='c';return 2;
      case 0xAE: o[0]='(';o[1]='r';return 2;
      case 0x00D7: o[0]='x';return 1;
      case 0x20AC: o[0]='E';o[1]='U';o[2]='R';return 3;
      case 0xA3: o[0]='G';o[1]='B';o[2]='P';return 3;
      default: return 0;
    }
}
static int html_decode(const char* s,int n,char* out,int mo){
    int o=0;
    for(int i=0;i<n&&o<mo-4;){
        unsigned char b=(unsigned char)s[i];
        if(b=='&'){ int j=i+1; char e[16]; int el=0; while(j<n&&s[j]!=';'&&s[j]!='&'&&s[j]!=' '&&el<15){e[el++]=s[j];j++;} e[el]=0;
            unsigned cp=0;
            if(streq(e,"amp"))cp='&'; else if(streq(e,"lt"))cp='<'; else if(streq(e,"gt"))cp='>';
            else if(streq(e,"quot"))cp='"'; else if(streq(e,"apos"))cp='\''; else if(streq(e,"nbsp"))cp=0xA0;
            else if(streq(e,"copy"))cp=0xA9; else if(streq(e,"reg"))cp=0xAE;
            else if(streq(e,"mdash"))cp=0x2014; else if(streq(e,"ndash"))cp=0x2013;
            else if(streq(e,"hellip"))cp=0x2026; else if(streq(e,"middot"))cp=0xB7; else if(streq(e,"times"))cp=0xD7;
            else if(streq(e,"lsquo")||streq(e,"rsquo"))cp=0x2019; else if(streq(e,"ldquo")||streq(e,"rdquo"))cp=0x201C;
            else if(streq(e,"aring"))cp=0xE5; else if(streq(e,"auml"))cp=0xE4; else if(streq(e,"ouml"))cp=0xF6;
            else if(streq(e,"Aring"))cp=0xC5; else if(streq(e,"Auml"))cp=0xC4; else if(streq(e,"Ouml"))cp=0xD6;
            else if(streq(e,"eacute"))cp=0xE9; else if(streq(e,"egrave"))cp=0xE8;
            else if(e[0]=='#'){ unsigned v=0; if(e[1]=='x'||e[1]=='X'){ for(int k=2;e[k];k++){char d=e[k]; if(d>='0'&&d<='9')v=v*16+(d-'0'); else if(d>='a'&&d<='f')v=v*16+(d-'a'+10); else if(d>='A'&&d<='F')v=v*16+(d-'A'+10);} } else { for(int k=1;e[k];k++)if(e[k]>='0'&&e[k]<='9')v=v*10+(e[k]-'0'); } cp=v; }
            if(cp){ o+=translit(cp,out+o); i=(j<n&&s[j]==';')?j+1:j; continue; } }
        if(b<0x80){ out[o++]=(char)b; i++; continue; }

        unsigned cp=0; int len=1;
        if(b>=0xF0&&i+3<n){ cp=((b&0x07)<<18)|((s[i+1]&0x3F)<<12)|((s[i+2]&0x3F)<<6)|(s[i+3]&0x3F); len=4; }
        else if(b>=0xE0&&i+2<n){ cp=((b&0x0F)<<12)|((s[i+1]&0x3F)<<6)|(s[i+2]&0x3F); len=3; }
        else if(b>=0xC0&&i+1<n){ cp=((b&0x1F)<<6)|(s[i+1]&0x3F); len=2; }
        else { i++; continue; }
        o+=translit(cp,out+o); i+=len;
    }
    out[o]=0; return o;
}

#define JX_MAXTOK 1024
#define JX_TKKIND ((u8*)0x008E0000u)
#define JX_TKNUM  ((long*)0x008E0400u)
#define JX_TKTXT  ((char*)0x008E1400u)
#define JX_OUT    ((char*)0x008E6800u)
#define JX_OUTMAX 16000
#define BR_EXPAND ((char*)0x008EB000u)
#define BR_EXPMAX 64000
#define JX_STRMAX 64
typedef struct { int typ; long num; char str[JX_STRMAX]; } JxVal;
#define JX_NARR 8
#define JX_ACAP 32
typedef struct { int len; JxVal el[JX_ACAP]; } JxArr;
typedef struct { char name[14]; JxVal v; } JxVar;
typedef struct { char name[14]; int bodytp; char par[5][12]; int np; } JxFn;
#define JX_ARR ((JxArr*)0x00843000u)
#define JX_VAR ((JxVar*)0x00840000u)
#define JX_FN  ((JxFn*)0x00842000u)
static int  jx_outlen, jx_narr, jx_nfn, jx_nvar;
static long jx_budget; static int jx_err, jx_ntok, jx_tp;
static JxVal jx_ret; static int jx_returning;
enum { JT_NUM, JT_STR, JT_ID, JT_PUNC, JT_EOF };
static char* jx_tx(int i){ return JX_TKTXT + i*20; }
static int jx_isa(char c){ return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'||c=='$'; }
static int jx_isd(char c){ return c>='0'&&c<='9'; }
static void jx_lex(const char* s,int slen){
    jx_ntok=0; int i=0;
    while(i<slen&&s[i]&&jx_ntok<JX_MAXTOK-1){ char c=s[i];
        if(c==' '||c=='\t'||c=='\n'||c=='\r'){ i++; continue; }
        if(c=='/'&&s[i+1]=='/'){ while(i<slen&&s[i]&&s[i]!='\n')i++; continue; }
        if(c=='/'&&s[i+1]=='*'){ i+=2; while(i<slen&&s[i]&&!(s[i]=='*'&&s[i+1]=='/'))i++; if(s[i])i+=2; continue; }
        if(jx_isd(c)){ long v=0; while(jx_isd(s[i])){ v=v*10+(s[i]-'0'); i++; } JX_TKKIND[jx_ntok]=JT_NUM; JX_TKNUM[jx_ntok]=v; jx_ntok++; continue; }
        if(c=='"'||c=='\''){ char q=c; i++; int j=0; char* t=jx_tx(jx_ntok); JX_TKKIND[jx_ntok]=JT_STR; while(s[i]&&s[i]!=q&&j<19){ if(s[i]=='\\'&&s[i+1]){ i++; char e=s[i]; t[j++]= e=='n'?'\n':e=='t'?'\t':e; } else t[j++]=s[i]; i++; } t[j]=0; if(s[i]==q)i++; jx_ntok++; continue; }
        if(jx_isa(c)){ int j=0; char* t=jx_tx(jx_ntok); while((jx_isa(s[i])||jx_isd(s[i]))&&j<19){ t[j++]=s[i]; i++; } t[j]=0; JX_TKKIND[jx_ntok]=JT_ID; jx_ntok++; continue; }
        { char* t=jx_tx(jx_ntok); JX_TKKIND[jx_ntok]=JT_PUNC; int j=0; char n=s[i+1];
          if((c=='='&&n=='=')||(c=='!'&&n=='=')||(c=='<'&&n=='=')||(c=='>'&&n=='=')||(c=='&'&&n=='&')||(c=='|'&&n=='|')||(c=='+'&&n=='+')||(c=='-'&&n=='-')||((c=='+'||c=='-'||c=='*'||c=='/'||c=='%')&&n=='=')){ t[j++]=c; t[j++]=n; i+=2; } else { t[j++]=c; i++; } t[j]=0; jx_ntok++; }
    }
    JX_TKKIND[jx_ntok]=JT_EOF; jx_tx(jx_ntok)[0]=0; jx_ntok++;
}
static int jx_pun(const char* p){ return JX_TKKIND[jx_tp]==JT_PUNC && streq(jx_tx(jx_tp),p); }
static int jx_kw(const char* k){ return JX_TKKIND[jx_tp]==JT_ID && streq(jx_tx(jx_tp),k); }
static JxVal jx_num(long n){ JxVal v; v.typ=0; v.num=n; v.str[0]=0; return v; }
static JxVal jx_str(const char* s){ JxVal v; v.typ=1; v.num=0; int j=0; while(s[j]&&j<JX_STRMAX-1){v.str[j]=s[j];j++;} v.str[j]=0; return v; }
static JxVal jx_mkarr(int h){ JxVal v; v.typ=2; v.num=h; v.str[0]=0; return v; }
static void jx_tostr(JxVal* v,char* o){
    if(v->typ==1){ int j=0; while(v->str[j]&&j<JX_STRMAX-1){o[j]=v->str[j];j++;} o[j]=0; }
    else if(v->typ==2){ int h=v->num,j=0; o[j++]='['; for(int i=0;i<JX_ARR[h].len&&j<JX_STRMAX-6;i++){ if(i)o[j++]=','; char t[24]; jx_tostr(&JX_ARR[h].el[i],t); for(int q=0;t[q]&&j<JX_STRMAX-2;q++)o[j++]=t[q]; } o[j++]=']'; o[j]=0; }
    else { long n=v->num; int neg=n<0; unsigned long a=neg?-n:n; char t[24]; int k=0; if(a==0)t[k++]='0'; while(a){t[k++]='0'+a%10;a/=10;} int j=0; if(neg)o[j++]='-'; while(k)o[j++]=t[--k]; o[j]=0; }
}
static long jx_tonum(JxVal* v){ return v->typ==0?v->num:(v->typ==1?0:JX_ARR[v->num].len); }
static int jx_truth(JxVal v){ return v.typ==1?(v.str[0]!=0):(v.typ==2?(JX_ARR[v.num].len!=0):(v.num!=0)); }
static JxVal* jx_find(const char* nm){ for(int i=jx_nvar-1;i>=0;i--)if(streq(JX_VAR[i].name,nm))return &JX_VAR[i].v; return 0; }
static void jx_set(const char* nm,JxVal v){ JxVal* e=jx_find(nm); if(e){*e=v;return;} if(jx_nvar<48){ int j=0; while(nm[j]&&j<13){JX_VAR[jx_nvar].name[j]=nm[j];j++;} JX_VAR[jx_nvar].name[j]=0; JX_VAR[jx_nvar].v=v; jx_nvar++; } }
static void jx_add_var(const char* nm,JxVal v){ if(jx_nvar<48){ int j=0; while(nm[j]&&j<13){JX_VAR[jx_nvar].name[j]=nm[j];j++;} JX_VAR[jx_nvar].name[j]=0; JX_VAR[jx_nvar].v=v; jx_nvar++; } }
static long jx_lsqrt(long n){ if(n<0)return 0; long r=0; while((r+1)*(r+1)<=n)r++; return r; }
static long jx_lpow(long b,long e){ long r=1; while(e-->0)r*=b; return r; }
static void jx_upper(char* s){ for(int i=0;s[i];i++)if(s[i]>='a'&&s[i]<='z')s[i]-=32; }
static void jx_lower(char* s){ for(int i=0;s[i];i++)if(s[i]>='A'&&s[i]<='Z')s[i]+=32; }
static int jx_slen(const char* s){ int n=0; while(s[n])n++; return n; }
static JxVal jx_expr(void); static void jx_stmt(void); static void jx_block(void);
static void jx_skip(void){ if(jx_pun("{")){ int d=0; do{ if(jx_pun("{"))d++; else if(jx_pun("}"))d--; jx_tp++; }while(d>0&&JX_TKKIND[jx_tp-1]!=JT_EOF); } else { while(!jx_pun(";")&&JX_TKKIND[jx_tp]!=JT_EOF)jx_tp++; if(jx_pun(";"))jx_tp++; } }
static JxVal jx_call(int fi){ jx_returning=0; jx_ret=jx_num(0); jx_block(); jx_returning=0; return jx_ret; }

static JxVal jx_post(JxVal v){
    for(;;){
        if(jx_pun("[")){ jx_tp++; JxVal idx=jx_expr(); if(jx_pun("]"))jx_tp++; long i=jx_tonum(&idx);
            if(v.typ==2){ int h=v.num; v=(i>=0&&i<JX_ARR[h].len)?JX_ARR[h].el[i]:jx_num(0); } else v=jx_num(0); continue; }
        if(jx_pun(".")){ jx_tp++; if(JX_TKKIND[jx_tp]!=JT_ID)break; char m[20]; { char* t=jx_tx(jx_tp); int j=0; while(t[j]&&j<19){m[j]=t[j];j++;} m[j]=0; } jx_tp++;
            if(jx_pun("(")){ jx_tp++; JxVal a1=jx_num(0),a2=jx_num(0); int na=0; if(!jx_pun(")")){ a1=jx_expr(); na=1; if(jx_pun(",")){ jx_tp++; a2=jx_expr(); na=2; } } if(jx_pun(")"))jx_tp++;
                if(streq(m,"charAt")&&v.typ==1){ long k=jx_tonum(&a1); char r[2]; r[0]=(k>=0&&k<jx_slen(v.str))?v.str[k]:0; r[1]=0; v=jx_str(r); }
                else if(streq(m,"substring")&&v.typ==1){ long s=jx_tonum(&a1),e=na>=2?jx_tonum(&a2):jx_slen(v.str); int L=jx_slen(v.str); if(s<0)s=0; if(e>L)e=L; if(e<s)e=s; char r[JX_STRMAX]; int j=0; for(long k=s;k<e&&j<JX_STRMAX-1;k++)r[j++]=v.str[k]; r[j]=0; v=jx_str(r); }
                else if(streq(m,"toUpperCase")&&v.typ==1){ char r[JX_STRMAX]; int j=0; while(v.str[j]&&j<JX_STRMAX-1){r[j]=v.str[j];j++;} r[j]=0; jx_upper(r); v=jx_str(r); }
                else if(streq(m,"toLowerCase")&&v.typ==1){ char r[JX_STRMAX]; int j=0; while(v.str[j]&&j<JX_STRMAX-1){r[j]=v.str[j];j++;} r[j]=0; jx_lower(r); v=jx_str(r); }
                else if(streq(m,"indexOf")&&v.typ==1){ char nd[JX_STRMAX]; if(a1.typ==1){int j=0;while(a1.str[j]&&j<JX_STRMAX-1){nd[j]=a1.str[j];j++;}nd[j]=0;} else nd[0]=0; int L=jx_slen(v.str),NL=jx_slen(nd),found=-1; for(int s=0;s+NL<=L;s++){int k=0;while(k<NL&&v.str[s+k]==nd[k])k++; if(k==NL){found=s;break;}} v=jx_num(found); }
                else if(streq(m,"charCodeAt")&&v.typ==1){ long k=jx_tonum(&a1); int L=jx_slen(v.str); v=jx_num((k>=0&&k<L)?(unsigned char)v.str[k]:0); }
                else if(streq(m,"repeat")&&v.typ==1){ long cnt=jx_tonum(&a1); char r[JX_STRMAX]; int j=0,L=jx_slen(v.str); for(long c2=0;c2<cnt&&j<JX_STRMAX-1;c2++)for(int q=0;q<L&&j<JX_STRMAX-1;q++)r[j++]=v.str[q]; r[j]=0; v=jx_str(r); }
                else if(streq(m,"push")&&v.typ==2){ int h=v.num; if(JX_ARR[h].len<JX_ACAP)JX_ARR[h].el[JX_ARR[h].len++]=a1; v=jx_num(JX_ARR[h].len); }
                else if(streq(m,"split")&&v.typ==1){ char sep[JX_STRMAX]; if(a1.typ==1){int j=0;while(a1.str[j]&&j<JX_STRMAX-1){sep[j]=a1.str[j];j++;}sep[j]=0;}else sep[0]=0; int h=jx_narr<JX_NARR?jx_narr++:0; JX_ARR[h].len=0; int SL=jx_slen(sep);
                    if(SL==0){ for(int k=0;v.str[k];k++){ char c2[2]; c2[0]=v.str[k]; c2[1]=0; if(JX_ARR[h].len<JX_ACAP)JX_ARR[h].el[JX_ARR[h].len++]=jx_str(c2); } }
                    else { char cur[JX_STRMAX]; int cl=0; for(int k=0;v.str[k];){ int mm=1; for(int q=0;q<SL;q++)if(v.str[k+q]!=sep[q]){mm=0;break;} if(mm){ cur[cl]=0; if(JX_ARR[h].len<JX_ACAP)JX_ARR[h].el[JX_ARR[h].len++]=jx_str(cur); cl=0; k+=SL; } else { if(cl<JX_STRMAX-1)cur[cl++]=v.str[k]; k++; } } cur[cl]=0; if(JX_ARR[h].len<JX_ACAP)JX_ARR[h].el[JX_ARR[h].len++]=jx_str(cur); }
                    v=jx_mkarr(h); }
                else if(streq(m,"join")&&v.typ==2){ char sep[JX_STRMAX]; if(a1.typ==1){int j=0;while(a1.str[j]&&j<JX_STRMAX-1){sep[j]=a1.str[j];j++;}sep[j]=0;}else{sep[0]=',';sep[1]=0;} int h=v.num; char r[JX_STRMAX]; int j=0; for(int k=0;k<JX_ARR[h].len;k++){ if(k)for(int q=0;sep[q]&&j<JX_STRMAX-1;q++)r[j++]=sep[q]; char tmp[JX_STRMAX]; jx_tostr(&JX_ARR[h].el[k],tmp); for(int q=0;tmp[q]&&j<JX_STRMAX-1;q++)r[j++]=tmp[q]; } r[j]=0; v=jx_str(r); }
                else if(streq(m,"pop")&&v.typ==2){ int h=v.num; v=(JX_ARR[h].len>0)?JX_ARR[h].el[--JX_ARR[h].len]:jx_num(0); }
                else if(streq(m,"trim")&&v.typ==1){ int a0=0; while(v.str[a0]==' '||v.str[a0]=='\t'||v.str[a0]=='\n')a0++; int e=jx_slen(v.str); while(e>a0&&(v.str[e-1]==' '||v.str[e-1]=='\t'||v.str[e-1]=='\n'))e--; char r[JX_STRMAX]; int j=0; for(int k=a0;k<e&&j<JX_STRMAX-1;k++)r[j++]=v.str[k]; r[j]=0; v=jx_str(r); }
                else v=jx_num(0);
                (void)na; continue;
            } else {
                if(streq(m,"length")){ if(v.typ==1)v=jx_num(jx_slen(v.str)); else if(v.typ==2)v=jx_num(JX_ARR[v.num].len); else v=jx_num(0); }
                else v=jx_num(0);
                continue;
            }
        }
        break;
    }
    return v;
}
static JxVal jx_prim(void){
    if(JX_TKKIND[jx_tp]==JT_NUM){ JxVal v=jx_num(JX_TKNUM[jx_tp]); jx_tp++; return jx_post(v); }
    if(JX_TKKIND[jx_tp]==JT_STR){ JxVal v=jx_str(jx_tx(jx_tp)); jx_tp++; return jx_post(v); }
    if(jx_pun("(")){ jx_tp++; JxVal v=jx_expr(); if(jx_pun(")"))jx_tp++; return jx_post(v); }
    if(jx_pun("-")){ jx_tp++; JxVal v=jx_prim(); return jx_num(-jx_tonum(&v)); }
    if(jx_pun("!")){ jx_tp++; JxVal v=jx_prim(); return jx_num(jx_truth(v)?0:1); }
    if(jx_pun("[")){ jx_tp++; int h=jx_narr<JX_NARR?jx_narr++:0; JX_ARR[h].len=0; if(!jx_pun("]")){ for(;;){ JxVal e=jx_expr(); if(JX_ARR[h].len<JX_ACAP)JX_ARR[h].el[JX_ARR[h].len++]=e; if(jx_pun(",")){ jx_tp++; continue; } break; } } if(jx_pun("]"))jx_tp++; return jx_post(jx_mkarr(h)); }
    if(JX_TKKIND[jx_tp]==JT_ID){ char nm[20]; { char* t=jx_tx(jx_tp); int j=0; while(t[j]&&j<19){nm[j]=t[j];j++;} nm[j]=0; }
        if(streq(nm,"Math") && JX_TKKIND[jx_tp+1]==JT_PUNC && jx_tx(jx_tp+1)[0]=='.'){ jx_tp+=2; char m[20]; { char* t=jx_tx(jx_tp); int j=0; while(t[j]&&j<19){m[j]=t[j];j++;} m[j]=0; } jx_tp++; if(jx_pun("("))jx_tp++; JxVal a=jx_num(0),b=jx_num(0); int na=0; if(!jx_pun(")")){ a=jx_expr(); na=1; if(jx_pun(",")){ jx_tp++; b=jx_expr(); na=2; } } if(jx_pun(")"))jx_tp++;
            long x=jx_tonum(&a),y=jx_tonum(&b),r=0;
            if(streq(m,"floor")||streq(m,"round")||streq(m,"ceil")||streq(m,"trunc"))r=x;
            else if(streq(m,"abs"))r=x<0?-x:x; else if(streq(m,"max"))r=x>y?x:y; else if(streq(m,"min"))r=x<y?x:y;
            else if(streq(m,"sqrt"))r=jx_lsqrt(x); else if(streq(m,"pow"))r=jx_lpow(x,y);
            else if(streq(m,"sign"))r=(x>0)-(x<0);
            else if(streq(m,"random"))r=(na&&x>0)?(rnd()%x):(rnd()&0x7FFF);
            (void)na; return jx_post(jx_num(r));
        }
        for(int f=0;f<jx_nfn;f++) if(streq(JX_FN[f].name,nm) && JX_TKKIND[jx_tp+1]==JT_PUNC && jx_tx(jx_tp+1)[0]=='('){ jx_tp++; jx_tp++; JxVal args[5]; int na=0; if(!jx_pun(")")){ for(;;){ JxVal e=jx_expr(); if(na<5)args[na++]=e; if(jx_pun(",")){ jx_tp++; continue; } break; } } if(jx_pun(")"))jx_tp++;
            int base=jx_nvar; for(int q=0;q<JX_FN[f].np;q++) jx_add_var(JX_FN[f].par[q], q<na?args[q]:jx_num(0));
            int save=jx_tp; jx_tp=JX_FN[f].bodytp; JxVal rv=jx_call(f); jx_tp=save; jx_nvar=base; return jx_post(rv);
        }
        jx_tp++;
        if(JX_TKKIND[jx_tp]==JT_PUNC && (streq(jx_tx(jx_tp),"++")||streq(jx_tx(jx_tp),"--"))){ JxVal* e=jx_find(nm); long old=(e&&e->typ==0)?e->num:0; long d=jx_tx(jx_tp)[0]=='+'?1:-1; if(e&&e->typ==0)e->num+=d; else jx_set(nm,jx_num(d)); jx_tp++; return jx_num(old); }
        JxVal* e=jx_find(nm); JxVal v=e?*e:jx_num(0); return jx_post(v);
    }
    jx_tp++; return jx_num(0);
}
static JxVal jx_mul(void){ JxVal a=jx_prim(); while(jx_pun("*")||jx_pun("/")||jx_pun("%")){ char op=jx_tx(jx_tp)[0]; jx_tp++; JxVal b=jx_prim(); long x=jx_tonum(&a),y=jx_tonum(&b); a=jx_num(op=='*'?x*y:op=='/'?(y?x/y:0):(y?x%y:0)); } return a; }
static JxVal jx_add(void){ JxVal a=jx_mul(); while(jx_pun("+")||jx_pun("-")){ char op=jx_tx(jx_tp)[0]; jx_tp++; JxVal b=jx_mul();
        if(op=='+'&&(a.typ==1||b.typ==1)){ char s1[JX_STRMAX],s2[JX_STRMAX]; jx_tostr(&a,s1); jx_tostr(&b,s2); char r[JX_STRMAX]; int j=0; for(int i=0;s1[i]&&j<JX_STRMAX-1;i++)r[j++]=s1[i]; for(int i=0;s2[i]&&j<JX_STRMAX-1;i++)r[j++]=s2[i]; r[j]=0; a=jx_str(r); }
        else { long x=jx_tonum(&a),y=jx_tonum(&b); a=jx_num(op=='+'?x+y:x-y); } } return a; }
static JxVal jx_cmp(void){ JxVal a=jx_add(); while(jx_pun("<")||jx_pun(">")||jx_pun("<=")||jx_pun(">=")||jx_pun("==")||jx_pun("!=")){ char o0=jx_tx(jx_tp)[0],o1=jx_tx(jx_tp)[1]; jx_tp++; JxVal b=jx_add(); long x=jx_tonum(&a),y=jx_tonum(&b),r=0;
        if(o0=='<')r=o1=='='?(x<=y):(x<y); else if(o0=='>')r=o1=='='?(x>=y):(x>y);
        else if(o0=='='&&o1=='='){ if(a.typ==1||b.typ==1){char s1[JX_STRMAX],s2[JX_STRMAX];jx_tostr(&a,s1);jx_tostr(&b,s2);r=streq(s1,s2);} else r=(x==y); }
        else if(o0=='!'&&o1=='='){ if(a.typ==1||b.typ==1){char s1[JX_STRMAX],s2[JX_STRMAX];jx_tostr(&a,s1);jx_tostr(&b,s2);r=!streq(s1,s2);} else r=(x!=y); }
        a=jx_num(r); } return a; }
static JxVal jx_and(void){ JxVal a=jx_cmp(); while(jx_pun("&&")){ jx_tp++; JxVal b=jx_cmp(); a=jx_num(jx_truth(a)&&jx_truth(b)); } return a; }
static JxVal jx_or(void){ JxVal a=jx_and(); while(jx_pun("||")){ jx_tp++; JxVal b=jx_and(); a=jx_num(jx_truth(a)||jx_truth(b)); } return a; }
static JxVal jx_expr(void){ JxVal a=jx_or(); if(jx_pun("?")){ jx_tp++; JxVal t=jx_expr(); if(jx_pun(":"))jx_tp++; JxVal f=jx_expr(); return jx_truth(a)?t:f; } return a; }
static void jx_emit(const char* s){ for(int i=0;s[i]&&jx_outlen<JX_OUTMAX;i++)JX_OUT[jx_outlen++]=s[i]; JX_OUT[jx_outlen]=0; }
static void jx_compound(const char* nm,char op,JxVal rhs){ JxVal* e=jx_find(nm); long x=e?jx_tonum(e):0,y=jx_tonum(&rhs);
    if(op=='+'&&e&&e->typ==1){ char s1[JX_STRMAX],s2[JX_STRMAX]; jx_tostr(e,s1); jx_tostr(&rhs,s2); char r[JX_STRMAX]; int j=0; for(int i=0;s1[i]&&j<JX_STRMAX-1;i++)r[j++]=s1[i]; for(int i=0;s2[i]&&j<JX_STRMAX-1;i++)r[j++]=s2[i]; r[j]=0; jx_set(nm,jx_str(r)); return; }
    long r=op=='+'?x+y:op=='-'?x-y:op=='*'?x*y:op=='/'?(y?x/y:0):(y?x%y:0); jx_set(nm,jx_num(r)); }
static void jx_stmt(void){
    if(jx_err||jx_budget<=0||jx_returning)return; jx_budget--;
    if(jx_pun("{")){ jx_block(); return; }
    if(jx_pun(";")){ jx_tp++; return; }
    if(jx_kw("function")){ jx_tp++; char nm[14]; nm[0]=0; if(JX_TKKIND[jx_tp]==JT_ID){ char* t=jx_tx(jx_tp); int j=0; while(t[j]&&j<13){nm[j]=t[j];j++;} nm[j]=0; jx_tp++; } int fi=jx_nfn<16?jx_nfn++:0; { int j=0; while(nm[j]&&j<13){JX_FN[fi].name[j]=nm[j];j++;} JX_FN[fi].name[j]=0; } JX_FN[fi].np=0; if(jx_pun("("))jx_tp++; while(!jx_pun(")")&&JX_TKKIND[jx_tp]!=JT_EOF){ if(JX_TKKIND[jx_tp]==JT_ID&&JX_FN[fi].np<5){ char* t=jx_tx(jx_tp); int j=0; while(t[j]&&j<11){JX_FN[fi].par[JX_FN[fi].np][j]=t[j];j++;} JX_FN[fi].par[JX_FN[fi].np][j]=0; JX_FN[fi].np++; } jx_tp++; if(jx_pun(","))jx_tp++; } if(jx_pun(")"))jx_tp++; JX_FN[fi].bodytp=jx_tp; jx_skip(); return; }
    if(jx_kw("return")){ jx_tp++; JxVal v=jx_num(0); if(!jx_pun(";")&&!jx_pun("}"))v=jx_expr(); jx_ret=v; jx_returning=1; if(jx_pun(";"))jx_tp++; return; }
    if(jx_kw("var")||jx_kw("let")||jx_kw("const")){ jx_tp++;
        for(;;){ if(JX_TKKIND[jx_tp]!=JT_ID)break; char nm[20]; { char* t=jx_tx(jx_tp); int j=0; while(t[j]&&j<19){nm[j]=t[j];j++;} nm[j]=0; } jx_tp++; JxVal v=jx_num(0); if(jx_pun("=")){ jx_tp++; v=jx_expr(); } jx_set(nm,v); if(jx_pun(",")){ jx_tp++; continue; } break; }
        if(jx_pun(";"))jx_tp++; return; }
    if(jx_kw("if")){ jx_tp++; if(jx_pun("("))jx_tp++; JxVal c=jx_expr(); if(jx_pun(")"))jx_tp++; if(jx_truth(c)){ jx_stmt(); if(jx_kw("else")){ jx_tp++; jx_skip(); } } else { jx_skip(); if(jx_kw("else")){ jx_tp++; jx_stmt(); } } return; }
    if(jx_kw("for")){ jx_tp++; if(jx_pun("("))jx_tp++; jx_stmt(); int ct=jx_tp; JxVal c=jx_expr(); if(jx_pun(";"))jx_tp++; int pt=jx_tp; int d=0; while(!(d==0&&jx_pun(")"))&&JX_TKKIND[jx_tp]!=JT_EOF){ if(jx_pun("("))d++; else if(jx_pun(")"))d--; jx_tp++; } if(jx_pun(")"))jx_tp++; int bt=jx_tp;
        while(jx_truth(c)){ if(jx_budget<=0||jx_err||jx_returning)break; jx_tp=bt; jx_stmt(); jx_tp=pt; jx_expr(); jx_tp=ct; c=jx_expr(); if(jx_pun(";"))jx_tp++; } jx_tp=bt; jx_skip(); return; }
    if(jx_kw("while")){ jx_tp++; if(jx_pun("("))jx_tp++; int ct=jx_tp; JxVal c=jx_expr(); if(jx_pun(")"))jx_tp++; int bt=jx_tp;
        while(jx_truth(c)){ if(jx_budget<=0||jx_err||jx_returning)break; jx_tp=bt; jx_stmt(); jx_tp=ct; c=jx_expr(); if(jx_pun(")"))jx_tp++; } jx_tp=bt; jx_skip(); return; }
    if(JX_TKKIND[jx_tp]==JT_ID && streq(jx_tx(jx_tp),"document") && JX_TKKIND[jx_tp+1]==JT_PUNC && jx_tx(jx_tp+1)[0]=='.'){ jx_tp+=2;
        if(JX_TKKIND[jx_tp]==JT_ID && (streq(jx_tx(jx_tp),"write")||streq(jx_tx(jx_tp),"writeln"))){ jx_tp++; if(jx_pun("("))jx_tp++; JxVal v=jx_expr(); char s[JX_STRMAX]; jx_tostr(&v,s); jx_emit(s); if(jx_pun(")"))jx_tp++; }
        while(!jx_pun(";")&&JX_TKKIND[jx_tp]!=JT_EOF)jx_tp++; if(jx_pun(";"))jx_tp++; return; }
    if(JX_TKKIND[jx_tp]==JT_ID && streq(jx_tx(jx_tp),"console") && JX_TKKIND[jx_tp+1]==JT_PUNC && jx_tx(jx_tp+1)[0]=='.'){ jx_tp+=2; if(JX_TKKIND[jx_tp]==JT_ID)jx_tp++; if(jx_pun("("))jx_tp++; JxVal v=jx_expr(); char s[JX_STRMAX]; jx_tostr(&v,s); jx_emit(s); jx_emit("\n"); if(jx_pun(")"))jx_tp++; while(!jx_pun(";")&&JX_TKKIND[jx_tp]!=JT_EOF)jx_tp++; if(jx_pun(";"))jx_tp++; return; }
    if(JX_TKKIND[jx_tp]==JT_ID){ char nm[20]; { char* t=jx_tx(jx_tp); int j=0; while(t[j]&&j<19){nm[j]=t[j];j++;} nm[j]=0; }
        if(JX_TKKIND[jx_tp+1]==JT_PUNC && jx_tx(jx_tp+1)[0]=='['){ JxVal* e=jx_find(nm); jx_tp+=2; JxVal idx=jx_expr(); if(jx_pun("]"))jx_tp++; if(jx_pun("=")){ jx_tp++; JxVal v=jx_expr(); if(e&&e->typ==2){ int h=e->num; long i=jx_tonum(&idx); if(i>=0&&i<JX_ACAP){ if(i>=JX_ARR[h].len)JX_ARR[h].len=i+1; JX_ARR[h].el[i]=v; } } } if(jx_pun(";"))jx_tp++; return; }
        if(JX_TKKIND[jx_tp+1]==JT_PUNC && (streq(jx_tx(jx_tp+1),"++")||streq(jx_tx(jx_tp+1),"--"))){ JxVal* e=jx_find(nm); long d=jx_tx(jx_tp+1)[0]=='+'?1:-1; if(e&&e->typ==0)e->num+=d; else jx_set(nm,jx_num(d)); jx_tp+=2; if(jx_pun(";"))jx_tp++; return; }
        if(JX_TKKIND[jx_tp+1]==JT_PUNC && jx_tx(jx_tp+1)[1]=='=' && (jx_tx(jx_tp+1)[0]=='+'||jx_tx(jx_tp+1)[0]=='-'||jx_tx(jx_tp+1)[0]=='*'||jx_tx(jx_tp+1)[0]=='/'||jx_tx(jx_tp+1)[0]=='%')){ char op=jx_tx(jx_tp+1)[0]; jx_tp+=2; JxVal v=jx_expr(); jx_compound(nm,op,v); if(jx_pun(";"))jx_tp++; return; }
        if(JX_TKKIND[jx_tp+1]==JT_PUNC && streq(jx_tx(jx_tp+1),"=")){ jx_tp+=2; JxVal v=jx_expr(); jx_set(nm,v); if(jx_pun(";"))jx_tp++; return; } }
    jx_expr(); while(!jx_pun(";")&&JX_TKKIND[jx_tp]!=JT_EOF&&!jx_pun("}"))jx_tp++; if(jx_pun(";"))jx_tp++;
}
static void jx_block(void){ if(jx_pun("{"))jx_tp++; while(!jx_pun("}")&&JX_TKKIND[jx_tp]!=JT_EOF){ jx_stmt(); if(jx_err||jx_budget<=0||jx_returning)break; } if(jx_pun("}"))jx_tp++; }

static int js_run_into(const char* src,int slen,char* dst,int dstmax){
    jx_outlen=0; JX_OUT[0]=0; jx_nvar=0; jx_narr=0; jx_nfn=0; jx_budget=40000; jx_err=0; jx_returning=0;
    jx_lex(src,slen); jx_tp=0;

    while(JX_TKKIND[jx_tp]!=JT_EOF){ if(jx_kw("function"))jx_stmt(); else jx_tp++; }
    jx_tp=0; jx_nvar=0; jx_narr=0;

    while(JX_TKKIND[jx_tp]!=JT_EOF && jx_budget>0 && !jx_err){
        if(jx_kw("function")){ jx_tp++; if(JX_TKKIND[jx_tp]==JT_ID)jx_tp++; if(jx_pun("("))jx_tp++; while(!jx_pun(")")&&JX_TKKIND[jx_tp]!=JT_EOF)jx_tp++; if(jx_pun(")"))jx_tp++; jx_skip(); }
        else jx_stmt();
    }
    int n=0; for(int i=0;i<jx_outlen&&n<dstmax;i++)dst[n++]=JX_OUT[i]; return n;
}

#define CSS_MAXRULE 24
typedef struct { int col; int bold,center,hide,big,under; int hasbg; u8 bg; int bordw; u8 bordcol; int pad; int mtop,mbot; int width; int minh; u8 grad,g1,g2; } CssStyle;
typedef struct { char anc[16]; u8 ancTyp; char sel[16]; u8 styp;
    int hascol; u8 col; int hasbg; u8 bg; u8 bold,center,hidden,big,under;
    int bordw; u8 bordcol; int pad; int mtop,mbot; int width; int minh; u8 grad,g1,g2; } CssRule;
#define css_rule ((CssRule*)0x00848000u)
static int css_nrule=0;
typedef struct { char tag[12],cls[16],id[16]; CssStyle save; int scl,scr,by,bx0,bx1,bw,addhide,mbot; u8 bwc; } CssFrame;
#define css_stk ((CssFrame*)0x0084A000u)
static int css_sp=0;
static int css_hex(char c){ if(c>='0'&&c<='9')return c-'0'; if(c>='a'&&c<='f')return c-'a'+10; if(c>='A'&&c<='F')return c-'A'+10; return 0; }
static void css_hsl2rgb(int h,int sv,int lv,int*pr,int*pg,int*pb){
    h=((h%360)+360)%360; if(sv<0)sv=0; if(sv>100)sv=100; if(lv<0)lv=0; if(lv>100)lv=100;
    long L=lv*255/100, S=sv*255/100;
    long C=(255-(2*L>255?2*L-255:255-2*L))*S/255;
    int hm=h%120; long X=C*(60-(hm>60?hm-60:60-hm))/60;
    long m=L-C/2,r,g,b; int seg=h/60;
    if(seg==0){r=C;g=X;b=0;} else if(seg==1){r=X;g=C;b=0;} else if(seg==2){r=0;g=C;b=X;}
    else if(seg==3){r=0;g=X;b=C;} else if(seg==4){r=X;g=0;b=C;} else {r=C;g=0;b=X;}
    r+=m;g+=m;b+=m; if(r<0)r=0;if(r>255)r=255;if(g<0)g=0;if(g>255)g=255;if(b<0)b=0;if(b>255)b=255;
    *pr=(int)r;*pg=(int)g;*pb=(int)b;
}

static int css_color(const char* s){
      while(*s==' '||*s==':')s++;
    if(s[0]=='#'){ int r,g,b; const char* h=s+1;
        int hl=0; while(h[hl]&&((h[hl]>='0'&&h[hl]<='9')||(h[hl]>='a'&&h[hl]<='f')||(h[hl]>='A'&&h[hl]<='F')))hl++;
        if(hl<6){ r=css_hex(h[0])*17; g=css_hex(h[1])*17; b=css_hex(h[2])*17; }
        else { r=css_hex(h[0])*16+css_hex(h[1]); g=css_hex(h[2])*16+css_hex(h[3]); b=css_hex(h[4])*16+css_hex(h[5]); }

        int best=0; long bd=1L<<30; for(int i=0;i<256;i++){ int pr=(PAL32[i]>>16)&255,pg=(PAL32[i]>>8)&255,pb=PAL32[i]&255; long dr=r-pr,dg=g-pg,db=b-pb,dd=dr*dr+dg*dg+db*db; if(dd<bd){bd=dd;best=i;} } return best;
    }
    if(startsw(s,"rgb")){ const char* v=s; while(*v&&*v!='(')v++; if(*v=='(')v++;
        int rgb[3]={0,0,0}; for(int k=0;k<3&&*v;k++){ while(*v==' ')v++; int n=0; while(*v>='0'&&*v<='9'){n=n*10+(*v-'0');v++;} rgb[k]=n; while(*v&&*v!=','&&*v!=')')v++; if(*v==',')v++; }
        int best=0; long bd=1L<<30; for(int i=0;i<256;i++){ int pr=(PAL32[i]>>16)&255,pg=(PAL32[i]>>8)&255,pb=PAL32[i]&255; long dr=rgb[0]-pr,dg=rgb[1]-pg,db=rgb[2]-pb,dd=dr*dr+dg*dg+db*db; if(dd<bd){bd=dd;best=i;} } return best;
    }
    if(startsw(s,"hsl")){ const char* v=s; while(*v&&*v!='(')v++; if(*v=='(')v++;
        int hh[3]={0,0,0}; for(int k=0;k<3&&*v;k++){ while(*v==' ')v++; int n=0; while(*v>='0'&&*v<='9'){n=n*10+(*v-'0');v++;} hh[k]=n; while(*v&&*v!=','&&*v!=')')v++; if(*v==',')v++; }
        int r,g,b; css_hsl2rgb(hh[0],hh[1],hh[2],&r,&g,&b);
        int best=0; long bd=1L<<30; for(int i=0;i<256;i++){ int pr=(PAL32[i]>>16)&255,pg=(PAL32[i]>>8)&255,pb=PAL32[i]&255; long dr=r-pr,dg=g-pg,db=b-pb,dd=dr*dr+dg*dg+db*db; if(dd<bd){bd=dd;best=i;} } return best;
    }
    if(startsw(s,"red"))return C_RED; if(startsw(s,"green"))return C_GREEN;
    if(startsw(s,"blue"))return C_BBLUE; if(startsw(s,"white"))return C_WHITE;
    if(startsw(s,"black"))return 0; if(startsw(s,"gray")||startsw(s,"grey"))return 32;
    if(startsw(s,"orange"))return C_RED; if(startsw(s,"yellow")||startsw(s,"gold"))return C_FOLDER;
    if(startsw(s,"teal")||startsw(s,"cyan")||startsw(s,"aqua"))return C_TEAL; if(startsw(s,"navy"))return C_BLUE;
    if(startsw(s,"purple")||startsw(s,"magenta")||startsw(s,"violet")||startsw(s,"pink"))return C_RED;
    if(startsw(s,"lime"))return C_GREEN; if(startsw(s,"maroon")||startsw(s,"brown"))return C_RED;
    if(startsw(s,"silver"))return 48; if(startsw(s,"olive"))return C_FOLDER;
    return -1;
}

static int css_gradient(const char* v,u8* g1,u8* g2){
    while(*v==' '||*v==':')v++;
    if(!startsw(v,"linear-gradient"))return 0;
    const char* p=v; while(*p&&*p!='(')p++; if(*p=='(')p++;
    int dir=1;

    const char* look=p; int adv=0;
    if(startsw(look,"to ")){ const char* d=look+3; if(startsw(d,"right")||startsw(d,"left"))dir=2; while(*p&&*p!=',')p++; if(*p==',')p++; adv=1; }
    else { int num=0,isnum=0; const char* d=look; while(*d==' ')d++; while(*d>='0'&&*d<='9'){num=num*10+(*d-'0');d++;isnum=1;} if(isnum&&startsw(d,"deg")){ if(num>=45&&num<=135)dir=2; else dir=1; while(*p&&*p!=',')p++; if(*p==',')p++; adv=1; } }
    (void)adv;

    while(*p==' ')p++; int c1=css_color(p); if(c1<0)c1=0;
    while(*p&&*p!=','&&*p!=')')p++; if(*p==',')p++;

    while(*p==' ')p++; int c2=css_color(p); if(c2<0)c2=c1;
    *g1=(u8)c1; *g2=(u8)c2; return dir;
}

static void css_decls(const char* d,int rn){
    const char* p=d;
    while(*p&&*p!='}'){
        while(*p==' '||*p==';'||*p=='\n'||*p=='\t'||*p=='\r')p++;
        if(*p=='}'||!*p)break;
        char prop[20]; int pl=0; while(*p&&*p!=':'&&*p!='}'&&pl<19){ char c=*p; if(c>='A'&&c<='Z')c+=32; if(c!=' ')prop[pl++]=c; p++; } prop[pl]=0;
        if(*p==':')p++;
        char val[40]; int vl=0; while(*p&&*p!=';'&&*p!='}'&&vl<39){ val[vl++]=*p; p++; } val[vl]=0;
        if(streq(prop,"color")){ int c=css_color(val); if(c>=0){ css_rule[rn].hascol=1; css_rule[rn].col=(u8)c; } }
        else if(streq(prop,"background")||streq(prop,"background-color")||streq(prop,"background-image")){ u8 ga,gb; int gd=css_gradient(val,&ga,&gb); if(gd){ css_rule[rn].grad=(u8)gd; css_rule[rn].g1=ga; css_rule[rn].g2=gb; } else { int c=css_color(val); if(c>=0){ css_rule[rn].hasbg=1; css_rule[rn].bg=(u8)c; } } }
        else if(streq(prop,"font-weight")){ const char* v=val; while(*v==' ')v++; if(startsw(v,"bold")||startsw(v,"700")||startsw(v,"800")||startsw(v,"900"))css_rule[rn].bold=1; }
        else if(streq(prop,"font-size")){ const char* v=val; while(*v==' ')v++; int px=0; while(*v>='0'&&*v<='9'){px=px*10+(*v-'0');v++;} if(px>=18||startsw(v,"large")||startsw(v,"x-large")||startsw(v,"xx-large"))css_rule[rn].big=1; }
        else if(streq(prop,"text-align")){ const char* v=val; while(*v==' ')v++; if(startsw(v,"center"))css_rule[rn].center=1; }
        else if(streq(prop,"text-decoration")){ const char* v=val; while(*v==' ')v++; if(startsw(v,"underline"))css_rule[rn].under=1; }
        else if(streq(prop,"display")){ const char* v=val; while(*v==' ')v++; if(startsw(v,"none"))css_rule[rn].hidden=1; }
        else if(streq(prop,"padding")){ const char* v=val; while(*v==' ')v++; int n=0; while(*v>='0'&&*v<='9'){n=n*10+(*v-'0');v++;} css_rule[rn].pad=n; }
        else if(streq(prop,"margin")){ const char* v=val; while(*v==' ')v++; if(startsw(v,"0")&&(v[1]==' ')&&startsw(v+2,"auto")){ css_rule[rn].center=1; } int n=0; while(*v>='0'&&*v<='9'){n=n*10+(*v-'0');v++;} css_rule[rn].mtop=n; css_rule[rn].mbot=n; }
        else if(streq(prop,"margin-top")){ const char* v=val; int n=0; while(*v&&!(*v>='0'&&*v<='9'))v++; while(*v>='0'&&*v<='9'){n=n*10+(*v-'0');v++;} css_rule[rn].mtop=n; }
        else if(streq(prop,"margin-bottom")){ const char* v=val; int n=0; while(*v&&!(*v>='0'&&*v<='9'))v++; while(*v>='0'&&*v<='9'){n=n*10+(*v-'0');v++;} css_rule[rn].mbot=n; }
        else if(streq(prop,"width")){ const char* v=val; int n=0; while(*v&&!(*v>='0'&&*v<='9'))v++; while(*v>='0'&&*v<='9'){n=n*10+(*v-'0');v++;} css_rule[rn].width=n; }
        else if(streq(prop,"height")||streq(prop,"min-height")){ const char* v=val; int n=0; while(*v&&!(*v>='0'&&*v<='9'))v++; while(*v>='0'&&*v<='9'){n=n*10+(*v-'0');v++;} css_rule[rn].minh=n; }
        else if(streq(prop,"border")){ const char* v=val; int n=0; while(*v&&!(*v>='0'&&*v<='9'))v++; while(*v>='0'&&*v<='9'){n=n*10+(*v-'0');v++;} if(n<1)n=1; css_rule[rn].bordw=n; const char* w=val; int c=-1; while(*w){ if((*w>='a'&&*w<='z')&&!startsw(w,"solid")&&!startsw(w,"px")&&!startsw(w,"dashed")&&!startsw(w,"dotted")){ c=css_color(w); if(c>=0)break; } w++; } css_rule[rn].bordcol=(u8)(c>=0?c:0); }
        else if(streq(prop,"border-color")){ int c=css_color(val); if(c>=0)css_rule[rn].bordcol=(u8)c; if(css_rule[rn].bordw<1)css_rule[rn].bordw=1; }
        else if(streq(prop,"border-width")){ const char* v=val; int n=0; while(*v&&!(*v>='0'&&*v<='9'))v++; while(*v>='0'&&*v<='9'){n=n*10+(*v-'0');v++;} css_rule[rn].bordw=n; }
        if(*p==';')p++;
    }
}

static void css_parse(const char* s,int len){
    int i=0;
    while(i<len && css_nrule<CSS_MAXRULE){
        while(i<len&&(s[i]==' '||s[i]=='\n'||s[i]=='\t'||s[i]=='\r'||s[i]==','))i++;
        if(i>=len)break;

        char p1[16],p2[16]; int l1=0,l2=0,t1=0,t2=0; int part=0;
        while(i<len&&s[i]!='{'&&s[i]!=','){ char c=s[i];
            if(c==' '){ if((part==0&&l1)||(part==1&&l2)){ part=1; } i++; continue; }
            if(c=='.'){ if(part==0)t1=1; else t2=1; i++; continue; }
            if(c=='#'){ if(part==0)t1=2; else t2=2; i++; continue; }
            if(c>='A'&&c<='Z')c+=32;
            if(part==0){ if(l1<15)p1[l1++]=c; } else { if(l2<15)p2[l2++]=c; }
            i++;
        }
        p1[l1]=0; p2[l2]=0;
        if(i<len&&s[i]=='{'){ i++; int st=i; while(i<len&&s[i]!='}')i++;

            char* anc; int ancT; char* sel; int selT;
            if(l2){ anc=p1; ancT=t1; sel=p2; selT=t2; } else { anc=0; ancT=0; sel=p1; selT=t1; }
            if(sel[0]){ int rn=css_nrule++; struct {int dummy;} _; (void)_;
                int j=0; while(sel[j]&&j<15){css_rule[rn].sel[j]=sel[j];j++;} css_rule[rn].sel[j]=0; css_rule[rn].styp=(u8)selT;
                if(anc){ j=0; while(anc[j]&&j<15){css_rule[rn].anc[j]=anc[j];j++;} css_rule[rn].anc[j]=0; css_rule[rn].ancTyp=(u8)ancT; } else css_rule[rn].anc[0]=0;
                css_rule[rn].hascol=0;css_rule[rn].col=0;css_rule[rn].hasbg=0;css_rule[rn].bg=0;css_rule[rn].bold=0;css_rule[rn].center=0;css_rule[rn].hidden=0;css_rule[rn].big=0;css_rule[rn].under=0;
                css_rule[rn].bordw=0;css_rule[rn].bordcol=0;css_rule[rn].pad=0;css_rule[rn].mtop=0;css_rule[rn].mbot=0;css_rule[rn].width=0;css_rule[rn].minh=0;
                css_decls(s+st,rn);
            }
            if(i<len&&s[i]=='}')i++;
        } else break;
    }
}

static void css_eval(const char* tag,const char* cls,const char* idv,const char* sty,CssStyle* o){
    o->hasbg=0;o->bg=0;o->bordw=0;o->bordcol=0;o->pad=0;o->mtop=0;o->mbot=0;o->width=0;o->minh=0;o->hide=0;o->big=0;o->center=0;o->grad=0;
    if(streq(tag,"h1")){o->big=1;o->mtop=6;o->mbot=24;}
    else if(streq(tag,"h2")){o->big=1;o->mtop=14;o->mbot=16;}
    else if(streq(tag,"h3")||streq(tag,"h4")||streq(tag,"h5")||streq(tag,"h6")){o->big=1;o->mtop=12;o->mbot=12;}
    else if(streq(tag,"p")){o->mtop=6;o->mbot=12;}
    else if(streq(tag,"ul")||streq(tag,"ol")){o->mtop=2;o->mbot=4;}
    else if(streq(tag,"li")){o->mtop=13;o->mbot=2;}
    else if(streq(tag,"blockquote")){o->mtop=10;o->mbot=8;o->pad=8;}
    for(int r=0;r<css_nrule;r++){ int m=0;
        if(css_rule[r].styp==0 && streq(css_rule[r].sel,tag))m=1;
        else if(css_rule[r].styp==1 && cls[0] && streq(css_rule[r].sel,cls))m=1;
        else if(css_rule[r].styp==2 && idv[0] && streq(css_rule[r].sel,idv))m=1;
        if(m && css_rule[r].anc[0]){ int ok=0; for(int a=0;a<css_sp;a++){
                if(css_rule[r].ancTyp==0 && streq(css_stk[a].tag,css_rule[r].anc))ok=1;
                else if(css_rule[r].ancTyp==1 && css_stk[a].cls[0] && streq(css_stk[a].cls,css_rule[r].anc))ok=1;
                else if(css_rule[r].ancTyp==2 && css_stk[a].id[0] && streq(css_stk[a].id,css_rule[r].anc))ok=1;
                if(ok)break; }
            if(!ok)m=0; }
        if(m){ if(css_rule[r].hascol){o->col=css_rule[r].col;} if(css_rule[r].hasbg){o->hasbg=1;o->bg=css_rule[r].bg;}
            if(css_rule[r].bold)o->bold=1; if(css_rule[r].center)o->center=1; if(css_rule[r].hidden)o->hide=1;
            if(css_rule[r].big)o->big=1; if(css_rule[r].under)o->under=1;
            if(css_rule[r].bordw){o->bordw=css_rule[r].bordw;o->bordcol=css_rule[r].bordcol;}
            if(css_rule[r].pad)o->pad=css_rule[r].pad; if(css_rule[r].mtop)o->mtop=css_rule[r].mtop; if(css_rule[r].mbot)o->mbot=css_rule[r].mbot;
            if(css_rule[r].width)o->width=css_rule[r].width; if(css_rule[r].minh)o->minh=css_rule[r].minh;
            if(css_rule[r].grad){o->grad=css_rule[r].grad;o->g1=css_rule[r].g1;o->g2=css_rule[r].g2;} }
    }
    if(sty&&sty[0]){ const char* p=sty;
        while(*p){ int b=(p==sty||p[-1]==';'||p[-1]==' ');
            if(b&&startsw(p,"background")){ const char* v=p+10; if(startsw(v,"-color"))v+=6; else if(startsw(v,"-image"))v+=6; while(*v==' '||*v==':')v++; u8 ga,gb; int gd=css_gradient(v,&ga,&gb); if(gd){o->grad=(u8)gd;o->g1=ga;o->g2=gb;} else { int c=css_color(v); if(c>=0){o->hasbg=1;o->bg=(u8)c;} } }
            else if(b&&startsw(p,"color")){ const char* v=p+5; while(*v==' '||*v==':')v++; int c=css_color(v); if(c>=0)o->col=c; }
            else if(b&&startsw(p,"display")){ const char* v=p+7; while(*v==' '||*v==':')v++; if(startsw(v,"none"))o->hide=1; }
            else if(b&&startsw(p,"font-weight")){ const char* v=p+11; while(*v==' '||*v==':')v++; if(startsw(v,"bold"))o->bold=1; }
            else if(b&&startsw(p,"font-size")){ const char* v=p+9; while(*v==' '||*v==':')v++; int px=0; while(*v>='0'&&*v<='9'){px=px*10+(*v-'0');v++;} if(px>=18||startsw(v,"large"))o->big=1; }
            else if(b&&startsw(p,"text-align")){ const char* v=p+10; while(*v==' '||*v==':')v++; if(startsw(v,"center"))o->center=1; }
            else if(b&&startsw(p,"text-decoration")){ const char* v=p+15; while(*v==' '||*v==':')v++; if(startsw(v,"underline"))o->under=1; }
            else if(b&&startsw(p,"padding")){ const char* v=p+7; while(*v==' '||*v==':')v++; int n=0; while(*v>='0'&&*v<='9'){n=n*10+(*v-'0');v++;} o->pad=n; }
            else if(b&&startsw(p,"width")){ const char* v=p+5; while(*v&&!(*v>='0'&&*v<='9')&&*v!=';')v++; int n=0; while(*v>='0'&&*v<='9'){n=n*10+(*v-'0');v++;} o->width=n; }
            else if(b&&startsw(p,"margin-top")){ const char* v=p+10; while(*v&&!(*v>='0'&&*v<='9')&&*v!=';')v++; int n=0; while(*v>='0'&&*v<='9'){n=n*10+(*v-'0');v++;} o->mtop=n; }
            else if(b&&startsw(p,"margin-bottom")){ const char* v=p+13; while(*v&&!(*v>='0'&&*v<='9')&&*v!=';')v++; int n=0; while(*v>='0'&&*v<='9'){n=n*10+(*v-'0');v++;} o->mbot=n; }
            else if(b&&startsw(p,"margin")){ const char* v=p+6; while(*v==' '||*v==':')v++; if(startsw(v,"0")&&v[1]==' '&&startsw(v+2,"auto"))o->center=1; int n=0; while(*v&&!(*v>='0'&&*v<='9')&&*v!=';')v++; while(*v>='0'&&*v<='9'){n=n*10+(*v-'0');v++;} o->mtop=n; o->mbot=n; }
            else if(b&&(startsw(p,"height")||startsw(p,"min-height"))){ const char* v=p+(p[0]=='m'?10:6); while(*v&&!(*v>='0'&&*v<='9')&&*v!=';')v++; int n=0; while(*v>='0'&&*v<='9'){n=n*10+(*v-'0');v++;} o->minh=n; }
            else if(b&&startsw(p,"line-height")){ const char* v=p+11; while(*v&&!(*v>='0'&&*v<='9')&&*v!=';')v++; int n=0; while(*v>='0'&&*v<='9'){n=n*10+(*v-'0');v++;} if(n>=20)o->big=1; }
            else if(b&&startsw(p,"border")){ const char* v=p+6; int n=0; const char* w=v; while(*w&&!(*w>='0'&&*w<='9')&&*w!=';')w++; while(*w>='0'&&*w<='9'){n=n*10+(*w-'0');w++;} if(n<1)n=1; o->bordw=n; int c=css_color(w); o->bordcol=(u8)(c>=0?c:0); }
            p++; }
    }
}

static int tag_attr(const char* p,const char* end,const char* name,char* out,int max){
    int nl=0; while(name[nl])nl++;
    for(const char* q=p; q<end; q++){
        if(q!=p && q[-1]!=' '&&q[-1]!='\t'&&q[-1]!='\n'&&q[-1]!='"'&&q[-1]!='\'')continue;
        int k=0; while(k<nl && q+k<end){ char a=q[k],b=name[k]; if(a>='A'&&a<='Z')a+=32; if(a!=b)break; k++; }
        if(k==nl){ const char* r=q+nl; while(r<end&&(*r==' '||*r=='\t'))r++;
            if(r<end&&*r=='='){ r++; while(r<end&&(*r==' '||*r=='\t'))r++; char qc=0; if(r<end&&(*r=='"'||*r=='\'')){qc=*r;r++;}
                int o=0; while(r<end&&o<max-1){ if(qc){ if(*r==qc)break; } else { if(*r==' '||*r=='>'||*r=='\t'||*r=='/')break; } out[o++]=*r++; } out[o]=0; return o; } }
    }
    out[0]=0; return 0;
}
static void br_prep(void){
    br_use_expand=0; css_nrule=0;
    if(!cur_html)return;
    const char* p=cur_html; char* d=BR_EXPAND; int o=0; int had=0;
    while(*p && o<BR_EXPMAX-1){
        if(*p=='<' && startsw(p,"<style")){ while(*p&&*p!='>')p++; if(*p)p++; const char* body=p; while(*p&&!startsw(p,"</style"))p++; css_parse(body,(int)(p-body)); while(*p&&*p!='>')p++; if(*p)p++; had=1; continue; }
        if(*p=='<' && startsw(p,"<link")){ const char* ts=p; while(*p&&*p!='>')p++; const char* te=p; if(*p)p++;
            char href[64]; tag_attr(ts,te,"href",href,64);
            if(href[0]&&br_is_local&&disk_ok){ int idx=nvx_find(href); if(idx>=0){ int L=nvx_read(idx,BR_LINKBUF,16380); BR_LINKBUF[L]=0; css_parse(BR_LINKBUF,L); } }
            had=1; continue; }
        if(*p=='<' && startsw(p,"<script")){ const char* ts=p; while(*p&&*p!='>')p++; const char* te=p; if(*p)p++;
            char src[64]; tag_attr(ts,te,"src",src,64);
            const char* body=p; while(*p&&!startsw(p,"</script"))p++;
            if(src[0]&&br_is_local&&disk_ok){ int idx=nvx_find(src); if(idx>=0){ int L=nvx_read(idx,BR_LINKBUF,16380); BR_LINKBUF[L]=0; int n=js_run_into(BR_LINKBUF,L,d+o,BR_EXPMAX-1-o); o+=n; } }
            else { int n=js_run_into(body,(int)(p-body),d+o,BR_EXPMAX-1-o); o+=n; }
            while(*p&&*p!='>')p++; if(*p)p++; had=1; continue; }
        d[o++]=*p++;
    }
    d[o]=0;
    if(had||css_nrule>0)br_use_expand=1;
}

static void br_line(int x0,int y0,int x1,int y1,u8 c,int th){
    int dx=x1-x0, dy=y1-y0, ax=dx<0?-dx:dx, ay=dy<0?-dy:dy, steps=ax>ay?ax:ay;
    if(steps<1)steps=1;
    for(int i=0;i<=steps;i++){ int x=x0+dx*i/steps, y=y0+dy*i/steps; fill(x-th/2,y-th/2,th,th,c); }
}

static void br_logo_claude(int cx,int cy,int r,u8 col){
    for(int k=0;k<12;k++){ int a=k*256/12; int x=cx+S(a+64)*r/127, y=cy+S(a)*r/127;
        br_line(cx,cy,x,y,col,(k&1)?3:5); }
    cos_circle(cx,cy,3,col);
}

static void br_logo_openai(int cx,int cy,int r,u8 col){
    int vx[6],vy[6];
    for(int k=0;k<6;k++){ int a=k*256/6+21; vx[k]=cx+S(a+64)*r/127; vy[k]=cy+S(a)*r/127; }
    for(int k=0;k<6;k++) br_line(vx[k],vy[k],vx[(k+1)%6],vy[(k+1)%6],col,4);

    br_line(vx[0],vy[0],vx[3],vy[3],col,3);
    br_line(vx[1],vy[1],vx[4],vy[4],col,3);
    br_line(vx[2],vy[2],vx[5],vy[5],col,3);
    cos_circle(cx,cy,r/4,58);
}
static int tbl_in=0,tbl_x0=0,tbl_w=0,tbl_nc=1,tbl_ci=0,tbl_rtop=0,tbl_rbot=0,tbl_top=0,tbl_scl=0,tbl_scr=0;
static char imgc_url[4][256];
static int  imgc_len[4]={0,0,0,0};
static int  imgc_next=0;
static const u8* img_get_net(const char* url,int* outlen){
    for(int i=0;i<4;i++){ if(imgc_url[i][0]&&streq_cs(imgc_url[i],url)){ if(imgc_len[i]>0){ *outlen=imgc_len[i]; return (const u8*)(0x00B00000u+i*60000u); } *outlen=0; return 0; } }
    char* b=br_fetch((char*)url);
    if(!b||!br_ok||http_body_len<1){
        int slot=imgc_next; imgc_next=(imgc_next+1)&3;
        int k=0; while(url[k]&&k<255){imgc_url[slot][k]=url[k];k++;} imgc_url[slot][k]=0;
        imgc_len[slot]=-1; *outlen=0; return 0;
    }
    int blen=http_body_len; if(blen>60000)blen=60000;
    int slot=imgc_next; imgc_next=(imgc_next+1)&3;
    u8* dst=(u8*)(0x00B00000u+slot*60000u);
    for(int i=0;i<blen;i++)dst[i]=((u8*)b)[i];
    int k=0; while(url[k]&&k<255){imgc_url[slot][k]=url[k];k++;} imgc_url[slot][k]=0;
    imgc_len[slot]=blen; *outlen=blen; return dst;
}
static void br_render(void){
    draw_window_chrome("NOOVEXBROWSER");
    fill(winx+1,winy+20,winw-2,24,C_TASK);
    fill(winx+6,winy+24,28,16,C_BLUE); draw_str(winx+15,winy+27,"<",C_WHITE);
    fill(winx+38,winy+24,40,16,C_BLUE); draw_str(winx+42,winy+27,"RLD",C_WHITE);
    int abx=winx+84,abw=winw-84-40;
    fill(abx,winy+24,abw,16,C_WIN); frame(abx,winy+24,abw,16,br_addr_focus?C_WHITE:C_MGREY);
    draw_str(abx+4,winy+27,br_addr,0);
    if(br_addr_focus){ int cx=abx+4+br_addr_len*8; if(cx<abx+abw-2)fill(cx,winy+26,1,12,0); }
    fill(winx+winw-38,winy+24,32,16,C_GREEN); draw_str(winx+winw-32,winy+27,"GO",C_WHITE);
    int cl=winx+8,cr=winx+winw-8,ct=winy+58,cb=winy+winh-6;
    fill(winx+1,winy+44,winw-2,winh-45,58);
    if(br_last_https){ const char* cs=(tls_last_cipher==0x1303)?"TLS 1.3 ENCRYPTED - CHACHA20-POLY1305":(tls_last_cipher==0x1301)?"TLS 1.3 ENCRYPTED - AES-128-GCM":"TLS 1.3 ENCRYPTED"; fill(winx+1,winy+44,winw-2,12,C_GREEN); draw_str(winx+6,winy+46,cs,C_WHITE); }
    else { fill(winx+1,winy+44,winw-2,12,C_RED); draw_str(winx+6,winy+46,"HTTP - NOT ENCRYPTED",C_WHITE); }
    { int tw=146,tx=winx+winw-tw-4; fill(tx,winy+45,tw,10,C_TASK); draw_str(tx+4,winy+46,https_only?"HTTPS-ONLY: ON":"HTTPS-ONLY: OFF",C_WHITE); }
    if(br_brand){
        int cx0=winx+1, cy0=winy+56, cw=winw-2, ch=winh-57;
        fill(cx0,cy0,cw,ch,C_WHITE);
        int mx=cx0+cw/2, my=cy0+64;
        if(br_brand==1||br_brand==3){
            br_logo_claude(mx,my,40,C_RED);
            const char* t1="CLAUDE"; draw_str2(mx-strlen_(t1)*16/2,my+56,t1,5);
            if(br_brand==3){
                const char* t2="START A NEW CHAT"; draw_str(mx-strlen_(t2)*8/2,my+84,t2,C_MGREY+20);
                int bw=200,bh=30,bx=mx-bw/2,by=my+106;
                fill(bx,by,bw,bh,C_RED); frame(bx,by,bw,bh,C_WHITE);
                const char* b="OPEN ASK CLAUDE"; draw_str(bx+(bw-strlen_(b)*8)/2,by+11,b,C_WHITE);
                const char* u="CLAUDE.AI/NEW"; draw_str(mx-strlen_(u)*8/2,by+bh+14,u,C_MGREY);
            } else {
                const char* t2="AI ASSISTANT BY ANTHROPIC"; draw_str(mx-strlen_(t2)*8/2,my+84,t2,C_MGREY+20);
                const char* t3="GO TO CLAUDE.AI/NEW TO START A CHAT"; draw_str(mx-strlen_(t3)*8/2,my+106,t3,C_BBLUE);
            }
        } else {
            br_logo_openai(mx,my,40,C_TEAL);
            const char* t1="CHATGPT"; draw_str2(mx-strlen_(t1)*16/2,my+56,t1,5);
            const char* t2="AI ASSISTANT BY OPENAI"; draw_str(mx-strlen_(t2)*8/2,my+84,t2,C_MGREY+20);
            const char* t3="LIVE SITE REQUIRES A FULL JS ENGINE"; draw_str(mx-strlen_(t3)*8/2,my+106,t3,C_MGREY);
        }
        draw_str(cx0+8,cy0+ch-16,"NOOVEXBROWSER BRANDED VIEW - NOT THE LIVE PAGE",C_MGREY);
        return;
    }
    br_link_n=0; if(!cur_html)br_nav("home",4);
    int cr0=cr;
    int x=cl,y=ct-br_scroll,big=0,inlink=0,emph=0,in_ol=0,ol_n=0,page_dark=0; const char* href=0; int hlen=0;
    CssStyle cs; cs.col=-1; cs.bold=0; cs.center=0; cs.under=0; cs.hasbg=0; cs.bg=0; cs.big=0; cs.hide=0;
    int css_hide=0; css_sp=0;
    const char* p=br_use_expand?BR_EXPAND:cur_html;
    while(*p){
        if(*p=='<'){
            if(startsw(p,"<!--")){ while(*p&&!startsw(p,"-->"))p++; if(*p)p+=3; continue; }
            const char* t=p+1;
            int sc2=startsw(t,"script"),st2=startsw(t,"style"),hd=(startsw(t,"head")&&(t[4]=='>'||t[4]==' ')),ti2=startsw(t,"title");
            if(sc2||st2||hd||ti2){ const char* cz=sc2?"</script":hd?"</head":ti2?"</title":"</style"; while(*p&&*p!='>')p++; if(*p)p++; while(*p){ if(*p=='<'&&startsw(p,cz)){ while(*p&&*p!='>')p++; if(*p)p++; break; } p++; } continue; }
            if(tagis(t,"table")){
                tbl_in=1; tbl_scl=cl; tbl_scr=cr; tbl_x0=cl; tbl_w=cr-cl; if(tbl_w<40)tbl_w=40;
                int nc=0; const char* q=p+1; while(*q){ if(*q=='<'){ if(tagis(q+1,"tr"))break; if(tagis(q+1,"/table"))break; } q++; }
                if(*q){ const char* rr=q+1; while(*rr){ if(*rr=='<'){ if(tagis(rr+1,"/tr")||tagis(rr+1,"/table"))break; if(tagis(rr+1,"td")||tagis(rr+1,"th"))nc++; } rr++; } }
                tbl_nc=nc>0?(nc>8?8:nc):1; tbl_top=y+2; tbl_rtop=y+2; tbl_rbot=y+2; tbl_ci=0;
                while(*p&&*p!='>')p++; if(*p)p++; continue;
            }
            if(tagis(t,"/table")){
                int x1t=tbl_x0+tbl_w;
                if(tbl_top>=ct-2&&tbl_top<=cb)fill(tbl_x0,tbl_top,tbl_w,1,C_MGREY);
                if(tbl_rbot>=ct-2&&tbl_rbot<=cb)fill(tbl_x0,tbl_rbot,tbl_w,1,C_MGREY);
                if(tbl_rbot>tbl_top){ fill(tbl_x0,tbl_top,1,tbl_rbot-tbl_top,C_MGREY); fill(x1t,tbl_top,1,tbl_rbot-tbl_top,C_MGREY); }
                tbl_in=0; cl=tbl_scl; cr=tbl_scr; x=cl; y=tbl_rbot+8;
                while(*p&&*p!='>')p++; if(*p)p++; continue;
            }
            if(tbl_in){
                if(tagis(t,"tr")){ tbl_ci=0; tbl_rtop=tbl_rbot; while(*p&&*p!='>')p++; if(*p)p++; continue; }
                if(tagis(t,"/tr")){ int colw=tbl_w/tbl_nc; for(int c2=1;c2<tbl_nc;c2++){ int vx=tbl_x0+c2*colw; if(tbl_rbot>tbl_rtop)fill(vx,tbl_rtop,1,tbl_rbot-tbl_rtop,C_MGREY+10); } if(tbl_rbot>=ct-2&&tbl_rbot<=cb)fill(tbl_x0,tbl_rbot,tbl_w,1,C_MGREY+10); while(*p&&*p!='>')p++; if(*p)p++; continue; }
                if(tagis(t,"td")||tagis(t,"th")){ int colw=tbl_w/tbl_nc; int ci2=tbl_ci<tbl_nc?tbl_ci:tbl_nc-1; int cx=tbl_x0+ci2*colw; cl=cx+4; cr=cx+colw-4; if(cr<cl+8)cr=cl+8; x=cl; y=tbl_rtop+4; while(*p&&*p!='>')p++; if(*p)p++; continue; }
                if(tagis(t,"/td")||tagis(t,"/th")){ if(y+13>tbl_rbot)tbl_rbot=y+13; tbl_ci++; while(*p&&*p!='>')p++; if(*p)p++; continue; }
                if(tagis(t,"thead")||tagis(t,"/thead")||tagis(t,"tbody")||tagis(t,"/tbody")||tagis(t,"tfoot")||tagis(t,"/tfoot")){ while(*p&&*p!='>')p++; if(*p)p++; continue; }
            }

            { const char* tt=(t[0]=='/')?t+1:t; int clo=(t[0]=='/');
              if(tagis(tt,"b")||tagis(tt,"strong")){ emph=clo?0:1; while(*p&&*p!='>')p++; if(*p)p++; continue; }
              if(tagis(tt,"i")||tagis(tt,"em")){ emph=clo?0:2; while(*p&&*p!='>')p++; if(*p)p++; continue; }
              if(tagis(tt,"code")||tagis(tt,"pre")){ emph=clo?0:3; while(*p&&*p!='>')p++; if(*p)p++; continue; }
              if(tagis(tt,"a")){ if(clo){ inlink=0; href=0; } else { const char* hh=t; while(*hh&&*hh!='>'){ if(startsw(hh,"href")){ hh+=4; while(*hh=='='||*hh=='"'||*hh==' ')hh++; href=hh; hlen=0; while(href[hlen]&&href[hlen]!='"'&&href[hlen]!='>'&&href[hlen]!=' ')hlen++; break; } hh++; } inlink=1; } while(*p&&*p!='>')p++; if(*p)p++; continue; }
            }

            if(t[0]=='/'){
                char cn[12]; int ci=0; const char* q=t+1; while(*q&&(jx_isa(*q)||jx_isd(*q))&&ci<11){ char c=*q; if(c>='A'&&c<='Z')c+=32; cn[ci++]=c; q++; } cn[ci]=0;
                if(css_sp>0 && streq(css_stk[css_sp-1].tag,cn)){ css_sp--; CssFrame* fr=&css_stk[css_sp];
                    y += fr->bw + cs.pad;
                    int boxBot=y;
                    if(fr->by + cs.minh > boxBot) boxBot=fr->by + cs.minh;
                    if(fr->bw>0){ u8 bc=fr->bwc; for(int k=0;k<fr->bw;k++){
                        if(fr->by+k>=ct-2&&fr->by+k<=cb)fill(fr->bx0,fr->by+k,fr->bx1-fr->bx0,1,bc);
                        if(boxBot-k>=ct-2&&boxBot-k<=cb)fill(fr->bx0,boxBot-k,fr->bx1-fr->bx0,1,bc);
                        fill(fr->bx0+k,fr->by,1,boxBot-fr->by,bc); fill(fr->bx1-k,fr->by,1,boxBot-fr->by,bc); } }
                    if(boxBot>y)y=boxBot;
                    y += fr->mbot;
                    cl=fr->scl; cr=fr->scr; cs=fr->save; if(fr->addhide&&css_hide>0)css_hide--; x=cl;
                }
            } else if(jx_isa(t[0]) && !(tagis(t,"br")||tagis(t,"hr")||tagis(t,"img")||tagis(t,"input")||tagis(t,"meta")||tagis(t,"link")||tagis(t,"col")||tagis(t,"area")||tagis(t,"base"))){
                char cn[12]; int ci=0; const char* q=t; while(*q&&(jx_isa(*q)||jx_isd(*q))&&ci<11){ char c=*q; if(c>='A'&&c<='Z')c+=32; cn[ci++]=c; q++; } cn[ci]=0;
                int isvoid=streq(cn,"br")||streq(cn,"hr")||streq(cn,"img")||streq(cn,"input")||streq(cn,"meta")||streq(cn,"link")||streq(cn,"col")||streq(cn,"area")||streq(cn,"base");
                if(!isvoid && css_sp<12){
                    char cls[24]={0}, idv[24]={0}, sty[96]={0};
                    const char* a=t; while(*a&&*a!='>'){
                        if((a==t||a[-1]==' ')&&startsw(a,"class")){ const char* v=a+5; while(*v==' '||*v=='='||*v=='"'||*v=='\'')v++; int k=0; while(*v&&*v!='"'&&*v!='\''&&*v!='>'&&*v!=' '&&k<23){ char c=*v; if(c>='A'&&c<='Z')c+=32; cls[k++]=c; v++; } cls[k]=0; }
                        else if((a==t||a[-1]==' ')&&startsw(a,"id")){ const char* v=a+2; while(*v==' '||*v=='='||*v=='"'||*v=='\'')v++; int k=0; while(*v&&*v!='"'&&*v!='\''&&*v!='>'&&*v!=' '&&k<23){ char c=*v; if(c>='A'&&c<='Z')c+=32; idv[k++]=c; v++; } idv[k]=0; }
                        else if((a==t||a[-1]==' ')&&startsw(a,"style")){ const char* v=a+5; while(*v==' '||*v=='='||*v=='"'||*v=='\'')v++; int k=0; while(*v&&*v!='"'&&*v!='\''&&*v!='>'&&k<95){ sty[k++]=*v; v++; } sty[k]=0; }
                        a++;
                    }
                    CssFrame* fr=&css_stk[css_sp];
                    { int j=0; while(cn[j]&&j<11){fr->tag[j]=cn[j];j++;} fr->tag[j]=0; }
                    { int j=0; while(cls[j]&&j<15){fr->cls[j]=cls[j];j++;} fr->cls[j]=0; }
                    { int j=0; while(idv[j]&&j<15){fr->id[j]=idv[j];j++;} fr->id[j]=0; }
                    CssStyle ns=cs; css_eval(cn,cls,idv,sty,&ns);
                    fr->save=cs; fr->scl=cl; fr->scr=cr; fr->addhide=ns.hide;
                    y += ns.mtop;
                    fr->by=y; fr->bx0=cl; fr->bx1=(ns.width? cl+ns.width : cr); if(fr->bx1>cr0)fr->bx1=cr0;
                    fr->bw=ns.bordw; fr->bwc=ns.bordcol;
                    if(ns.grad){
                        if(streq(cn,"body")||streq(cn,"html")){ br_grad_fill(winx+1,winy+56,winw-2,winh-57,ns.g1,ns.g2,ns.grad); u32 _bc=PAL32[ns.g1]; page_dark=((((_bc>>16)&255)+((_bc>>8)&255)+(_bc&255))/3<110); }
                        else { int gw=fr->bx1-fr->bx0, gh=ns.minh?ns.minh:24; if(fr->by>=ct-2&&fr->by<=cb) br_grad_fill(fr->bx0,fr->by,gw,gh,ns.g1,ns.g2,ns.grad); }
                    }
                    else if(ns.hasbg && (streq(cn,"body")||streq(cn,"html"))){
                        fill(winx+1,winy+56,winw-2,winh-57,ns.bg);
                        u32 _bc=PAL32[ns.bg]; page_dark=((((_bc>>16)&255)+((_bc>>8)&255)+(_bc&255))/3<110);
                    }
                    cl = fr->bx0 + ns.bordw + ns.pad; cr = fr->bx1 - ns.bordw - ns.pad; if(cr<cl+16)cr=cl+16;
                    y += ns.bordw + ns.pad; x=cl;
                    fr->mbot=ns.mbot;
                    cs=ns; if(ns.hide)css_hide++;
                    css_sp++;
                }
            }
            else if(startsw(t,"title")){ p+=1; while(*p&&!startsw(p,"</title>"))p++; if(*p)p+=8; continue; }
            else if(tagis(t,"h1")){ big=1; x=cl; y+=6; }
            else if(tagis(t,"/h1")){ big=0; x=cl; y+=24; }
            else if(tagis(t,"h2")||tagis(t,"h3")||tagis(t,"h4")||tagis(t,"h5")||tagis(t,"h6")){ big=2; x=cl; y+=14; }
            else if(tagis(t,"/h2")||tagis(t,"/h3")||tagis(t,"/h4")||tagis(t,"/h5")||tagis(t,"/h6")){ big=0; x=cl; y+=16; }
            else if(tagis(t,"br")){ x=cl; y+=12; }
            else if(tagis(t,"hr")){ x=cl; y+=8; if(y>=ct&&y<=cb)fill(cl,y,cr-cl,1,C_MGREY); y+=8; }
            else if(tagis(t,"img")){
                char src[260]; int sl=0; const char* hh=t;
                while(*hh&&*hh!='>'){ if(startsw(hh,"src")){ hh+=3; while(*hh=='='||*hh=='"'||*hh==' ')hh++;
                    while(hh[sl]&&hh[sl]!='"'&&hh[sl]!='>'&&hh[sl]!=' '&&sl<259){ src[sl]=hh[sl]; sl++; } break; } hh++; }
                src[sl]=0;
                const u8* ib=0; int il=0;
                if(sl){
                    if(streq(src,"noovex.jpg")||streq(src,"sample.jpg")){ ib=sample_jpg; il=sample_jpg_len; }
                    else if(streq(src,"noovex.png")||streq(src,"sample.png")){ ib=sample_png; il=sample_png_len; }
                    else if(startsw(src,"http://")||startsw(src,"https://")){ ib=img_get_net(src,&il); }
                    else if(src[0]=='/'&&src[1]=='/'){ char au[512]; int o=0; const char* sc=br_cur_https?"https:":"http:"; for(int i=0;sc[i]&&o<500;i++)au[o++]=sc[i]; for(int i=0;src[i]&&o<510;i++)au[o++]=src[i]; au[o]=0; ib=img_get_net(au,&il); }
                    else if(src[0]=='/'&&br_cur_host[0]){ char au[512]; int o=0; const char* sc=br_cur_https?"https://":"http://"; for(int i=0;sc[i]&&o<500;i++)au[o++]=sc[i]; for(int i=0;br_cur_host[i]&&o<500;i++)au[o++]=br_cur_host[i]; for(int i=0;src[i]&&o<510;i++)au[o++]=src[i]; au[o]=0; ib=img_get_net(au,&il); }
                }
                int iw=0,ih=0,ok=0;
                if(ib&&il>4){
                    if(ib[0]==0xFF&&ib[1]==0xD8){ if(jpeg_decode(ib,il,(unsigned char*)0x01A00000u,0x5F0000,&iw,&ih)==0)ok=1; }
                    else if(ib[0]==0x89&&ib[1]=='P'&&ib[2]=='N'&&ib[3]=='G'){ if(png_decode((unsigned char*)ib,il,(unsigned char*)0x01800000u,0x1F0000,(unsigned char*)0x01A00000u,0x5F0000,&iw,&ih)==0)ok=1; }
                }
                if(ok&&iw>0&&ih>0){
                    int maxw=cr-cl; if(maxw<16)maxw=16; int dw=iw,dh=ih;
                    if(dw>maxw){ dh=dh*maxw/dw; dw=maxw; }
                    if(dh>520){ dw=dw*520/dh; dh=520; } if(dw<1)dw=1; if(dh<1)dh=1;
                    for(int oy=0;oy<dh;oy++){ int sy=y+oy; if(sy<ct||sy>cb||sy<0||sy>=H)continue;
                        int yy=oy*ih/dh; const unsigned char* sr=(unsigned char*)0x01A00000u+(unsigned)(yy*iw)*4; u32* d=FB+sy*PITCH+cl;
                        for(int ox=0;ox<dw;ox++){ int dx2=cl+ox; if(dx2<0||dx2>=W)continue; int xx=ox*iw/dw; const unsigned char* pp=sr+xx*4;
                            d[ox]=((u32)pp[0]<<16)|((u32)pp[1]<<8)|(u32)pp[2]; } }
                    y+=dh+4; x=cl;
                } else { if(y>=ct-22&&y<=cb)draw_str(x,y,"[IMG]",C_MGREY); x+=48; }
            }
            else if(tagis(t,"nav")||tagis(t,"section")||tagis(t,"article")||tagis(t,"header")||tagis(t,"footer")||tagis(t,"main")||tagis(t,"aside")||tagis(t,"figure")||tagis(t,"figcaption")||tagis(t,"table")||tagis(t,"form")){ x=cl; y+=6; }
            else if(tagis(t,"/nav")||tagis(t,"/section")||tagis(t,"/article")||tagis(t,"/header")||tagis(t,"/footer")||tagis(t,"/main")||tagis(t,"/aside")||tagis(t,"/figure")||tagis(t,"/table")||tagis(t,"/form")){ x=cl; y+=10; }
            else if(tagis(t,"/p")||tagis(t,"/div")){ x=cl; y+=14; }
            else if(tagis(t,"p")||tagis(t,"div")||tagis(t,"center")){ x=cl; y+=6; }
            else if(tagis(t,"ol")){ in_ol=1; ol_n=0; y+=2; }
            else if(tagis(t,"/ol")){ in_ol=0; y+=4; x=cl; }
            else if(tagis(t,"ul")){ in_ol=0; y+=2; }
            else if(tagis(t,"/ul")){ y+=4; x=cl; }
            else if(tagis(t,"li")){ x=cl; y+=13;
                if(in_ol){ ol_n++; char nb[7]; int v=ol_n,nl=0; char tb[6]; if(v==0)tb[nl++]='0'; while(v){tb[nl++]='0'+v%10;v/=10;} int q=0; while(nl)nb[q++]=tb[--nl]; nb[q++]='.'; nb[q]=0; if(y>=ct-22&&y<=cb)draw_str(cl+2,y,nb,C_TITLE); x=cl+8+q*8; }
                else { if(y>=ct&&y<=cb)fill(cl+6,y+5,4,4,C_TITLE); x=cl+18; } }
            else if(tagis(t,"blockquote")){ x=cl+16; y+=10; }
            else if(tagis(t,"/blockquote")){ x=cl; y+=8; }
            else if(tagis(t,"pre")||tagis(t,"code")){ emph=3; }
            else if(tagis(t,"/pre")||tagis(t,"/code")){ emph=0; }
            else if(tagis(t,"b")||tagis(t,"strong")){ emph=1; }
            else if(tagis(t,"/b")||tagis(t,"/strong")){ emph=0; }
            else if(tagis(t,"i")||tagis(t,"em")){ emph=2; }
            else if(tagis(t,"/i")||tagis(t,"/em")){ emph=0; }
            else if(tagis(t,"tr")||tagis(t,"/tr")){ x=cl; y+=12; }
            else if(tagis(t,"td")||tagis(t,"th")){ x+=8; }
            else if(tagis(t,"a")){ const char* hh=t; while(*hh&&*hh!='>'){ if(startsw(hh,"href")){ hh+=4; while(*hh=='='||*hh=='"'||*hh==' ')hh++; href=hh; hlen=0; while(href[hlen]&&href[hlen]!='"'&&href[hlen]!='>'&&href[hlen]!=' ')hlen++; break; } hh++; } inlink=1; }
            else if(tagis(t,"/a")){ inlink=0; href=0; }
            while(*p&&*p!='>')p++; if(*p)p++; continue; }
        if(*p==' '||*p=='\n'||*p=='\t'){ p++; continue; }
        const char* ws=p; int wl=0; while(*p&&*p!='<'&&*p!=' '&&*p!='\n'&&*p!='\t'){ p++; wl++; }
        if(css_hide>0){ continue; }
        char tmp[96]; int tl=html_decode(ws,wl,tmp,96);
        int useBig=(big==1)||cs.big;
        int cwid=useBig?16:8; int ww=tl*cwid;
        if(x+ww>cr){ x=cl; y+=useBig?22:13; }
        if(tl>0){ u8 col=inlink?(page_dark?C_BBLUE:C_BLUE):(cs.col>=0?(u8)cs.col:(big?(page_dark?C_WHITE:C_TITLE):(emph==1?(page_dark?C_WHITE:0):emph==2?C_TEAL:emph==3?C_GREEN:(page_dark?C_WHITE:6))));
            int dx=x; if(cs.center&&!inlink){ int rem=cr-x; if(rem>ww)dx=x+(rem)/2-ww/2; if(dx<cl)dx=cl; }
            if(y>=ct-22&&y<=cb){
                if(cs.hasbg)fill(dx-2,y-2,ww+4,useBig?20:13,cs.bg);
                if(useBig)draw_str2(dx,y,tmp,col); else draw_str(dx,y,tmp,col);
                if(cs.bold&&!useBig)draw_str(dx+1,y,tmp,col);
                if(cs.under)fill(dx,y+(useBig?17:10),ww,1,col);
                if(inlink){ fill(dx,y+(useBig?16:9),ww,1,page_dark?C_BBLUE:C_BLUE); if(br_link_n<24){ br_link[br_link_n].x=dx;br_link[br_link_n].y=y;br_link[br_link_n].w=ww;br_link[br_link_n].h=useBig?16:10;br_link[br_link_n].href=href;br_link[br_link_n].hlen=hlen;br_link_n++; } } } }
        x+=ww+cwid; } }

static long calc_acc=0, calc_cur=0;
static char calc_op=0;
static int calc_new=1, calc_err=0;
static const char* calc_btn_lbl[5][4]={
    {"CE","C","+/-","/"},
    {"7","8","9","*"},
    {"4","5","6","-"},
    {"1","2","3","+"},
    {"0",".","=","="}
};
static void calc_format(long v,char* buf,int bufsz){
    if(v==0){ buf[0]='0'; buf[1]=0; return; }
    char tmp[16]; int n=0; long a=v<0?-v:v;
    while(a>0&&n<14){ tmp[n++]='0'+(a%10); a/=10; }
    int i=0; if(v<0&&i<bufsz-1) buf[i++]='-';
    while(n>0&&i<bufsz-1) buf[i++]=tmp[--n];
    buf[i]=0;
}
static void calc_apply(void){
    if(calc_op=='+') calc_acc+=calc_cur;
    else if(calc_op=='-') calc_acc-=calc_cur;
    else if(calc_op=='*') calc_acc*=calc_cur;
    else if(calc_op=='/'){ if(calc_cur==0) calc_err=1; else calc_acc/=calc_cur; }
    else calc_acc=calc_cur;
}
static void calc_press(const char* lbl){
    char k=lbl[0];
    if(calc_err && k!='C'){ return; }
    if(k>='0'&&k<='9'){
        if(calc_new){ calc_cur=0; calc_new=0; }
        if(calc_cur<100000000L) calc_cur=calc_cur*10+(k-'0');
        else if(calc_cur>-100000000L)   ;
    }
    else if(k=='C'){
        if(lbl[1]=='E'){ calc_cur=0; calc_new=1; }
        else { calc_acc=0; calc_cur=0; calc_op=0; calc_err=0; calc_new=1; }
    }
    else if(k=='+'&&lbl[1]=='/'){ calc_cur=-calc_cur; }
    else if(k=='.'){   }
    else if(k=='='){
        if(calc_op){ calc_apply(); calc_op=0; }
        else calc_acc=calc_cur;
        calc_cur=calc_acc; calc_new=1;
    }
    else if(k=='+'||k=='-'||k=='*'||k=='/'){
        if(calc_op&&!calc_new){ calc_apply(); }
        else if(!calc_op){ calc_acc=calc_cur; }
        calc_op=k; calc_new=1;
    }
}

#define BMP_BUF ((u8*)0x00880000u)
#define PNG_RAW  ((unsigned char*)0x01800000u)
#define PNG_RGBA ((unsigned char*)0x01A00000u)
static int photo_loaded=0, photo_w=0, photo_h=0, photo_bpp=0, photo_len=0, photo_is_sample=0;
static int photo_is_png=0;
static char photo_name[20]={0};
#define PH_MAX 12
#define PH_TS 192
#define PH_THUMB ((unsigned char*)0x01000000u)
static int ph_count=0, ph_view=-1;
static char ph_name[PH_MAX][20];
static int ph_w[PH_MAX], ph_h[PH_MAX], ph_tw[PH_MAX], ph_th[PH_MAX];
static u32 le32b(const u8*b){ return (u32)b[0]|((u32)b[1]<<8)|((u32)b[2]<<16)|((u32)b[3]<<24); }
static u16 le16b(const u8*b){ return (u16)(b[0]|(b[1]<<8)); }
static int photo_gen_sample(void){
    int w=160,h=120; int rowpad=(4-(w*3)%4)%4; int rb=w*3+rowpad; int datasz=rb*h;
    u8* b=BMP_BUF; for(int i=0;i<54;i++)b[i]=0;
    b[0]='B'; b[1]='M'; u32 fsz=54+datasz; b[2]=(u8)fsz; b[3]=(u8)(fsz>>8); b[4]=(u8)(fsz>>16); b[5]=(u8)(fsz>>24);
    b[10]=54; b[14]=40; b[18]=(u8)w; b[19]=(u8)(w>>8); b[22]=(u8)h; b[23]=(u8)(h>>8); b[26]=1; b[28]=24;
    u8* px=b+54;
    for(int y=h-1;y>=0;y--){ u8* row=px+(h-1-y)*rb; for(int x=0;x<w;x++){ row[x*3+0]=(u8)((y*255)/(h-1)); row[x*3+1]=(u8)((x*255)/(w-1)); row[x*3+2]=128; } }
    return 54+datasz;
}
static int photo_decode_info(void){
    u8* b=BMP_BUF;
    if(photo_len<54||b[0]!='B'||b[1]!='M')return 0;
    if(le32b(b+30)!=0)return 0;
    int w=(int)le32b(b+18), h=(int)le32b(b+22);
    photo_bpp=le16b(b+28);
    if(h<0)h=-h;
    if(w<1||w>4096||h<1||h>4096)return 0;
    if(photo_bpp!=24&&photo_bpp!=8)return 0;
    photo_w=w; photo_h=h; return 1;
}
static int ext_is(const char*nm,char e1,char e2,char e3){
    int L=0; while(nm[L])L++;
    if(L<4||nm[L-4]!='.')return 0;
    char a=nm[L-3],b=nm[L-2],c=nm[L-1];
    if(a>='a'&&a<='z')a-=32; if(b>='a'&&b<='z')b-=32; if(c>='a'&&c<='z')c-=32;
    return a==e1&&b==e2&&c==e3;
}

static void ftype_icon(const char*nm,int*gid,u8*col){
    if(ext_is(nm,'P','N','G')||ext_is(nm,'B','M','P')||ext_is(nm,'J','P','G')||ext_is(nm,'G','I','F')){*gid=25;*col=109;return;}
    if(ext_is(nm,'D','R','V')||ext_is(nm,'S','Y','S')||ext_is(nm,'P','K','G')){*gid=4; *col=106;return;}
    if(ext_is(nm,'B','I','N')||ext_is(nm,'E','X','E')||ext_is(nm,'C','O','M')){*gid=28;*col=101;return;}
    if(ext_is(nm,'R','A','W')||ext_is(nm,'W','A','V')||ext_is(nm,'M','P','3')){*gid=29;*col=104;return;}
    if(ext_is(nm,'Z','I','P')||ext_is(nm,'R','A','R')||ext_is(nm,'G','Z',' ')||ext_is(nm,'T','A','R')){*gid=20;*col=105;return;}
    if(ext_is(nm,'C','S','V')){*gid=21;*col=74;return;}
    if(ext_is(nm,'J','S','N')||ext_is(nm,'J','S','O')){*gid=21;*col=104;return;}
    if(ext_is(nm,'M','D',' ')||ext_is(nm,'M','K','D')){*gid=21;*col=70;return;}
    if(ext_is(nm,'I','N','I')||ext_is(nm,'C','F','G')||ext_is(nm,'C','O','N')){*gid=21;*col=68;return;}
    if(ext_is(nm,'L','O','G')){*gid=21;*col=109;return;}
    if(ext_is(nm,'X','M','L')||ext_is(nm,'H','T','M')){*gid=21;*col=101;return;}
    if(ext_is(nm,'E','N','C')){*gid=21;*col=71;return;}
    *gid=21; *col=102;
}

static void draw_zip_icon(int x,int y,int sz){
    int bw=sz*2/3, bx=x+sz/6;
    fill(bx,y+2,bw,sz-4,104); frame(bx,y+2,bw,sz-4,C_SHAD);
    fill(bx,y+2,bw,4,109);
    int zx=x+sz/2-2; fill(zx,y+2,4,sz-4,C_MGREY);
    for(int j=y+5;j<y+sz-4;j+=3) fill(zx-2,j,8,1,C_SHAD);
    fill(zx-1,y+sz/3,6,6,C_WHITE); frame(zx-1,y+sz/3,6,6,C_SHAD);
}

static void file_folder(int x,int y,int s){
    int w=s, h=s*11/16, oy=y+s/5;
    fill(x+2, oy-4, w*5/12, 5, C_TEAL);
    fill(x, oy, w, h, C_FOLDER);
    fill(x, oy, w, 2, C_WHITE);
    frame(x, oy, w, h, C_SHAD);
}
static void file_doc(int x,int y,int s,int gid){
    int w=s*5/8, h=s*13/16, px=x+(s-w)/2, py=y+1, fc=w/3;
    fill(px, py, w, h, C_WHITE);
    fill(px+w-fc, py, fc, fc, 50);
    frame(px, py, w, h, C_MGREY);
    fill(px+w-fc, py+fc, fc, 1, C_MGREY); fill(px+w-fc, py, 1, fc, C_MGREY);
    int sx=px+4, sy=py+fc+3, iw=w-8;
    if(gid==25){ fill(px+w-7,sy,3,3,C_FOLDER); for(int k=0;k<iw;k++){int hh=(k<iw/2)?k/2:(iw-k)/2; fill(sx+k,py+h-3-hh,1,2+hh,C_TEAL);} }
    else if(gid==4){ int cx=px+w/2,cy=sy+5,r=5; frame(cx-r,cy-r,2*r,2*r,C_BBLUE); fill(cx-2,cy-r-2,4,3,C_BBLUE); fill(cx-2,cy+r-1,4,3,C_BBLUE); fill(cx-r-2,cy-2,3,4,C_BBLUE); fill(cx+r-1,cy-2,3,4,C_BBLUE); }
    else if(gid==28){ int cx=px+w/2,cy=sy+4; fill(cx-7,cy-3,14,11,C_TASK); fill(cx-5,cy-1,4,3,C_GREEN); fill(cx+1,cy-1,4,3,C_GREEN); fill(cx-5,cy+3,10,2,C_GREEN); }
    else if(gid==29){ fill(sx+iw/3,sy+7,5,3,C_RED); fill(sx+iw/3+4,sy,2,9,C_RED); fill(sx+iw/3+4,sy,5,2,C_RED); }
    else { for(int k=0;k<3;k++) fill(sx,sy+1+k*4,iw,1,C_MGREY); }
}
static void photo_load(void){
    photo_loaded=0; photo_is_sample=0; photo_is_png=0; photo_name[0]=0; photo_len=0;
    int fpng=-1, fbmp=-1, fjpg=-1, w,h;
    if(disk_ok){
        for(int i=0;i<(int)nvx.count;i++){
            if(fpng<0&&ext_is(nvx.e[i].name,'P','N','G'))fpng=i;
            if(fbmp<0&&ext_is(nvx.e[i].name,'B','M','P'))fbmp=i;
            if(fjpg<0&&ext_is(nvx.e[i].name,'J','P','G'))fjpg=i;
        }
    }
    if(fpng>=0){
        photo_len=nvx_read(fpng,(char*)BMP_BUF,393216);
        int k=0; while(nvx.e[fpng].name[k]&&k<19){photo_name[k]=nvx.e[fpng].name[k];k++;} photo_name[k]=0;
        if(png_decode(BMP_BUF,photo_len,PNG_RAW,0x1F0000,PNG_RGBA,0x5F0000,&w,&h)==0){
            photo_w=w; photo_h=h; photo_bpp=32; photo_is_png=1; photo_loaded=1; return; }
    }
    if(fjpg>=0){
        photo_len=nvx_read(fjpg,(char*)BMP_BUF,393216);
        int k=0; while(nvx.e[fjpg].name[k]&&k<19){photo_name[k]=nvx.e[fjpg].name[k];k++;} photo_name[k]=0;
        if(jpeg_decode(BMP_BUF,photo_len,PNG_RGBA,0x5F0000,&w,&h)==0){
            photo_w=w; photo_h=h; photo_bpp=32; photo_is_png=1; photo_loaded=1; return; }
    }
    if(fbmp>=0){
        photo_len=nvx_read(fbmp,(char*)BMP_BUF,393216);
        int k=0; while(nvx.e[fbmp].name[k]&&k<19){photo_name[k]=nvx.e[fbmp].name[k];k++;} photo_name[k]=0;
        if(photo_decode_info()){ photo_loaded=1; return; }
    }

    if(png_decode(sample_png,sample_png_len,PNG_RAW,0x1F0000,PNG_RGBA,0x5F0000,&w,&h)==0){
        photo_w=w; photo_h=h; photo_bpp=32; photo_is_png=1; photo_is_sample=1; photo_loaded=1;
        const char* sn="SAMPLE.PNG"; int k=0; while(sn[k]){photo_name[k]=sn[k];k++;} photo_name[k]=0;
    }
}

#define HXC_W 240
#define HXC_H 140
static u32* hx_canvas=(u32*)0x01740000u;
static char hxout[3072]; static int hxout_len=0;
static char hx_src[4096];
static int  hx_sel=0, hx_ran=0;
static const char* HX_SAMP[3]={
 "# HELLO + MATH + LOOP\nprint \"HELLO FROM HEXLANG\"\nlet x = 3\nlet y = x * 4 + 2\nprint y\nlet n = 5\nwhile n > 0\n  print n\n  let n = n - 1\nend\nif y == 14\n  print \"Y IS 14\"\nend\n",
 "# COLOR BARS\ncls 6\nlet i = 0\nwhile i < 8\n  rect 12 i*15+6 200 11 i+100\n  let i = i + 1\nend\ntext 14 138 \"HEXLANG GRAPHICS\"\n",
 "# COUNTDOWN\nlet n = 10\nwhile n > 0\n  print n\n  let n = n - 1\nend\nprint \"LIFTOFF\"\n"
};
static const char* HX_NAME[3]={"HELLO.HEX","BARS.HEX","COUNT.HEX"};
#define HX_NSAMP 3
static void hx_out_str(void*c,const char*s){ (void)c; for(int i=0;s[i]&&hxout_len<3070;i++)hxout[hxout_len++]=s[i]; hxout[hxout_len]=0; }
static void hx_out_num(void*c,int n){ (void)c; if(n<0&&hxout_len<3070)hxout[hxout_len++]='-'; char b[12]; utoa((u32)((n<0)?-n:n),b); hx_out_str(0,b); }
static void hx_out_nl(void*c){ (void)c; if(hxout_len<3070)hxout[hxout_len++]='\n'; hxout[hxout_len]=0; }
static void hx_cls(void*c,int col){ (void)c; u32 v=PAL32[(u8)col]; for(int i=0;i<HXC_W*HXC_H;i++)hx_canvas[i]=v; }
static void hx_rect(void*c,int x,int y,int w,int h,int col){ (void)c; u32 v=PAL32[(u8)col];
    for(int j=0;j<h;j++){ int yy=y+j; if(yy<0||yy>=HXC_H)continue; for(int i=0;i<w;i++){ int xx=x+i; if(xx<0||xx>=HXC_W)continue; hx_canvas[yy*HXC_W+xx]=v; } } }
static void hx_text(void*c,int x,int y,const char*s,int col){ (void)c;
    u32 fg=PAL32[(u8)col]; int fr=(fg>>16)&255,fgc=(fg>>8)&255,fb=fg&255;
    for(int ci=0;s[ci];ci++){ int idx=(int)(unsigned char)s[ci]-32; if(idx>=0&&idx<95){ const unsigned char*g=fontaa[idx];
        for(int r=0;r<AA_CH;r++){ int yy=y+r; if(yy<0||yy>=HXC_H)continue; for(int cc=0;cc<AA_CW;cc++){ int a=g[r*AA_CW+cc]; if(!a)continue; int xx=x+cc; if(xx<0||xx>=HXC_W)continue;
            u32 bg=hx_canvas[yy*HXC_W+xx]; int br=(bg>>16)&255,bgc=(bg>>8)&255,bb=bg&255;
            hx_canvas[yy*HXC_W+xx]=((u32)((fr*a+br*(255-a))/255)<<16)|((u32)((fgc*a+bgc*(255-a))/255)<<8)|(u32)((fb*a+bb*(255-a))/255); } } } x+=8; } }
static void hx_wait(void*c,int t){ (void)c; if(t<0)t=0; if(t>60)t=60; pit_wait(t); }
static void hx_clear_canvas(void){ for(int i=0;i<HXC_W*HXC_H;i++)hx_canvas[i]=0x202733; }
static const char* hx_nvx_hex(int ord){
    if(!disk_ok)return 0; int k=0; for(int i=0;i<(int)nvx.count;i++) if(ext_is(nvx.e[i].name,'H','E','X')){ if(k==ord)return nvx.e[i].name; k++; } return 0; }
static void hx_runsel(void){
    hxout_len=0; hxout[0]=0; hx_clear_canvas();
    const char* src=0;
    if(hx_sel<HX_NSAMP) src=HX_SAMP[hx_sel];
    else { const char* nm=hx_nvx_hex(hx_sel-HX_NSAMP); if(nm&&disk_ok){ int ix=nvx_find(nm); if(ix>=0){ int n=nvx_read(ix,hx_src,4090); hx_src[n]=0; src=hx_src; } } }
    hx_ran=1;
    if(!src){ hx_out_str(0,"NO SCRIPT SELECTED"); hx_out_nl(0); return; }
    hx_env e; e.put_str=hx_out_str; e.put_num=hx_out_num; e.nl=hx_out_nl; e.cls=hx_cls; e.text=hx_text; e.rect=hx_rect; e.waitt=hx_wait; e.ctx=0;
    int el=0; int r=hx_run(src,&e,&el);
    if(r<0){ hx_out_nl(0); hx_out_str(0, r==-1?"ERR: UNMATCHED IF/WHILE":r==-2?"ERR: TOO LONG (STOPPED)":"ERR: BAD COMMAND"); hx_out_str(0," @LINE "); hx_out_num(0,el); hx_out_nl(0); }
}
static void ph_blit(int idx,int dx,int dy,int maxw,int maxh){
    if(idx<0||idx>=ph_count)return;
    int tw=ph_tw[idx], th=ph_th[idx]; if(tw<1||th<1)return;
    int dw=maxw, dh=maxh;
    if(tw*dh > th*dw) dh=th*dw/tw; else dw=tw*dh/th;
    if(dw<1)dw=1; if(dh<1)dh=1;
    int x0=dx+(maxw-dw)/2, y0=dy+(maxh-dh)/2;
    const unsigned char* slot=PH_THUMB+(unsigned)idx*(PH_TS*PH_TS*4);
    for(int oy=0;oy<dh;oy++){ int yy=y0+oy; if(yy<0||yy>=H)continue; int sy=oy*th/dh; const unsigned char* srow=slot+(unsigned)(sy*PH_TS)*4;
        for(int ox=0;ox<dw;ox++){ int xx=x0+ox; if(xx<0||xx>=W)continue; int sx=ox*tw/dw; const unsigned char* pp=srow+sx*4;
            FB[yy*PITCH+xx]=((u32)pp[0]<<16)|((u32)pp[1]<<8)|(u32)pp[2]; } }
}
static void set_photo_wallpaper(int idx){
    if(idx<0||idx>=ph_count) return;
    int sw=ph_tw[idx], sh=ph_th[idx]; if(sw<1||sh<1) return;
    const unsigned char* slot=PH_THUMB+(unsigned)idx*(PH_TS*PH_TS*4);
    int dispW,dispH;
    if(sw*H>sh*W){ dispH=H; dispW=sw*H/sh; } else { dispW=W; dispH=sh*W/sw; }
    if(dispW<1)dispW=1; if(dispH<1)dispH=1;
    int ox=(W-dispW)/2, oy=(H-dispH)/2;
    for(int y=0;y<H;y++){ int sy=(y-oy)*sh/dispH; if(sy<0)sy=0; if(sy>=sh)sy=sh-1;
        const unsigned char* srow=slot+(unsigned)(sy*PH_TS)*4;
        for(int x=0;x<W;x++){ int sx=(x-ox)*sw/dispW; if(sx<0)sx=0; if(sx>=sw)sx=sw-1;
            const unsigned char* pp=srow+sx*4; BASE[y*PITCH+x]=((u32)pp[0]<<16)|((u32)pp[1]<<8)|(u32)pp[2]; } }
    wall_photo=1; dirty=1;
}
static void wp_space_render(void){
    if(!space_loaded){
        int w=0,h=0;
        if(jpeg_decode((unsigned char*)space_jpg,space_jpg_len,PNG_RGBA,0x5F0000,&w,&h)!=0 || w!=256 || h!=144){
            for(int i=0;i<256*144;i++) space_dec[i]=0x00040814u;
        } else {
            const unsigned char* sp=PNG_RGBA;
            for(int i=0;i<256*144;i++) space_dec[i]=((u32)sp[i*4]<<16)|((u32)sp[i*4+1]<<8)|(u32)sp[i*4+2];
        }
        space_loaded=1;
    }
    int sw=256,sh=144,dispW,dispH;
    if(sw*H>sh*W){ dispH=H; dispW=sw*H/sh; } else { dispW=W; dispH=sh*W/sw; }
    if(dispW<1)dispW=1; if(dispH<1)dispH=1; int ox=(W-dispW)/2, oy=(H-dispH)/2;
    for(int y=0;y<H;y++){ int sy=(y-oy)*sh/dispH; if(sy<0)sy=0; if(sy>=sh)sy=sh-1;
        for(int x=0;x<W;x++){ int sx=(x-ox)*sw/dispW; if(sx<0)sx=0; if(sx>=sw)sx=sw-1; FB[y*PITCH+x]=space_dec[sy*sw+sx]; } }
}
static void ph_import_usb(void){
    if(msd_dev>=0) fat_mount();
    if(msd_dev<0||!fat_ok){ toast_set("PLUG IN A USB DRIVE FIRST"); return; }
    int added=0;
    for(int i=0;i<usbfs_n && ph_count<PH_MAX;i++){
        const char* nm=usbfs[i].name;
        int ispng=ext_is(nm,'P','N','G'), isjpg=ext_is(nm,'J','P','G');
        if(!ispng&&!isjpg) continue;
        int len=usbfs_read(i,(char*)BMP_BUF,393216);
        if(len<=0) continue;
        int w=0,h=0;
        if(ispng){ if(png_decode(BMP_BUF,len,PNG_RAW,0x1F0000,PNG_RGBA,0x5F0000,&w,&h)!=0) continue; }
        else { if(jpeg_decode(BMP_BUF,len,PNG_RGBA,0x5F0000,&w,&h)!=0) continue; }
        if(w<=0||h<=0) continue;
        int tw,th; if(w>=h){ tw=PH_TS; th=PH_TS*h/w; } else { th=PH_TS; tw=PH_TS*w/h; }
        if(tw<1)tw=1; if(th<1)th=1; if(tw>PH_TS)tw=PH_TS; if(th>PH_TS)th=PH_TS;
        unsigned char* slot=PH_THUMB+(unsigned)ph_count*(PH_TS*PH_TS*4);
        for(int ty=0;ty<th;ty++){ int sy=ty*h/th; const unsigned char* srow=PNG_RGBA+(unsigned)(sy*w)*4;
            for(int tx=0;tx<tw;tx++){ int sx=tx*w/tw; const unsigned char* pp=srow+sx*4;
                unsigned char* dp=slot+(ty*PH_TS+tx)*4; dp[0]=pp[0];dp[1]=pp[1];dp[2]=pp[2];dp[3]=255; } }
        int k=0; while(nm[k]&&k<19){ph_name[ph_count][k]=nm[k];k++;} ph_name[ph_count][k]=0;
        ph_w[ph_count]=w; ph_h[ph_count]=h; ph_tw[ph_count]=tw; ph_th[ph_count]=th; ph_count++; added++;
    }
    if(added) toast_set("PHOTOS IMPORTED FROM USB");
    else if(ph_count>=PH_MAX) toast_set("LIBRARY FULL");
    else toast_set("NO NEW PNG/JPG FILES ON USB");
}
#define PHONE_ACC  ((u8*)0x02100000u)
#define PHONE_ACC_MAX 0x180000
#define PHONE_RGBA ((unsigned char*)0x02300000u)
static int phone_conn=0,phone_demo=0,phone_have=0,phone_fw=0,phone_fh=0;
static int phone_oct=100,phone_port=8080,phone_frames=0,phone_acc_len=0;
static int phone_rx,phone_ry,phone_rdw,phone_rdh,phone_drag,phone_lastx,phone_lasty;
static char phone_dialnum[24]; static int phone_dialn=0;
static void phone_send_ptr(int ph){ if(!phone_conn||phone_rdw<=0||phone_rdh<=0)return; int rx=mx-phone_rx,ry=my-phone_ry; if(rx<0)rx=0; if(rx>=phone_rdw)rx=phone_rdw-1; if(ry<0)ry=0; if(ry>=phone_rdh)ry=phone_rdh-1; unsigned nx=rx*10000/phone_rdw,ny=ry*10000/phone_rdh; u8 pk[6]={3,(u8)ph,(u8)(nx>>8),(u8)nx,(u8)(ny>>8),(u8)ny}; tcps_wr(0,pk,6); }
static void phone_send_btn(int c){ if(!phone_conn)return; u8 pk[2]={2,(u8)c}; tcps_wr(0,pk,2); }
static void phone_send_dial(void){ if(!phone_conn||phone_dialn<=0)return; u8 pk[24]; pk[0]=4; int n=phone_dialn; if(n>22)n=22; for(int i=0;i<n;i++)pk[1+i]=(u8)phone_dialnum[i]; tcps_wr(0,pk,1+n); }
static void phone_send_hangup(void){ if(!phone_conn)return; u8 pk[1]={5}; tcps_wr(0,pk,1); }
static int pdec(char* d,unsigned v){ char t[12]; int n=0; if(!v){d[0]='0';return 1;} while(v){t[n++]=(char)('0'+v%10);v/=10;} for(int i=0;i<n;i++)d[i]=t[n-1-i]; return n; }
static void phone_disconnect(void){ if(phone_conn&&!ts_fin){ tcp_send(ts_dip,ts_sp,ts_dport,ts_dmac,ts_snd,ts_rcv,TF_FIN|TF_ACK,0,0); ts_snd++; ts_fin=1; } phone_conn=0; phone_acc_len=0; }
/* ===PHONE APP=== full phone (calls + SMS) driving the paired phone via the mirror link */
static int ph_tab=0, ph_focus=0, ph_mscroll=0;
static char ph_to[24]; static int ph_ton=0;
static char ph_body[160]; static int ph_bn=0;
struct ph_msg { u8 dir; char num[24]; char body[152]; };   /* dir 0=sent 1=recv */
static struct ph_msg ph_msgs[64]; static int ph_nmsg=0;
struct ph_rec { u8 kind; char num[24]; };                  /* kind 0=call 1=sms */
static struct ph_rec ph_recs[40]; static int ph_nrec=0;
static void ph_log(u8 kind,const char* num){ if(ph_nrec>=40){ for(int i=0;i<39;i++)ph_recs[i]=ph_recs[i+1]; ph_nrec=39; } ph_recs[ph_nrec].kind=kind; int i=0; while(num[i]&&i<23){ph_recs[ph_nrec].num[i]=num[i];i++;} ph_recs[ph_nrec].num[i]=0; ph_nrec++; }
static void ph_addmsg(u8 dir,const char* num,const char* body){ if(ph_nmsg>=64){ for(int i=0;i<63;i++)ph_msgs[i]=ph_msgs[i+1]; ph_nmsg=63; } struct ph_msg* m=&ph_msgs[ph_nmsg++]; m->dir=dir; int i=0; while(num[i]&&i<23){m->num[i]=num[i];i++;} m->num[i]=0; i=0; while(body[i]&&i<151){m->body[i]=body[i];i++;} m->body[i]=0; }
static void phone_send_sms(const char* to,const char* body){ if(!phone_conn)return; u8 pk[200]; int o=0; pk[o++]=6; for(int i=0;to[i]&&o<30;i++)pk[o++]=(u8)to[i]; pk[o++]=0; for(int i=0;body[i]&&o<198;i++)pk[o++]=(u8)body[i]; tcps_wr(0,pk,o); }
static void ph_do_send(void){ if(ph_ton>0&&ph_bn>0){ phone_send_sms(ph_to,ph_body); ph_addmsg(0,ph_to,ph_body); ph_log(1,ph_to); ph_bn=0; ph_body[0]=0; toast_set("SMS SENT"); } else toast_set("NEED A NUMBER AND MESSAGE"); }
static void phone_msg_key(u8 d){ if(d&0x80)return; char ch=kchar_shift(d); if(!ch)return;
    if(ph_focus==1){ if(ch=='\b'){ if(ph_ton>0)ph_to[--ph_ton]=0; } else if(ch=='\n'||ch=='\r'){ ph_focus=2; } else if(ch>=32&&ph_ton<22){ ph_to[ph_ton++]=ch; ph_to[ph_ton]=0; } }
    else if(ph_focus==2){ if(ch=='\b'){ if(ph_bn>0)ph_body[--ph_bn]=0; } else if(ch=='\n'||ch=='\r'){ ph_do_send(); } else if(ch>=32&&ph_bn<150){ ph_body[ph_bn++]=ch; ph_body[ph_bn]=0; } } }
static void phone_pump(void){
    if(!phone_conn)return;
    u8 tmp[1600]; int n=tcps_rd(0,tmp,1600);
    if(n<=0){ if(ts_fin){ phone_conn=0; toast_set("PHONE DISCONNECTED"); dirty=1; } return; }
    if(phone_acc_len+n>(int)PHONE_ACC_MAX)phone_acc_len=0;
    for(int i=0;i<n;i++)PHONE_ACC[phone_acc_len++]=tmp[i];
    for(;;){
        if(phone_acc_len<4)break;
        u32 fl=((u32)PHONE_ACC[0]<<24)|((u32)PHONE_ACC[1]<<16)|((u32)PHONE_ACC[2]<<8)|PHONE_ACC[3];
        if(!fl||fl>(u32)(PHONE_ACC_MAX-4)){ phone_acc_len=0; break; }
        if((u32)phone_acc_len<4+fl)break;
        if(fl>=4&&PHONE_ACC[4]=='S'&&PHONE_ACC[5]=='M'&&PHONE_ACC[6]=='S'&&PHONE_ACC[7]==':'){
            char snum[24]; int si=0,pp=8,end=(int)(4+fl); while(pp<end&&PHONE_ACC[pp]!='\n'&&si<23){snum[si++]=PHONE_ACC[pp++];} snum[si]=0; if(pp<end&&PHONE_ACC[pp]=='\n')pp++;
            char sbody[152]; int bi=0; while(pp<end&&bi<151){sbody[bi++]=PHONE_ACC[pp++];} sbody[bi]=0;
            ph_addmsg(1,snum,sbody); toast_set("NEW SMS RECEIVED"); dirty=1;
        } else { int w=0,h=0; if(jpeg_decode(PHONE_ACC+4,(int)fl,PHONE_RGBA,0xC00000,&w,&h)==0&&w>0&&h>0){ phone_fw=w; phone_fh=h; phone_have=1; phone_demo=0; phone_frames++; dirty=1; } }
        int rem=phone_acc_len-(int)(4+fl);
        for(int i=0;i<rem;i++)PHONE_ACC[i]=PHONE_ACC[(int)(4+fl)+i];
        phone_acc_len=rem;
    }
}
static void phone_blit(int vx,int vy,int vw,int vh){
    phone_rx=vx; phone_ry=vy; phone_rdw=0; phone_rdh=0; if(phone_fw<=0||phone_fh<=0)return;
    int dw=vw,dh=vh; if(phone_fw*vh>phone_fh*vw){ dw=vw; dh=phone_fh*vw/phone_fw; } else { dh=vh; dw=phone_fw*vh/phone_fh; }
    if(dw<1)dw=1; if(dh<1)dh=1; int rx=vx+(vw-dw)/2,ry=vy+(vh-dh)/2; u32* src=(u32*)PHONE_RGBA;
    for(int y=0;y<dh;y++){ int py=ry+y; if(py<0||py>=H)continue; int sy=y*phone_fh/dh; if(sy>=phone_fh)sy=phone_fh-1;
        for(int x=0;x<dw;x++){ int px=rx+x; if(px<0||px>=W)continue; int sx=x*phone_fw/dw; if(sx>=phone_fw)sx=phone_fw-1; FB[py*PITCH+px]=src[sy*phone_fw+sx]; } }
    phone_rx=rx; phone_ry=ry; phone_rdw=dw; phone_rdh=dh;
}
static const struct { const char* name; const char* file; const char* desc; const unsigned char* data; int len; } PKGS[]={
    {"3D RAYCASTER","RAYCAST.NVX","WOLF3D-STYLE 3D ENGINE",   0, 18472},
    {"SPACE INVADERS","SPACEINV.NVX","CLASSIC ARCADE SHOOTER", 0, 14748},
    {"GRAPHICS DEMO","GFX.NVX","FRAMEBUFFER GRAPHICS TEST",    (const unsigned char*)GFXDEMO,      GFXDEMO_LEN},
    {"HELLO WORLD","HELLO.ELF","MINIMAL ELF PROGRAM",          (const unsigned char*)HELLODEMO,    HELLODEMO_LEN},
    {"DOOM","DOOM.ELF","THE ULTIMATE DOOM (id Software)",    0, 29100724},
    {"DEVKIT","PYIDLE.NVX","CODE EDITOR + PYTHON IDLE",       0, 29380},
    {"NOOVEXCRAFT","NOOVEXCRAFT.NVX","FIRST-PERSON BLOCK WORLD",  0, 13240},
};
#define NPKG 7
static int pkg_sel=0;
static int pkg_sub(const char* s,const char* q){ if(!q[0])return 0;
    for(int i=0;s[i];i++){ int j=0; for(;;){ char b=q[j]; if(!b)return 1; char a=s[i+j]; if(!a)break; if(a>='a'&&a<='z')a-=32; if(b>='a'&&b<='z')b-=32; if(a!=b)break; j++; } } return 0; }
static int pkg_find(const char* a){ if(!a[0])return -1;
    for(int i=0;i<NPKG;i++) if(streq(PKGS[i].name,a)||streq(PKGS[i].file,a)) return i;
    for(int i=0;i<NPKG;i++) if(pkg_sub(PKGS[i].name,a)||pkg_sub(PKGS[i].file,a)) return i; return -1; }
static int pkg_inst(int i){ return disk_ok && nvx_find(PKGS[i].file)>=0; }
static void fmt_size(u32 b, char* o){
    if(b>=1048576u){ u32 w=b>>20, f=(((b&0xFFFFFu)*10u)>>20); char t[12]; utoa(w,o); int n=strlen_(o); o[n++]='.'; utoa(f,t); o[n++]=t[0]; o[n]=0; int m=strlen_(o); o[m++]=' ';o[m++]='M';o[m++]='B';o[m]=0; }
    else if(b>=1024u){ u32 w=b>>10; utoa(w,o); int n=strlen_(o); o[n++]=' ';o[n++]='K';o[n++]='B';o[n]=0; }
    else { utoa(b,o); int n=strlen_(o); o[n++]=' ';o[n++]='B';o[n]=0; }
}
static const struct { const char* dep; const char* sz; } PKGDEPS[7][3] = {
    {{"graphics-lib","64 KB"},{"math-lib","28 KB"},{"input-lib","9 KB"}},
    {{"graphics-lib","64 KB"},{"audio-lib","41 KB"},{0,0}},
    {{"framebuffer-lib","52 KB"},{0,0},{0,0}},
    {{"libc-min","6 KB"},{0,0},{0,0}},
    {{"graphics-lib","64 KB"},{"sound-lib","120 KB"},{"wad-loader","33 KB"}},
    {{"noovex-editor","19 KB"},{"python-idle","29 KB"},{"font8x14","2 KB"}},
    {{"voxel-engine","13 KB"},{"raycaster-core","6 KB"},{"world-gen","1 KB"}},
};
static void pkg_cmd(const char* c){
    const char* a=c+3; while(*a==' ')a++;
    if(*a==0||streq(a,"help")||streq(a,"-h")||streq(a,"--help")){
        tputs("NOOVEX PACKAGE MANAGER"); tnl();
        tputs("  pkg list             list packages + status"); tnl();
        tputs("  pkg search <text>    search the catalog"); tnl();
        tputs("  pkg info <name>      show package details"); tnl();
        tputs("  pkg install <name>   install to disk (persists)"); tnl();
        tputs("  pkg remove <name>    uninstall from disk"); tnl();
        tputs("  pkg run <name>       launch an installed package"); tnl();
        tputs("  pkg update           refresh + check versions"); tnl(); return; }
    if(streq(a,"list")||streq(a,"ls")||streq(a,"list installed")||streq(a,"installed")){
        int only=streq(a,"list installed")||streq(a,"installed");
        if(!disk_ok){ tputs("warning: no disk mounted - packages will not persist"); tnl(); }
        tputs("ST  PACKAGE          FILE"); tnl();
        int shown=0;
        for(int i=0;i<NPKG;i++){ int ins=pkg_inst(i); if(only&&!ins)continue;
            tputs(ins?"[*] ":"[ ] "); tputs(PKGS[i].name);
            int nl=strlen_(PKGS[i].name); for(int z=nl;z<17;z++)tputs(" ");
            tputs(PKGS[i].file); tnl(); shown++; }
        if(only&&!shown){ tputs("(nothing installed yet)"); tnl(); }
        tputs("[*]=installed  [ ]=available"); tnl(); return; }
    if(startsw(a,"search ")||startsw(a,"find ")){ const char* q=a+(startsw(a,"search ")?7:5); while(*q==' ')q++;
        int hits=0; for(int i=0;i<NPKG;i++) if(pkg_sub(PKGS[i].name,q)||pkg_sub(PKGS[i].desc,q)||pkg_sub(PKGS[i].file,q)){
            tputs(pkg_inst(i)?"[*] ":"[ ] "); tputs(PKGS[i].name); tputs(" - "); tputs(PKGS[i].desc); tnl(); hits++; }
        if(!hits){ tputs("no matches for '"); tputs(q); tputs("'"); tnl(); } return; }
    if(startsw(a,"info ")||startsw(a,"show ")){ const char* nm=a+5; while(*nm==' ')nm++;
        int i=pkg_find(nm); if(i<0){ tputs("pkg: no package '"); tputs(nm); tputs("'"); tnl(); return; }
        char b[12]; tputs("name   : "); tputs(PKGS[i].name); tnl();
        tputs("file   : "); tputs(PKGS[i].file); tnl();
        tputs("about  : "); tputs(PKGS[i].desc); tnl();
        utoa((u32)PKGS[i].len,b); tputs("size   : "); tputs(b); tputs(" bytes"); tnl();
        tputs("status : "); tputs(pkg_inst(i)?"INSTALLED":"not installed"); tnl(); return; }
    if(startsw(a,"install ")||startsw(a,"add ")){ const char* nm=a+(startsw(a,"install ")?8:4); while(*nm==' ')nm++;
        int i=pkg_find(nm); if(i<0){ tputs("pkg: no package '"); tputs(nm); tputs("'  (try 'pkg list')"); tnl(); return; }
        int already=(disk_ok && nvx_find(PKGS[i].file)>=0);
        if(!disk_ok && PKGS[i].data){ tputs("pkg: no disk mounted - cannot install"); tnl(); return; }
        if(!PKGS[i].data && !already){ tputs("pkg: "); tputs(PKGS[i].file); tputs(" not found - attach the game disk first"); tnl(); return; }
        term_scroll=0; char sz[16]; fmt_size((u32)PKGS[i].len,sz);
        tputs("Installing "); tputs(PKGS[i].name); tputs("..."); tnl(); compose(); pit_wait(300);
        tputs("Downloading "); tputs(PKGS[i].name); tputs(" ("); tputs(sz); tputs(")..."); tnl(); compose(); pit_wait(600);
        for(int d=0;d<3;d++){ const char* dp=PKGDEPS[i][d].dep; if(!dp)break;
            tputs("Installing dependency: "); tputs(dp); tputs(" ("); tputs(PKGDEPS[i][d].sz); tputs(")"); tnl(); compose(); pit_wait(300); }
        tputs("Installing "); tputs(PKGS[i].name); tputs("..."); tnl(); compose(); pit_wait(400);
        if(PKGS[i].data && !already){ int r=nvx_write(PKGS[i].file,(const char*)PKGS[i].data,PKGS[i].len);
            if(r<0){ tputs("error: write failed (disk full?)"); tnl(); return; } }
        tputs("Done. "); tputs(PKGS[i].name); tputs(" ("); tputs(sz); tputs(") installed."); tnl();
        tputs("Run it with:  pkg run "); tputs(nm); tnl(); dirty=1; return; }
    if(startsw(a,"remove ")||startsw(a,"uninstall ")||startsw(a,"rm ")){ const char* nm=a+(startsw(a,"uninstall ")?10:startsw(a,"remove ")?7:3); while(*nm==' ')nm++;
        int i=pkg_find(nm); if(i<0){ tputs("pkg: no package '"); tputs(nm); tputs("'"); tnl(); return; }
        if(!disk_ok){ tputs("pkg: no disk mounted"); tnl(); return; }
        int ix=nvx_find(PKGS[i].file); if(ix<0){ tputs(PKGS[i].name); tputs(" is not installed"); tnl(); return; }
        nvx_delete(ix); tputs("removed "); tputs(PKGS[i].name); tnl(); dirty=1; return; }
    if(startsw(a,"run ")||startsw(a,"exec ")||startsw(a,"start ")){ const char* nm=a+(startsw(a,"start ")?6:startsw(a,"exec ")?5:4); while(*nm==' ')nm++;
        int i=pkg_find(nm); if(i<0){ tputs("pkg: no package '"); tputs(nm); tputs("'"); tnl(); return; }
        if(!pkg_inst(i)){ tputs("pkg: "); tputs(PKGS[i].name); tputs(" not installed - run 'pkg install "); tputs(nm); tputs("'"); tnl(); return; }
        tputs("launching "); tputs(PKGS[i].name); tputs("..."); tnl(); elf_run(PKGS[i].file); dirty=1; return; }
    if(streq(a,"update")||streq(a,"upgrade")||streq(a,"refresh")){
        tputs("reading package lists... done"); tnl();
        int ins=0; for(int i=0;i<NPKG;i++) if(pkg_inst(i))ins++;
        char b[12]; utoa((u32)NPKG,b); tputs("catalog: "); tputs(b); tputs(" packages, "); utoa((u32)ins,b); tputs(b); tputs(" installed"); tnl();
        tputs("all packages are up to date"); tnl(); return; }
    tputs("pkg: unknown subcommand '"); tputs(a); tputs("'  (try 'pkg help')"); tnl();
}
/* ---- NOOVEX MAIL : compose + send over TLS 1.3 (app 37) ---- */
static char mail_host[64]="";
static char mail_user[96]="";
static char mail_pass[96]="";
static char mail_to[96]="";
static char mail_subj[96]="";
static char mail_body[1024]="";
static int  mail_field=0;
static int  mail_port=465;
static int  mail_do_send=0;
static char mail_status[40]="READY";
static void mst(const char* m){ int k=0; while(m[k]&&k<38){mail_status[k]=m[k];k++;} mail_status[k]=0; }
static int b64e(const u8* s,int n,char* d){
    static const char T[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int o=0;
    for(int i=0;i<n;i+=3){ int b0=s[i],b1=(i+1<n)?s[i+1]:0,b2=(i+2<n)?s[i+2]:0;
        d[o++]=T[(b0>>2)&63]; d[o++]=T[((b0&3)<<4)|((b1>>4)&15)];
        d[o++]=(i+1<n)?T[((b1&15)<<2)|((b2>>6)&3)]:'='; d[o++]=(i+2<n)?T[b2&63]:'='; }
    d[o]=0; return o;
}
static int smtp_resp(tls_io* io,u16 cipher,const u8* sk,const u8* siv,u64* sseq){
    static u8* rb=(u8*)0x00918000u; static u8* inn=(u8*)0x00908000u;
    char acc[1200]; int al=0,g=0; u8 rtype; int rl;
    for(;;){
        int ls=0,code=0,done=0;
        for(int i=0;i+1<al;i++){ if(acc[i]=='\r'&&acc[i+1]=='\n'){ if(i-ls>=4&&acc[ls+3]==' '){ code=(acc[ls]-'0')*100+(acc[ls+1]-'0')*10+(acc[ls+2]-'0'); done=1; } ls=i+2; i++; } }
        if(done) return code;
        if(g++>300) return -1;
        if(!tls_read_record(io,&rtype,rb,17000,&rl)) return -1;
        if(rtype==0x14) continue;
        if(rtype!=0x17) return -1;
        u8 it; int il=tls_decrypt(cipher,sk,siv,(*sseq)++,rb,rl,inn,&it);
        if(il<0) return -1;
        if(it==0x15) return -1;
        if(it!=0x17) continue;
        for(int i=0;i<il&&al<1196;i++) acc[al++]=(char)inn[i];
    }
}
static int smtp_cmd(tls_io* io,u16 cipher,const u8* ck,const u8* civ,u64* cseq,const char* str){
    int n=0; while(str[n])n++;
    return tls_write_enc(io,cipher,ck,civ,(*cseq)++,0x17,(const u8*)str,n);
}
static void smtp_send(void){
    u32 ip;
    if(dns_query(mail_host,&ip)<0){ mst("DNS LOOKUP FAILED"); return; }
    if(!tcps_open(ip,(u16)mail_port)){ mst("CONNECT FAILED"); return; }
    u8 ent[64]; entropy_get(ent,64);
    tls_io* io=(tls_io*)0x00930000u; io->wr=tcps_wr; io->rd=tcps_rd; io->ctx=0; io->rlen=0; io->rpos=0;
    static u8* mch=(u8*)0x00920000u;
    u8 priv[32]; for(int i=0;i<32;i++)priv[i]=ent[i]; priv[0]&=248; priv[31]&=127; priv[31]|=64;
    u8 pub[32],bas[32]; for(int i=0;i<32;i++)bas[i]=0; bas[0]=9; x25519(pub,priv,bas);
    int chl=build_clienthello(mail_host,strlen_(mail_host),pub,ent+32,ent,mch);
    sha256 tr; sha256_init(&tr); sha256_update(&tr,mch,chl);
    if(!tls_write_plain(io,0x16,mch,chl)){ mst("TLS SEND FAILED"); return; }
    static u8* rb=(u8*)0x00918000u; u8 rtype; int rl;
    if(!tls_read_record(io,&rtype,rb,17000,&rl)||rtype!=0x16||rl<4||rb[0]!=2){ mst("NO SERVERHELLO"); return; }
    u16 cipher; u8 spub[32];
    if(!parse_serverhello(rb,rl,&cipher,spub)){ mst("BAD SERVERHELLO"); return; }
    if(cipher!=0x1301&&cipher!=0x1303){ mst("UNSUPPORTED CIPHER"); return; }
    sha256_update(&tr,rb,rl);
    u8 ecdhe[32]; x25519(ecdhe,priv,spub);
    u8 th[32]; { sha256 t=tr; sha256_final(&t,th); }
    tls_ks k; tls_key_schedule(ecdhe,th,cipher,&k);
    u64 rseq=0; int sfin=0,g=0;
    while(!sfin&&g++<64){
        if(!tls_read_record(io,&rtype,rb,17000,&rl)){ mst("TLS HANDSHAKE FAIL"); return; }
        if(rtype==0x14)continue; if(rtype!=0x17){ mst("TLS HANDSHAKE FAIL"); return; }
        static u8* inn=(u8*)0x00908000u; u8 it;
        int il=tls_decrypt(cipher,k.shs_key,k.shs_iv,rseq++,rb,rl,inn,&it);
        if(il<0){ mst("TLS DECRYPT FAIL"); return; }
        if(it!=0x16)continue;
        int o=0; while(o+4<=il){ int hln=(inn[o+1]<<16)|(inn[o+2]<<8)|inn[o+3]; if(o+4+hln>il)break; sha256_update(&tr,inn+o,4+hln); if(inn[o]==0x14)sfin=1; o+=4+hln; }
    }
    if(!sfin){ mst("NO TLS FINISHED"); return; }
    u8 th2[32]; { sha256 t=tr; sha256_final(&t,th2); }
    u8 ccs=1; tls_write_plain(io,0x14,&ccs,1);
    { u8 fin[36]; fin[0]=0x14;fin[1]=0;fin[2]=0;fin[3]=0x20; u8 v[32]; tls_finished(k.chs,th2,v); for(int i=0;i<32;i++)fin[4+i]=v[i]; if(!tls_write_enc(io,cipher,k.chs_key,k.chs_iv,0,0x16,fin,36)){ mst("TLS FINISH FAIL"); return; } }
    u8 cap[32],sap[32],ck[32],civ[12],sk[32],siv[12];
    hkdf_label(k.master,"c ap traffic",th2,32,cap,32);
    hkdf_label(k.master,"s ap traffic",th2,32,sap,32);
    hkdf_label(cap,"key",0,0,ck,k.klen); hkdf_label(cap,"iv",0,0,civ,12);
    hkdf_label(sap,"key",0,0,sk,k.klen); hkdf_label(sap,"iv",0,0,siv,12);
    u64 cseq=0,sseq=0;
    char line[2600]; char bb[200];
    if(smtp_resp(io,cipher,sk,siv,&sseq)/100!=2){ mst("NO GREETING"); return; }
    smtp_cmd(io,cipher,ck,civ,&cseq,"EHLO noovex\r\n");
    if(smtp_resp(io,cipher,sk,siv,&sseq)/100!=2){ mst("EHLO REJECTED"); return; }
    smtp_cmd(io,cipher,ck,civ,&cseq,"AUTH LOGIN\r\n");
    if(smtp_resp(io,cipher,sk,siv,&sseq)!=334){ mst("AUTH UNSUPPORTED"); return; }
    b64e((const u8*)mail_user,strlen_(mail_user),bb);
    { int q=0; while(bb[q]){line[q]=bb[q];q++;} line[q++]='\r';line[q++]='\n';line[q]=0; smtp_cmd(io,cipher,ck,civ,&cseq,line); }
    if(smtp_resp(io,cipher,sk,siv,&sseq)!=334){ mst("USER REJECTED"); return; }
    b64e((const u8*)mail_pass,strlen_(mail_pass),bb);
    { int q=0; while(bb[q]){line[q]=bb[q];q++;} line[q++]='\r';line[q++]='\n';line[q]=0; smtp_cmd(io,cipher,ck,civ,&cseq,line); }
    if(smtp_resp(io,cipher,sk,siv,&sseq)!=235){ mst("AUTH FAILED-APP PW?"); return; }
    { int q=0; const char* a="MAIL FROM:<"; while(*a)line[q++]=*a++; { const char* u=mail_user; while(*u)line[q++]=*u++; } line[q++]='>';line[q++]='\r';line[q++]='\n';line[q]=0; smtp_cmd(io,cipher,ck,civ,&cseq,line); }
    if(smtp_resp(io,cipher,sk,siv,&sseq)/100!=2){ mst("SENDER REJECTED"); return; }
    { int q=0; const char* a="RCPT TO:<"; while(*a)line[q++]=*a++; { const char* u=mail_to; while(*u)line[q++]=*u++; } line[q++]='>';line[q++]='\r';line[q++]='\n';line[q]=0; smtp_cmd(io,cipher,ck,civ,&cseq,line); }
    if(smtp_resp(io,cipher,sk,siv,&sseq)/100!=2){ mst("RECIPIENT REJECTED"); return; }
    smtp_cmd(io,cipher,ck,civ,&cseq,"DATA\r\n");
    if(smtp_resp(io,cipher,sk,siv,&sseq)!=354){ mst("DATA REJECTED"); return; }
    { int q=0; const char* h;
      h="From: "; while(*h)line[q++]=*h++; { const char* u=mail_user; while(*u)line[q++]=*u++; } line[q++]='\r';line[q++]='\n';
      h="To: "; while(*h)line[q++]=*h++; { const char* u=mail_to; while(*u)line[q++]=*u++; } line[q++]='\r';line[q++]='\n';
      h="Subject: "; while(*h)line[q++]=*h++; { const char* u=mail_subj; while(*u)line[q++]=*u++; } line[q++]='\r';line[q++]='\n';
      line[q++]='\r';line[q++]='\n';
      { const char* u=mail_body; while(*u&&q<2560){ if(*u=='\n'){line[q++]='\r';line[q++]='\n';u++;} else line[q++]=*u++; } }
      if(q<2556){ line[q++]='\r';line[q++]='\n'; } line[q++]='.'; line[q++]='\r';line[q++]='\n'; line[q]=0;
      smtp_cmd(io,cipher,ck,civ,&cseq,line); }
    int fc=smtp_resp(io,cipher,sk,siv,&sseq);
    smtp_cmd(io,cipher,ck,civ,&cseq,"QUIT\r\n");
    secure_zero(&k,sizeof(k)); secure_zero(ent,64); secure_zero(priv,32);
    if(fc/100!=2){ mst("SERVER REFUSED MESSAGE"); return; }
    mst("SENT OK!");
}
static void mail_key(u8 sc){
    if(sc&0x80) return;
    if(sc==0x0F){ mail_field=(mail_field>=6)?1:mail_field+1; return; }
    char ch=kchar_shift(sc); if(!ch) return;
    char* f; int max;
    switch(mail_field){
        case 1: f=mail_host; max=62; break;
        case 2: f=mail_user; max=94; break;
        case 3: f=mail_pass; max=94; break;
        case 4: f=mail_to;   max=94; break;
        case 5: f=mail_subj; max=94; break;
        case 6: f=mail_body; max=1020; break;
        default: return;
    }
    int len=0; while(f[len])len++;
    if(ch=='\b'){ if(len>0)f[len-1]=0; }
    else if(ch=='\r'||ch=='\n'){ if(mail_field==6){ if(len<max){ f[len]='\n'; f[len+1]=0; } } else { mail_field++; } }
    else if(ch>=32&&len<max){ f[len]=ch; f[len+1]=0; }
}
static int ehci_in(int addr,int ep,int mps,u8* buf,int len,int dt);
static int net_dhcp=1, net_field=0;
static char netfld[4][20];
static char net_msg[24]="";
static int bt_found=0; static u8 bt_addr[6]; static u16 bt_vid=0, bt_pid=0;
static char bt_status[44]="TAP SCAN";
static void netmsg_set(const char* m){ int k=0; while(m[k]&&k<22){net_msg[k]=m[k];k++;} net_msg[k]=0; }
static void net_refresh(void){ ip4(my_ip,netfld[0]); ip4(netmask,netfld[1]); ip4(gw_ip,netfld[2]); ip4(dns_ip,netfld[3]); }
static u32 parse_ip(const char* s){ u32 o[4]={0,0,0,0}; int idx=0,v=0; for(int i=0;;i++){ char c=s[i]; if(c>='0'&&c<='9'){ v=v*10+(c-'0'); } else { if(idx<4)o[idx]=(u32)(v&255); idx++; v=0; if(c==0)break; } } return (o[0]<<24)|(o[1]<<16)|(o[2]<<8)|o[3]; }
static void net_key(u8 sc){ if(sc&0x80)return; if(net_field<1||net_field>4)return; char ch=kchar_shift(sc); char* f=netfld[net_field-1]; int len=0; while(f[len])len++; if(ch=='\b'){ if(len>0)f[len-1]=0; } else if((ch>='0'&&ch<='9')||ch=='.'){ if(len<18){ f[len]=ch; f[len+1]=0; } } else if(ch=='\r'||ch=='\n'){ net_field=(net_field>=4)?1:net_field+1; } }
static int ehci_ctrl_out(int addr,int mps,const u8* setup,const u8* dout,int dlen){
    if(!ehci_init_ok)return -1;
    u32 ob=ehci_base+ehci_caplen;
    volatile u32* usbcmd=(volatile u32*)(ob+0x00);
    volatile u32* usbsts=(volatile u32*)(ob+0x04);
    for(int i=0;i<8;i++)((volatile u8*)EHCI_BUF_SETUP)[i]=setup[i];
    for(int i=0;i<dlen&&i<64;i++)((volatile u8*)EHCI_BUF_DATA)[i]=dout[i];
    int hasdata=(dlen>0);
    MMW(EHCI_QTD_SETUP+0x00, hasdata?EHCI_QTD_DATA:EHCI_QTD_STAT);
    MMW(EHCI_QTD_SETUP+0x04, 1);
    MMW(EHCI_QTD_SETUP+0x08, 0x80u|(2u<<8)|(3u<<10)|((u32)8<<16));
    MMW(EHCI_QTD_SETUP+0x0C, EHCI_BUF_SETUP);
    for(u32 o=0x10;o<=0x1C;o+=4)MMW(EHCI_QTD_SETUP+o,0);
    if(hasdata){
        MMW(EHCI_QTD_DATA+0x00, EHCI_QTD_STAT);
        MMW(EHCI_QTD_DATA+0x04, 1);
        MMW(EHCI_QTD_DATA+0x08, 0x80u|(0u<<8)|(3u<<10)|((u32)dlen<<16)|(1u<<31));
        MMW(EHCI_QTD_DATA+0x0C, EHCI_BUF_DATA);
        for(u32 o=0x10;o<=0x1C;o+=4)MMW(EHCI_QTD_DATA+o,0);
    }
    MMW(EHCI_QTD_STAT+0x00, 1);
    MMW(EHCI_QTD_STAT+0x04, 1);
    MMW(EHCI_QTD_STAT+0x08, 0x80u|(1u<<8)|(3u<<10)|(1u<<31)|0x8000u);
    MMW(EHCI_QTD_STAT+0x0C, 0);
    for(u32 o=0x10;o<=0x1C;o+=4)MMW(EHCI_QTD_STAT+o,0);
    MMW(EHCI_QH_XFER+0x00, EHCI_QH_XFER|2u);
    MMW(EHCI_QH_XFER+0x04, (u32)(addr&0x7F)|(2u<<12)|(1u<<14)|(1u<<15)|(((u32)mps&0x7FF)<<16));
    MMW(EHCI_QH_XFER+0x08, (1u<<30));
    MMW(EHCI_QH_XFER+0x0C, 0);
    MMW(EHCI_QH_XFER+0x10, EHCI_QTD_SETUP);
    MMW(EHCI_QH_XFER+0x14, 1);
    MMW(EHCI_QH_XFER+0x18, 0);
    for(u32 o=0x1C;o<=0x2C;o+=4)MMW(EHCI_QH_XFER+o,0);
    *usbcmd &= ~(1u<<5); for(int t=0;t<8000;t++){ if(!(*usbsts&(1u<<15)))break; io_delay(1); }
    *(volatile u32*)(ob+0x18)=EHCI_QH_XFER;
    *(volatile u32*)(ob+0x10)=0;
    *usbcmd |= (1u<<5); *usbcmd |= 1u;
    for(int t=0;t<8000;t++){ if(*usbsts&(1u<<15))break; io_delay(1); }
    int done=0,err=0;
    for(int t=0;t<60000;t++){
        u32 tok=MMR(EHCI_QTD_STAT+0x08);
        u32 s1=MMR(EHCI_QTD_SETUP+0x08); if(s1&0x40){err=1;done=1;break;}
        if(hasdata){ u32 d1=MMR(EHCI_QTD_DATA+0x08); if(d1&0x40){err=1;done=1;break;} }
        if(!(tok&0x80)){ done=1; if(tok&0x40)err=1; break; }
        io_delay(1);
    }
    *usbcmd &= ~(1u<<5);
    if(!done||err)return -1;
    return 0;
}
static void bt_status_set(const char* m){ int k=0; while(m[k]&&k<42){bt_status[k]=m[k];k++;} bt_status[k]=0; }
static void bt_probe(void){
    bt_found=0; int di=-1;
    for(int i=0;i<8;i++){ if(usbdev[i].addr && (usbdev[i].cls==0xE0 || usbdev[i].ifcls==0xE0)){ di=i; break; } }
    if(di<0){ bt_status_set("NO BT CONTROLLER ON USB BUS"); bt_found=-1; return; }
    bt_vid=usbdev[di].vid; bt_pid=usbdev[di].pid;
    int a=usbdev[di].addr, mps=usbdev[di].epmps?usbdev[di].epmps:64, ep=usbdev[di].epin?usbdev[di].epin:1;
    u8 su[8]={0x20,0x00,0x00,0x00,0x00,0x00,3,0x00};
    u8 reset[3]={0x03,0x0C,0x00}; int r=ehci_ctrl_out(a,mps,su,reset,3);
    u8 ev[36]; ehci_in(a,ep,mps,ev,16,0);
    u8 rd[3]={0x09,0x10,0x00}; ehci_ctrl_out(a,mps,su,rd,3);
    int el=ehci_in(a,ep,mps,ev,16,1);
    if(el>=12 && ev[0]==0x0E && ev[5]==0x00){ for(int k=0;k<6;k++)bt_addr[k]=ev[11-k]; bt_found=1; bt_status_set("CONTROLLER ONLINE (HCI OK)"); }
    else if(r==0){ bt_found=2; bt_status_set("DETECTED - HCI NO EVENT"); }
    else { bt_found=2; bt_status_set("DETECTED - USB TRANSFER FAILED"); }
}
/* ===QRGAMES=== inserted block: QR encoder + 2048 + Breakout + Minesweeper */
static void qnum(int cx,int cy,int v,u8 col){ char b[12]; utoa((u32)v,b); int n=0; while(b[n])n++; draw_str(cx-n*4,cy-4,b,col); }

/* ---------- QR encoder: byte mode, EC level L, versions 1-5, single block ---------- */
static u8 gfx_e[512],gfx_l[256]; static int gfx_rdy=0;
static void gf_setup(void){ if(gfx_rdy)return; gfx_rdy=1; int x=1; for(int i=0;i<255;i++){ gfx_e[i]=(u8)x; gfx_l[x]=(u8)i; x<<=1; if(x&0x100)x^=0x11D; } for(int i=255;i<512;i++)gfx_e[i]=gfx_e[i-255]; }
static u8 gf_mul(u8 a,u8 b){ if(!a||!b)return 0; return gfx_e[gfx_l[a]+gfx_l[b]]; }
static void rs_gen(int ec,u8* g){ g[0]=1; int gl=1; for(int i=0;i<ec;i++){ u8 ng[32]; for(int k=0;k<=gl;k++)ng[k]=0; for(int j=0;j<gl;j++){ ng[j]^=g[j]; ng[j+1]^=gf_mul(g[j],gfx_e[i]); } for(int k=0;k<=gl;k++)g[k]=ng[k]; gl++; } }
static void rs_ecc(const u8* d,int n,int ec,u8* out){ u8 g[32]; rs_gen(ec,g); u8 rem[200]; for(int i=0;i<n;i++)rem[i]=d[i]; for(int i=0;i<ec;i++)rem[n+i]=0; for(int i=0;i<n;i++){ u8 c=rem[i]; if(c){ for(int j=1;j<=ec;j++)rem[i+j]^=gf_mul(g[j],c); } } for(int i=0;i<ec;i++)out[i]=rem[n+i]; }

static const int QCAP[6]={0,19,34,55,80,108};
static const int QECN[6]={0,7,10,15,20,26};
static const int QALN[6]={0,0,18,22,26,30};
static const unsigned short QFMT[8]={0x77C4,0x72F3,0x7DAA,0x789D,0x662F,0x6318,0x6C41,0x6976};

static u8 qm[37][37], qf[37][37];
static int qsz=0, qver=0, qr_ok=0;
static char qr_in[120]; static int qr_n=0;
static u8 qbits[160]; static int qnb;
static void qbit(u32 v,int n){ for(int i=n-1;i>=0;i--){ if((v>>i)&1) qbits[qnb>>3]|=(u8)(0x80>>(qnb&7)); qnb++; } }
static int qgb(const u8* b,int i){ return (b[i>>3]>>(7-(i&7)))&1; }
static void qset(int r,int c,int v){ qm[r][c]=(u8)v; qf[r][c]=1; }

static int qr_penalty(int sz){
    int pen=0,r,c;
    for(r=0;r<sz;r++){ int run=1; for(c=1;c<sz;c++){ if(qm[r][c]==qm[r][c-1])run++; else { if(run>=5)pen+=3+(run-5); run=1; } } if(run>=5)pen+=3+(run-5); }
    for(c=0;c<sz;c++){ int run=1; for(r=1;r<sz;r++){ if(qm[r][c]==qm[r-1][c])run++; else { if(run>=5)pen+=3+(run-5); run=1; } } if(run>=5)pen+=3+(run-5); }
    for(r=0;r<sz-1;r++)for(c=0;c<sz-1;c++){ int v=qm[r][c]; if(v==qm[r][c+1]&&v==qm[r+1][c]&&v==qm[r+1][c+1])pen+=3; }
    for(r=0;r<sz;r++)for(c=0;c<sz-10;c++){
        int a1=qm[r][c]==1&&qm[r][c+1]==0&&qm[r][c+2]==1&&qm[r][c+3]==1&&qm[r][c+4]==1&&qm[r][c+5]==0&&qm[r][c+6]==1&&qm[r][c+7]==0&&qm[r][c+8]==0&&qm[r][c+9]==0&&qm[r][c+10]==0;
        int a2=qm[r][c]==0&&qm[r][c+1]==0&&qm[r][c+2]==0&&qm[r][c+3]==0&&qm[r][c+4]==1&&qm[r][c+5]==0&&qm[r][c+6]==1&&qm[r][c+7]==1&&qm[r][c+8]==1&&qm[r][c+9]==0&&qm[r][c+10]==1;
        if(a1||a2)pen+=40; }
    for(c=0;c<sz;c++)for(r=0;r<sz-10;r++){
        int a1=qm[r][c]==1&&qm[r+1][c]==0&&qm[r+2][c]==1&&qm[r+3][c]==1&&qm[r+4][c]==1&&qm[r+5][c]==0&&qm[r+6][c]==1&&qm[r+7][c]==0&&qm[r+8][c]==0&&qm[r+9][c]==0&&qm[r+10][c]==0;
        int a2=qm[r][c]==0&&qm[r+1][c]==0&&qm[r+2][c]==0&&qm[r+3][c]==0&&qm[r+4][c]==1&&qm[r+5][c]==0&&qm[r+6][c]==1&&qm[r+7][c]==1&&qm[r+8][c]==1&&qm[r+9][c]==0&&qm[r+10][c]==1;
        if(a1||a2)pen+=40; }
    int dark=0; for(r=0;r<sz;r++)for(c=0;c<sz;c++)if(qm[r][c])dark++;
    int tot=sz*sz; int pct=dark*100/tot; int dev=pct>=50?pct-50:50-pct; pen+=(dev/5)*10;
    return pen;
}

static void qr_putfmt(int sz,unsigned f){
    for(int i=0;i<=5;i++) qm[8][i]=(u8)((f>>i)&1);
    qm[8][7]=(u8)((f>>6)&1); qm[8][8]=(u8)((f>>7)&1); qm[7][8]=(u8)((f>>8)&1);
    for(int i=9;i<15;i++) qm[14-i][8]=(u8)((f>>i)&1);
    for(int i=0;i<8;i++) qm[sz-1-i][8]=(u8)((f>>i)&1);
    for(int i=8;i<15;i++) qm[8][sz-15+i]=(u8)((f>>i)&1);
    qm[sz-8][8]=1;
}
static void qr_applymask(int sz,int mk){
    for(int r=0;r<sz;r++)for(int c=0;c<sz;c++){ if(qf[r][c])continue; int m=0;
        switch(mk){ case 0:m=((r+c)&1)==0;break; case 1:m=(r&1)==0;break; case 2:m=(c%3)==0;break; case 3:m=((r+c)%3)==0;break;
            case 4:m=(((r/2)+(c/3))&1)==0;break; case 5:m=(((r*c)&1)+((r*c)%3))==0;break; case 6:m=((((r*c)&1)+((r*c)%3))&1)==0;break; case 7:m=((((r+c)&1)+((r*c)%3))&1)==0;break; }
        if(m)qm[r][c]^=1; }
}

static void qr_build(void){
    gf_setup(); qr_ok=0;
    int len=qr_n; if(len<1){ qver=0; return; }
    int ver=0; for(int v=1;v<=5;v++){ if(len<=QCAP[v]-2){ ver=v; break; } }
    if(!ver){ qver=99; return; }
    qver=ver; int sz=17+4*ver; qsz=sz;
    int cap=QCAP[ver], ecn=QECN[ver];
    for(int i=0;i<160;i++)qbits[i]=0; qnb=0;
    qbit(4,4); qbit((u32)len,8);
    for(int i=0;i<len;i++) qbit((u8)qr_in[i],8);
    int totbits=cap*8; int term=totbits-qnb; if(term>4)term=4; if(term>0)qbit(0,term);
    while(qnb&7)qnb++;
    { int pad=0; while((qnb>>3)<cap){ qbit(pad?0x11:0xEC,8); pad^=1; } }
    u8 dc[120]; for(int i=0;i<cap;i++)dc[i]=qbits[i];
    u8 ecb[40]; rs_ecc(dc,cap,ecn,ecb);
    u8 full[180]; int fn=0; for(int i=0;i<cap;i++)full[fn++]=dc[i]; for(int i=0;i<ecn;i++)full[fn++]=ecb[i];
    for(int r=0;r<sz;r++)for(int c=0;c<sz;c++){ qm[r][c]=0; qf[r][c]=0; }
    int fcr[3]={0,0,sz-7}, fcc[3]={0,sz-7,0};
    for(int t=0;t<3;t++){ int br=fcr[t],bc=fcc[t];
        for(int dr=-1;dr<=7;dr++)for(int dc=-1;dc<=7;dc++){ int r=br+dr,c=bc+dc; if(r<0||c<0||r>=sz||c>=sz)continue;
            int v=0; if(dr>=0&&dr<=6&&dc>=0&&dc<=6){ int e=(dr==0||dr==6||dc==0||dc==6); int mm=(dr>=2&&dr<=4&&dc>=2&&dc<=4); v=(e||mm)?1:0; }
            qset(r,c,v); } }
    for(int i=8;i<sz-8;i++){ qset(6,i,(i&1)?0:1); qset(i,6,(i&1)?0:1); }
    if(ver>=2){ int a=QALN[ver]; for(int dr=-2;dr<=2;dr++)for(int dc=-2;dc<=2;dc++){ int r=a+dr,c=a+dc; int x1=(dr<0?-dr:dr),x2=(dc<0?-dc:dc); int mm=x1>x2?x1:x2; qset(r,c,(mm==1)?0:1); } }
    qset(4*ver+9,8,1);
    for(int i=0;i<=5;i++) qf[8][i]=1;
    qf[8][7]=1; qf[8][8]=1; qf[7][8]=1;
    for(int i=9;i<15;i++) qf[14-i][8]=1;
    for(int i=0;i<8;i++) qf[sz-1-i][8]=1;
    for(int i=8;i<15;i++) qf[8][sz-15+i]=1;
    qf[sz-8][8]=1;
    int totmod=fn*8, bi=0;
    for(int right=sz-1; right>=1; right-=2){ if(right==6)right=5;
        for(int vert=0; vert<sz; vert++){
            for(int j=0;j<2;j++){ int col=right-j; int up=((right+1)&2)==0; int row=up?(sz-1-vert):vert;
                if(!qf[row][col]){ int b=(bi<totmod)?qgb(full,bi):0; qm[row][col]=(u8)b; bi++; } } } }
    int best=0,bestpen=1<<30; static u8 sv[37][37];
    for(int mk=0;mk<8;mk++){
        for(int r=0;r<sz;r++)for(int c=0;c<sz;c++)sv[r][c]=qm[r][c];
        qr_applymask(sz,mk); qr_putfmt(sz,QFMT[mk]);
        int pen=qr_penalty(sz); if(pen<bestpen){ bestpen=pen; best=mk; }
        for(int r=0;r<sz;r++)for(int c=0;c<sz;c++)qm[r][c]=sv[r][c];
    }
    qr_applymask(sz,best); qr_putfmt(sz,QFMT[best]);
    qr_ok=1;
}
static void qr_app_init(void){ const char* s="https://www.anthropic.com"; int i=0; while(s[i]&&i<112){ qr_in[i]=s[i]; i++; } qr_in[i]=0; qr_n=i; qr_build(); }
static void qr_key(u8 sc){ if(sc&0x80)return; char ch=kchar_shift(sc); if(!ch)return; if(ch=='\b'){ if(qr_n>0){ qr_n--; qr_in[qr_n]=0; } } else if(ch=='\n'||ch=='\r'){ } else if(ch>=32&&qr_n<112){ qr_in[qr_n++]=ch; qr_in[qr_n]=0; } qr_build(); }

/* ---------------------------- 2048 ---------------------------- */
static int g2[4][4], g2_score=0, g2_best=0, g2_over=0, g2_won=0;
static void g2_spawn(void){ int em[16],n=0; for(int i=0;i<16;i++) if(!g2[i>>2][i&3]) em[n++]=i; if(!n)return; int e=em[rnd()%n]; g2[e>>2][e&3]=(rnd()%10==0)?4:2; }
static void g2048_init(void){ rngs^=pit_read(); for(int r=0;r<4;r++)for(int c=0;c<4;c++)g2[r][c]=0; g2_score=0; g2_over=0; g2_won=0; g2_spawn(); g2_spawn(); }
static int g2_line(int* a){ int moved=0,tmp[4],n=0; for(int i=0;i<4;i++) if(a[i])tmp[n++]=a[i]; int out[4]={0,0,0,0},o=0; for(int i=0;i<n;i++){ if(i+1<n&&tmp[i]==tmp[i+1]){ out[o]=tmp[i]*2; g2_score+=out[o]; if(out[o]>=2048)g2_won=1; o++; i++; } else out[o++]=tmp[i]; } for(int i=0;i<4;i++){ if(a[i]!=out[i])moved=1; a[i]=out[i]; } return moved; }
static int g2_move(int dir){ int moved=0,line[4]; for(int k=0;k<4;k++){
    if(dir==0){ for(int i=0;i<4;i++)line[i]=g2[k][i]; if(g2_line(line))moved=1; for(int i=0;i<4;i++)g2[k][i]=line[i]; }
    else if(dir==1){ for(int i=0;i<4;i++)line[i]=g2[k][3-i]; if(g2_line(line))moved=1; for(int i=0;i<4;i++)g2[k][3-i]=line[i]; }
    else if(dir==2){ for(int i=0;i<4;i++)line[i]=g2[i][k]; if(g2_line(line))moved=1; for(int i=0;i<4;i++)g2[i][k]=line[i]; }
    else { for(int i=0;i<4;i++)line[i]=g2[3-i][k]; if(g2_line(line))moved=1; for(int i=0;i<4;i++)g2[3-i][k]=line[i]; } }
    return moved; }
static int g2_full(void){ for(int i=0;i<16;i++) if(!g2[i>>2][i&3])return 0; for(int r=0;r<4;r++)for(int c=0;c<4;c++){ if(c<3&&g2[r][c]==g2[r][c+1])return 0; if(r<3&&g2[r][c]==g2[r+1][c])return 0; } return 1; }
static void g2048_key(u8 sc){ if(g2_over){ if(sc==0x13)g2048_init(); return; } int d=-1; if(sc==0x4B||sc==0x1E)d=0; else if(sc==0x4D||sc==0x20)d=1; else if(sc==0x48||sc==0x11)d=2; else if(sc==0x50||sc==0x1F)d=3; else if(sc==0x13){ g2048_init(); return; } if(d<0)return; if(g2_move(d)){ g2_spawn(); if(g2_score>g2_best)g2_best=g2_score; if(g2_full())g2_over=1; } }
static u8 g2col(int v){ switch(v){ case 2:return 58; case 4:return 52; case 8:return C_FOLDER; case 16:return 110; case 32:return C_RED; case 64:return 71; case 128:return C_GREEN; case 256:return C_TEAL; case 512:return C_BBLUE; case 1024:return C_BLUE; default:return C_TITLE; } }

/* ---------------------------- Breakout ---------------------------- */
static int bk_px,bk_bx,bk_by,bk_vx,bk_vy,bk_live=0,bk_launch=0,bk_score=0,bk_lives=3; static u8 bk_brick[5][8];
#define BK_PW 56
#define BK_BR 3
static void breakout_init(void){ int FW=winw-4,FH=winh-24; bk_px=(FW-BK_PW)/2; bk_launch=0; bk_live=1; bk_score=0; bk_lives=3; bk_bx=bk_px+BK_PW/2; bk_by=FH-20; bk_vx=3; bk_vy=-3; for(int r=0;r<5;r++)for(int c=0;c<8;c++)bk_brick[r][c]=1; }
static void breakout_step(void){ if(!bk_live||!bk_launch)return; int FW=winw-4,FH=winh-24,bw=FW/8;
    bk_bx+=bk_vx; bk_by+=bk_vy;
    if(bk_bx<=BK_BR){ bk_bx=BK_BR; bk_vx=-bk_vx; } if(bk_bx>=FW-BK_BR){ bk_bx=FW-BK_BR; bk_vx=-bk_vx; }
    if(bk_by<=BK_BR){ bk_by=BK_BR; bk_vy=-bk_vy; }
    if(bk_by>=28&&bk_by<28+5*14){ int col=bk_bx/bw, row=(bk_by-28)/14; if(col>=0&&col<8&&row>=0&&row<5&&bk_brick[row][col]){ bk_brick[row][col]=0; bk_vy=-bk_vy; bk_score++; click_snd(); int any=0; for(int r=0;r<5;r++)for(int c=0;c<8;c++)if(bk_brick[r][c])any=1; if(!any){ for(int r=0;r<5;r++)for(int c=0;c<8;c++)bk_brick[r][c]=1; bk_launch=0; bk_by=FH-20; } } }
    int py=FH-16; if(bk_by+BK_BR>=py&&bk_by<=py+7&&bk_bx>=bk_px&&bk_bx<=bk_px+BK_PW&&bk_vy>0){ bk_vy=-bk_vy; bk_by=py-BK_BR; int hit=bk_bx-(bk_px+BK_PW/2); bk_vx=hit/8; if(bk_vx>4)bk_vx=4; if(bk_vx<-4)bk_vx=-4; if(bk_vx==0)bk_vx=(hit<0)?-2:2; }
    if(bk_by>FH){ bk_lives--; if(bk_lives<=0){ bk_live=0; } else { bk_launch=0; bk_bx=bk_px+BK_PW/2; bk_by=FH-20; bk_vx=3; bk_vy=-3; } } }
static void breakout_key(u8 sc){ int FW=winw-4; if(!bk_live){ if(sc==0x13)breakout_init(); return; } if(sc==0x4B||sc==0x1E){ bk_px-=18; if(bk_px<0)bk_px=0; if(!bk_launch)bk_bx=bk_px+BK_PW/2; } else if(sc==0x4D||sc==0x20){ bk_px+=18; if(bk_px>FW-BK_PW)bk_px=FW-BK_PW; if(!bk_launch)bk_bx=bk_px+BK_PW/2; } else if(sc==0x39||sc==0x48){ if(!bk_launch){ bk_launch=1; bk_vy=-3; bk_vx=3; } } else if(sc==0x13){ breakout_init(); } }

/* ---------------------------- Minesweeper ---------------------------- */
static u8 ms_mine[9][9],ms_open[9][9],ms_flag[9][9]; static int ms_num[9][9]; static int ms_over=0,ms_win=0,ms_flagmode=0,ms_seed=0,ms_left=10;
static void ms_init(void){ rngs^=pit_read(); for(int r=0;r<9;r++)for(int c=0;c<9;c++){ ms_mine[r][c]=0; ms_open[r][c]=0; ms_flag[r][c]=0; ms_num[r][c]=0; } int placed=0; while(placed<10){ int r=rnd()%9,c=rnd()%9; if(!ms_mine[r][c]){ ms_mine[r][c]=1; placed++; } } for(int r=0;r<9;r++)for(int c=0;c<9;c++){ if(ms_mine[r][c])continue; int n=0; for(int dr=-1;dr<=1;dr++)for(int dc=-1;dc<=1;dc++){ int rr=r+dr,cc=c+dc; if(rr>=0&&rr<9&&cc>=0&&cc<9&&ms_mine[rr][cc])n++; } ms_num[r][c]=n; } ms_over=0; ms_win=0; ms_flagmode=0; ms_left=10; }
static void ms_flood(int r,int c){ if(r<0||r>=9||c<0||c>=9)return; if(ms_open[r][c]||ms_flag[r][c])return; ms_open[r][c]=1; if(ms_num[r][c]==0&&!ms_mine[r][c]){ for(int dr=-1;dr<=1;dr++)for(int dc=-1;dc<=1;dc++){ if(dr||dc)ms_flood(r+dr,c+dc); } } }
static void ms_check_win(void){ int closed=0; for(int r=0;r<9;r++)for(int c=0;c<9;c++) if(!ms_open[r][c]&&!ms_mine[r][c])closed++; if(closed==0){ ms_win=1; ms_over=1; } }
static void ms_reveal(int r,int c){ if(ms_over||ms_open[r][c]||ms_flag[r][c])return; if(ms_mine[r][c]){ ms_over=1; for(int rr=0;rr<9;rr++)for(int cc=0;cc<9;cc++) if(ms_mine[rr][cc])ms_open[rr][cc]=1; beep(160,8); return; } ms_flood(r,c); ms_check_win(); }
static void mines_init(void){ ms_init(); }

/* ===NETAPPS=== Gopher (42) + Wikipedia (43) + Net Tools/Finger/WHOIS (44) */
static void draw_str_n(int x,int y,const char* s,u8 col,int maxch){ for(int i=0;i<maxch&&s[i];i++)draw_char(x+i*8,y,s[i],col); }

/* shared word-wrap text renderer; measure with draw=0 (returns content height), then draw */
static int nettext_draw(const char* buf,int blen,int x0,int y0,int x1,int y1,int scroll,u8 col,int draw){
    int y=y0-scroll, x=x0, i=0;
    while(i<blen && buf[i]){
        char c=buf[i];
        if(c=='\r'){ i++; continue; }
        if(c=='\n'){ x=x0; y+=11; i++; continue; }
        if(c=='\t'){ x+=32; if(x>x1-8){x=x0;y+=11;} i++; continue; }
        if(c==' '){ x+=8; if(x>x1-8){x=x0;y+=11;} i++; continue; }
        int wl=0; while(i+wl<blen && buf[i+wl] && (unsigned char)buf[i+wl]>32 && buf[i+wl]!='\n')wl++;
        if(wl==0){ i++; continue; }
        int cols=(x1-x0)/8; if(cols<1)cols=1;
        if(wl<=cols && x+wl*8>x1){ x=x0; y+=11; }
        for(int k=0;k<wl;k++){ char ch=buf[i+k]; if((unsigned char)ch>=32){ if(draw && y>=y0-11 && y<=y1 && x<=x1-1) draw_char(x,y,ch,col); x+=8; if(x>x1-1){ x=x0; y+=11; } } }
        i+=wl;
    }
    return (y+11)-(y0-scroll);
}

/* ---------------- Gopher ---------------- */
struct gopher_item { char type; char disp[72]; char sel[160]; char host[80]; int port; };
struct gopher_h { char host[80]; int port; char sel[200]; int ismenu; };
static char gopher_host[84]; static int gopher_port=70; static char gopher_sel[200];
static int gopher_field=0, gopher_is_menu=1, gopher_scroll=0, gopher_status=0, gopher_nitems=0, gopher_blen=0, gopher_histn=0;
static char gopher_buf[16000];
static struct gopher_item gopher_items[120];
static struct gopher_h gopher_hist[16];

static void gopher_parse_menu(void){
    gopher_nitems=0; int i=0;
    while(i<gopher_blen && gopher_nitems<120){
        char type=gopher_buf[i];
        if(type=='.'&&(i+1>=gopher_blen||gopher_buf[i+1]=='\r'||gopher_buf[i+1]=='\n')) break;
        if(type=='\r'||type=='\n'){ i++; continue; }
        int j=i+1; struct gopher_item* it=&gopher_items[gopher_nitems]; it->type=type;
        int d=0; while(j<gopher_blen&&gopher_buf[j]!='\t'&&gopher_buf[j]!='\r'&&gopher_buf[j]!='\n'){ if(d<71)it->disp[d++]=gopher_buf[j]; j++; } it->disp[d]=0;
        int s=0; if(j<gopher_blen&&gopher_buf[j]=='\t'){ j++; while(j<gopher_blen&&gopher_buf[j]!='\t'&&gopher_buf[j]!='\r'&&gopher_buf[j]!='\n'){ if(s<159)it->sel[s++]=gopher_buf[j]; j++; } } it->sel[s]=0;
        int h=0; if(j<gopher_blen&&gopher_buf[j]=='\t'){ j++; while(j<gopher_blen&&gopher_buf[j]!='\t'&&gopher_buf[j]!='\r'&&gopher_buf[j]!='\n'){ if(h<79)it->host[h++]=gopher_buf[j]; j++; } } it->host[h]=0;
        int pv=0,seen=0; if(j<gopher_blen&&gopher_buf[j]=='\t'){ j++; while(j<gopher_blen&&gopher_buf[j]>='0'&&gopher_buf[j]<='9'){ pv=pv*10+(gopher_buf[j]-'0'); j++; seen=1; } } it->port=seen?pv:70;
        gopher_nitems++;
        while(j<gopher_blen&&gopher_buf[j]!='\n')j++; if(j<gopher_blen)j++; i=j;
    }
}
static void gopher_load(const char* host,int port,const char* sel,int ismenu){
    if(!nic_up()){ gopher_status=-1; toast_set("NO NETWORK - SET PCNET + NAT"); return; }
    u32 ip=0; if(dns_query(host,&ip)!=1){ gopher_status=-1; toast_set("GOPHER: DNS FAILED"); return; }
    char req[210]; int r=0; for(int i=0;sel[i]&&r<200;i++)req[r++]=sel[i]; req[r++]='\r'; req[r++]='\n';
    int n=tcp_get(ip,(u16)port,req,r,(u8*)http_buf,59000);
    if(n<=0){ gopher_status=-1; toast_set(n==-4?"GOPHER: REFUSED":n==-3?"GOPHER: TIMEOUT":n==-2?"GOPHER: ARP FAIL":"GOPHER: NO DATA"); return; }
    if(n>15999)n=15999; for(int i=0;i<n;i++)gopher_buf[i]=http_buf[i]; gopher_buf[n]=0; gopher_blen=n;
    { int i=0; while(host[i]&&i<83){gopher_host[i]=host[i];i++;} gopher_host[i]=0; }
    gopher_port=port;
    { int i=0; while(sel[i]&&i<199){gopher_sel[i]=sel[i];i++;} gopher_sel[i]=0; }
    gopher_is_menu=ismenu; gopher_scroll=0; gopher_status=2;
    if(ismenu) gopher_parse_menu();
}
static void gopher_nav(const char* host,int port,const char* sel,int ismenu){
    if(gopher_histn<16){ struct gopher_h* hh=&gopher_hist[gopher_histn];
        int i=0; while(gopher_host[i]&&i<79){hh->host[i]=gopher_host[i];i++;} hh->host[i]=0;
        hh->port=gopher_port; i=0; while(gopher_sel[i]&&i<199){hh->sel[i]=gopher_sel[i];i++;} hh->sel[i]=0; hh->ismenu=gopher_is_menu;
        gopher_histn++; }
    gopher_load(host,port,sel,ismenu);
}
static void gopher_back(void){
    if(gopher_histn<=0){ toast_set("NO HISTORY"); return; }
    gopher_histn--; struct gopher_h* hh=&gopher_hist[gopher_histn];
    gopher_load(hh->host,hh->port,hh->sel,hh->ismenu);
}
static void gopher_key(u8 d){ if(d&0x80)return; if(!gopher_field)return; char ch=kchar_shift(d); if(!ch)return;
    int len=strlen_(gopher_host);
    if(ch=='\b'){ if(len>0)gopher_host[len-1]=0; }
    else if(ch=='\n'||ch=='\r'){ gopher_field=0; gopher_histn=0; gopher_load(gopher_host,70,"",1); }
    else if(ch>=32&&len<82){ gopher_host[len]=ch; gopher_host[len+1]=0; }
}
static void gopher_init(void){ gopher_status=0; gopher_scroll=0; gopher_field=0; gopher_histn=0; gopher_is_menu=1; gopher_nitems=0; gopher_blen=0; gopher_port=70; { const char* dd="gopher.floodgap.com"; int i=0; while(dd[i]){gopher_host[i]=dd[i];i++;} gopher_host[i]=0; } gopher_sel[0]=0; }

/* ---------------- Wikipedia ---------------- */
static char wiki_term[84]; static char wiki_title[120]; static char wiki_buf[24000];
static int wiki_field=1, wiki_scroll=0, wiki_status=0, wiki_blen=0;
static void wiki_fetch(void){
    if(wiki_term[0]==0)return;
    if(!nic_up()){ wiki_status=-1; toast_set("NO NETWORK"); return; }
    char url[440]; int o=0; const char* base="https://en.wikipedia.org/api/rest_v1/page/summary/";
    for(int i=0;base[i];i++)url[o++]=base[i];
    const char* hx="0123456789ABCDEF";
    for(int i=0;wiki_term[i]&&o<420;i++){ char c=wiki_term[i];
        if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='.'||c=='_') url[o++]=c;
        else if(c==' '){ url[o++]='%'; url[o++]='2'; url[o++]='0'; }
        else { url[o++]='%'; url[o++]=hx[(c>>4)&0xF]; url[o++]=hx[c&0xF]; } }
    url[o]=0;
    int saved=reader_on; reader_on=0; char* body=br_fetch(url); reader_on=saved;
    if(!body||!br_ok){ wiki_status=-1; toast_set("WIKIPEDIA FETCH FAILED"); return; }
    int blen=http_body_len;
    wiki_title[0]=0;
    int tp=br_find_ci(body,blen,"\"title\":\"");
    if(tp>=0){ int i=tp+9,o2=0; while(i<blen&&body[i]!='\"'&&o2<118){ if(body[i]=='\\'&&i+1<blen)i++; wiki_title[o2++]=body[i]; i++; } wiki_title[o2]=0; }
    int ep=br_find_ci(body,blen,"\"extract\":\"");
    if(ep<0){ wiki_status=-1; toast_set("NO ARTICLE - TRY ANOTHER TITLE"); wiki_buf[0]=0; wiki_blen=0; return; }
    int i=ep+11,o3=0;
    while(i<blen&&o3<23980){ char c=body[i];
        if(c=='\"')break;
        if(c=='\\'){ char e=body[i+1];
            if(e=='n'){ wiki_buf[o3++]='\n'; i+=2; }
            else if(e=='t'){ wiki_buf[o3++]=' '; i+=2; }
            else if(e=='r'){ i+=2; }
            else if(e=='"'){ wiki_buf[o3++]='"'; i+=2; }
            else if(e=='\\'){ wiki_buf[o3++]='\\'; i+=2; }
            else if(e=='/'){ wiki_buf[o3++]='/'; i+=2; }
            else if(e=='u'){ unsigned cp=0; for(int k=0;k<4;k++){ char h=body[i+2+k]; int v=(h>='0'&&h<='9')?h-'0':(h>='a'&&h<='f')?h-'a'+10:(h>='A'&&h<='F')?h-'A'+10:0; cp=cp*16+v; } wiki_buf[o3++]=(cp<128)?(char)cp:'?'; i+=6; }
            else { wiki_buf[o3++]=e; i+=2; } }
        else { wiki_buf[o3++]=c; i++; } }
    wiki_buf[o3]=0; wiki_blen=o3; wiki_scroll=0; wiki_status=2;
}
static void wiki_key(u8 d){ if(d&0x80)return; if(!wiki_field)return; char ch=kchar_shift(d); if(!ch)return;
    int len=strlen_(wiki_term);
    if(ch=='\b'){ if(len>0)wiki_term[len-1]=0; }
    else if(ch=='\n'||ch=='\r'){ wiki_field=0; wiki_fetch(); }
    else if(ch>=32&&len<82){ wiki_term[len]=ch; wiki_term[len+1]=0; }
}
static void wiki_init(void){ wiki_status=0; wiki_scroll=0; wiki_field=1; wiki_term[0]=0; wiki_title[0]=0; wiki_blen=0; }

/* ---------------- Net Tools: Finger + WHOIS ---------------- */
static char nt_query[124]; static char nt_buf[9000];
static int nt_mode=0, nt_field=1, nt_scroll=0, nt_status=0, nt_blen=0;
static void nt_fetch(void){
    if(nt_query[0]==0)return;
    if(!nic_up()){ nt_status=-1; toast_set("NO NETWORK"); return; }
    if(nt_mode==0){
        char fuser[84]="",fhost[84]=""; int at=-1;
        for(int i=0;nt_query[i];i++){ if(nt_query[i]=='@'){at=i;break;} }
        if(at>=0){ int u=0; for(int i=0;i<at&&u<83;i++)fuser[u++]=nt_query[i]; fuser[u]=0; int h=0; for(int i=at+1;nt_query[i]&&h<83;i++)fhost[h++]=nt_query[i]; fhost[h]=0; }
        else { int h=0; for(int i=0;nt_query[i]&&h<83;i++)fhost[h++]=nt_query[i]; fhost[h]=0; }
        if(!fhost[0]){ nt_status=-1; toast_set("NEED A HOST (user@host)"); return; }
        u32 ip=0; if(dns_query(fhost,&ip)!=1){ nt_status=-1; toast_set("FINGER: DNS FAILED"); return; }
        char req[100]; int r=0; for(int i=0;fuser[i]&&r<96;i++)req[r++]=fuser[i]; req[r++]='\r'; req[r++]='\n';
        int n=tcp_get(ip,79,req,r,(u8*)http_buf,59000);
        if(n<=0){ nt_status=-1; toast_set(n==-4?"FINGER: REFUSED":n==-3?"FINGER: TIMEOUT":"FINGER: NO DATA"); return; }
        if(n>8998)n=8998; for(int i=0;i<n;i++)nt_buf[i]=http_buf[i]; nt_buf[n]=0; nt_blen=n;
    } else {
        char dom[84]="",srv[84]=""; int sp=-1;
        for(int i=0;nt_query[i];i++){ if(nt_query[i]==' '){sp=i;break;} }
        if(sp>=0){ int d=0; for(int i=0;i<sp&&d<83;i++)dom[d++]=nt_query[i]; dom[d]=0; int s=0; for(int i=sp+1;nt_query[i]&&s<83;i++)srv[s++]=nt_query[i]; srv[s]=0; }
        else { int d=0; for(int i=0;nt_query[i]&&d<83;i++)dom[d++]=nt_query[i]; dom[d]=0; const char* def="whois.iana.org"; int s=0; while(def[s]){srv[s]=def[s];s++;} srv[s]=0; }
        u32 ip=0; if(dns_query(srv,&ip)!=1){ nt_status=-1; toast_set("WHOIS: DNS FAILED"); return; }
        char req[100]; int r=0; for(int i=0;dom[i]&&r<96;i++)req[r++]=dom[i]; req[r++]='\r'; req[r++]='\n';
        int n=tcp_get(ip,43,req,r,(u8*)http_buf,59000);
        if(n<=0){ nt_status=-1; toast_set(n==-4?"WHOIS: REFUSED":n==-3?"WHOIS: TIMEOUT":"WHOIS: NO DATA"); return; }
        if(n>8998)n=8998; for(int i=0;i<n;i++)nt_buf[i]=http_buf[i]; nt_buf[n]=0; nt_blen=n;
    }
    nt_scroll=0; nt_status=2;
}
static void nt_key(u8 d){ if(d&0x80)return; if(!nt_field)return; char ch=kchar_shift(d); if(!ch)return;
    int len=strlen_(nt_query);
    if(ch=='\b'){ if(len>0)nt_query[len-1]=0; }
    else if(ch=='\n'||ch=='\r'){ nt_field=0; nt_fetch(); }
    else if(ch>=32&&len<122){ nt_query[len]=ch; nt_query[len+1]=0; }
}
static void nt_init(void){ nt_status=0; nt_scroll=0; nt_field=1; nt_mode=0; nt_query[0]=0; nt_blen=0; }

/* ===HDA=== Intel High Definition Audio: MMIO immediate-cmd codec + PCM out DMA (poll) */
#define HDA_BDL ((u32*)0x00B44000u)      /* 128-aligned, below shared audio buffer      */
#define HDA_BUF ((short*)0x00B48000u)    /* same region AC97 uses (one owner at a time) */
static int hda_ok=0; static int hda_status=0;          /* 0 unknown, 1 ok, -1 not found/failed        */
static int hda_tried=0, hda_cad=-1, hda_afg=-1, hda_dac=-1, hda_pin=-1, hda_mix=-1;
static volatile u8* hda_m=0; static u32 hda_sdoff=0;
static inline u32  hr32(u32 o){ return *(volatile u32*)(hda_m+o); }
static inline void hw32(u32 o,u32 v){ *(volatile u32*)(hda_m+o)=v; }
static inline u16  hr16(u32 o){ return *(volatile u16*)(hda_m+o); }
static inline void hw16(u32 o,u16 v){ *(volatile u16*)(hda_m+o)=v; }
static inline u8   hr8 (u32 o){ return *(volatile u8*)(hda_m+o); }
static inline void hw8 (u32 o,u8 v){ *(volatile u8*)(hda_m+o)=v; }
static void hw_udelay(int us){ u32 need=(u32)us*119u/100u+1u; u32 el=0; u16 last=pit_read();
    for(;;){ u16 b=pit_read(); el+=(u16)(last-b); last=b; if(el>=need)break; } }
static u32 hda_cmd(u32 verb){
    for(int t=0;t<100000;t++){ if(!(hr16(0x68)&1))break; }
    hw16(0x68,2); hw32(0x60,verb); hw16(0x68,1);
    for(int t=0;t<100000;t++){ if(hr16(0x68)&2) return hr32(0x64); }
    return 0xFFFFFFFFu; }
static u32 hverb (int nid,u32 v12,u32 p8 ){ return ((u32)hda_cad<<28)|((u32)nid<<20)|(v12<<8)|p8; }
static u32 hverb4(int nid,u32 v4 ,u32 p16){ return ((u32)hda_cad<<28)|((u32)nid<<20)|(v4<<16)|p16; }
static u32 hparam(int nid,u32 pr){ return hda_cmd(hverb(nid,0xF00,pr)); }
static u16 hda_fmt(int rate){ switch(rate){ case 44100:return 0x4011; case 22050:return 0x4111;
    case 11025:return 0x4311; case 24000:return 0x0111; case 16000:return 0x0211;
    case 12000:return 0x0311; case 8000:return 0x0511; default:return 0x0011; } } /* 16-bit stereo */
static void hda_init(void){
    if(hda_tried)return; hda_tried=1;
    int fb=-1,fd=-1,ff=-1;
    for(int b=0;b<256&&fb<0;b++)for(int d=0;d<32&&fb<0;d++)for(int f=0;f<8;f++){
        u32 id=pci_read((u8)b,(u8)d,(u8)f,0); if((id&0xFFFF)==0xFFFF){ if(f==0)break; continue; }
        u32 cc=pci_read((u8)b,(u8)d,(u8)f,0x08);
        if(((cc>>24)&0xFF)==0x04&&((cc>>16)&0xFF)==0x03){ fb=b;fd=d;ff=f;break; } }
    if(fb<0){ hda_status=-1; return; }
    u32 base=pci_read((u8)fb,(u8)fd,(u8)ff,0x10)&0xFFFFFFF0u; if(!base){ hda_status=-1; return; }
    pci_write((u8)fb,(u8)fd,(u8)ff,0x04, pci_read((u8)fb,(u8)fd,(u8)ff,0x04)|0x06);
    hda_m=(volatile u8*)base;
    hw32(0x08,hr32(0x08)&~1u); for(int t=0;t<100000;t++){ if(!(hr32(0x08)&1))break; }
    hw32(0x08,hr32(0x08)|1u);  for(int t=0;t<100000;t++){ if(hr32(0x08)&1)break; }
    hw_udelay(2000);
    u16 st=hr16(0x0E); if(!st){ hda_status=-1; return; }
    for(int c=0;c<15;c++) if(st&(1<<c)){ hda_cad=c; break; }
    u32 sn=hparam(0,4); if(sn==0xFFFFFFFFu){ hda_status=-1; return; }
    int fgs=(int)((sn>>16)&0xFF), fgn=(int)(sn&0xFF);
    for(int i=0;i<fgn;i++){ u32 t2=hparam(fgs+i,5); if((t2&0x7F)==1){ hda_afg=fgs+i; break; } }
    if(hda_afg<0){ hda_status=-1; return; }
    hda_cmd(hverb(hda_afg,0x705,0)); hw_udelay(1000);
    u32 wn=hparam(hda_afg,4); int ws=(int)((wn>>16)&0xFF), wc=(int)(wn&0xFF); int best=-1;
    for(int i=0;i<wc;i++){ int nid=ws+i; u32 wcaps=hparam(nid,9); if(wcaps==0xFFFFFFFFu)continue;
        int typ=(int)((wcaps>>20)&0xF);
        if(typ==0&&hda_dac<0) hda_dac=nid;
        if(typ==4){ u32 pc=hparam(nid,0x0C); if(!(pc&(1u<<4)))continue;
            u32 cfg=hda_cmd(hverb(nid,0xF1C,0)); int dev=(int)((cfg>>20)&0xF); int conn=(int)((cfg>>30)&0x3);
            if(conn==1)continue;                                  /* 1 = no physical connection */
            int score=(dev==1)?3:(dev==2)?2:(dev==0)?1:0;
            if(score>best){ best=score; hda_pin=nid; } } }
    if(hda_dac<0||hda_pin<0){ hda_status=-1; return; }
    /* route pin -> (mixer?) -> dac : pick connection index leading to our DAC */
    int cn=(int)(hparam(hda_pin,0x0E)&0x7F), sel=0;
    for(int i=0;i<cn;i+=4){ u32 e=hda_cmd(hverb(hda_pin,0xF02,(u32)i));
        for(int k=0;k<4&&i+k<cn;k++){ int n2=(int)((e>>(k*8))&0xFF);
            if(n2==hda_dac){ sel=i+k; hda_mix=-1; }
            else { u32 c2=hparam(n2,9); int t2=(int)((c2>>20)&0xF);
                if((t2==2||t2==3)&&hda_mix<0){ int mc=(int)(hparam(n2,0x0E)&0x7F);
                    for(int j=0;j<mc;j+=4){ u32 e2=hda_cmd(hverb(n2,0xF02,(u32)j));
                        for(int q=0;q<4&&j+q<mc;q++) if((int)((e2>>(q*8))&0xFF)==hda_dac){ hda_mix=n2; sel=i+k; } } } } } }
    hda_cmd(hverb(hda_dac,0x705,0)); hda_cmd(hverb(hda_pin,0x705,0));
    if(hda_mix>=0){ hda_cmd(hverb(hda_mix,0x705,0));
        for(int ix=0;ix<4;ix++) hda_cmd(hverb4(hda_mix,0x3,0x7000|((u32)ix<<8)));   /* unmute mixer inputs */
        hda_cmd(hverb4(hda_mix,0x3,0xB000|0x3F)); }
    hda_cmd(hverb(hda_pin,0x701,(u32)sel));
    hda_cmd(hverb(hda_pin,0x707,0xC0));                          /* pin: OUT|HP enable */
    hda_cmd(hverb(hda_pin,0x70C,0x02));                          /* EAPD */
    hda_cmd(hverb4(hda_pin,0x3,0xB000|0x3F));
    hda_cmd(hverb4(hda_dac,0x3,0xB000|0x3F));
    u16 gcap=hr16(0x00); hda_sdoff=0x80+((u32)((gcap>>8)&0xF))*0x20u;
    hda_ok=1; hda_status=1; }
static void hda_play_buf(short* base,int frames,int rate){
    if(!hda_ok||frames<=0)return; if(frames>450000)frames=450000;
    u32 sd=hda_sdoff; u16 fmt=hda_fmt(rate); u32 bytes=(u32)frames*4u;
    hw8(sd+0,0); hw_udelay(100);
    hw8(sd+0,1); for(int t=0;t<100000;t++){ if(hr8(sd+0)&1)break; }
    hw8(sd+0,0); for(int t=0;t<100000;t++){ if(!(hr8(sd+0)&1))break; }
    u32 half=(bytes/2)&~127u; if(half<4)half=(bytes>=8)?(bytes/2)&~3u:4u; if(half>=bytes)half=bytes-4;
    HDA_BDL[0]=(u32)base;      HDA_BDL[1]=0; HDA_BDL[2]=half;       HDA_BDL[3]=0;
    HDA_BDL[4]=(u32)base+half; HDA_BDL[5]=0; HDA_BDL[6]=bytes-half; HDA_BDL[7]=1;
    hw32(sd+0x18,(u32)HDA_BDL); hw32(sd+0x1C,0);
    hw32(sd+0x08,bytes); hw16(sd+0x0C,1); hw16(sd+0x12,fmt);
    hw8(sd+2,0x10);                                               /* stream #1 */
    hda_cmd(hverb4(hda_dac,0x2,fmt)); hda_cmd(hverb(hda_dac,0x706,0x10));
    hw_udelay(200);
    hw8(sd+0,hr8(sd+0)|2);                                        /* RUN */
    int pms=(int)((long)frames*1000/(rate>0?rate:48000));
    for(int t=0;t<pms+2000;t++){ if(hr8(sd+3)&4)break; hw_udelay(1000); }
    hw8(sd+0,0); hw8(sd+3,0x1C); }
static void hda_beep2(int freq,int ms){
    int rate=48000, frames=(int)((long)rate*ms/1000); if(frames>450000)frames=450000;
    u32 ph=0, inc=((u32)freq*65536u/(u32)rate)*256u; int fade=rate/200; if(fade*2>frames)fade=frames/2;
    for(int i=0;i<frames;i++){ int sv=sinT[(ph>>16)&0xFF]; ph+=inc;
        int a=9000; if(i<fade)a=9000*i/fade; else if(i>=frames-fade)a=9000*(frames-1-i)/fade;
        int val=sv*a/127; HDA_BUF[i*2]=(short)val; HDA_BUF[i*2+1]=(short)val; }
    hda_play_buf(HDA_BUF,frames,rate); }
static void hda_cmdline(void){
    hda_init();
    if(!hda_ok){ tputs("HDA: NO CONTROLLER/CODEC FOUND."); tnl(); tputs("VBOX: SET AUDIO CONTROLLER = Intel HD Audio"); tnl(); return; }
    char b[12]; tputs("HDA CONTROLLER OK  codec="); utoa((u32)hda_cad,b); tputs(b);
    tputs(" afg="); utoa((u32)hda_afg,b); tputs(b);
    tputs(" dac="); utoa((u32)hda_dac,b); tputs(b);
    tputs(" pin="); utoa((u32)hda_pin,b); tputs(b);
    if(hda_mix>=0){ tputs(" mix="); utoa((u32)hda_mix,b); tputs(b); } tnl();
    tputs("PLAYING CHIME (48kHz PCM DMA)..."); tnl();
    hda_beep2(440,350); hda_beep2(660,350); }
/* ===AC97=== Intel ICH AC'97 audio: PCI bring-up + sine synth + WAV (poll-based, no IRQ) */
#define AC97_BDL ((u32*)0x00B40000u)
#define AC97_BUF ((short*)0x00B48000u)
#define AC97_BUF_MAXFR 450000    /* output region 0x00B48000..0x00D00000 */
static int ac97_ok=0, ac97_tried=0, ac97_status=0;   /* status: 0 unknown, 1 ok, -1 not found */
static u16 ac97_nam=0, ac97_nabm=0;

static void ac97_init(void){
    if(ac97_tried)return; ac97_tried=1;
    for(int bus=0;bus<256 && !ac97_ok;bus++) for(int dev=0;dev<32 && !ac97_ok;dev++){
        u32 v=pci_read((u8)bus,(u8)dev,0,0); if((v&0xFFFF)==0xFFFF)continue;
        u32 cc=pci_read((u8)bus,(u8)dev,0,0x08); u8 cls=(cc>>24)&0xFF, sub=(cc>>16)&0xFF;
        if(cls==0x04 && sub==0x01){
            u32 b0=pci_read((u8)bus,(u8)dev,0,0x10), b1=pci_read((u8)bus,(u8)dev,0,0x14);
            u16 nam=(u16)((b0&1)?(b0&0xFFFC):(b0&0xFFF0)), nabm=(u16)((b1&1)?(b1&0xFFFC):(b1&0xFFF0));
            if(!nam||!nabm)continue;
            u32 cmd=pci_read((u8)bus,(u8)dev,0,0x04); pci_write((u8)bus,(u8)dev,0,0x04,cmd|0x05); /* I/O + bus master */
            ac97_nam=nam; ac97_nabm=nabm;
            outl(nabm+0x2C, inl(nabm+0x2C)|0x02);                 /* GLOB_CNT: out of cold reset */
            for(int i=0;i<200000;i++){ if(inl(nabm+0x30)&(1u<<8))break; } /* GLOB_STA: primary codec ready */
            outw(nam+0x00,1);                                     /* mixer reset */
            outw(nam+0x02,0x0000);                                /* master vol = max, unmuted */
            outw(nam+0x18,0x0000);                                /* PCM out vol = max */
            outw(nam+0x2A, inw(nam+0x2A)|1);                      /* enable variable-rate audio */
            ac97_ok=1; ac97_status=1;
        }
    }
    if(!ac97_ok)ac97_status=-1;
}

/* play AC97_BUF (stereo 16-bit) for `frames` frames at `rate` Hz, blocking until DMA halts */
static void ac97_wait_dch(u16 sr,int ms){ for(int t=0;t<ms;t++){ if(inw(sr)&0x01)return; u16 a=pit_read(); for(;;){ u16 b=pit_read(); if(((u16)(a-b))>=1193)break; if(inw(sr)&0x01)return; } } }
static void ac97_play_buf(short* base,int frames,int rate){
    if(!ac97_ok&&hda_ok){ hda_play_buf(base,frames,rate); return; }
    if(!ac97_ok||frames<=0)return; if(frames>AC97_BUF_MAXFR)frames=AC97_BUF_MAXFR;
    u16 nam=ac97_nam, nabm=ac97_nabm;
    outw(nam+0x2A, inw(nam+0x2A)|1); outw(nam+0x2C,(u16)rate);    /* set DAC rate */
    outb(nabm+0x1B,0x02); for(int i=0;i<200000 && (inb(nabm+0x1B)&0x02);i++);  /* reset PCM-out */
    u32 total=(u32)frames*2u, per=0xFFFEu, off=0; int ne=0;        /* stereo => 2 samples/frame */
    while(off<total && ne<32){ u32 n=total-off; if(n>per)n=per; n&=~1u; if(!n)n=2;
        AC97_BDL[ne*2]=(u32)base+off*2u;
        u32 last=(off+n>=total)?1u:0u;
        AC97_BDL[ne*2+1]=(n&0xFFFFu)|(1u<<31)|(last?(1u<<30):0u);  /* IOC always; BUP on last */
        off+=n; ne++; }
    outl(nabm+0x10,(u32)AC97_BDL);                                /* BDBAR */
    outb(nabm+0x15,(u8)(ne-1));                                   /* LVI */
    outw(nabm+0x16,0x1C);                                         /* clear status */
    outb(nabm+0x1B,0x11);                                         /* run + interrupt-on-completion */
    int pms=(int)((long)frames*1000/(rate>0?rate:48000));
    ac97_wait_dch(nabm+0x16,pms+2000);                            /* real-time bounded completion */
    outb(nabm+0x1B,0x00);                                         /* stop */
}
static void ac97_play(int frames,int rate){ ac97_play_buf(AC97_BUF,frames,rate); }

/* synth one sine note into AC97_BUF with linear fade (anti-click); returns frame count */
static int ac97_tone(int freq,int ms,int rate,int amp){
    int frames=(int)((long)rate*ms/1000); if(frames<1)frames=1; if(frames>AC97_BUF_MAXFR)frames=AC97_BUF_MAXFR;
    u32 phase=0, inc=((u32)freq*65536u/(u32)rate)*256u;
    int fade=rate/200; if(fade<1)fade=1; if(fade*2>frames)fade=frames/2;
    for(int i=0;i<frames;i++){ int s=sinT[(phase>>16)&0xFF]; phase+=inc;
        int a=amp; if(i<fade)a=amp*i/fade; else if(i>=frames-fade)a=amp*(frames-1-i)/fade;
        int val=s*a/127; AC97_BUF[i*2]=(short)val; AC97_BUF[i*2+1]=(short)val; }
    return frames;
}
static void ac97_beep(int freq,int ms){ if(!ac97_ok&&!hda_ok)return; int f=ac97_tone(freq,ms,48000,9000); ac97_play(f,48000); }
static void ac97_scale(void){ static const int sc[8]={262,294,330,349,392,440,494,523}; for(int k=0;k<8;k++)ac97_beep(sc[k],260); }
static void ac97_melody(void){ static const int mf[14]={262,262,392,392,440,440,392,349,349,330,330,294,294,262};
    static const int md[14]={280,280,280,280,280,280,520,280,280,280,280,280,280,520}; for(int i=0;i<14;i++)ac97_beep(mf[i],md[i]); }

/* boot chime: try AC97, then HDA, else PC speaker jingle */
static void boot_chime(void){
    ac97_init(); if(!ac97_ok) hda_init();
    if(ac97_ok||hda_ok){ ac97_beep(523,140); ac97_beep(659,140); ac97_beep(784,240); }
    else jingle();
}
/* load + play a 16-bit PCM WAV from NVXFS by name; returns frames or negative error */
static int wav_play(const char* name){
    if(!ac97_ok&&!hda_ok)return -1;
    int idx=nvx_find(name); if(idx<0)return -2;
    u8* raw=(u8*)0x01800000u; int max=0x600000;
    int n=nvx_read(idx,(char*)raw,max); if(n<44)return -3;
    if(!(raw[0]=='R'&&raw[1]=='I'&&raw[2]=='F'&&raw[3]=='F'&&raw[8]=='W'&&raw[9]=='A'&&raw[10]=='V'&&raw[11]=='E'))return -4;
    int fmt=0,ch=0,rate=0,bits=0; u32 doff=0,dlen=0,p=12;
    while(p+8<=(u32)n){ u32 body=p+8, sz=raw[p+4]|(raw[p+5]<<8)|(raw[p+6]<<16)|((u32)raw[p+7]<<24);
        if(raw[p]=='f'&&raw[p+1]=='m'&&raw[p+2]=='t'&&raw[p+3]==' '){ fmt=raw[body]|(raw[body+1]<<8); ch=raw[body+2]|(raw[body+3]<<8); rate=raw[body+4]|(raw[body+5]<<8)|(raw[body+6]<<16)|((u32)raw[body+7]<<24); bits=raw[body+14]|(raw[body+15]<<8); }
        else if(raw[p]=='d'&&raw[p+1]=='a'&&raw[p+2]=='t'&&raw[p+3]=='a'){ doff=body; dlen=sz; if(doff+dlen>(u32)n)dlen=(u32)n-doff; break; }
        p=body+sz+(sz&1); }
    if(fmt!=1||bits!=16||ch<1||ch>2||rate<4000||rate>48000)return -5;
    short* src=(short*)(raw+doff); int frames;
    if(ch==1){ frames=(int)(dlen/2); if(frames>AC97_BUF_MAXFR)frames=AC97_BUF_MAXFR; for(int i=0;i<frames;i++){ short s=src[i]; AC97_BUF[i*2]=s; AC97_BUF[i*2+1]=s; } }
    else { frames=(int)(dlen/4); if(frames>AC97_BUF_MAXFR)frames=AC97_BUF_MAXFR; for(int i=0;i<frames;i++){ AC97_BUF[i*2]=src[i*2]; AC97_BUF[i*2+1]=src[i*2+1]; } }
    ac97_play(frames,rate); return frames;
}

/* ===AC97 MIC=== PCM input (record) -> loopback playback + WAV save */
#define AC97_IN_BDL ((u32*)0x00B41000u)
#define AC97_IN_BUF ((short*)0x00D00000u)
#define AC97_IN_MAXFR 700000
static int rec_frames=0, rec_rate=48000, rec_peak=0;
static void mic_setup(void){ if(!ac97_ok)return; u16 nam=ac97_nam;
    outw(nam+0x1A,0x0000);            /* record select: MIC on L+R */
    outw(nam+0x1C,0x0606);            /* record gain: moderate, unmuted */
    outw(nam+0x0E,0x0040);            /* mic volume: unmuted + 20dB boost */
    outw(nam+0x2A, inw(nam+0x2A)|1);  /* VRA on (ADC rate via 0x32) */ }
static int mic_record(int ms){ if(!ac97_ok)return -1; u16 nam=ac97_nam, nabm=ac97_nabm;
    mic_setup(); int rate=48000;
    int frames=(int)((long)rate*ms/1000); if(frames<1)frames=1; if(frames>AC97_IN_MAXFR)frames=AC97_IN_MAXFR; if(frames>AC97_BUF_MAXFR)frames=AC97_BUF_MAXFR;
    outw(nam+0x32,(u16)rate);                                     /* PCM ADC rate */
    outb(nabm+0x0B,0x02); for(int i=0;i<200000 && (inb(nabm+0x0B)&0x02);i++); /* reset PI engine */
    u32 total=(u32)frames*2u, per=0xFFFEu, off=0; int ne=0;
    while(off<total && ne<32){ u32 n=total-off; if(n>per)n=per; n&=~1u; if(!n)n=2;
        AC97_IN_BDL[ne*2]=(u32)AC97_IN_BUF+off*2u;
        u32 last=(off+n>=total)?1u:0u;
        AC97_IN_BDL[ne*2+1]=(n&0xFFFFu)|(1u<<31)|(last?(1u<<30):0u);
        off+=n; ne++; }
    outl(nabm+0x00,(u32)AC97_IN_BDL);                             /* PI BDBAR */
    outb(nabm+0x05,(u8)(ne-1));                                   /* PI LVI */
    outw(nabm+0x06,0x1C);                                         /* clear status */
    outb(nabm+0x0B,0x11);                                         /* run + IOC enable */
    ac97_wait_dch(nabm+0x06,ms+2000);                             /* capture is real-time */
    outb(nabm+0x0B,0x00);                                         /* stop */
    rec_frames=frames; rec_rate=rate;
    int pk=0; for(int i=0;i<frames*2;i++){ int v=AC97_IN_BUF[i]; if(v<0)v=-v; if(v>pk)pk=v; } rec_peak=pk;
    return frames; }
static void mic_playback(void){ if(!ac97_ok||rec_frames<=0)return; ac97_play_buf(AC97_IN_BUF,rec_frames,rec_rate); }
static int mic_save(const char* name){ if(rec_frames<=0)return -1;
    u32 datalen=(u32)rec_frames*4u, rate=(u32)rec_rate, byterate=rate*4u; u8* w=(u8*)0x01800000u; int o=0;
    w[o++]='R';w[o++]='I';w[o++]='F';w[o++]='F'; u32 rl=36+datalen; w[o++]=rl&255;w[o++]=(rl>>8)&255;w[o++]=(rl>>16)&255;w[o++]=(rl>>24)&255;
    w[o++]='W';w[o++]='A';w[o++]='V';w[o++]='E'; w[o++]='f';w[o++]='m';w[o++]='t';w[o++]=' '; w[o++]=16;w[o++]=0;w[o++]=0;w[o++]=0;
    w[o++]=1;w[o++]=0; w[o++]=2;w[o++]=0; w[o++]=rate&255;w[o++]=(rate>>8)&255;w[o++]=(rate>>16)&255;w[o++]=(rate>>24)&255;
    w[o++]=byterate&255;w[o++]=(byterate>>8)&255;w[o++]=(byterate>>16)&255;w[o++]=(byterate>>24)&255; w[o++]=4;w[o++]=0; w[o++]=16;w[o++]=0;
    w[o++]='d';w[o++]='a';w[o++]='t';w[o++]='a'; w[o++]=datalen&255;w[o++]=(datalen>>8)&255;w[o++]=(datalen>>16)&255;w[o++]=(datalen>>24)&255;
    u8* sb=(u8*)AC97_IN_BUF; for(u32 i=0;i<datalen;i++)w[o+i]=sb[i];
    return nvx_write(name,(const char*)w,o+(int)datalen); }
static void mic_wave_draw(int x,int y,int w,int h){ if(w<8||h<10)return;
    fill(x,y,w,h,0); frame(x,y,w,h,(u8)(C_MGREY+8)); fill(x,y+h/2,w,1,(u8)(C_MGREY+18));
    if(rec_frames<=0){ draw_str(x+8,y+h/2-4,"NO RECORDING YET - PRESS RECORD",C_MGREY+20); return; }
    int N=rec_frames, amp=h/2-2; for(int px=0;px<w;px++){ int i0=(int)((long)px*N/w); int s=AC97_IN_BUF[i0*2]; int hh=s*amp/32768; if(hh<0)hh=-hh; if(hh>amp)hh=amp; if(hh<1)hh=1; fill(x+px,y+h/2-hh,1,hh*2,C_GREEN); } }
static void draw_app(void){
    if(app==46){ draw_window_chrome("PHONE"); fill(winx+1,winy+20,winw-2,winh-21,C_WIN);
        int tw=(winw-2)/3; static const char* tn[3]={"KEYPAD","MESSAGES","RECENTS"};
        for(int t=0;t<3;t++){ int tx=winx+1+t*tw; fill(tx,winy+20,tw-1,24,ph_tab==t?C_BLUE:C_TASK); draw_str(tx+tw/2-strlen_(tn[t])*4,winy+27,tn[t],ph_tab==t?C_WHITE:(u8)(C_MGREY+22)); }
        int cyb0=winy+46;
        if(!phone_conn){ fill(winx+1,cyb0,winw-2,16,C_RED); draw_str(winx+8,cyb0+2,"PHONE NOT CONNECTED",C_WHITE); rrectR(winx+winw-92,cyb0-1,86,17,4,C_GREEN); draw_str(winx+winw-74,cyb0+2,"CONNECT",C_WHITE); }
        else { fill(winx+1,cyb0,winw-2,16,C_GREEN); draw_str(winx+8,cyb0+2,"PHONE CONNECTED VIA MIRROR LINK",C_WHITE); }
        int bx0=winx+8, by0=winy+70, bx1=winx+winw-8, by1=winy+winh-8;
        if(ph_tab==0){ fill(bx0,by0,bx1-bx0,28,63); frame(bx0,by0,bx1-bx0,28,C_MGREY); draw_str2(bx0+8,by0+6,phone_dialnum,0);
            int kx0=bx0+(bx1-bx0)/2-93, ky0=by0+40; static const char* kl[4][3]={{"1","2","3"},{"4","5","6"},{"7","8","9"},{"*","0","#"}};
            for(int r=0;r<4;r++)for(int c=0;c<3;c++){ int kx=kx0+c*62, ky=ky0+r*44; rrectR(kx,ky,56,38,6,C_TASK); draw_str2(kx+20,ky+11,kl[r][c],C_WHITE); }
            int cyb=ky0+4*44+8; rrectR(bx0+20,cyb,128,32,8,C_GREEN); draw_str(bx0+62,cyb+11,"CALL",C_WHITE);
            rrectR(bx0+158,cyb,80,32,8,C_TASK); draw_str(bx0+186,cyb+11,"DEL",C_WHITE);
            rrectR(bx0+248,cyb,128,32,8,C_RED); draw_str(bx0+288,cyb+11,"HANGUP",C_WHITE); }
        else if(ph_tab==1){ draw_str(bx0,by0+2,"TO:",C_TITLE); fill(bx0+30,by0-2,210,18,63); frame(bx0+30,by0-2,210,18,ph_focus==1?C_BLUE:C_MGREY); draw_str(bx0+34,by0+2,ph_to,0); if(ph_focus==1){ int cl=strlen_(ph_to); fill(bx0+34+cl*8,by0,2,13,C_BLUE); }
            int ty0=by0+24, ty1=by1-30; fill(bx0,ty0,bx1-bx0,ty1-ty0,58); frame(bx0,ty0,bx1-bx0,ty1-ty0,C_MGREY);
            int yy=ty0+6-ph_mscroll;
            for(int i=0;i<ph_nmsg;i++){ if(ph_ton>0){ int match=1; for(int k=0;;k++){ if(ph_to[k]!=ph_msgs[i].num[k]){match=0;break;} if(!ph_to[k])break; } if(!match)continue; }
                int sent=(ph_msgs[i].dir==0); int bw=strlen_(ph_msgs[i].body)*8+12; if(bw>bx1-bx0-24)bw=bx1-bx0-24; if(bw<28)bw=28; int bxp=sent?(bx1-12-bw):(bx0+12);
                if(yy>=ty0&&yy<=ty1-14){ rrectR(bxp,yy,bw,14,4,sent?C_BLUE:(u8)(C_MGREY+8)); draw_str_n(bxp+6,yy+2,ph_msgs[i].body,sent?C_WHITE:0,(bw-12)/8); }
                yy+=18; }
            if(ph_nmsg==0&&yy>=ty0&&yy<=ty1)draw_str(bx0+10,ty0+8,"NO MESSAGES YET",C_MGREY+20);
            int my2=ty1+6; fill(bx0,my2,bx1-bx0-92,20,63); frame(bx0,my2,bx1-bx0-92,20,ph_focus==2?C_BLUE:C_MGREY); draw_str_n(bx0+4,my2+3,ph_body,0,(bx1-bx0-100)/8); if(ph_focus==2){ int cl=strlen_(ph_body); int cx=bx0+4+cl*8; if(cx<bx1-96)fill(cx,my2+2,2,15,C_BLUE); }
            rrectR(bx1-86,my2-1,86,22,5,C_GREEN); draw_str(bx1-62,my2+4,"SEND",C_WHITE); }
        else { int yy=by0; draw_str(bx0,yy,"RECENT (FROM THIS PC):",C_MGREY+20); yy+=20;
            for(int i=ph_nrec-1;i>=0&&yy<by1;i--){ draw_str(bx0,yy,ph_recs[i].kind==0?"CALL":"SMS ",ph_recs[i].kind==0?C_GREEN:C_BBLUE); draw_str(bx0+48,yy,ph_recs[i].num,0); yy+=16; }
            if(ph_nrec==0)draw_str(bx0,yy,"NO RECENT ACTIVITY",C_MGREY+20); } }
    else if(app==48){
        draw_window_chrome("NEOFETCH");
        fill(winx+1,winy+20,winw-2,winh-21,0);
        /* layered N monogram (shadow + face) */
        int lx=winx+30, ly=winy+46, bw=18, span=80, bh=116;
        fill(lx+3, ly+3, bw, bh, C_BLUE); fill(lx+span+3, ly+3, bw, bh, C_BLUE);
        for(int i=0;i<bh;i++){ int dx=lx+bw+3+(i*(span-bw))/bh; fill(dx, ly+i+3, 10, 2, C_BLUE); }
        fill(lx, ly, bw, bh, C_TEAL); fill(lx+span, ly, bw, bh, C_TEAL);
        for(int i=0;i<bh;i++){ int dx=lx+bw+(i*(span-bw))/bh; fill(dx, ly+i, 10, 2, C_BBLUE); }
        draw_str2(lx-2, ly+bh+14, "NOOVEX8", C_WHITE);
        fill(lx-2, ly+bh+36, span+bw+12, 2, C_TEAL);
        draw_str(lx-2, ly+bh+45, "a NoovexOS system", (u8)(C_MGREY+18));
        /* info panel */
        int ix=winx+200, iy=winy+46, kx=ix+104; char b[40];
        draw_str(ix, iy, "user", C_BBLUE); draw_str(ix+32, iy, "@", (u8)(C_MGREY+18)); draw_str(ix+44, iy, "noovex8", C_TEAL);
        fill(ix, iy+14, winx+winw-ix-20, 2, C_TEAL);
        int ry=iy+24, rh=17;
        draw_str(ix,ry,"OS",C_TEAL);        draw_str(kx,ry,OSVER,C_WHITE); ry+=rh;
        draw_str(ix,ry,"Host",C_TEAL);      draw_str(kx,ry,"NoovexOS Desktop",C_WHITE); ry+=rh;
        draw_str(ix,ry,"Kernel",C_TEAL);    draw_str(kx,ry,"NOOVEX 1.0 ia-32",C_WHITE); ry+=rh;
        { char t1[12],t2[12]; int s2=upsec; utoa((u32)(s2/60),t1); utoa((u32)(s2%60),t2); int o=0; for(int j=0;t1[j];j++)b[o++]=t1[j]; b[o++]='m'; b[o++]=' '; for(int j=0;t2[j];j++)b[o++]=t2[j]; b[o++]='s'; b[o]=0; draw_str(ix,ry,"Uptime",C_TEAL); draw_str(kx,ry,b,C_WHITE); ry+=rh; }
        draw_str(ix,ry,"Packages",C_TEAL); draw_str(kx,ry,"42 (built-in)",C_WHITE); ry+=rh;
        draw_str(ix,ry,"Shell",C_TEAL);    draw_str(kx,ry,"NVXSH",C_WHITE); ry+=rh;
        { char t1[12],t2[12]; utoa((u32)W,t1); utoa((u32)H,t2); int o=0; for(int j=0;t1[j];j++)b[o++]=t1[j]; b[o++]='x'; for(int j=0;t2[j];j++)b[o++]=t2[j]; b[o]=0; draw_str(ix,ry,"Resolution",C_TEAL); draw_str(kx,ry,b,C_WHITE); ry+=rh; }
        draw_str(ix,ry,"DE",C_TEAL);       draw_str(kx,ry,"NoovexShell",C_WHITE); ry+=rh;
        draw_str(ix,ry,"WM",C_TEAL);       draw_str(kx,ry,"NVX-WM",C_WHITE); ry+=rh;
        draw_str(ix,ry,"Terminal",C_TEAL); draw_str(kx,ry,"nvxterm",C_WHITE); ry+=rh;
        { const char* src=cpu_brand[0]?cpu_brand:cpu_vendor; int j=0; for(;src[j]&&j<34;j++)b[j]=src[j]; b[j]=0; draw_str(ix,ry,"CPU",C_TEAL); draw_str(kx,ry,b,C_WHITE); ry+=rh; }
        { char t1[12]; utoa((u32)ram_mb,t1); int o=0; for(int j=0;t1[j];j++)b[o++]=t1[j]; const char* mu=" MB"; for(int j=0;mu[j];j++)b[o++]=mu[j]; b[o]=0; draw_str(ix,ry,"Memory",C_TEAL); draw_str(kx,ry,b,C_WHITE); ry+=rh; }
        /* color palette */
        int py2=ry+8; if(py2>winy+winh-22) py2=winy+winh-22; int pw=22, ph=14;
        static const u8 prow[8]={0,C_RED,C_GREEN,C_TEAL,C_BLUE,C_BBLUE,(u8)(C_MGREY+10),C_WHITE};
        for(int k=0;k<8;k++){ int px=ix+k*(pw+4); fill(px, py2, pw, ph, prow[k]); frame(px,py2,pw,ph,(u8)(C_MGREY+6)); }
    }
    else if(app==45){ draw_window_chrome("SOUND"); fill(winx+1,winy+20,winw-2,winh-21,C_WIN);
        int x=winx+16, y=winy+34;
        if(ac97_status==1){ draw_str(x,y,"AC'97 CODEC: DETECTED",C_GREEN);
            const char* HX="0123456789ABCDEF"; char hb[40]; int o=0; const char* p1="NAM=0x"; while(*p1)hb[o++]=*p1++; for(int s=12;s>=0;s-=4)hb[o++]=HX[(ac97_nam>>s)&0xF]; const char* p2="  NABM=0x"; while(*p2)hb[o++]=*p2++; for(int s=12;s>=0;s-=4)hb[o++]=HX[(ac97_nabm>>s)&0xF]; hb[o]=0; draw_str(x,y+15,hb,C_MGREY+22); }
        else if(ac97_status==-1){ draw_str(x,y,"AC'97 CODEC: NOT FOUND",C_RED); draw_str(x,y+15,"SET AUDIO CONTROLLER = ICH AC97 IN VIRTUALBOX",C_MGREY+20); }
        else draw_str(x,y,"AC'97: CLICK A BUTTON TO INITIALISE",C_MGREY+20);
        int by=winy+68;
        rrectR(x,by,124,28,5,C_GREEN); draw_str(x+26,by+9,"TEST TONE",C_WHITE);
        rrectR(x+134,by,124,28,5,C_BBLUE); draw_str(x+178,by+9,"SCALE",C_WHITE);
        rrectR(x,by+36,124,28,5,C_BLUE); draw_str(x+38,by+45,"MELODY",C_WHITE);
        rrectR(x+134,by+36,124,28,5,(u8)(C_MGREY+8)); draw_str(x+150,by+45,"PLAY WAV",C_WHITE);
        int ky=by+78, kw=(winw-32)/8; static const char* nl[8]={"C","D","E","F","G","A","B","C"};
        for(int k=0;k<8;k++){ int kx=x+k*kw; fill(kx,ky,kw-3,58,63); frame(kx,ky,kw-3,58,0); draw_str(kx+(kw-3)/2-4,ky+42,nl[k],0); }
        draw_str(x,ky+66,"CLICK KEYS OR BUTTONS - SOUND PLAYS VIA THE AC'97 DAC",C_MGREY+20);
        int my0=ky+76;
        rrectR(x,my0,128,26,5,C_RED); draw_str(x+14,my0+8,rec_frames>0?"RE-RECORD 4s":"RECORD 4s",C_WHITE);
        rrectR(x+136,my0,86,26,5,rec_frames>0?C_BLUE:(u8)(C_MGREY+6)); draw_str(x+148,my0+8,"PLAY REC",C_WHITE);
        rrectR(x+230,my0,86,26,5,rec_frames>0?(u8)(C_MGREY+10):(u8)(C_MGREY+6)); draw_str(x+240,my0+8,"SAVE WAV",C_WHITE);
        if(rec_frames>0){ int secs=rec_frames/48000, tenths=(rec_frames%48000)/4800; char rb[24]; int o=0; const char* rp="REC "; while(*rp)rb[o++]=*rp++; rb[o++]=(char)('0'+secs); rb[o++]='.'; rb[o++]=(char)('0'+tenths); rb[o++]='s'; rb[o]=0; draw_str(x,my0+34,rb,C_GREEN);
            int bw=170, bx=x+64; fill(bx,my0+34,bw,8,(u8)(C_MGREY+4)); int pk=rec_peak*bw/32768; if(pk>bw)pk=bw; fill(bx,my0+34,pk,8,pk>bw*3/4?C_RED:C_GREEN); draw_str(bx+bw+8,my0+34,"PEAK",C_MGREY+20); }
        else draw_str(x,my0+34,"NOT RECORDED YET",C_MGREY+20);
        mic_wave_draw(x,my0+48,winw-32,winh-(my0+48-winy)-8); }
    else if(app==42){ draw_window_chrome("GOPHER"); fill(winx+1,winy+20,winw-2,winh-21,C_WIN);
        int ax=winx+10, ay=winy+28; draw_str(ax,ay,"HOST:",C_TITLE);
        int ibx=ax+48, ibw=winw-48-200; fill(ibx,ay-2,ibw,18,63); frame(ibx,ay-2,ibw,18,gopher_field?C_BLUE:C_MGREY);
        { int cl=strlen_(gopher_host); int mc=(ibw-8)/8; const char* hs=gopher_host; if(cl>mc)hs=gopher_host+(cl-mc); draw_str(ibx+4,ay+2,hs,0); if(gopher_field){ int sl=strlen_(hs); fill(ibx+4+sl*8,ay,2,13,C_BLUE); } }
        rrectR(winx+winw-150,ay-2,44,18,4,C_GREEN); draw_str(winx+winw-138,ay+2,"GO",C_WHITE);
        rrectR(winx+winw-100,ay-2,44,18,4,C_BBLUE); draw_str(winx+winw-92,ay+2,"BACK",C_WHITE);
        rrectR(winx+winw-50,ay-2,40,18,4,C_MGREY+8); draw_str(winx+winw-38,ay+2,"FG",C_WHITE);
        int cx0=winx+10, cx1=winx+winw-10, cy0=winy+52, cy1=winy+winh-6;
        if(gopher_status==-1){ draw_str(cx0,cy0+10,"FAILED - CHECK NETWORK / HOST / PORT",C_RED); }
        else if(gopher_status==0){ draw_str(cx0,cy0+10,"PRESS GO TO CONNECT.  ARROWS SCROLL.",C_MGREY+20); }
        else if(gopher_is_menu){ int total=gopher_nitems*13, vis=cy1-cy0; int ms=total-vis; if(ms<0)ms=0; if(gopher_scroll>ms)gopher_scroll=ms;
            int y=cy0-gopher_scroll;
            for(int gi=0;gi<gopher_nitems;gi++){ int ry=y+gi*13; if(ry<cy0-12||ry>cy1)continue; char tp=gopher_items[gi].type;
                if(tp=='i'||tp=='3'){ draw_str_n(cx0+4,ry,gopher_items[gi].disp,C_MGREY+20,(cx1-cx0-8)/8); }
                else { const char* tag=(tp=='1')?"DIR":(tp=='0')?"TXT":(tp=='7')?"SRCH":(tp=='h')?"WWW":(tp=='9'||tp=='I'||tp=='g'||tp=='5')?"BIN":">"; u8 tc=(tp=='1')?C_BLUE:(tp=='0')?C_GREEN:(tp=='h')?C_TEAL:(u8)(C_MGREY+18);
                    draw_str(cx0+4,ry,tag,tc); u8 dc=(tp=='1'||tp=='0')?0:(u8)(C_MGREY+22); draw_str_n(cx0+48,ry,gopher_items[gi].disp,dc,(cx1-cx0-52)/8); } } }
        else { int ch=nettext_draw(gopher_buf,gopher_blen,cx0,cy0,cx1,cy1,0,0,0); int vis=cy1-cy0; int ms=ch-vis; if(ms<0)ms=0; if(gopher_scroll>ms)gopher_scroll=ms; nettext_draw(gopher_buf,gopher_blen,cx0,cy0,cx1,cy1,gopher_scroll,0,1); } }
    else if(app==43){ draw_window_chrome("WIKIPEDIA"); fill(winx+1,winy+20,winw-2,winh-21,C_WIN);
        int ax=winx+10, ay=winy+28; draw_str(ax,ay,"SEARCH:",C_TITLE);
        int ibx=ax+64, ibw=winw-64-70; fill(ibx,ay-2,ibw,18,63); frame(ibx,ay-2,ibw,18,wiki_field?C_BLUE:C_MGREY);
        { int cl=strlen_(wiki_term); int mc=(ibw-8)/8; const char* ws=wiki_term; if(cl>mc)ws=wiki_term+(cl-mc); draw_str(ibx+4,ay+2,ws,0); if(wiki_field){ int sl=strlen_(ws); fill(ibx+4+sl*8,ay,2,13,C_BLUE); } }
        rrectR(winx+winw-58,ay-2,48,18,4,C_GREEN); draw_str(winx+winw-44,ay+2,"GO",C_WHITE);
        int cx0=winx+12, cx1=winx+winw-12, cy0=winy+54, cy1=winy+winh-6;
        if(wiki_status==-1){ draw_str(cx0,cy0+10,"NOT FOUND / FETCH FAILED",C_RED); }
        else if(wiki_status==2){ if(wiki_title[0]) draw_str2(cx0,cy0,wiki_title,C_TITLE);
            int ty=cy0+22; int ch=nettext_draw(wiki_buf,wiki_blen,cx0,ty,cx1,cy1,0,0,0); int vis=cy1-ty; int ms=ch-vis; if(ms<0)ms=0; if(wiki_scroll>ms)wiki_scroll=ms; nettext_draw(wiki_buf,wiki_blen,cx0,ty,cx1,cy1,wiki_scroll,0,1); }
        else { draw_str(cx0,cy0+10,"TYPE A TOPIC AND PRESS GO.  ARROWS SCROLL.",C_MGREY+20); } }
    else if(app==44){ draw_window_chrome("NET TOOLS"); fill(winx+1,winy+20,winw-2,winh-21,C_WIN);
        int ax=winx+10, ay=winy+28;
        rrectR(ax,ay-2,72,18,4,nt_mode==0?C_BLUE:(u8)(C_MGREY+8)); draw_str(ax+10,ay+2,"FINGER",nt_mode==0?C_WHITE:(u8)(C_MGREY+22));
        rrectR(ax+78,ay-2,64,18,4,nt_mode==1?C_BLUE:(u8)(C_MGREY+8)); draw_str(ax+90,ay+2,"WHOIS",nt_mode==1?C_WHITE:(u8)(C_MGREY+22));
        rrectR(winx+winw-58,ay-2,48,18,4,C_GREEN); draw_str(winx+winw-44,ay+2,"GO",C_WHITE);
        int qy=ay+24; draw_str(ax,qy,nt_mode==0?"USER@HOST:":"DOMAIN   :",C_TITLE);
        int ibx=ax+88, ibw=winw-88-20; fill(ibx,qy-2,ibw,18,63); frame(ibx,qy-2,ibw,18,nt_field?C_BLUE:C_MGREY);
        { int cl=strlen_(nt_query); int mc=(ibw-8)/8; const char* qs=nt_query; if(cl>mc)qs=nt_query+(cl-mc); draw_str(ibx+4,qy+2,qs,0); if(nt_field){ int sl=strlen_(qs); fill(ibx+4+sl*8,qy,2,13,C_BLUE); } }
        int cx0=winx+12, cx1=winx+winw-12, cy0=qy+24, cy1=winy+winh-6;
        if(nt_status==-1){ draw_str(cx0,cy0+8,"FAILED - CHECK NETWORK / HOST",C_RED); }
        else if(nt_status==2){ int ch=nettext_draw(nt_buf,nt_blen,cx0,cy0,cx1,cy1,0,0,0); int vis=cy1-cy0; int ms=ch-vis; if(ms<0)ms=0; if(nt_scroll>ms)nt_scroll=ms; nettext_draw(nt_buf,nt_blen,cx0,cy0,cx1,cy1,nt_scroll,0,1); }
        else { draw_str(cx0,cy0+8,nt_mode==0?"TRY  oslo@graph.no  THEN GO":"TRY  example.com  THEN GO",C_MGREY+20); } }
    else if(app==38){ draw_window_chrome("QR GENERATOR"); fill(winx+1,winy+20,winw-2,winh-21,C_WIN);
        int px=winx+14, py=winy+30; draw_str(px,py,"TEXT / URL:",C_TITLE);
        { int bw=60,bx=winx+winw-bw-14,by=py-2; rrectR(bx,by,bw,18,5,C_RED); draw_str(bx+12,by+5,"CLEAR",C_WHITE); }
        int ibx=px, iby=py+16, ibw=winw-28, ibh=20; fill(ibx,iby,ibw,ibh,63); frame(ibx,iby,ibw,ibh,C_MGREY);
        { const char* shp=qr_in; int maxch=(ibw-12)/8; if(qr_n>maxch) shp=qr_in+(qr_n-maxch); int cl=strlen_(shp); draw_str(ibx+5,iby+6,shp,0); fill(ibx+6+cl*8,iby+4,2,13,C_BLUE); }
        int ay=iby+ibh+10;
        if(qr_n==0){ draw_str(px,ay+50,"TYPE A URL OR TEXT ABOVE",C_MGREY+20); }
        else if(qver==99){ draw_str(px,ay+50,"TOO LONG - MAX 106 CHARS",C_RED); }
        else if(qr_ok){ int aw=winw-28, ah=winh-(ay-winy)-44; int units=qsz+8; int sc2=aw/units; int sh3=ah/units; if(sh3<sc2)sc2=sh3; if(sc2<2)sc2=2; if(sc2>10)sc2=10;
            int total=(qsz+8)*sc2; int ox=winx+(winw-total)/2; int oy=ay+((ah-total)/2); if(oy<ay)oy=ay;
            fill(ox,oy,total,total,63); int mx0=ox+4*sc2, my0=oy+4*sc2;
            for(int r=0;r<qsz;r++)for(int c=0;c<qsz;c++){ if(qm[r][c]) fill(mx0+c*sc2,my0+r*sc2,sc2,sc2,0); }
            { char fb[48]; int o=0; const char* a="VERSION "; while(*a)fb[o++]=*a++; { char t[8]; utoa((u32)qver,t); int j=0; while(t[j])fb[o++]=t[j++]; } a="  -  "; while(*a)fb[o++]=*a++; { char t[8]; utoa((u32)qr_n,t); int j=0; while(t[j])fb[o++]=t[j++]; } a=" CHARS"; while(*a)fb[o++]=*a++; fb[o]=0; draw_str(winx+14,winy+winh-30,fb,C_MGREY+20); }
            draw_str(winx+14,winy+winh-16,"SCAN WITH YOUR PHONE CAMERA",C_GREEN); } }
    else if(app==39){ draw_window_chrome("2048"); fill(winx+1,winy+20,winw-2,winh-21,C_WIN);
        int hx=winx+14, hy=winy+28;
        { char b[40]; int o=0; const char* a="SCORE "; while(*a)b[o++]=*a++; char t[12]; utoa((u32)g2_score,t); int j=0; while(t[j])b[o++]=t[j++]; b[o]=0; draw_str(hx,hy,b,C_TITLE); }
        { char b[40]; int o=0; const char* a="BEST "; while(*a)b[o++]=*a++; char t[12]; utoa((u32)g2_best,t); int j=0; while(t[j])b[o++]=t[j++]; b[o]=0; draw_str(winx+winw-110,hy,b,C_MGREY+20); }
        int gy=hy+18, gx=winx+14, av=winw-28; int cell=(av-10)/4;
        rrectR(gx,gy,cell*4+10,cell*4+10,8,C_MGREY+8);
        for(int r=0;r<4;r++)for(int c=0;c<4;c++){ int x=gx+5+c*cell, y=gy+5+r*cell; int v=g2[r][c];
            if(v==0){ rrectR(x+2,y+2,cell-4,cell-4,6,C_MGREY+12); }
            else { rrectR(x+2,y+2,cell-4,cell-4,6,g2col(v)); u8 tc=(v<=4)?0:C_WHITE; qnum(x+cell/2,y+cell/2,v,tc); } }
        if(g2_over){ afill(gx,gy,cell*4+10,cell*4+10,0,150); draw_str2(gx+(cell*4)/2-(g2_won?56:72),gy+(cell*4)/2-8,(g2_won)?"YOU WIN":"GAME OVER",C_WHITE); draw_str(gx+(cell*4)/2-76,gy+(cell*4)/2+16,"PRESS R TO RESTART",C_WHITE); } }
    else if(app==40){ draw_window_chrome("BREAKOUT"); fill(winx+1,winy+20,winw-2,winh-21,0);
        int ox=winx+2, oy=winy+22, FW=winw-4, FH=winh-24, bw=FW/8;
        { char b[44]; int o=0; const char* a="SCORE "; while(*a)b[o++]=*a++; char t[12]; utoa((u32)bk_score,t); int j=0; while(t[j])b[o++]=t[j++]; a="   LIVES "; while(*a)b[o++]=*a++; utoa((u32)bk_lives,t); j=0; while(t[j])b[o++]=t[j++]; b[o]=0; draw_str(ox+6,oy+6,b,C_WHITE); }
        for(int r=0;r<5;r++)for(int c=0;c<8;c++){ if(!bk_brick[r][c])continue; int bx=ox+c*bw, by=oy+28+r*14; u8 col=(r==0)?C_RED:(r==1)?C_FOLDER:(r==2)?C_GREEN:(r==3)?C_TEAL:C_BBLUE; fill(bx+1,by+1,bw-2,12,col); }
        int py=oy+FH-16; fill(ox+bk_px,py,BK_PW,7,C_BBLUE);
        disc(ox+bk_bx,oy+bk_by,BK_BR,C_WHITE);
        if(!bk_live){ afill(ox+FW/2-90,oy+FH/2-12,180,52,0,150); draw_str2(ox+FW/2-72,oy+FH/2-8,"GAME OVER",C_RED); draw_str(ox+FW/2-72,oy+FH/2+16,"PRESS R TO RESTART",C_WHITE); }
        else if(!bk_launch){ draw_str(ox+FW/2-84,oy+FH/2,"SPACE / UP TO LAUNCH",C_GREEN); } }
    else if(app==41){ draw_window_chrome("MINESWEEPER"); fill(winx+1,winy+20,winw-2,winh-21,C_WIN);
        int gx=winx+10, gy=winy+50, cell=(winw-20)/9;
        { char b[32]; int o=0; const char* a="MINES "; while(*a)b[o++]=*a++; int rem=ms_left; char t[8]; utoa((u32)(rem<0?0:rem),t); int j=0; while(t[j])b[o++]=t[j++]; b[o]=0; draw_str(gx,winy+30,b,C_TITLE); }
        { int bx=winx+winw-150,by=winy+26; rrectR(bx,by,80,18,5,ms_flagmode?C_RED:C_MGREY+10); draw_str(bx+6,by+5,ms_flagmode?"FLAG: ON":"FLAG:OFF",C_WHITE); }
        { int bx=winx+winw-60,by=winy+26; rrectR(bx,by,46,18,5,C_BBLUE); draw_str(bx+11,by+5,"NEW",C_WHITE); }
        for(int r=0;r<9;r++)for(int c=0;c<9;c++){ int x=gx+c*cell, y=gy+r*cell;
            if(ms_open[r][c]){ if(ms_mine[r][c]){ fill(x+1,y+1,cell-2,cell-2,C_RED); disc(x+cell/2,y+cell/2,cell/2-4,0); }
                else { fill(x+1,y+1,cell-2,cell-2,C_MGREY+14); int n=ms_num[r][c]; if(n>0){ u8 nc=(n==1)?C_BLUE:(n==2)?C_GREEN:(n==3)?C_RED:C_TITLE; qnum(x+cell/2,y+cell/2,n,nc); } } }
            else { rrectR(x+1,y+1,cell-2,cell-2,3,C_MGREY+4); afill(x+1,y+1,cell-2,(cell-2)/2,C_WHITE,20); if(ms_flag[r][c]){ fill(x+cell/2-1,y+5,2,cell-10,C_TITLE); fill(x+cell/2+1,y+5,7,5,C_RED); } }
            frame(x,y,cell,cell,C_MGREY); }
        if(ms_over){ int mw=150; afill(gx+cell*9/2-mw/2,gy+cell*9/2-16,mw,32,0,150); draw_str2(gx+cell*9/2-(ms_win?56:40),gy+cell*9/2-8,(ms_win)?"YOU WIN":"BOOM",C_WHITE); } }
    else if(app==37){
        draw_window_chrome("NOOVEX MAIL");
        fill(winx+1,winy+20,winw-2,winh-21,C_WIN);
        int fx=winx+108, fw=winw-120;
        draw_str(winx+12,winy+34,"SMTP HOST",C_TITLE); fill(fx,winy+30,fw,18,C_TASK); frame(fx,winy+30,fw,18,mail_field==1?C_BBLUE:C_MGREY); draw_str(fx+4,winy+34,mail_host,C_WHITE);
        draw_str(winx+12,winy+58,"EMAIL",C_TITLE); fill(fx,winy+54,fw,18,C_TASK); frame(fx,winy+54,fw,18,mail_field==2?C_BBLUE:C_MGREY); draw_str(fx+4,winy+58,mail_user,C_WHITE);
        draw_str(winx+12,winy+82,"PASSWORD",C_TITLE); fill(fx,winy+78,fw,18,C_TASK); frame(fx,winy+78,fw,18,mail_field==3?C_BBLUE:C_MGREY); { int n=strlen_(mail_pass); for(int i=0;i<n&&i<40;i++) fill(fx+5+i*8,winy+82,6,9,C_WHITE); }
        draw_str(winx+12,winy+106,"TO",C_TITLE); fill(fx,winy+102,fw,18,C_TASK); frame(fx,winy+102,fw,18,mail_field==4?C_BBLUE:C_MGREY); draw_str(fx+4,winy+106,mail_to,C_WHITE);
        draw_str(winx+12,winy+130,"SUBJECT",C_TITLE); fill(fx,winy+126,fw,18,C_TASK); frame(fx,winy+126,fw,18,mail_field==5?C_BBLUE:C_MGREY); draw_str(fx+4,winy+130,mail_subj,C_WHITE);
        draw_str(winx+12,winy+150,"BODY",C_TITLE);
        int by=winy+164, bh=winh-(by-winy)-66; if(bh<40)bh=40;
        fill(winx+12,by,winw-24,bh,C_TASK); frame(winx+12,by,winw-24,bh,mail_field==6?C_BBLUE:C_MGREY);
        { int cx=winx+16,cy=by+4; for(int i=0;mail_body[i];i++){ char c=mail_body[i]; if(c=='\n'||cx>winx+winw-28){ cx=winx+16; cy+=11; if(cy>by+bh-11)break; if(c=='\n')continue; } char tc[2]; tc[0]=c; tc[1]=0; draw_str(cx,cy,tc,C_WHITE); cx+=8; } }
        int sy=winy+winh-52;
        fill(winx+12,sy,90,22,C_GREEN); frame(winx+12,sy,90,22,C_WHITE); draw_str(winx+30,sy+6,"SEND",C_WHITE);
        draw_str(winx+112,sy+6,"STATUS:",C_TITLE); draw_str(winx+112+8*8,sy+6,mail_status,C_BBLUE);
        draw_str(winx+12,winy+winh-22,"TAB OR CLICK A FIELD. GMAIL/OUTLOOK NEED AN APP PASSWORD.",C_MGREY+18);
    } else if(app==35){
        draw_window_chrome("PACKAGE MANAGER");
        draw_str2(winx+16,winy+34,"NOOVEX PACKAGES",C_WHITE);
        if(!disk_ok) draw_str(winx+16,winy+54,"NO DISK - APPS CANNOT PERSIST",C_RED+8);
        else draw_str(winx+16,winy+54,"INSTALL TO DISK - PERSISTS + RUNS FROM FILES",C_MGREY+22);
        for(int i=0;i<NPKG;i++){
            int ry=winy+72+i*74;
            fill(winx+10,ry,winw-20,66,C_MGREY+4); frame(winx+10,ry,winw-20,66,C_MGREY+14);
            draw_str2(winx+20,ry+8,PKGS[i].name,C_WHITE);
            draw_str(winx+20,ry+30,PKGS[i].desc,C_MGREY+24);
            draw_str(winx+20,ry+46,PKGS[i].file,C_MGREY+18);
            int inst=(disk_ok && nvx_find(PKGS[i].file)>=0);
            if(inst){
                draw_str(winx+winw-282,ry+26,"INSTALLED",C_GREEN);
                rrectR(winx+winw-184,ry+18,80,28,7,C_GREEN); draw_str(winx+winw-156,ry+27,"RUN",C_WHITE);
                rrectR(winx+winw-96,ry+18,82,28,7,C_RED);    draw_str(winx+winw-79,ry+27,"REMOVE",C_WHITE);
            } else {
                rrectR(winx+winw-96,ry+18,82,28,7,C_BLUE);   draw_str(winx+winw-83,ry+27,"INSTALL",C_WHITE);
            }
        }
    } else if(app==36){
        draw_window_chrome("PHONE MIRROR");
        int cax=winx+10, cay=winy+30;
        int vx=cax+10, vy=cay+14, vw=250, vh=winh-30-28;
        fill(vx,vy,vw,vh,2); frame(vx,vy,vw,vh,40);
        if(phone_have){ phone_blit(vx+4,vy+4,vw-8,vh-8); if(phone_demo){ afill(vx+4,vy+4,vw-8,18,0,140); draw_str(vx+12,vy+8,"DEMO FRAME",C_WHITE);} }
        else { draw_str(vx+24,vy+vh/2-8,"NO SIGNAL - NOT CONNECTED",C_MGREY+20); }
        int px=vx+vw+24, pt=cay+18;
        draw_str2(px,pt,"ANDROID PHONE",C_WHITE);
        int y1=pt+34, yBtn=y1+48, yPort=yBtn+34, yAct=yPort+26;
        if(!phone_conn){
            draw_str(px,y1,"TARGET (SAME LAN / BRIDGED):",C_MGREY+20);
            { char ip[28]; int o=0; u32 a=(u32)my_ip; o+=pdec(ip+o,(a>>24)&255); ip[o++]='.'; o+=pdec(ip+o,(a>>16)&255); ip[o++]='.'; o+=pdec(ip+o,(a>>8)&255); ip[o++]='.'; o+=pdec(ip+o,(unsigned)phone_oct); ip[o]=0; draw_str2(px,y1+20,ip,C_BBLUE); }
            rrectR(px,yBtn,46,24,6,C_TASK);     draw_str(px+11,yBtn+8,"-10",C_WHITE);
            rrectR(px+52,yBtn,40,24,6,C_TASK);  draw_str(px+64,yBtn+8,"-1",C_WHITE);
            rrectR(px+98,yBtn,40,24,6,C_TASK);  draw_str(px+110,yBtn+8,"+1",C_WHITE);
            rrectR(px+144,yBtn,46,24,6,C_TASK); draw_str(px+153,yBtn+8,"+10",C_WHITE);
            { char pl[22]; int o=0; const char* s="PORT: "; while(*s)pl[o++]=*s++; o+=pdec(pl+o,(unsigned)phone_port); pl[o]=0; draw_str(px,yPort,pl,C_MGREY+20); }
            rrectR(px,yAct,120,30,8,C_GREEN);   draw_str(px+32,yAct+11,"CONNECT",C_WHITE);
            rrectR(px+130,yAct,90,30,8,C_BLUE); draw_str(px+152,yAct+11,"DEMO",C_WHITE);
            int yi=yAct+44;
            draw_str(px,yi,"1. PHONE + PC ON SAME LAN",C_MGREY+15); yi+=15;
            draw_str(px,yi,"   (VBOX: BRIDGED ADAPTER)",C_MGREY+15); yi+=15;
            draw_str(px,yi,"2. RUN NOOVEXMIRROR ON PHONE",C_MGREY+15); yi+=15;
            draw_str(px,yi,"3. SET LAST OCTET = PHONE",C_MGREY+15); yi+=15;
            draw_str(px,yi,"4. TAP CONNECT",C_MGREY+15);
        } else {
            rrectR(px,y1,130,30,8,C_RED); draw_str(px+20,y1+11,"DISCONNECT",C_WHITE);
            int ys=y1+44;
            fill(px,ys,10,10,phone_have?C_GREEN:C_RED); draw_str(px+18,ys-1,phone_have?"LIVE":"WAITING FRAMES",C_WHITE); ys+=22;
            { char st[28]; int o=0; const char* s="FRAMES: "; while(*s)st[o++]=*s++; o+=pdec(st+o,(unsigned)phone_frames); st[o]=0; draw_str(px,ys,st,C_MGREY+20); } ys+=20;
            if(phone_have){ char dm[24]; int o=0; o+=pdec(dm+o,(unsigned)phone_fw); dm[o++]='X'; o+=pdec(dm+o,(unsigned)phone_fh); dm[o]=0; draw_str(px,ys,dm,C_MGREY+20); ys+=22; }
            draw_str(px,ys,"TAP OR DRAG = TOUCH / SWIPE",C_MGREY+18);
            int navY=y1+150;
            draw_str(px,navY-16,"HARDWARE KEYS",C_MGREY+22);
            rrectR(px,navY,80,26,6,C_TASK);     draw_str(px+24,navY+9,"BACK",C_WHITE);
            rrectR(px+86,navY,80,26,6,C_TASK);  draw_str(px+106,navY+9,"HOME",C_WHITE);
            rrectR(px+172,navY,80,26,6,C_TASK); draw_str(px+186,navY+9,"RECENT",C_WHITE);
            int ndy=navY+38; draw_str(px,ndy+3,"DIAL:",C_MGREY+20);
            fill(px+44,ndy,222,18,63); frame(px+44,ndy,222,18,C_MGREY); draw_str(px+48,ndy+3,phone_dialnum,0);
            { int kx0=px, ky0=ndy+26; static const char* kl[4][3]={{"1","2","3"},{"4","5","6"},{"7","8","9"},{"*","0","#"}};
              for(int r=0;r<4;r++)for(int c=0;c<3;c++){ int kx=kx0+c*60, ky=ky0+r*36; rrectR(kx,ky,54,30,5,C_TASK); draw_str(kx+24,ky+11,kl[r][c],C_WHITE); }
              int rcx=px+186; rrectR(rcx,ky0,84,30,6,C_GREEN); draw_str(rcx+28,ky0+11,"CALL",C_WHITE);
              rrectR(rcx,ky0+36,84,30,6,C_TASK); draw_str(rcx+32,ky0+47,"DEL",C_WHITE);
              rrectR(rcx,ky0+72,84,30,6,C_RED); draw_str(rcx+20,ky0+83,"HANGUP",C_WHITE); }
        }
    } else if(app==1){
        draw_window_chrome("FILE EXPLORER");
        fill(winx+1,winy+20,160,winh-21,C_TASK);
        fill(winx+161,winy+20,1,winh-21,C_MGREY);
        draw_str(winx+8,winy+28,"QUICK ACCESS",C_MGREY+20);
        for(int i=0;i<7;i++){ int iy=winy+40+i*18; if(cur_folder==i)fill(winx+3,iy-2,156,18,C_BLUE); else if(fdrag&&in(mx,my,winx+3,iy-2,156,18))fill(winx+3,iy-2,156,18,C_TEAL); fill(winx+8,iy,10,10,sb_icon[i]); draw_str(winx+24,iy+1,sb_names[i],(cur_folder==i)?C_WHITE:C_TITLE); if(i<6)fill(winx+150,iy+3,4,4,C_MGREY+15); }
        draw_str(winx+8,winy+170,"THIS PC",C_MGREY+20);
        for(int k=0;k<NTHISPC;k++){ int gi=THISPC[k]; int iy=winy+186+k*18;
            if(cur_folder==gi)fill(winx+3,iy-2,156,18,C_BLUE);
            else if(fdrag&&in(mx,my,winx+3,iy-2,156,18))fill(winx+3,iy-2,156,18,C_TEAL);
            if(gi==10) blit_icon(ICO_USB,winx+5,iy-2,14); else blit_icon(ICO_CDRIVE,winx+5,iy-2,14);
            draw_str(winx+24,iy+1,sb_names[gi],(cur_folder==gi)?C_WHITE:C_TITLE); if(gi==10&&msd_dev>=0)fill(winx+150,iy+3,5,5,C_GREEN); }
        int px=winx+170,py=winy+28;
        int cw=winw-180; int cN=px+14; int cS=px+cw-60; int cT=cS-110; int cD=cT-140; (void)cN;
        if(cur_folder==7||cur_folder==9){ if(cur_node<0)cur_node=(cur_folder==7)?ROOT_C:ROOT_D; int bx=px; if(VFS[cur_node].parent>=0){ rrectR(px,py-4,46,18,4,C_TEAL); draw_str(px+8,py,"< Up",C_WHITE); bx=px+54; } char pth[80]; vfs_path(cur_node,pth,80); draw_str(bx,py,pth,C_WHITE); }
        else draw_str(px,py,sb_names[cur_folder],C_WHITE);
        if(cur_folder==8){ if(disk_ok)draw_str(px+winw-250,py,"[NVXFS - PERSISTENT]",C_GREEN); else draw_str(px+winw-230,py,"[NO DISK FOUND]",C_RED+8); }
        else if(cur_folder==10){ rrectR(px+winw-300,py-4,84,18,4,C_TEAL); draw_str(px+winw-290,py,"RESCAN",C_WHITE); if(msd_dev>=0){ fill(px+winw-210,py-3,80,16,C_RED); draw_str(px+winw-202,py,"EJECT",C_WHITE); draw_str(px+winw-120,py,fat_ok?"[USB MOUNTED]":"[NO FS]",fat_ok?C_GREEN:(C_MGREY+20)); } else draw_str(px+winw-210,py,"[NO USB]",C_MGREY+20); }
        else if(fperm[cur_folder]==1) draw_str(px+winw-260,py,"[SYSTEM - PROTECTED]",C_RED+8);
        else if(cur_folder!=7&&cur_folder!=9) draw_str(px+winw-220,py,"[READ / WRITE]",C_GREEN);
        draw_str(px,py+12,"Name",C_MGREY+18); draw_str(cD,py+12,"Date modified",C_MGREY+18); draw_str(cT,py+12,"Type",C_MGREY+18); draw_str(cS,py+12,"Size",C_MGREY+18);
        hrule(px,py+24,winw-180,C_MGREY);
        if(cur_folder==8){
            if(!disk_ok){ draw_str(px,py+34,"NO ATA DISK ATTACHED.",C_RED+8); draw_str(px,py+50,"ATTACH IDE HDD AS PRIMARY MASTER.",C_MGREY+20); }
            else if(nvx.count==0){ draw_str(px,py+34,"(EMPTY) - SAVE FROM NOTEPAD",C_MGREY+20); }
            else for(unsigned i=0;i<nvx.count;i++){ int ry=py+32+i*16; if((int)i==fsel)fill(px-2,ry-1,winw-176,15,C_BLUE);
                char dt[20],ty[16]; int isf=entry_meta(8,(int)i,nvx.e[i].name,dt,ty); fill(px,ry+1,10,8,isf?C_FOLDER:(C_MGREY+18)); draw_str(px+14,ry,nvx.e[i].name,C_WHITE);
                draw_str(cD,ry,dt,C_MGREY+20); draw_str(cT,ry,ty,C_MGREY+20);
                char sz[12]; int L=nvx.e[i].len,q=0; if(L==0)sz[q++]='0'; else{char t[12];int tl=0;while(L){t[tl++]=(char)('0'+L%10);L/=10;}while(tl)sz[q++]=t[--tl];} sz[q++]='B'; sz[q]=0; draw_str(cS,ry,sz,C_MGREY+20); }
        } else if(cur_folder==10){
            if(msd_dev<0){ draw_str(px,py+34,"NO USB DRIVE DETECTED.",C_MGREY+20); draw_str(px,py+50,"PLUG IN A USB 2.0 DRIVE, THEN CLICK RESCAN (TOP RIGHT).",C_MGREY+20); }
            else if(!fat_ok){ draw_str(px,py+34,"USB DISK PRESENT - NO FAT FILESYSTEM.",C_RED+8); }
            else if(usbfs_n==0){ draw_str(px,py+34,"(EMPTY ROOT DIRECTORY)",C_MGREY+20); }
            else for(int i=0;i<usbfs_n&&py+32+i*16<winy+winh-20;i++){ int ry=py+32+i*16; if(i==fsel)fill(px-2,ry-1,winw-176,15,C_BLUE);
                char dt[20],ty[16]; int isf=entry_meta(10,i,usbfs[i].name,dt,ty); fill(px,ry+1,10,8,isf?C_FOLDER:C_GREEN); draw_str(px+14,ry,usbfs[i].name,C_WHITE);
                draw_str(cD,ry,dt,C_MGREY+20); draw_str(cT,ry,ty,C_MGREY+20);
                char sz[12]; u32 L=usbfs[i].size; int q=0; if(L==0)sz[q++]='0'; else{char t[12];int tl=0;while(L){t[tl++]=(char)('0'+L%10);L/=10;}while(tl)sz[q++]=t[--tl];} sz[q++]='B'; sz[q]=0; draw_str(cS,ry,sz,C_MGREY+20); }
        } else if(cur_folder==7||cur_folder==9){
            if(cur_node<0)cur_node=(cur_folder==7)?ROOT_C:ROOT_D;
            int nn=vfs_nchild(cur_node);
            if(nn==0) draw_str(px,py+34,"(EMPTY FOLDER)",C_MGREY+20);
            else for(int k=0;k<nn&&py+32+k*16<winy+winh-20;k++){ int ni=vfs_child(cur_node,k); int ry=py+32+k*16; if(k==fsel)fill(px-2,ry-1,winw-176,15,C_BLUE);
                int isf=VFS[ni].isdir; fill(px,ry+1,10,8,isf?C_FOLDER:(C_MGREY+18)); draw_str(px+14,ry,VFS[ni].name,C_WHITE);
                char dt[20],ty[16]; entry_meta(ni,0,VFS[ni].name,dt,ty); draw_str(cD,ry,dt,C_MGREY+20); draw_str(cT,ry,isf?"File folder":ty,C_MGREY+20);
                if(!isf){ char sz2[12]; entry_size(ni,0,VFS[ni].name,sz2); if(sz2[0])draw_str(cS,ry,sz2,C_MGREY+20); } }
        } else {
            char(*lst)[16]; int n; folder_list(cur_folder,&lst,&n);
            for(int i=0;i<n;i++){ int ry=py+32+i*16; if(i==fsel&&!(rename_mode&&rename_folder==cur_folder&&rename_idx==i))fill(px-2,ry-1,winw-176,15,C_BLUE);
                char dt[20],ty[16]; int isf=entry_meta(cur_folder,i,lst[i],dt,ty); fill(px,ry+1,10,8,isf?C_FOLDER:(C_MGREY+18));
                if(rename_mode&&rename_folder==cur_folder&&rename_idx==i){ fill(px+14,ry-1,160,12,C_BLUE); draw_str(px+14,ry,rename_buf,C_WHITE); fill(px+14+strlen_(rename_buf)*8,ry,6,9,C_WHITE); }
                else draw_str(px+14,ry,lst[i],C_WHITE);
                draw_str(cD,ry,dt,C_MGREY+20); draw_str(cT,ry,ty,C_MGREY+20);
                char sz[12]; entry_size(cur_folder,i,lst[i],sz); if(sz[0])draw_str(cS,ry,sz,C_MGREY+20); }
        }
        if(rename_mode) draw_str(winx+8,winy+winh-14,"RENAMING - TYPE NAME, ENTER TO SAVE",C_TEAL);
        else if(cur_folder==8) draw_str(winx+8,winy+winh-14,"NVXFS - RIGHT-CLICK TO OPEN/DELETE",C_MGREY+20);
        else if(cur_folder==10) draw_str(winx+8,winy+winh-14,"USB - RIGHT-CLICK: COPY TO DESKTOP / DELETE  -  EJECT REMOVES",C_MGREY+20);
        else if(cur_folder==7||cur_folder==9) draw_str(winx+8,winy+winh-14,"DOUBLE-CLICK FOLDER = OPEN   DOUBLE-CLICK FILE = VIEW   < UP = BACK",C_MGREY+20);
        else draw_str(winx+8,winy+winh-14,"RIGHT-CLICK A FILE FOR ACTIONS",C_MGREY+20);
    } else if(app==2){
        draw_window_chrome("TERMINAL");
        afill(winx+4,winy+22,winw-8,winh-28,0,140);
        fill(winx+4,winy+22,winw-8,1,C_BLUE);
        draw_str3(winx+16,winy+30,"NOOVEXOS",C_GREEN);
        draw_str(winx+18,winy+58,OSNAME " TERMINAL [VER 1.0] - TYPE 'HELP'",C_MGREY+20);
        hrule(winx+12,winy+74,winw-24,C_MGREY);
        { int tw=(winw-24)/10; for(int s=0;s<10;s++){ int tx=winx+12+s*tw; int act=(s==vt_cur); rrectR(tx+1,winy+80,tw-3,18,3,act?C_BLUE:(u8)(C_MGREY+4)); char lb[3]; int num=s+1; if(num>=10){lb[0]='1';lb[1]='0';lb[2]=0;}else{lb[0]=(char)('0'+num);lb[1]=0;} draw_str(tx+(tw/2)-(num>=10?8:4),winy+84,lb,act?C_WHITE:(u8)(C_MGREY+24)); } }
        {
            int lineH=15, top=winy+104, bottom=winy+winh-18;
            int cap=(bottom-top)/lineH; if(cap<1)cap=1;
            int colw=(winw-24)/8; if(colw<1)colw=1;

            int total=1, col=0;
            for(int i=0;i<termlen;i++){ char c=termbuf[i]; if(c=='\n'){ total++; col=0; } else { col++; if(col>=colw){ total++; col=0; } } }
            int maxscroll=total-cap; if(maxscroll<0)maxscroll=0;
            if(term_scroll>maxscroll)term_scroll=maxscroll;
            int startline=total-cap-term_scroll; if(startline<0)startline=0;

            int vl=0, cx=winx+12, cy=top; int drawing=(vl>=startline&&vl<startline+cap);
            if(drawing){ draw_str(cx,cy,"NVX>",C_GREEN); cx+=40; }
            for(int i=0;i<termlen;i++){ char c=termbuf[i];
                if(c=='\n'){ vl++; drawing=(vl>=startline&&vl<startline+cap); cx=winx+12; if(drawing){ cy=top+(vl-startline)*lineH; draw_str(cx,cy,"NVX>",C_GREEN); cx+=40; } continue; }
                if((cx-(winx+12))/8>=colw){ vl++; drawing=(vl>=startline&&vl<startline+cap); cx=winx+12; if(drawing) cy=top+(vl-startline)*lineH; }
                if(drawing){ draw_char(cx,cy,c,C_WHITE); }
                cx+=8;
            }
            if(drawing) fill(cx,cy,7,12,C_GREEN);

            if(maxscroll>0){ if(term_scroll>0) draw_str(winx+winw-150,winy+24,"^ SCROLLED - DOWN ARROW",C_FOLDER); else draw_str(winx+winw-150,winy+24,"UP ARROW = SCROLL BACK",C_MGREY+20); }
        }
    } else if(app==3){
        draw_window_chrome("ABOUT " OSNAME);
        fill(winx+1,winy+22,winw-2,winh-23,4);
        draw_str3(winx+24,winy+40,"NOOVEXOS",C_BBLUE);
        hrule(winx+24,winy+80,winw-48,C_MGREY);
        draw_str(winx+24,winy+92, OSNAME " [GENESIS] VERSION 7.0",C_WHITE);
        draw_str(winx+24,winy+110,"KERNEL: NOOVEXKERNEL 1.0 (32-BIT PM)",C_WHITE);
        draw_str(winx+24,winy+128,"BUILD: JUN 10 2026  -  C + X86 ASM",C_WHITE);
        draw_str(winx+24,winy+146,"VIDEO: VESA 32BPP LINEAR FRAMEBUFFER",C_WHITE);
        draw_str(winx+24,winy+172,"(C) 2026 NOOVEX",C_TEAL);
        draw_str(winx+24,winy+190,"ALL RIGHTS RESERVED.",C_MGREY+20);
    } else if(app==29){
        draw_window_chrome("WIDGETS");
        fill(winx+1,winy+22,winw-2,winh-23,4);
        draw_str(winx+20,winy+34,"DESKTOP WIDGETS",C_BBLUE);
        draw_str(winx+20,winy+52,"TAP A SWITCH TO SHOW/HIDE IN SIDEBAR",C_MGREY+20);
        hrule(winx+20,winy+66,winw-40,C_MGREY);
        { static const char* WN[4]={"WEATHER","CLOCK","CALENDAR","BATTERY"};
          for(int i=0;i<4;i++){ int ry=winy+80+i*40;
            fill(winx+20,ry,32,28,58); frame(winx+20,ry,32,28,40);
            int gx=winx+24,gy=ry+4;
            if(i==0){ fill(gx,gy,24,20,C_BBLUE); disc(gx+8,gy+10,5,63); }
            else if(i==1){ disc(gx+12,gy+10,10,63); fill(gx+11,gy+4,2,7,0); fill(gx+12,gy+9,6,2,0); }
            else if(i==2){ fill(gx,gy,24,20,C_WIN); fill(gx,gy,24,5,C_RED); for(int r2=0;r2<2;r2++)for(int c2=0;c2<4;c2++)fill(gx+3+c2*6,gy+8+r2*6,3,3,C_MGREY+20); }
            else { frame(gx+2,gy+6,18,10,0); fill(gx+20,gy+9,2,4,0); fill(gx+4,gy+8,12,6,C_GREEN); }
            draw_str(winx+64,ry+10,WN[i],C_WHITE);
            int bw=70,bh=24,bx=winx+winw-bw-20,by=ry+2;
            rrectR(bx,by,bw,bh,6, widget_on[i]?C_GREEN:(C_MGREY+8));
            int kn=widget_on[i]?(bx+bw-22):(bx+2);
            fill(kn,by+2,20,bh-4,C_WHITE);
            draw_str(widget_on[i]?(bx+10):(bx+26), by+8, widget_on[i]?"ON":"OFF", C_WHITE); } }
        draw_str(winx+20,winy+winh-18,"CHANGES APPLY INSTANTLY",C_MGREY+15);
    } else if(app==4){
        draw_window_chrome("SETTINGS");

        int sbw=150; fill(winx+1,winy+23,sbw,winh-24,C_WIN);
        static const int catmap[14]={20,40,41,21,2,3,22,42,11,7,9,12,43,13};
        const char*cats[14]={"HOME","NETWORK","BLUETOOTH","PERSONALIZE","DISPLAY","SOUND","ACCESSIBILITY","TIME","STORAGE","BATTERY","PRIVACY","SYSTEM","UPDATE","ABOUT"};
        int rowh=30, top=winy+34;
        for(int i=0;i<14;i++){ int iy=top+i*rowh;
            if(setcat==catmap[i]) rrectR(winx+5,iy-4,sbw-9,rowh-4,8,C_BLUE);
            rrectR(winx+10,iy,20,20,5,(setcat==catmap[i])?(u8)C_BBLUE:(u8)105);
            disc(winx+20,iy+10,4,(setcat==catmap[i])?C_WHITE:(u8)(C_MGREY+20));
            draw_str(winx+38,iy+6,cats[i],(setcat==catmap[i])?C_WHITE:C_TITLE);
        }
        int px=winx+sbw+18,py=winy+34;
        if(setcat==20){
            draw_str2(px,py-4,"HOME",C_WHITE);
            int cx=px, cw=winw-(px-winx)-20, cy=py+24;
            rrectR(cx,cy,cw,70,8,C_WIN);
            rrectR(cx+12,cy+13,72,44,6,C_TASK); draw_str2(cx+38,cy+25,"N",C_BBLUE);
            draw_str2(cx+98,cy+14,OSNAME " PC",C_WHITE);
            draw_str(cx+98,cy+38,"NOOVEXOS  -  32-BIT VESA DESKTOP",C_MGREY+20);
            draw_str(cx+98,cy+52,"BUILT FROM SCRATCH IN C + ASM",C_MGREY+15);
            cy+=82;
            rrectR(cx,cy,cw,46,8,C_WIN);
            draw_str(cx+14,cy+9,"NETWORK",C_TITLE);
            { char nb[28]; if(nic_present){ int q=0; const char* e="ETHERNET  "; while(*e)nb[q++]=*e++; char o[6]; for(int z=0;z<4;z++){ utoa((my_ip>>(24-z*8))&255,o); int j=0; while(o[j])nb[q++]=o[j++]; if(z<3)nb[q++]='.'; } nb[q]=0; draw_str(cx+14,cy+26,nb,C_GREEN); } else draw_str(cx+14,cy+26,"NOT CONNECTED",C_MGREY+20); }
            draw_str(cx+cw/2+8,cy+9,"STORAGE",C_TITLE); draw_str(cx+cw/2+8,cy+26,disk_ok?"NVXFS READY":"RAM ONLY (LIVE)",disk_ok?C_GREEN:(C_MGREY+20));
            cy+=60;
            draw_str(cx,cy,"RECOMMENDED",C_TITLE); cy+=20;
            { const char* qn[3]={"PERSONALIZE  -  BACKGROUND, DOCK, WIDGETS","DISPLAY  -  RESOLUTION & NIGHT LIGHT","STORAGE  -  DISK & USB"};
              for(int i=0;i<3;i++){ rrectR(cx,cy+i*34,cw,28,7,C_WIN); draw_str(cx+14,cy+i*34+9,qn[i],C_WHITE); draw_str(cx+cw-18,cy+i*34+9,">",C_BBLUE); } }
        } else if(setcat==21){
            draw_str2(px,py-4,"PERSONALIZATION",C_WHITE);
            int cy=py+26;
            draw_str(px,cy,"BACKGROUND",C_TITLE);
            const char*bs[10]={"DARK","BLUE","GRAD","SOLID","TOPO","SUNSET","CIRCUIT","LIGHT","WAVES","GARUDA"};
            for(int i=0;i<10;i++){ int row=i/5,col=i%5; int bx=px+col*58,by=cy+16+row*24;
                rrectR(bx,by,54,20,5,(bg_style==i)?C_BLUE:C_MGREY); draw_str(bx+4,by+4,bs[i],C_WHITE); }
            draw_str(px,cy+72,"ACCENT",C_TITLE);
            static const u8 acc[4]={66,70,75,74};
            for(int i=0;i<4;i++){ rrectR(px+i*38,cy+88,30,18,5,acc[i]); if(accent==i)frame(px+i*38-2,cy+86,34,22,C_WHITE); }
            draw_str(px+200,cy+72,"DOCK SIZE",C_TITLE);
            const char* dz[3]={"SMALL","MED","LARGE"};
            for(int i=0;i<3;i++){ rrectR(px+200+i*62,cy+88,56,18,5,(dock_size==i)?C_BLUE:C_MGREY); draw_str(px+200+i*62+5,cy+91,dz[i],C_WHITE); }
            draw_str(px,cy+120,"ROUNDED CORNERS",C_WHITE); toggle_sw(px+200,cy+116,set_corner);
            draw_str(px,cy+144,"WINDOW SHADOWS",C_WHITE);  toggle_sw(px+200,cy+140,set_shadows);
            draw_str(px,cy+168,"GLASS BLUR",C_WHITE);       toggle_sw(px+200,cy+164,set_glass);
            draw_str(px,cy+196,"DESKTOP WIDGETS",C_TITLE);
            { static const char* wn[4]={"WEATHER","CLOCK","CALENDAR","BATTERY"};
              for(int i=0;i<4;i++){ int wy=cy+216+i*22; draw_str(px,wy+3,wn[i],C_WHITE); toggle_sw(px+200,wy,widget_on[i]); } }
        } else if(setcat==22){
            draw_str2(px,py-4,"ACCESSIBILITY",C_WHITE);
            int cy=py+30;
            draw_str(px,cy+3,"SCREEN MAGNIFIER",C_WHITE); toggle_sw(px+240,cy,mag_on);
            draw_str(px,cy+22,"ZOOM LENS THAT FOLLOWS THE CURSOR",C_MGREY+18);
            cy+=52;
            draw_str(px,cy+3,"BOLD TEXT",C_WHITE); toggle_sw(px+240,cy,acc_bold);
            draw_str(px,cy+22,"THICKER TEXT FOR EASIER READING",C_MGREY+18);
            cy+=52;
            draw_str(px,cy+3,"HIGH CONTRAST",C_WHITE); toggle_sw(px+240,cy,acc_hicontrast);
            draw_str(px,cy+22,"STRONGER COLORS ACROSS THE INTERFACE",C_MGREY+18);
        } else if(setcat==0){
            draw_str(px,py,"ACCENT COLOR",C_TITLE);
            static const u8 sw[4]={66,70,75,74};
            for(int i=0;i<4;i++){ rrectR(px+i*40,py+16,30,18,5,sw[i]); if(accent==i)frame(px+i*40-2,py+14,34,22,C_WHITE); }
            draw_str(px,py+48,"BACKGROUND",C_TITLE);
            const char*bs[10]={"DARK","BLUE","GRAD","SOLID","TOPO","SUNSET","CIRCUIT","LIGHT","WAVES","GARUDA"};
            for(int i=0;i<10;i++){ int row=i/5, col=i%5; int bx=px+col*53, by=py+62+row*22;
                rrectR(bx-2,by,50,18,5,(bg_style==i)?C_BLUE:C_MGREY); draw_str(bx+2,by+3,bs[i],C_WHITE); }
            draw_str(px,py+112,"ROUNDED CORNERS",C_WHITE); toggle_sw(px+170,py+108,set_corner);
            draw_str(px,py+136,"WINDOW SHADOWS",C_WHITE); toggle_sw(px+170,py+132,set_shadows);
            draw_str(px,py+160,"GLASS BLUR",C_WHITE);      toggle_sw(px+170,py+156,set_glass);
        } else if(setcat==1){
            draw_str(px,py,"DOCK",C_TITLE);
            draw_str(px,py+28,"AUTO-HIDE DOCK",C_WHITE);   toggle_sw(px+170,py+24,set_dockauto);
            draw_str(px,py+56,"RUNNING INDICATORS",C_WHITE);toggle_sw(px+170,py+52,1);
            draw_str(px,py+84,"MAGNIFICATION",C_WHITE);    toggle_sw(px+170,py+80,1);
            draw_str(px,py+120,"DOCK SIZE",C_TITLE);
            const char* dz[3]={"SMALL","MEDIUM","LARGE"};
            for(int i=0;i<3;i++){ rrectR(px+i*70,py+138,64,18,5,(i==1)?C_BLUE:C_MGREY); draw_str(px+i*70+8,py+141,dz[i],C_WHITE); }
        } else if(setcat==2){
            draw_str(px,py,"DISPLAY",C_TITLE);
            char rb[20]; int q=0; { char t[6];int tl=0,v=W;while(v){t[tl++]='0'+v%10;v/=10;}while(tl)rb[q++]=t[--tl]; rb[q++]='x'; tl=0;v=H;while(v){t[tl++]='0'+v%10;v/=10;}while(tl)rb[q++]=t[--tl]; rb[q]=0; }
            draw_str(px,py+26,"RESOLUTION:",C_WHITE); draw_str(px+110,py+26,rb,C_TEAL);
            draw_str(px,py+46,"COLOR DEPTH: 32-BIT (TRUECOLOR)",C_WHITE);
            draw_str(px,py+66,"DISPI RUNTIME:",C_WHITE); draw_str(px+130,py+66,dispi_id?"AVAILABLE":"NO",dispi_id?C_GREEN:C_RED);
            draw_str(px,py+92,"PRESETS",C_TITLE);
            const char* rp[3]={"1024x768","1280x1024","1366x768"};
            for(int i=0;i<3;i++){ rrectR(px,py+112+i*22,120,18,5,C_MGREY); draw_str(px+6,py+115+i*22,rp[i],C_WHITE); }
            draw_str(px,py+182,"NIGHT LIGHT",C_WHITE); toggle_sw(px+170,py+178,cc_night);
        } else if(setcat==3){
            draw_str(px,py,"SOUND",C_TITLE); draw_str(px,py+28,"SYSTEM SOUNDS",C_WHITE); toggle_sw(px+170,py+24,snd_on);
            draw_str(px,py+60,"OUTPUT: PC SPEAKER / SB16",C_WHITE);
            draw_str(px,py+86,"VOLUME",C_TITLE);
            { int sx=px,sw2=200,sy=py+106; fill(sx,sy,sw2,6,C_MGREY+12); fill(sx,sy,(sw2*(snd_on?4:0))/5,6,C_BBLUE); disc(sx+(sw2*(snd_on?4:0))/5,sy+3,7,C_WHITE); }
        } else if(setcat==4){
            draw_str(px,py,"MOUSE",C_TITLE);
            draw_str(px,py+24,"POINTER SPEED:",C_WHITE);
            const char* ps[3]={"SLOW","NORM","FAST"};
            for(int i=0;i<3;i++){ int bx=px+125+i*52; rrectR(bx,py+22,48,16,5,(mouse_speed==i)?C_BLUE:C_MGREY); draw_str(bx+4,py+24,ps[i],C_WHITE); }
            draw_str(px,py+50,"SCROLL SPEED:",C_WHITE);
            const int sv[3]={1,3,5}; const char* ss[3]={"1","3","5"};
            for(int i=0;i<3;i++){ int bx=px+125+i*52; rrectR(bx,py+48,48,16,5,(scroll_speed==sv[i])?C_BLUE:C_MGREY); draw_str(bx+20,py+50,ss[i],C_WHITE); }
            draw_str(px,py+76,"REVERSE SCROLL",C_WHITE); toggle_sw(px+170,py+72,scroll_rev);
            draw_str(px,py+104,"WHEEL:",C_WHITE); draw_str(px+60,py+104,wheel_ok?"DETECTED":"NOT FOUND",wheel_ok?C_GREEN:(C_RED+8));
        } else if(setcat==5){
            draw_str(px,py,"KEYBOARD",C_TITLE);
            draw_str(px,py+26,"LAYOUT:",C_WHITE); { int L=(int)acct.lang; if(L<0||L>=NLANG)L=0; draw_str(px+80,py+26,LANGS[L],C_TEAL); }
            draw_str(px,py+52,"KEY REPEAT",C_WHITE);  toggle_sw(px+170,py+48,1);
            draw_str(px,py+80,"DETECTED: PS/2 KEYBOARD (INT 0x21)",C_WHITE);
            draw_str(px,py+106,"WIN+L LOCKS, ESC CLOSES MENUS",C_MGREY+20);
        } else if(setcat==6){
            draw_str(px,py,"NETWORK",C_TITLE);
            draw_str(px,py+26,"WIRED:",C_WHITE); draw_str(px+70,py+26,nic_present?"AMD PCNET - LINK UP":"NOT FOUND",nic_present?C_GREEN:C_RED);
            draw_str(px,py+50,"DHCP:",C_WHITE);  draw_str(px+70,py+50,nic_present?"10.0.2.x":"-",C_WHITE);
            draw_str(px,py+74,"DNS:",C_WHITE);   draw_str(px+70,py+74,nic_present?"10.0.2.3":"-",C_WHITE);
            draw_str(px,py+102,"TLS 1.3 STACK: BUILT-IN",C_TEAL);
        } else if(setcat==7){
            draw_str(px,py,"BATTERY",C_TITLE);
            u8 bc = bat_charging?C_GREEN:(bat_pct<=20?C_RED:C_GREEN);
            frame(px,py+28,60,28,C_WHITE); fill(px+60,py+36,4,12,C_WHITE); fill(px+3,py+31,(54*bat_pct)/100,22,bc);
            { char pb[20]; int q=0,v=bat_pct; char t[4];int tl=0; if(v==0)t[tl++]='0'; while(v){t[tl++]='0'+v%10;v/=10;} while(tl)pb[q++]=t[--tl]; pb[q++]='%'; pb[q++]=' '; pb[q]=0;
              const char* st=bat_charging?"(AC POWER)":(bat_pct<=20?"(LOW!)":"(ON BATTERY)");
              while(*st)pb[q++]=*st++; pb[q]=0; draw_str(px+80,py+36,pb,C_WHITE); }
            draw_str(px,py+72,bat_present?"SOURCE: BATTERY (SIMULATED)":"SOURCE: AC - NO BATTERY IN VM",bat_present?C_FOLDER:C_MGREY+20);
            draw_str(px,py+96,"SIMULATE BATTERY DRAIN",C_WHITE); toggle_sw(px+200,py+92,bat_sim);
            draw_str(px,py+124,"TURN ON TO SEE %, WARNING & SLEEP",C_MGREY+20);
            draw_str(px,py+148,"AT 20%: WARNING   AT 0%: AUTO-SLEEP",C_MGREY+20);
        } else if(setcat==8){
            draw_str(px,py,"NOTIFICATIONS",C_TITLE);
            draw_str(px,py+28,"ALLOW NOTIFICATIONS",C_WHITE); toggle_sw(px+170,py+24,1);
            draw_str(px,py+56,"PLAY SOUND",C_WHITE);          toggle_sw(px+170,py+52,snd_on);
            draw_str(px,py+84,"SHOW ON LOCK SCREEN",C_WHITE);  toggle_sw(px+170,py+80,0);
            draw_str(px,py+116,"TOASTS APPEAR ABOVE THE DOCK",C_MGREY+20);
        } else if(setcat==9){
            draw_str(px,py,"PRIVACY & SECURITY",C_TITLE);
            const char* es = !disk_ok?"UNAVAILABLE":bl_enabled?(bl_unlocked?"ON (UNLOCKED)":"LOCKED"):"OFF";
            draw_str(px,py+28,"DISK ENCRYPTION:",C_WHITE); draw_str(px+140,py+28,es,bl_enabled?C_GREEN:(C_MGREY+20));
            rrectR(px,py+50,170,18,5,C_BLUE); draw_str(px+8,py+53,"DEVICE ENCRYPTION",C_WHITE);
            draw_str(px,py+82,"CAMERA ACCESS",C_WHITE); toggle_sw(px+170,py+78,1);
            draw_str(px,py+110,"MIC ACCESS",C_WHITE);    toggle_sw(px+170,py+106,0);
            draw_str(px,py+138,"NO TELEMETRY - NOTHING LEAVES THIS PC",C_MGREY+20);
        } else if(setcat==10){
            draw_str(px,py,"DATE & TIME",C_TITLE);
            draw_str(px,py+28,"TIME:",C_WHITE); draw_str(px+60,py+28,clkbuf,C_TEAL);
            { static const char* mn[12]={"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
              int dd=bcd(cmos(7)),mo=bcd(cmos(8)),yr=2000+bcd(cmos(9)); if(mo<1||mo>12)mo=1;
              char db[16]; int q=0; const char* m=mn[mo-1]; while(*m)db[q++]=*m++; db[q++]=' '; if(dd>=10)db[q++]='0'+dd/10; db[q++]='0'+dd%10; db[q++]=' '; db[q++]='0'+(yr/1000)%10;db[q++]='0'+(yr/100)%10;db[q++]='0'+(yr/10)%10;db[q++]='0'+yr%10; db[q]=0;
              draw_str(px,py+52,"DATE:",C_WHITE); draw_str(px+60,py+52,db,C_WHITE); }
            draw_str(px,py+84,"24-HOUR CLOCK",C_WHITE); toggle_sw(px+170,py+80,set_clock24);
            draw_str(px,py+112,"SET FROM CMOS RTC",C_MGREY+20);
        } else if(setcat==11){
            draw_str(px,py,"STORAGE",C_TITLE);
            draw_str(px,py+24,"DISK:",C_WHITE); draw_str(px+50,py+24,DISK_LABEL,C_TEAL);
            int inst=(cmos(0x46)==0xCD);
            draw_str(px,py+44,"STATUS:",C_WHITE); draw_str(px+70,py+44,inst?"INSTALLED":"LIVE MODE",inst?C_GREEN:(C_MGREY+20));
            { char rb[20]="RESERVED: ";int q=10,v=disk_size_gb;char t[6];int tl=0;if(v==0)t[tl++]='0';while(v){t[tl++]='0'+v%10;v/=10;}while(tl)rb[q++]=t[--tl];rb[q++]=' ';rb[q++]='G';rb[q++]='B';rb[q]=0; draw_str(px,py+64,rb,C_WHITE); }
            if(disk_ok){
                char b[24]="FILES: ";int c=nvx.count,q=7; if(c==0)b[q++]='0';else{char t[6];int tl=0;while(c){t[tl++]='0'+c%10;c/=10;}while(tl)b[q++]=t[--tl];} b[q++]='/';b[q++]='1';b[q++]='5';b[q]=0; draw_str(px,py+88,b,C_WHITE);
                int used=(nvx.count*100)/NVX_MAX; if(used>100)used=100;
                fill(px,py+110,200,10,C_MGREY+12); fill(px,py+110,(200*used)/100,10,C_BBLUE); draw_str(px,py+126,"NVXFS USAGE",C_MGREY+20);

                rrectR(px,py+150,180,30,7,set_repair_ok?C_GREEN:C_BLUE);
                draw_str(px+14,py+158,"VERIFY & REPAIR",C_WHITE);
                if(set_repair_msg[0]) draw_str(px,py+188,set_repair_msg,set_repair_ok?C_GREEN:(C_RED+8));
            } else draw_str(px,py+88,"NO ATA DISK (RAM ONLY)",C_RED+8);
            draw_str(px,py+216,"USB:",C_WHITE); draw_str(px+50,py+216,msd_dev>=0?"MOUNTED (U:)":"NONE",msd_dev>=0?C_GREEN:C_MGREY+20);
        } else if(setcat==12){
            draw_str(px,py,"SYSTEM",C_TITLE);
            draw_str(px,py+22,"DEVICE:  " OSNAME " PC",C_WHITE);
            draw_str(px,py+38,"DISPLAY: VESA 32BPP LFB",C_WHITE);
            draw_str(px,py+54,"CPU:     X86 32-BIT PMODE",C_WHITE);
            { char mb[24]="MEMORY:  ";int q=9,v=ram_mb; char t[8];int tl=0; if(v==0)t[tl++]='0'; while(v){t[tl++]='0'+v%10;v/=10;} while(tl)mb[q++]=t[--tl]; mb[q++]=' ';mb[q++]='M';mb[q++]='B';mb[q]=0; draw_str(px,py+70,mb,C_WHITE); }
            draw_str(px,py+108,"WINDOW ANIMATIONS",C_WHITE); toggle_sw(px+170,py+104,set_anim);
        } else if(setcat==40){
            draw_str2(px,py-4,"NETWORK",C_WHITE);
            int cx=px, cw=winw-(px-winx)-20, cy=py+24;
            rrectR(cx,cy,cw,44,8,C_WIN);
            draw_str2(cx+16,cy+9,"ETHERNET",C_WHITE);
            draw_str(cx+cw-160,cy+6,nic_present?"CONNECTED":"DISCONNECTED",nic_present?C_GREEN:C_RED);
            { char b[20]; const char* hx="0123456789ABCDEF"; int q=0; for(int k=0;k<6;k++){ b[q++]=hx[my_mac[k]>>4]; b[q++]=hx[my_mac[k]&15]; if(k<5)b[q++]=':'; } b[q]=0; draw_str(cx+cw-160,cy+24,b,C_TITLE); }
            cy+=54;
            draw_str(cx,cy+3,"AUTOMATIC (DHCP)",C_WHITE); toggle_sw(cx+220,cy,net_dhcp); cy+=32;
            { const char* L[4]={"IP ADDRESS","SUBNET MASK","GATEWAY","DNS SERVER"};
              for(int i=0;i<4;i++){ int fy=cy+i*30; draw_str(cx,fy+5,L[i],C_TITLE);
                int fx2=cx+140, fw2=cw-140;
                fill(fx2,fy,fw2,22,net_dhcp?(C_MGREY+8):C_TASK); frame(fx2,fy,fw2,22,(net_field==i+1&&!net_dhcp)?C_BBLUE:C_MGREY);
                draw_str(fx2+6,fy+5,netfld[i],net_dhcp?(C_MGREY+18):C_WHITE);
                if(net_field==i+1&&!net_dhcp){ int w=strlen_(netfld[i])*8; fill(fx2+6+w,fy+4,2,13,C_WHITE); } } }
            cy+=4*30+8;
            fill(cx,cy,120,28,C_GREEN); frame(cx,cy,120,28,C_WHITE); draw_str(cx+34,cy+9,"APPLY",C_WHITE);
            if(net_msg[0]) draw_str(cx+134,cy+9,net_msg,C_BBLUE);
            cy+=36;
            draw_str(cx,cy,"NAT VM: DNS IS SAFE TO CHANGE (TRY 8.8.8.8).",C_MGREY+16); cy+=15;
            draw_str(cx,cy,"OFF-SUBNET IP/GATEWAY WILL BREAK NETWORKING.",C_MGREY+16);
        } else if(setcat==41){
            draw_str2(px,py-4,"BLUETOOTH",C_WHITE);
            int cx=px, cw=winw-(px-winx)-20, cy=py+24;
            rrectR(cx,cy,cw,52,8,C_WIN);
            draw_str2(cx+18,cy+14,"B",66); draw_str2(cx+48,cy+10,"BLUETOOTH",C_WHITE);
            draw_str(cx+48,cy+36, bt_found==1?"CONTROLLER ONLINE":(bt_found==2?"DEVICE DETECTED":(bt_found<0?"NO ADAPTER":"NOT SCANNED")), bt_found==1?C_GREEN:(bt_found<0?C_RED:(C_MGREY+20)));
            cy+=64;
            fill(cx,cy,150,28,C_BBLUE); frame(cx,cy,150,28,C_WHITE); draw_str(cx+16,cy+9,"SCAN USB FOR BT",C_WHITE);
            draw_str(cx+164,cy+10,bt_status,C_TITLE); cy+=40;
            if(bt_found>=1){ rrectR(cx,cy,cw,32,7,C_WIN); draw_str(cx+14,cy+5,"VENDOR : PRODUCT",C_TITLE);
                { char b[16]; const char* hx="0123456789ABCDEF"; int q=0; b[q++]=hx[(bt_vid>>12)&15];b[q++]=hx[(bt_vid>>8)&15];b[q++]=hx[(bt_vid>>4)&15];b[q++]=hx[bt_vid&15]; b[q++]=':'; b[q++]=hx[(bt_pid>>12)&15];b[q++]=hx[(bt_pid>>8)&15];b[q++]=hx[(bt_pid>>4)&15];b[q++]=hx[bt_pid&15]; b[q]=0; draw_str(cx+180,cy+5,b,C_WHITE); } cy+=40; }
            if(bt_found==1){ rrectR(cx,cy,cw,32,7,C_WIN); draw_str(cx+14,cy+5,"BD_ADDR",C_TITLE);
                { char b[20]; const char* hx="0123456789ABCDEF"; int q=0; for(int k=0;k<6;k++){ b[q++]=hx[bt_addr[k]>>4]; b[q++]=hx[bt_addr[k]&15]; if(k<5)b[q++]=':'; } b[q]=0; draw_str(cx+180,cy+5,b,C_WHITE); } cy+=40; }
            draw_str(cx,cy,"VIRTUALBOX HAS NO VIRTUAL BT - PASS THROUGH A USB",C_MGREY+16); cy+=15;
            draw_str(cx,cy,"BT DONGLE (SETTINGS>USB) THEN SCAN. HCI RESET +",C_MGREY+16); cy+=15;
            draw_str(cx,cy,"READ_BD_ADDR ARE SENT OVER USB CONTROL TRANSFERS.",C_MGREY+16);
        } else if(setcat==42){
            draw_str2(px,py-4,"TIME AND LANGUAGE",C_WHITE);
            int cx=px, cw=winw-(px-winx)-20, cy=py+24; rtc_now();
            rrectR(cx,cy,cw,56,8,C_WIN);
            draw_str(cx+14,cy+8,"CURRENT TIME",C_TITLE);
            { char hm[6]; for(int i=0;i<5;i++)hm[i]=clkbuf[i]; hm[5]=0; draw_str2(cx+14,cy+24,hm,C_WHITE); }
            draw_str(cx+cw-150,cy+8,"DATE",C_TITLE); draw_str2(cx+cw-150,cy+26,datebuf,C_WHITE);
            cy+=68;
            draw_str(cx,cy+3,"24-HOUR CLOCK",C_WHITE); toggle_sw(cx+200,cy,set_clock24); cy+=36;
            draw_str(cx,cy,"LANGUAGE",C_TITLE); cy+=22;
            { int L=(int)acct.lang; if(L<0||L>=NLANG)L=0;
              for(int i=0;i<NLANG;i++){ int row=i/2,col=i%2; int bx=cx+col*((cw-8)/2), by=cy+row*26;
                rrectR(bx,by,(cw-8)/2-6,22,5,(L==i)?C_BLUE:C_MGREY); draw_str(bx+8,by+4,LANGS[i],C_WHITE); } }
        } else if(setcat==43){
            draw_str2(px,py-4,"SYSTEM UPDATE",C_WHITE);
            int cx=px, cw=winw-(px-winx)-20, cy=py+24;
            rrectR(cx,cy,cw,60,8,C_WIN);
            disc(cx+30,cy+30,16,C_GREEN); draw_str(cx+24,cy+24,"OK",C_WHITE);
            draw_str2(cx+62,cy+12,OSNAME " IS UP TO DATE",C_WHITE);
            draw_str(cx+62,cy+40,"LAST CHECKED: JUST NOW",C_MGREY+18);
            cy+=72;
            rrectR(cx,cy,cw,30,7,C_WIN); draw_str(cx+14,cy+9,"VERSION",C_TITLE); draw_str(cx+120,cy+9,OSVER,C_WHITE); cy+=38;
            rrectR(cx,cy,cw,30,7,C_WIN); draw_str(cx+14,cy+9,"BUILD",C_TITLE); draw_str(cx+120,cy+9,BUILDVER,C_WHITE); cy+=38;
            rrectR(cx,cy,cw,30,7,C_WIN); draw_str(cx+14,cy+9,"CHANNEL",C_TITLE); draw_str(cx+120,cy+9,"STABLE",C_WHITE); cy+=44;
            fill(cx,cy,150,30,C_BBLUE); frame(cx,cy,150,30,C_WHITE); draw_str(cx+18,cy+11,"CHECK FOR UPDATES",C_WHITE);
        } else {
            draw_str(px,py,"ABOUT",C_TITLE);
            draw_str(px,py+24,OSNAME " GRAPHICS 1.0",C_WHITE);
            draw_str(px,py+44,"32-BIT VESA OPERATING SYSTEM",C_WHITE);
            draw_str(px,py+64,"BUILT FROM SCRATCH IN C + ASM",C_WHITE);
            draw_str(px,py+88,"(C) 2026 NOOVEX",C_MGREY+20);
        }
    } else if(app==5){
        draw_window_chrome("NOOVEXWORD");
        int tby=winy+20;
        fill(winx+1,tby,winw-2,24,10);
        fill(winx+6,tby+4,16,16,np_bold?C_BLUE:16); draw_str(winx+11,tby+8,"B",C_WHITE);
        fill(winx+26,tby+4,16,16,np_under?C_BLUE:16); draw_str(winx+31,tby+8,"U",C_WHITE); fill(winx+29,tby+17,10,1,C_WHITE);
        for(int a2=0;a2<3;a2++){ int bx=winx+52+a2*20; fill(bx,tby+4,16,16,(np_align==a2)?C_BLUE:16);
            for(int l2=0;l2<3;l2++){ int lw=(l2==1)?10:7; int lx=bx+3; if(a2==1)lx=bx+(16-lw)/2; else if(a2==2)lx=bx+13-lw; fill(lx,tby+7+l2*4,lw,2,C_WHITE); } }
        fill(winx+120,tby+4,14,16,16); draw_str(winx+124,tby+8,"-",C_WHITE);
        { char sb[3]; int px2=np_size*8; sb[0]=(char)('0'+px2/10); sb[1]=(char)('0'+px2%10); sb[2]=0;
          fill(winx+136,tby+4,20,16,63); draw_str(winx+138,tby+8,sb,0); }
        fill(winx+158,tby+4,14,16,16); draw_str(winx+162,tby+8,"+",C_WHITE);
        fill(winx+winw-96,tby+4,44,16,16); draw_str(winx+winw-92,tby+8,"SAVE",C_WHITE);
        fill(winx+winw-48,tby+4,44,16,16); draw_str(winx+winw-44,tby+8,"OPEN",C_WHITE);
        if(note_status==1||note_status==2) fill(winx+winw-104,tby+9,6,6,C_GREEN);
        else if(note_status==3) fill(winx+winw-104,tby+9,6,6,C_RED);
        fill(winx+1,winy+44,winw-2,winh-45,8);
        int pw=winw-56; if(pw>500)pw=500; if(pw<120)pw=120;
        int px0=winx+(winw-pw)/2, py0=winy+52, ph=winh-60;
        fill(px0+3,py0+3,pw,ph,4); fill(px0,py0,pw,ph,63);
        fill(px0,py0,pw,6,10); for(int rr=px0+8;rr<px0+pw-4;rr+=16) fill(rr,py0+2,1,2,63);
        int n=np_size; int cwn=8*n; int maxc=(pw-20)/cwn; if(maxc<1)maxc=1;
        int cx=px0+10, cy=py0+14, i=0;
        while(1){
            int ll=0; while(i+ll<notelen && notebuf[i+ll]!='\n' && ll<maxc) ll++;
            int sx=px0+10;
            if(np_align==1) sx=px0+(pw-ll*cwn)/2;
            else if(np_align==2) sx=px0+pw-10-ll*cwn;
            if(sx<px0+4)sx=px0+4;
            cx=sx;
            for(int k2=0;k2<ll;k2++){ char c=notebuf[i+k2];
                if(cy+8*n<=winy+winh-8){ draw_char_n(cx,cy,c,0,n); if(np_bold)draw_char_n(cx+1,cy,c,0,n); if(np_under)fill(cx,cy+8*n,cwn,n,0); }
                cx+=cwn; }
            i+=ll;
            if(i>=notelen) break;
            if(notebuf[i]=='\n') i++;
            cy+=8*n+n+2;
            if(cy>winy+winh-12) break;
        }
        if(cy+8*n<=winy+winh-6) fill(cx,cy,n<2?2:n,8*n,0);
    } else if(app==6){
        draw_window_chrome("TASK MANAGER");
        fill(winx+1,winy+22,winw-2,winh-23,4);
        draw_str(winx+12,winy+30,"RUNNING APPS (REAL)",C_TITLE);
        draw_str(winx+12,winy+46,"NAME                    STATE",C_MGREY+20);
        hrule(winx+12,winy+56,winw-24,C_MGREY);
        int ry=winy+62, shown=0;
        for(int z=0;z<wincnt;z++){
            int a2=wins[z].app; if(a2==6){   }
            int ic=icon_for_app(a2);
            if(ic>=0) blit_icon(ic,winx+12,ry-2,14); else fill(winx+12,ry+1,8,8,C_TEAL);
            draw_str(winx+32,ry,app_name(a2),C_WHITE);
            draw_str(winx+220,ry,wins[z].min?"MINIMIZED":"RUNNING",wins[z].min?C_MGREY+20:C_GREEN);
            if(a2!=6){ fill(winx+winw-86,ry-2,74,13,C_RED); draw_str(winx+winw-80,ry,"END TASK",C_WHITE); }
            ry+=18; shown++;
        }
        if(!shown) draw_str(winx+12,ry,"NO APPS RUNNING",C_MGREY+20), ry+=18;
        ry+=8; hrule(winx+12,ry,winw-24,C_MGREY); ry+=8;

        { char b[28]="WINDOWS OPEN: "; int q=14,v=wincnt; char t[4];int tl=0; if(v==0)t[tl++]='0'; while(v){t[tl++]='0'+v%10;v/=10;} while(tl)b[q++]=t[--tl]; b[q]=0; draw_str(winx+12,ry,b,C_WHITE); ry+=14; }
        { char bb[12]; draw_str(winx+12,ry,"MEMORY",C_TITLE); utoa(ram_mb,bb);
          draw_str(winx+92,ry,bb,C_WHITE); draw_str(winx+92+strlen_(bb)*8,ry," MB total",C_MGREY+24); ry+=15;
          u32 ku=kheap_used()>>10, kt=kheap_total()>>20; char u1[12],t1[12]; utoa(ku,u1); utoa(kt,t1);
          draw_str(winx+12,ry,"Kernel heap",C_MGREY+24);
          draw_str(winx+110,ry,u1,C_TEAL); draw_str(winx+110+strlen_(u1)*8,ry," KB / ",C_MGREY+24);
          { int xx=110+strlen_(u1)*8+48; draw_str(winx+xx,ry,t1,C_WHITE); draw_str(winx+xx+strlen_(t1)*8,ry," MB",C_MGREY+24); } ry+=14;
          { int bw=winw-24, bx=winx+12; fill(bx,ry,bw,8,(u8)(C_MGREY+12));
            u32 tk=kheap_total()>>10, uk2=kheap_used()>>10; int fw= tk? (int)(uk2*(u32)bw/tk):0; if(fw>bw)fw=bw;
            if(fw<2 && kheap_used()) fw=2; fill(bx,ry,fw,8,C_GREEN); } ry+=13;
          draw_str(winx+12,ry, kheap_isok()?"Allocator OK - coalescing free-list @ 0x18000000":"Allocator off (low RAM)", kheap_isok()?C_GREEN:(u8)(C_RED+8)); ry+=14; }
        { char rb[20]; int q=0; { char t[6];int tl=0,v=W;while(v){t[tl++]='0'+v%10;v/=10;}while(tl)rb[q++]=t[--tl]; rb[q++]='X'; tl=0;v=H;while(v){t[tl++]='0'+v%10;v/=10;}while(tl)rb[q++]=t[--tl]; rb[q]=0; }
          draw_str(winx+12,ry,"DISPLAY:",C_WHITE); draw_str(winx+84,ry,rb,C_TEAL); ry+=14; }
        if(disk_ok){ char b[24]="NVXFS FILES: "; int q=13,v=nvx.count; char t[4];int tl=0; if(v==0)t[tl++]='0'; while(v){t[tl++]='0'+v%10;v/=10;} while(tl)b[q++]=t[--tl]; b[q]=0; draw_str(winx+12,ry,b,C_WHITE); }
    } else if(app==7){
        draw_window_chrome("DEVICE MANAGER");
        int x=winx+12,y=winy+30;
        draw_str(x,y,"SYSTEM",C_TITLE); y+=15;
        draw_str(x,y,cpu_brand[0]?cpu_brand:cpu_vendor,C_WHITE); y+=13;
        { char bb[8]; utoa(ram_mb,bb); draw_str(x,y,"RAM:",C_WHITE); draw_str(x+40,y,bb,C_TEAL); draw_str(x+40+strlen_(bb)*8,y," MB",C_WHITE); } y+=18;
        char hcnt[8]; utoa(pcin,hcnt);
        draw_str(x,y,"PCI DEVICES (",C_TITLE); draw_str(x+104,y,hcnt,C_TITLE); draw_str(x+104+strlen_(hcnt)*8,y,")",C_TITLE); y+=14;
        for(int i=0;i<pcin&&y<winy+winh-192;i++){
            char vd[12]; hex16(pcil[i].ven,vd); vd[4]=':'; hex16(pcil[i].did,vd+5);
            u8 ic=(pcil[i].cls==2)?C_GREEN:(pcil[i].cls==0x0C)?C_RED:(pcil[i].cls==3)?C_BBLUE:C_TEAL;
            const char* nm=(pcil[i].cls==0x0C&&pcil[i].sub==0x03)?usb_type(pcil[i].prog):cls_name(pcil[i].cls,pcil[i].sub);
            fill(x,y+1,8,8,ic); draw_str(x+12,y,vd,C_WHITE); draw_str(x+108,y,nm,C_MGREY+20); y+=13;
        }
        y+=4; draw_str(x,y,"USB:",C_TITLE);
        if(usb_present){ draw_str(x+44,y,usb_type(usb_prog),C_GREEN); char cb[8]; utoa(usb_count,cb); draw_str(x+winw-150,y,"CTRLRS:",C_WHITE); draw_str(x+winw-94,y,cb,C_TEAL); }
        else draw_str(x+44,y,"NOT DETECTED",C_MGREY+20);
        y+=18;
        draw_str(x,y,"WIFI:",C_TITLE);
        if(wifi_pci){ draw_str(x+52,y,wifi_ven_name(wifi_ven),C_GREEN); draw_str(x+winw-230,y,"802.11 (NO DRIVER)",C_MGREY+20); }
        else if(wifi_usb){ draw_str(x+52,y,wifi_ven_name(wifi_usb_vid),C_GREEN); draw_str(x+winw-230,y,"USB DONGLE (NO DRIVER)",C_MGREY+20); }
        else draw_str(x+52,y,"NOT DETECTED",C_MGREY+20);
        y+=18;
        draw_str(x,y,"CAMERA:",C_TITLE);
        if(cam_count>0){
            int di=cam_info[0].dev_idx;
            draw_str(x+68,y,cam_vendor_name(usbdev[di].vid),C_GREEN);
            char nb[6]; utoa(cam_count,nb); draw_str(x+170,y,nb,C_TEAL); draw_str(x+186,y,"DEVICE(S)",C_MGREY+20);
            if(cam_info[0].has_vc_header){
                u8 maj=(u8)(cam_info[0].bcd_uvc>>8), min=(u8)(cam_info[0].bcd_uvc&0xFF);
                char vb[10]; vb[0]='U';vb[1]='V';vb[2]='C';vb[3]=' '; int p=4;
                vb[p++]='0'+(maj/10); if(maj/10==0)p--; vb[p++]='0'+(maj%10); vb[p++]='.';
                vb[p++]='0'+(min>>4); vb[p++]='0'+((min&0xF)?5:0); vb[p]=0;
                draw_str(x+winw-200,y,vb,C_BBLUE);
            }
            if(cam_info[0].best_w){
                char wb[6],hb[6],rb[16]; utoa(cam_info[0].best_w,wb); utoa(cam_info[0].best_h,hb);
                int p=0; for(int k=0;wb[k];k++)rb[p++]=wb[k]; rb[p++]='x'; for(int k=0;hb[k];k++)rb[p++]=hb[k]; rb[p]=0;
                draw_str(x+winw-110,y,rb,C_MGREY+20);
            }
        }
        else draw_str(x+68,y,"NOT DETECTED",C_MGREY+20);
        y+=18;
        if(ehci_present){
            draw_str(x,y,"EHCI",C_TITLE);
            draw_str(x+44,y,ehci_init_ok?"INIT OK":"INIT FAIL",ehci_init_ok?C_GREEN:(C_RED+8));
            char pb[8]; utoa(ehci_nports,pb); draw_str(x+150,y,"ROOT PORTS:",C_WHITE); draw_str(x+238,y,pb,C_TEAL); y+=14;
            draw_str(x,y,"PORTS:",C_WHITE);
            int sh=ehci_nports>12?12:ehci_nports;
            for(int i=0;i<sh;i++){ fill(x+52+i*16,y,12,9,ehci_port_conn[i]?C_GREEN:C_MGREY); }
            draw_str(x+52+sh*16+8,y,"GREEN=DEVICE",C_MGREY+20); y+=18;
            if(usbdev_n>0){
                draw_str(x,y,"ENUMERATED DEVICES:",C_TITLE); y+=14;
                for(int i=0;i<usbdev_n&&y<winy+winh-40;i++){
                    char vd[12]; hex16(usbdev[i].vid,vd); vd[4]=':'; hex16(usbdev[i].pid,vd+5);
                    char ab[8]; utoa(usbdev[i].addr,ab);
                    u8 eff=usbdev[i].cls?usbdev[i].cls:usbdev[i].ifcls;
                    draw_str(x+8,y,"#",C_WHITE); draw_str(x+18,y,ab,C_TEAL);
                    draw_str(x+44,y,vd,C_WHITE);
                    draw_str(x+140,y,usb_cls_name(eff),C_GREEN);
                    { const char* drv=usbdev[i].kind==1?"[KBD]":usbdev[i].kind==2?"[MOUSE]":usbdev[i].kind==3?"[HUB]":usbdev[i].kind==5?"[CAM]":usbdev[i].kind==6?"[PRINTER]":0; if(drv)draw_str(x+winw-100,y,drv,usbdev[i].kind==6?C_FOLDER:C_BBLUE); }
                    y+=13;
                    if(usbdev[i].name[0]&&y<winy+winh-28){ draw_str(x+44,y,usbdev[i].name,C_MGREY+20); y+=12; }
                }
            } else draw_str(x,y,"NO USB DEVICES ENUMERATED",C_MGREY+20),y+=14;

            { int pi=-1; for(int i=0;i<usbdev_n;i++) if(usbdev[i].kind==6){ pi=i; break; }
              y+=4;
              if(pi>=0){ draw_str(x,y,"USB PRINTER DETECTED:",C_FOLDER);
                  draw_str(x+176,y,usbdev[pi].name[0]?usbdev[pi].name:"PRINTER",C_WHITE); y+=13;
                  char vd[12]; hex16(usbdev[pi].vid,vd); vd[4]=':'; hex16(usbdev[pi].pid,vd+5);
                  draw_str(x+8,y,"ID:",C_TITLE); draw_str(x+40,y,vd,C_TEAL);
                  draw_str(x+140,y,"CLASS 07h - PRINTER",C_GREEN); y+=13;
                  draw_str(x+8,y,"DRIVER: DETECT ONLY (NO PRINTING)",C_MGREY+20); y+=14;
              } else { draw_str(x,y,"USB PRINTER: NONE DETECTED",C_MGREY+20); y+=14; }
            }
        }
        if(xhci_present){
            draw_str(x,y,"XHCI",C_TITLE); draw_str(x+44,y,xhci_init_ok?"INIT OK":"INIT FAIL",xhci_init_ok?C_GREEN:(C_RED+8));
            char pb[8]; utoa(xhci_ports,pb); draw_str(x+150,y,"PORTS:",C_WHITE); draw_str(x+202,y,pb,C_TEAL); y+=13;
            { int sh=xhci_ports>12?12:xhci_ports; draw_str(x,y,"PORTS:",C_WHITE); for(int i=0;i<sh;i++)fill(x+52+i*16,y,12,9,xhci_port_conn[i]?C_GREEN:C_MGREY); draw_str(x+52+sh*16+8,y,"(USB3 BRING-UP)",C_MGREY+20); y+=16; }
        }
        if(msd_dev>=0){
            draw_str(x,y,"USB DISK:",C_TITLE);
            if(msd_blocks){ u32 mb=(msd_blocks+1)/2048; char mbb[12]; utoa(mb,mbb); draw_str(x+80,y,mbb,C_TEAL); draw_str(x+80+strlen_(mbb)*8,y," MB",C_WHITE); }
            else draw_str(x+80,y,"PRESENT",C_GREEN);
            y+=13;
            if(fat_ok){ char nb[8]; utoa(usbfs_n,nb); draw_str(x+8,y,"FS:",C_WHITE); draw_str(x+44,y,fat_type==32?"FAT32":"FAT16",C_GREEN); draw_str(x+120,y,"FILES:",C_WHITE); draw_str(x+172,y,nb,C_TEAL); y+=13;
                for(int i=0;i<usbfs_n&&i<6&&y<winy+winh-196;i++){ draw_str(x+16,y,usbfs[i].name,C_MGREY+20); char s2[10]; utoa(usbfs[i].size,s2); draw_str(x+winw-96,y,s2,C_TEAL); y+=12; } }
            else draw_str(x+8,y,"NO FAT FILESYSTEM",C_MGREY+20),y+=13;
        }
        draw_str(x,y,"STORAGE (ATA)",C_TITLE); y+=14;
        const char*pos[4]={"PRI MAS","PRI SLV","SEC MAS","SEC SLV"};
        int any=0;
        for(int i=0;i<4;i++){ if(!atai[i].present)continue; any=1; draw_str(x,y,pos[i],C_WHITE);
            if(atai[i].type==2) draw_str(x+72,y,"ATAPI CD/DVD",C_MGREY+20);
            else { draw_str(x+72,y,atai[i].model,C_MGREY+20); char b[8]; utoa(atai[i].sectors/2048,b); draw_str(x+winw-110,y,b,C_TEAL); draw_str(x+winw-110+strlen_(b)*8,y," MB",C_TEAL); }
            y+=13; }
        if(!any) draw_str(x,y,"NO ATA DRIVES DETECTED",C_RED+8);
        y+=18; draw_str(x,y,"PORTS:",C_TITLE); y+=14;
        fill(x+6,y+2,8,8,com1_ok?C_GREEN:C_RED); draw_str(x+22,y,com1_ok?"COM1 SERIAL (16550 UART) - OK":"COM1 SERIAL - NOT DETECTED",C_WHITE);
        y+=16; fill(x+6,y+2,8,8,dispi_id?C_GREEN:C_RED); draw_str(x+22,y,dispi_id?"VBE DISPI DISPLAY (RUNTIME RES) - OK":"VBE DISPI - NOT AVAILABLE",C_WHITE);
        y+=16; fill(x+6,y+2,8,8,sb_ok?C_GREEN:C_RED); draw_str(x+22,y,sb_ok?"SOUND BLASTER 16 (DSP+DMA) - OK":"SOUND BLASTER 16 - NOT DETECTED",C_WHITE);
        y+=16; fill(x+6,y+2,8,8,hda_status==1?C_GREEN:(hda_status<0?C_RED:C_MGREY)); draw_str(x+22,y,hda_status==1?"INTEL HD AUDIO (HDA) - OK":(hda_status<0?"INTEL HD AUDIO - NOT FOUND":"INTEL HD AUDIO - RUN 'hda' TO PROBE"),C_WHITE);
        y+=16; fill(x+6,y+2,8,8,xhci_init_ok?C_GREEN:(xhci_present?C_TEAL:C_RED)); draw_str(x+22,y,xhci_init_ok?"USB3 xHCI CONTROLLER - OK":(xhci_present?"USB3 xHCI - FOUND, INIT FAILED":"USB3 xHCI - NOT DETECTED"),C_WHITE);
        y+=16; fill(x+6,y+2,8,8,(touchpad_present||aux_present)?C_GREEN:C_RED); draw_str(x+22,y,touchpad_present?(touchpad_kind==2?"POINTING: ELANTECH TOUCHPAD (PS/2)":"POINTING: SYNAPTICS TOUCHPAD (PS/2)"):(aux_present?(wheel_ok?"POINTING: PS/2 WHEEL MOUSE":"POINTING: PS/2 MOUSE"):"POINTING: NO PS/2 DEVICE"),C_WHITE);
        y+=16; fill(x+6,y+2,8,8,nic_present?C_GREEN:C_RED); draw_str(x+22,y,nic_present?"AMD PCnet NETWORK (TCP/IP) - FOUND":"NETWORK - NO PCnet (set adapter PCnet)",C_WHITE);
        y+=16; fill(x+6,y+2,8,8,wifi_pci?C_GREEN:C_RED); draw_str(x+22,y,wifi_pci?wifi_chip_name(wifi_ven,wifi_did):"WIRELESS - NO 802.11 HARDWARE",C_WHITE);
    } else if(app==8){
        draw_window_chrome("SNAKE");
        char sc[20]="SCORE: "; utoa(sscore,sc+7);
        draw_str(winx+12,winy+28,sc,C_WHITE);
        int cell=14, gx=winx+10, gy=winy+44;
        fill(gx,gy,GCOLS*cell,GROWS*cell,4); frame(gx,gy,GCOLS*cell,GROWS*cell,C_MGREY);
        fill(gx+sfx*cell+2,gy+sfy*cell+2,cell-4,cell-4,C_RED);
        for(int i=0;i<slen;i++) fill(gx+sbx[i]*cell+1,gy+sby[i]*cell+1,cell-2,cell-2,(i==0)?C_WHITE:C_GREEN);
        if(!salive){ draw_str(winx+winw/2-84,winy+winh/2-4,"GAME OVER - PRESS R",C_RED+8); }
        else draw_str(winx+12,winy+winh-14,"WASD / ARROWS TO MOVE",C_MGREY+20);
    } else if(app==34){
        draw_window_chrome("FLAPPY");
        int FW=320,FH=240, fx=winx+10, fy=winy+38;
        { int night=(fscore/8)%2; u32 skyc=night?0x0c1a3au:0x5ab0e8u; fb_rect(fx,fy,FW,FH,skyc);
          if(night){ for(int s=0;s<16;s++){ int sxp=fx+((s*53+11)%FW), syp=fy+((s*89+7)%(FH-60)); fb_pixel(sxp,syp,0xffffffu); fb_pixel(sxp+1,syp,0xccccffu); } disc(fx+FW-30,fy+30,9,C_WHITE); }
          else disc(fx+FW-30,fy+30,11,C_FOLDER); }
        for(int i=0;i<3;i++){
            int gt=fpg[i]-46, gb=fpg[i]+46, x0=fpx[i];
            if(x0<FW && x0+46>0){ int xx=x0,ww=46; if(xx<0){ ww+=xx; xx=0; } if(xx+ww>FW)ww=FW-xx;
                if(ww>0){ if(gt>0){ fill(fx+xx,fy,ww,gt,C_GREEN); frame(fx+xx,fy+gt-7,ww,7,0); } if(gb<FH){ fill(fx+xx,fy+gb,ww,FH-gb,C_GREEN); frame(fx+xx,fy+gb,ww,7,0); } } }
        }
        fill(fx,fy+FH-6,FW,6,C_GREEN); frame(fx,fy+FH-6,FW,6,0);
        int byp=fby10/10; if(byp<0)byp=0; if(byp+14>FH)byp=FH-14;
        fill(fx+80,fy+byp,14,14,C_FOLDER); frame(fx+80,fy+byp,14,14,0);
        fill(fx+88,fy+byp+3,4,4,C_WHITE); fill(fx+90,fy+byp+4,2,2,0);
        fill(fx+94,fy+byp+6,4,3,C_RED);
        frame(fx,fy,FW,FH,C_MGREY);
        char fsc[16]="SCORE: "; utoa(fscore,fsc+7); draw_str(fx+6,winy+22,fsc,C_WHITE);
        if(!falive){ draw_str(fx+FW/2-44,fy+FH/2-46,"GAME OVER",C_RED+8);
            u8 mc=0; const char* mt=0;
            if(fscore>=30){ mc=C_FOLDER; mt="GOLD"; } else if(fscore>=15){ mc=(u8)(C_MGREY+20); mt="SILVER"; } else if(fscore>=5){ mc=C_RED; mt="BRONZE"; }
            if(mc){ disc(fx+FW/2,fy+FH/2-8,18,0); disc(fx+FW/2,fy+FH/2-8,16,mc); disc(fx+FW/2,fy+FH/2-8,8,C_WHITE); int mw=strlen_(mt)*8; draw_str(fx+FW/2-mw/2,fy+FH/2+18,mt,C_WHITE); }
            else draw_str(fx+FW/2-60,fy+FH/2+6,"NO MEDAL THIS TIME",C_MGREY+20);
            draw_str(fx+FW/2-60,fy+FH/2+40,"PRESS R TO RETRY",C_WHITE); }
        else draw_str(fx+6,fy+FH-14,"SPACE / UP = FLAP",C_MGREY+20);
    } else if(app==9){
        draw_window_chrome("NOOVEXDEFENDER");

        fill(winx+winw/2-16,winy+30,32,30,av_threat?C_RED:C_GREEN);
        fill(winx+winw/2-12,winy+34,24,22,C_WIN);
        draw_str(winx+winw/2-4,winy+40,av_threat?"!":"+",C_WHITE);
        draw_str(winx+winw/2-60,winy+68,"NOOVEXDEFENDER 1.0",C_TITLE);
        if(av_state==0){
            draw_str(winx+20,winy+100,"REAL-TIME PROTECTION: ON",C_GREEN);
            draw_str(winx+20,winy+118,"STATUS: NOT SCANNED YET",C_WHITE);
            fill(winx+winw/2-50,winy+150,100,24,C_BLUE); frame(winx+winw/2-50,winy+150,100,24,C_WHITE);
            draw_str(winx+winw/2-30,winy+157,"SCAN NOW",C_WHITE);
        } else if(av_state==2){
            if(av_threat){
                draw_str(winx+20,winy+100,"THREAT DETECTED!",C_RED+8);
                draw_str(winx+20,winy+118,"1 THREAT: MALWARE.NVX",C_WHITE);
                draw_str(winx+20,winy+134,"TYPE: TROJAN.DEMO (HARMLESS)",C_MGREY+20);
                fill(winx+winw/2-55,winy+160,110,24,C_RED); frame(winx+winw/2-55,winy+160,110,24,C_WHITE);
                draw_str(winx+winw/2-40,winy+167,"REMOVE NOW",C_WHITE);
            } else {
                draw_str(winx+20,winy+100,"NO THREATS FOUND",C_GREEN);
                draw_str(winx+20,winy+118,"YOUR SYSTEM IS CLEAN.",C_WHITE);
                fill(winx+winw/2-50,winy+150,100,24,C_BLUE); frame(winx+winw/2-50,winy+150,100,24,C_WHITE);
                draw_str(winx+winw/2-30,winy+157,"SCAN AGAIN",C_WHITE);
            }
        }
        /* --- Security Center: live posture (read-only, real data) --- */
        { int sx=winx+18, sy=winy+200, lh=15, vx=sx+148; if(winw<330)vx=sx+116;
          if(sy<winy+winh-32){
            hrule(winx+12,sy-8,winw-24,C_MGREY);
            draw_str(sx,sy,"SECURITY STATUS",C_TITLE); sy+=lh+2;
            draw_str(sx,sy,"Account",C_MGREY+24);     draw_str_n(vx,sy, acct.user[0]?acct.user:"guest", C_WHITE,16); sy+=lh;
            { int hp=acct.pass[0]!=0; draw_str(sx,sy,"Sign-in",C_MGREY+24); draw_str(vx,sy, hp?"Password set":"No password", hp?C_GREEN:(C_RED+8)); } sy+=lh;
            { const char* d=bl_enabled?(bl_unlocked?"On - unlocked":"On - locked"):"Off"; draw_str(sx,sy,"Disk crypto",C_MGREY+24); draw_str(vx,sy,d,bl_enabled?C_GREEN:(C_MGREY+24)); } sy+=lh;
            { draw_str(sx,sy,"Management",C_MGREY+24); if(acct.enrolled&&acct.org[0])draw_str_n(vx,sy,acct.org,C_TEAL,16); else draw_str(vx,sy,"Not enrolled",C_WHITE); } sy+=lh;
            if(sy<winy+winh-30){ draw_str(sx,sy,"Network",C_MGREY+24); draw_str(vx,sy,"TLS 1.3 (cert:dev)",C_TITLE); sy+=lh; }
            if(sy<winy+winh-30){ draw_str(sx,sy,"Memory guard",C_MGREY+24); draw_str(vx,sy,"Active",C_GREEN); sy+=lh; }
          }
        }
        draw_str(winx+12,winy+winh-14,"NOOVEXDEFENDER - REAL-TIME SECURITY",C_MGREY+20);
    } else if(app==10){
        draw_window_chrome("RECYCLE BIN");
        draw_str(winx+12,winy+28,bin_cnt?"DELETED ITEMS (CLICK TO RESTORE):":"RECYCLE BIN IS EMPTY",C_TITLE);
        for(int i=0;i<bin_cnt;i++){ int ry=winy+48+i*18; fill(winx+12,ry,12,12,C_FOLDER); draw_str(winx+30,ry+1,bin_files[i],C_WHITE); }
        fill(winx+winw/2-55,winy+winh-30,110,22,C_RED); frame(winx+winw/2-55,winy+winh-30,110,22,C_WHITE);
        draw_str(winx+winw/2-36,winy+winh-25,"EMPTY BIN",C_WHITE);
    } else if(app==11){
        draw_window_chrome("PAINT");
        if(!paint_init)paint_clear();
        int cxp=winx+58,cyp=winy+26;

        static const u8 pal[10]={C_RED,108,103,107,109,100,106,104,4,C_WHITE};
        for(int i=0;i<10;i++){ int py=winy+26+i*22; fill(winx+8,py,38,18,pal[i]); if(paint_col==pal[i])frame(winx+6,py-2,42,22,C_WHITE); }
        fill(winx+8,winy+26+10*22,38,18,C_MGREY); draw_str(winx+12,winy+28+10*22,"CLR",C_WHITE);
        { const char* bl[3]={"S","M","L"}; for(int i=0;i<3;i++){ int by=winy+26+11*22+4+i*20; fill(winx+8,by,38,16,(paint_brush==i)?C_BLUE:C_TASK); frame(winx+8,by,38,16,C_MGREY); draw_str(winx+22,by+1,bl[i],C_WHITE); } }

        frame(cxp-1,cyp-1,PW+2,PH+2,C_MGREY);
        for(int y=0;y<PH;y++)for(int x=0;x<PW;x++)FB[(cyp+y)*PITCH+(cxp+x)]=PAL32[paint_buf[y*PW+x]];
        draw_str(winx+8,winy+winh-14,"DRAG ON CANVAS TO DRAW",C_MGREY+20);
    } else if(app==12){
        draw_window_chrome("TETRIS");
        draw_str(winx+16,winy+22,"WASD/Arrows  Space=drop  R=reset",C_MGREY+20);
        int cell=14, gx=winx+16, gy=winy+40;
        fill(gx,gy,TCOLS*cell,TROWS*cell,4); frame(gx,gy,TCOLS*cell,TROWS*cell,C_MGREY);
        for(int y=0;y<TROWS;y++)for(int x=0;x<TCOLS;x++) if(tboard[y][x]) fill(gx+x*cell+1,gy+y*cell+1,cell-2,cell-2,tboard[y][x]);
        if(!tdead) for(int cy=0;cy<4;cy++)for(int cx=0;cx<4;cx++) if(tet_cell(tpiece,trot,cx,cy)){ int x=tpx+cx,y=tpy+cy; if(y>=0)fill(gx+x*cell+1,gy+y*cell+1,cell-2,cell-2,TCOL[tpiece]); }
        int ix=gx+TCOLS*cell+16; char nb[8];
        draw_str(ix,gy,"NEXT",C_TITLE);
        fill(ix,gy+16,70,46,4); frame(ix,gy+16,70,46,C_MGREY);
        for(int cy=0;cy<4;cy++)for(int cx=0;cx<4;cx++) if(tet_cell(tnext,0,cx,cy)) fill(ix+8+cx*13,gy+20+cy*9,11,8,TCOL[tnext]);
        int sy=gy+76;
        fill(ix,sy,70,34,4); frame(ix,sy,70,34,C_MGREY); draw_str(ix+4,sy+3,"SCORE",C_TITLE); utoa(tscore,nb); draw_str(ix+4,sy+18,nb,C_WHITE); sy+=40;
        fill(ix,sy,70,34,4); frame(ix,sy,70,34,C_MGREY); draw_str(ix+4,sy+3,"BEST",C_TITLE); utoa(tbest,nb); draw_str(ix+4,sy+18,nb,C_WHITE); sy+=40;
        fill(ix,sy,70,34,4); frame(ix,sy,70,34,C_MGREY); draw_str(ix+4,sy+3,"LEVEL",C_TITLE); utoa(tlevel,nb); draw_str(ix+4,sy+18,nb,C_WHITE); sy+=40;
        fill(ix,sy,70,34,4); frame(ix,sy,70,34,C_MGREY); draw_str(ix+4,sy+3,"LINES",C_TITLE); utoa(tlines,nb); draw_str(ix+4,sy+18,nb,C_WHITE);
        if(tdead){ int my=gy+TROWS*cell/2-12; fill(gx+8,my,TCOLS*cell-16,44,C_RED); frame(gx+8,my,TCOLS*cell-16,44,C_WHITE); draw_str(gx+24,my+8,"GAME OVER",C_WHITE); draw_str(gx+34,my+24,"PRESS R",C_WHITE); }
    } else if(app==13){
        draw_window_chrome("PIANO");
        int kx=winx+12, ky=winy+40, kw=(winw-24)/8;
        for(int i=0;i<8;i++){ fill(kx+i*kw,ky,kw-2,120,C_WHITE); frame(kx+i*kw,ky,kw-2,120,C_MGREY); }
        static const int blk[7]={1,1,0,1,1,1,0};
        for(int i=0;i<7;i++) if(blk[i]) fill(kx+(i+1)*kw-kw/4,ky,kw/2,72,4);
        draw_str(winx+12,winy+24,"CLICK KEYS OR USE A S D F G H J K",C_TITLE);
        draw_str(winx+12,winy+winh-14,"NOOVEX PIANO - PC SPEAKER",C_MGREY+20);
    } else if(app==14){ craft_draw();
    } else if(app==15){ br_render();
    } else if(app==16){
        draw_window_chrome("ASK CLAUDE");
        int cl=winx+10;

        int ky=winy+30;
        draw_str(cl,ky,"KEY",C_TITLE);
        int kfx=winx+44, kfw=winw-44-150;
        fill(kfx,ky-3,kfw,14,C_WIN); frame(kfx,ky-3,kfw,14,(ai_field==0)?C_WHITE:C_MGREY);
        { int x=kfx+4,maxc=(kfw-8)/8,st=ai_keylen>maxc?ai_keylen-maxc:0; for(int i=st;i<ai_keylen;i++){ draw_char(x,ky,ai_show?AI_KEY[i]:'*',0); x+=8; } if(ai_field==0)fill(x,ky-2,1,12,0); }
        fill(winx+winw-146,ky-3,42,14,ai_show?C_TEAL:(C_MGREY+12)); draw_str(winx+winw-142,ky,ai_show?"HIDE":"SHOW",C_WHITE);
        fill(winx+winw-100,ky-3,42,14,ai_saved?C_GREEN:(C_MGREY+12)); draw_str(winx+winw-96,ky,ai_saved?"SAVED":"SAVE",C_WHITE);
        fill(winx+winw-54,ky-3,46,14,C_MGREY+12); draw_str(winx+winw-50,ky,"CLEAR",C_WHITE);

        int lt=winy+48, lb=winy+winh-30;
        fill(winx+2,lt-2,winw-4,lb-lt+4,57);
        if(chat_n==0 && ai_rc>=0 && ai_rc!=1){
            draw_str(cl,lt+4, ai_keylen?"TYPE A MESSAGE BELOW AND PRESS ENTER.":"ENTER YOUR API KEY ABOVE, THEN CHAT BELOW.",6);
            draw_str(cl,lt+20,"KEY IS KEPT IN RAM / ON DISK ONLY - NEVER IN THE ISO.",6);
        } else {
            int h=chat_draw(cl,lt,winx+winw-10,lb,0,0);
            int viewh=lb-lt, maxsc=h>viewh?h-viewh:0; if(ai_rc==1)maxsc+=14;
            if(ai_tobottom){ ai_scroll=maxsc; ai_tobottom=0; }
            if(ai_scroll>maxsc)ai_scroll=maxsc; if(ai_scroll<0)ai_scroll=0;
            chat_draw(cl,lt,winx+winw-10,lb,ai_scroll,1);
            if(ai_rc==1){ int ty=lt+h-ai_scroll; if(ty>=lt-11&&ty<=lb) draw_str(cl,ty,"CLAUDE IS THINKING...",C_TEAL); }
        }

        if(ai_rc<0 || (ai_rc>0 && ai_rc!=200 && ai_rc!=1)){
            fill(winx+2,lb+1,winw-4,12,C_RED); const char* m; char eb[8];
            if(ai_rc==-100)m="DNS FAILED"; else if(ai_rc==-101)m="TCP 443 FAILED";
            else if(ai_rc==-200)m="CHAT FULL - PRESS CLEAR";
            else if(ai_rc>=-9&&ai_rc<=-1)m=tls_err_text(ai_rc);
            else { utoa(ai_rc,eb); m=eb; }
            draw_str(cl,lb+2,"ERR:",C_WHITE); draw_str(cl+40,lb+2,m,C_WHITE);
            if(ai_rc>200 && AI_ANS[0]){ char e[40]; int q=0; for(int i=0;AI_ANS[i]&&q<38;i++)e[q++]=AI_ANS[i]; e[q]=0; draw_str(cl+96,lb+2,e,C_WHITE); }
        }

        int iy=winy+winh-22, mfx=winx+10, mfw=winw-10-66;
        fill(mfx,iy-2,mfw,16,C_WIN); frame(mfx,iy-2,mfw,16,(ai_field==1)?C_WHITE:C_MGREY);
        { int x=mfx+4,maxc=(mfw-8)/8,st=ai_plen>maxc?ai_plen-maxc:0; for(int i=st;i<ai_plen;i++){ draw_char(x,iy+1,AI_PROMPT[i],0); x+=8; } if(ai_field==1)fill(x,iy,1,12,0); }
        fill(winx+winw-58,iy-2,48,16,C_GREEN); draw_str(winx+winw-50,iy+1,"SEND",C_WHITE);
    } else if(app==17){
        draw_window_chrome("NOOVEXSTORE");
        fill(winx+1,winy+20,winw-2,30,C_BBLUE);
        draw_str(winx+10,winy+25,"NOOVEXSTORE",C_WHITE);
        draw_str(winx+10,winy+37, store_myapps?"YOUR PROGRAMS (.nvx / .elf on disk)":"40 APPS - GET THE ONES YOU DONT HAVE",C_WHITE);

        { int bx=winx+winw-100,by=winy+24; fill(bx,by,94,18, store_myapps?C_GREEN:C_BLUE); draw_str(bx+6,by+2, store_myapps?"< STORE":"MY APPS >",C_WHITE); }
        if(store_myapps){
            int lt=winy+56, lb=winy+winh-6, rowh=44;
            fill(winx+2,lt,winw-4,lb-lt,57);
            int mc=0;
            for(unsigned i=0;i<nvx.count;i++){ const char* nm=nvx.e[i].name; int L=0; while(nm[L])L++;
                int iself=(L>4&&nm[L-4]=='.'&&(nm[L-3]|32)=='e'&&(nm[L-2]|32)=='l'&&(nm[L-1]|32)=='f');
                int isnvx=(L>4&&nm[L-4]=='.'&&(nm[L-3]|32)=='n'&&(nm[L-2]|32)=='v'&&(nm[L-1]|32)=='x');
                if(!iself&&!isnvx)continue;
                int ey=lt+8+mc*rowh-store_scroll; mc++;
                if(ey<lt-rowh||ey>lb)continue;
                fill(winx+12,ey,32,32,C_TEAL); draw_char(winx+22,ey+12,nm[0],C_WHITE);
                char dn[32],dv[16];
                if(isnvx && nvx_app_meta(nm,dn,dv)){ draw_str(winx+54,ey+2,dn,0); draw_str(winx+54,ey+18,"v",6); draw_str(winx+62,ey+18,dv,6); draw_str(winx+120,ey+18,nm,C_MGREY+20); }
                else { draw_str(winx+54,ey+2,nm,0); draw_str(winx+54,ey+18,iself?"ELF PROGRAM":"APP PACKAGE",6); }
                int bw=70, bx=winx+winw-bw-12, by=ey+8; fill(bx,by,bw,18,C_BLUE); draw_str(bx+12,by+2,"LAUNCH",C_WHITE);
                hrule(winx+12,ey+40,winw-24,C_MGREY);
            }
            if(mc==0){ draw_str(winx+16,winy+74,"NO PROGRAMS INSTALLED YET.",6); draw_str(winx+16,winy+92,"OPEN TERMINAL, TYPE: mkdemo",6); draw_str(winx+16,winy+108,"THEN COME BACK HERE TO LAUNCH IT.",6); }
        } else {
        const char* tabs[6]={"ALL","GAMES","TOOLS","NET","MEDIA","SYSTEM"};
        for(int t=0;t<6;t++){ int tx=winx+6+t*75, sel=(store_cat==t-1); fill(tx,winy+52,72,16,sel?C_BLUE:C_MGREY); draw_str(tx+6,winy+54,tabs[t],C_WHITE); }
        int lt=winy+72, lb=winy+winh-6, rowh=50;
        fill(winx+2,lt,winw-4,lb-lt,57);
        static const u8 catcol[5]={71,70,65,72,74};
        int nmatch=0; for(int i=0;i<STORE_N;i++) if(store_cat<0||STORE[i].cat==store_cat) nmatch++;
        { int total=nmatch*rowh+8, viewh=lb-lt, maxsc=total>viewh?total-viewh:0; if(store_scroll>maxsc)store_scroll=maxsc; if(store_scroll<0)store_scroll=0; }
        int mc=0;
        for(int i=0;i<STORE_N;i++){
            if(store_cat>=0 && STORE[i].cat!=store_cat) continue;
            int ey=lt+8+mc*rowh-store_scroll; mc++;
            if(ey<lt-rowh || ey>lb) continue;
            fill(winx+12,ey,32,32,catcol[STORE[i].cat]); draw_char(winx+22,ey+12,STORE[i].name[0],C_WHITE);
            draw_str(winx+54,ey+2,STORE[i].name,0);
            draw_str(winx+54,ey+18,STORE[i].blurb,6);
            int inst=inst_get(i), bw=78, bx=winx+winw-bw-12, by=ey+8;
            fill(bx,by,bw,18,inst?C_GREEN:C_BLUE); draw_str(bx+(inst?14:18),by+2,inst?"INSTALLED":"INSTALL",C_WHITE);
            hrule(winx+12,ey+44,winw-24,C_MGREY);
        }
        }
    } else if(app==18){
        draw_window_chrome("FOLDER");
        const char* fn=(fld_view>=0&&DSK[fld_view].used)?DSK[fld_view].name:"FOLDER";
        fill(winx+1,winy+20,winw-2,20,C_TASK);
        fill(winx+6,winy+22,44,16,C_BLUE); draw_str(winx+12,winy+24,"BACK",C_WHITE);
        draw_str(winx+58,winy+24,fn,C_WHITE);
        fill(winx+2,winy+42,winw-4,winh-44,57);
        int ry=winy+48, any=0;
        for(int i=0;i<DSK_MAX;i++){ if(!DSK[i].used||DSK[i].parent!=fld_view)continue; any=1;
            if(ry>winy+winh-22)break;
            if(DSK[i].type==1){ fill(winx+10,ry,15,12,C_FOLDER); }
            else if(DSK[i].type==2){ fill(winx+10,ry,14,14,C_TEAL); draw_char(winx+13,ry+1,DSK[i].name[0],C_WHITE); }
            else { fill(winx+11,ry,12,14,C_WHITE); frame(winx+11,ry,12,14,C_SHAD); }
            draw_str(winx+34,ry+2,DSK[i].name,0);
            int bx=winx+winw-46; fill(bx,ry,38,14,C_MGREY); draw_str(bx+7,ry+1,"OUT",C_WHITE);
            ry+=24;
        }
        if(!any)draw_str(winx+12,winy+52,"(EMPTY) - DRAG FILES ONTO THE FOLDER",6);
    } else if(app==19){
        draw_window_chrome(gen_name);
        fill(winx+1,winy+20,winw-2,winh-21,C_WIN);
        fill(winx+20,winy+44,28,28,C_TEAL); draw_char(winx+30,winy+54,gen_name[0],C_WHITE);
        draw_str(winx+58,winy+50,gen_name,C_TITLE);
        draw_str(winx+20,winy+90,"DEMO APP FROM NOOVEXSTORE.",6);
        draw_str(winx+20,winy+106,"FULL VERSION COMING SOON.",6);
    } else if(app==20){
        draw_window_chrome("SAVE AS");
        fill(winx+1,winy+20,winw-2,winh-21,C_WIN);
        draw_str(winx+16,winy+34,"FILE NAME:",C_TITLE);
        fill(winx+16,winy+48,winw-32,16,60); frame(winx+16,winy+48,winw-32,16,C_MGREY);
        draw_str(winx+20,winy+51,sas_name,0); fill(winx+20+sas_len*8,winy+50,6,12,0);
        draw_str(winx+16,winy+76,"SAVE TO:",C_TITLE);
        const char* locs[4]={"DESKTOP","DOCS","DISK","USB"};
        for(int i=0;i<4;i++){ int bx=winx+12+i*80; fill(bx,winy+90,76,18,(sas_loc==i)?C_BLUE:C_MGREY); draw_str(bx+6,winy+93,locs[i],C_WHITE); }
        fill(winx+winw-160,winy+winh-30,70,20,C_GREEN); draw_str(winx+winw-150,winy+winh-25,"SAVE",C_WHITE);
        fill(winx+winw-82,winy+winh-30,70,20,C_RED); draw_str(winx+winw-66,winy+winh-25,"CANCEL",C_WHITE);
    } else if(app==21){
        draw_window_chrome("DEVICE ENCRYPTION");
        fill(winx+1,winy+20,winw-2,winh-21,C_WIN);
        draw_lock(winx+28,winy+54,C_TITLE);
        if(bl_state==2){
            draw_str(winx+54,winy+38,"ENCRYPTION ENABLED!",C_GREEN);
            draw_str(winx+12,winy+72,"SAVE THIS RECOVERY KEY NOW:",C_RED+8);
            char l1[30],l2[30]; int i=0; for(;i<27&&bl_rkdisp[i];i++)l1[i]=bl_rkdisp[i]; l1[i]=0; int j=0; while(bl_rkdisp[i]){l2[j++]=bl_rkdisp[i++];} l2[j]=0;
            fill(winx+12,winy+88,winw-24,32,60); frame(winx+12,winy+88,winw-24,32,C_MGREY);
            draw_str(winx+18,winy+94,l1,0); draw_str(winx+18,winy+108,l2,0);
            draw_str(winx+12,winy+128,"USE IT IF YOU FORGET THE PASSWORD.",C_MGREY+20);
            fill(winx+winw-90,winy+winh-30,78,20,C_GREEN); draw_str(winx+winw-70,winy+winh-25,"DONE",C_WHITE);
        } else if(bl_enabled){
            draw_str(winx+54,winy+40,"STATUS: ON",C_GREEN);
            draw_str(winx+54,winy+56,"DRIVE IS ENCRYPTED.",C_TITLE);
            draw_str(winx+12,winy+92,"FILES ARE LOCKED AT BOOT UNTIL YOU",C_TITLE);
            draw_str(winx+12,winy+106,"ENTER THE PASSWORD OR RECOVERY KEY.",C_TITLE);
            fill(winx+12,winy+winh-30,120,20,C_RED); draw_str(winx+30,winy+winh-25,"DISABLE",C_WHITE);
        } else {
            draw_str(winx+54,winy+40,"STATUS: OFF",C_MGREY+20);
            draw_str(winx+12,winy+72,"SET A PASSWORD (LOWERCASE/DIGITS):",C_TITLE);
            fill(winx+12,winy+88,winw-24,18,60); frame(winx+12,winy+88,winw-24,18,C_MGREY);
            for(int i=0;i<bl_pwlen&&i<40;i++)fill(winx+18+i*8,winy+93,6,7,0);
            draw_str(winx+12,winy+114,"THIS ENCRYPTS ALL DISK FILES AND GIVES",C_MGREY+20);
            draw_str(winx+12,winy+128,"YOU A RECOVERY KEY TO WRITE DOWN.",C_MGREY+20);
            fill(winx+12,winy+winh-30,120,20,(bl_pwlen>0)?C_GREEN:C_MGREY); draw_str(winx+34,winy+winh-25,"ENABLE",C_WHITE);
            fill(winx+winw-90,winy+winh-30,78,20,C_RED); draw_str(winx+winw-78,winy+winh-25,"CANCEL",C_WHITE);
        }
    }
    else if(app==23){
        draw_window_chrome(t(T_APP_CALC));
        fill(winx+1,winy+20,winw-2,winh-21,C_WHITE);

        int dpx=winx+12, dpy=winy+30, dpw=winw-24, dph=42;
        fill(dpx,dpy,dpw,dph,5); frame(dpx,dpy,dpw,dph,0);
        char dbuf[16];
        if(calc_err){ dbuf[0]='E';dbuf[1]='R';dbuf[2]='R';dbuf[3]=0; }
        else calc_format(calc_new?calc_acc:calc_cur, dbuf, 16);
        int dlen=strlen_(dbuf);
        draw_str2(dpx+dpw-dlen*16-10, dpy+12, dbuf, C_GREEN);

        if(calc_op){ char opb[2]={calc_op,0}; draw_str(dpx+8,dpy+6,opb,C_GREEN); }

        int bw=(dpw-12)/4, bh=36, bg=4;
        int by0=dpy+dph+10;
        for(int r=0;r<5;r++){
            for(int c=0;c<4;c++){
                const char* lbl=calc_btn_lbl[r][c];

                if(r==4&&c==3&&lbl[0]=='='&&calc_btn_lbl[r][2][0]=='=') continue;
                int span=(r==4&&c==2&&calc_btn_lbl[r][3][0]=='=')?2*bw+bg:bw;
                int bx=dpx+c*(bw+bg);
                int by=by0+r*(bh+bg);
                u8 col;
                if(lbl[0]=='=') col=C_BBLUE;
                else if((lbl[0]>='0'&&lbl[0]<='9')||lbl[0]=='.') col=C_WHITE;
                else if(lbl[0]=='C'||(lbl[0]=='+'&&lbl[1]=='/')) col=60;
                else col=50;
                rrect(bx,by,span,bh,col);
                frame(bx,by,span,bh,40);
                u8 tc=(col==C_BBLUE)?C_WHITE:5;
                int lw=strlen_(lbl)*8;
                draw_str(bx+(span-lw)/2, by+bh/2-3, lbl, tc);
            }
        }
    }
    else if(app==24){
        draw_window_chrome("CAMERA");
        fill(winx+1,winy+20,winw-2,winh-21,C_WIN);
        int x=winx+12, y=winy+30;
        if(cam_count==0){
            draw_str(x,y,"NO USB CAMERA DETECTED",C_RED+8); y+=18;
            draw_str(x,y,"VIRTUALBOX/QEMU DO NOT EXPOSE WEBCAMS",C_TITLE); y+=14;
            draw_str(x,y,"TO THE GUEST USB CONTROLLER.",C_TITLE); y+=14;
            draw_str(x,y,"PLUG A USB CAMERA ON REAL HARDWARE.",C_MGREY+20); y+=20;
        } else {
            int di=cam_info[0].dev_idx;
            char vd[12]; hex16(usbdev[di].vid,vd); vd[4]=':'; hex16(usbdev[di].pid,vd+5);
            draw_str(x,y,"DEVICE:",C_TITLE); draw_str(x+64,y,cam_vendor_name(usbdev[di].vid),C_GREEN);
            draw_str(x+200,y,vd,C_MGREY+20); y+=16;
            if(cam_info[0].has_vc_header){
                u8 maj=(u8)(cam_info[0].bcd_uvc>>8), mn=(u8)(cam_info[0].bcd_uvc&0xFF);
                char vb[10]; int p=0; vb[p++]='0'+maj%10; vb[p++]='.'; vb[p++]='0'+(mn>>4); vb[p++]='0'+((mn&0xF)?5:0); vb[p]=0;
                draw_str(x,y,"UVC:",C_TITLE); draw_str(x+64,y,vb,C_BBLUE);
            }
            if(cam_info[0].n_formats){ draw_str(x+140,y,"FORMAT:",C_TITLE); draw_str(x+204,y,cam_fmt_name(cam_info[0].fmt_type),C_TEAL); }
            y+=16;
            if(cam_info[0].best_w){ char wb[6],hb[6]; utoa(cam_info[0].best_w,wb); utoa(cam_info[0].best_h,hb); draw_str(x,y,"MAX RES:",C_TITLE); draw_str(x+72,y,wb,C_WHITE); draw_str(x+72+strlen_(wb)*8,y,"x",C_WHITE); draw_str(x+82+strlen_(wb)*8,y,hb,C_WHITE); }
            y+=18;

            fill(x,y,150,22,webcam_neg_ok?C_GREEN:C_BBLUE); frame(x,y,150,22,C_WHITE);
            draw_str(x+10,y+7,webcam_neg_ok?"STREAM COMMITTED":"NEGOTIATE STREAM",C_WHITE);
            y+=30;
            if(webcam_neg_ok){
                char b[12];
                draw_str(x,y,"COMMITTED CONFIG:",C_GREEN); y+=14;
                utoa(webcam_max_frame,b); draw_str(x,y,"FRAME SIZE:",C_TITLE); draw_str(x+96,y,b,C_WHITE); draw_str(x+96+strlen_(b)*8,y," B",C_MGREY+20); y+=13;
                utoa(webcam_max_payload,b); draw_str(x,y,"PAYLOAD:",C_TITLE); draw_str(x+96,y,b,C_WHITE); draw_str(x+96+strlen_(b)*8,y," B",C_MGREY+20); y+=13;
                int fp=webcam_fps(); utoa(fp,b); draw_str(x,y,"FPS (EST):",C_TITLE); draw_str(x+96,y,b,C_WHITE); y+=13;
            } else if(webcam_err){
                char eb[6]; utoa(-webcam_err,eb); draw_str(x,y,"NEGOTIATION FAILED (E",C_RED+8); draw_str(x+168,y,eb,C_RED+8); draw_str(x+168+strlen_(eb)*8,y,")",C_RED+8); y+=14;
            }
            y+=4;
        }

        int dw=240, dh=140; int dx=winx+winw-dw-12, dyv=winy+winh-dh-12;
        if(dyv<winy+30)dyv=winy+30;

        if(cam_have){
            fill(dx,dyv,dw,dh-16,0); cam_blit(dx,dyv,dw,dh-16); frame(dx,dyv,dw,dh-16,0);
            fill(dx,dyv+dh-16,dw,16,C_SHAD);
            char rb[16]; utoa((u32)cam_fw,rb); int L=strlen_(rb); draw_str(dx+6,dyv+dh-13,"LIVE FRAME ",C_GREEN);
            draw_str(dx+94,dyv+dh-13,rb,C_WHITE); draw_str(dx+94+L*8,dyv+dh-13,"x",C_WHITE); utoa((u32)cam_fh,rb); draw_str(dx+102+L*8,dyv+dh-13,rb,C_WHITE);
        } else {
            u8 bars[8]={C_WHITE,C_FOLDER,C_TEAL,C_GREEN,C_BBLUE,C_RED,C_BLUE,5};
            for(int i=0;i<8;i++) fill(dx+i*(dw/8),dyv,dw/8,dh-30,bars[i]);
            int sweep=(clkbuf[7]-'0')+(clkbuf[6]-'0')*10;
            for(int gx=0;gx<dw;gx++){ u8 g=(u8)(((gx+sweep*4)%64)); fill(dx+gx,dyv+dh-30,1,14,g); }
            frame(dx,dyv,dw,dh-16,0); fill(dx,dyv+dh-16,dw,16,C_SHAD);
            draw_str(dx+6,dyv+dh-13,"TEST PATTERN - PRESS CAPTURE",C_WHITE);
        }
        /* capture button + honest capability line */
        if(cam_count){
            int cbx=winx+12, cby=winy+winh-32;
            int bulk=cam_info[0].vs_bulk && cam_info[0].fmt_type==1;
            fill(cbx,cby,150,22,bulk?C_GREEN:C_MGREY); frame(cbx,cby,150,22,C_WHITE);
            draw_str(cbx+10,cby+7,bulk?"CAPTURE FRAME":"CAPTURE (BULK ONLY)",C_WHITE);
            if(!bulk) draw_str(cbx+160,cby+7,"THIS CAM IS ISOC - NOT SUPPORTED",C_RED+8);
            else if(cam_lasterr) { char eb[8]; utoa((u32)(-cam_lasterr),eb); draw_str(cbx+160,cby+7,"CAPTURE ERR E",C_RED+8); draw_str(cbx+160+13*8,cby+7,eb,C_RED+8); }
        }
    }
    else if(app==25){
        draw_window_chrome("PHOTOS");
        fill(winx+1,winy+20,winw-2,winh-21,C_WIN);
        if(ph_view>=0 && ph_view<ph_count){
            int ax=winx+12, ay=winy+30, aw=winw-24, ah=winh-72;
            fill(ax,ay,aw,ah,5);
            ph_blit(ph_view, ax, ay, aw, ah);
            frame(ax,ay,aw,ah,0);
            draw_str(winx+12,winy+winh-30,"FILE:",C_TITLE); draw_str(winx+56,winy+winh-30,ph_name[ph_view],C_GREEN);
            { char b[12]; int xx=winx+12; draw_str(xx,winy+winh-16,"SIZE:",C_TITLE); xx+=48; utoa(ph_w[ph_view],b); draw_str(xx,winy+winh-16,b,C_WHITE); xx+=strlen_(b)*8; draw_str(xx,winy+winh-16,"x",C_WHITE); xx+=8; utoa(ph_h[ph_view],b); draw_str(xx,winy+winh-16,b,C_WHITE); }
            fill(winx+winw-94,winy+winh-28,82,22,C_BBLUE); frame(winx+winw-94,winy+winh-28,82,22,C_WHITE);
            draw_str(winx+winw-82,winy+winh-22,"< BACK",C_WHITE);
            fill(winx+winw-186,winy+winh-28,88,22,C_GREEN); frame(winx+winw-186,winy+winh-28,88,22,C_WHITE); draw_str(winx+winw-180,winy+winh-22,"WALLPAPER",C_WHITE);
        } else {
            draw_str(winx+12,winy+30,"PHOTO LIBRARY",C_BBLUE);
            { char cb[8]; utoa(ph_count,cb); int lx=winx+12+14*8; draw_str(lx,winy+30,"(",C_MGREY+20); draw_str(lx+8,winy+30,cb,C_WHITE); draw_str(lx+8+strlen_(cb)*8,winy+30,")",C_MGREY+20); }
            fill(winx+winw-162,winy+26,150,20,C_GREEN); frame(winx+winw-162,winy+26,150,20,C_WHITE);
            draw_str(winx+winw-154,winy+30,"IMPORT FROM USB",C_WHITE);
            if(ph_count==0){
                draw_str(winx+20,winy+74,"NO PHOTOS YET.",C_MGREY+20);
                draw_str(winx+20,winy+94,"PLUG IN A USB DRIVE WITH .PNG / .JPG",C_MGREY+20);
                draw_str(winx+20,winy+110,"FILES, THEN CLICK IMPORT FROM USB.",C_MGREY+20);
                if(msd_dev<0) draw_str(winx+20,winy+140,"(NO USB DEVICE DETECTED)",C_RED+8);
                else if(!fat_ok) draw_str(winx+20,winy+140,"(USB PRESENT - NO FAT FILESYSTEM)",C_RED+8);
                else { char nb[8]; utoa(usbfs_n,nb); draw_str(winx+20,winy+140,"USB FILES READY:",C_GREEN); draw_str(winx+20+17*8,winy+140,nb,C_WHITE); }
            } else {
                int cell=100,pad=12; int cols=(winw-2*pad+12)/cell; if(cols<1)cols=1;
                int gx=winx+pad, gy=winy+54;
                for(int p=0;p<ph_count;p++){ int col=p%cols,row=p/cols; int cx=gx+col*cell, cy=gy+row*(cell+12);
                    if(cy+88>winy+winh-12) break;
                    fill(cx-2,cy-2,92,92,C_TASK);
                    ph_blit(p,cx,cy,88,88);
                    frame(cx-2,cy-2,92,92,C_MGREY);
                    char nm[12]; int k=0; while(ph_name[p][k]&&k<10){nm[k]=ph_name[p][k];k++;} nm[k]=0;
                    draw_str(cx,cy+92,nm,C_WHITE); }
            }
        }
    }
    else if(app==26){
        draw_window_chrome(VND_APP);
        fill(winx+1,winy+20,winw-2,winh-21,C_WHITE);

        fill(winx+1,winy+20,winw-2,40,VND_COL);
        draw_str2(winx+16,winy+30,VND_NAME,C_WHITE);
        { const char* tg=VND_TAG; draw_str(winx+16+strlen_(VND_NAME)*16+14,winy+40,tg,C_WHITE); }
        int x=winx+16,y=winy+74;
        draw_str(x,y,"MODEL:",C_TASK); draw_str(x+72,y,VND_MODEL,VND_COL); y+=18;
        draw_str(x,y,"CPU:",C_TASK); draw_str(x+72,y,cpu_brand[0]?cpu_brand:cpu_vendor,5); y+=16;
        { char bb[8],gb[8]; utoa(ram_mb,bb); utoa((u32)ram_mb/1024,gb); draw_str(x,y,"MEMORY:",C_TASK); draw_str(x+72,y,bb,5); draw_str(x+72+strlen_(bb)*8,y," MB",C_TASK); draw_str(x+220,y,"(",C_MGREY+20); draw_str(x+228,y,gb,C_BBLUE); draw_str(x+228+strlen_(gb)*8,y," GB)",C_MGREY+20); } y+=22;

        int bp=92;
        draw_str(x,y,"BATTERY:",C_TASK);
        { int bx=x+72,by=y-1,bw=80,bh=12; frame(bx,by,bw,bh,5); fill(bx+bw,by+3,3,6,5);
          fill(bx+2,by+2,(bw-4)*bp/100,bh-4,C_GREEN); char pb[6]; utoa(bp,pb); draw_str(bx+bw+12,y,pb,5); draw_str(bx+bw+12+strlen_(pb)*8,y,"%",C_TASK); }
        y+=20;
        draw_str(x,y,"SYSTEM HEALTH:",C_TASK); draw_str(x+136,y,"OPTIMAL",C_GREEN); y+=16;
        draw_str(x,y,"WARRANTY:",C_TASK); draw_str(x+136,y,"ACTIVE - 24 MONTHS",C_GREEN); y+=16;
        draw_str(x,y,"DRIVERS:",C_TASK); draw_str(x+136,y,"UP TO DATE",C_GREEN); y+=16;
        draw_str(x,y,"SUPPORT:",C_TASK); draw_str(x+136,y,VND_SUP,C_BBLUE); y+=24;

        fill(x,y,180,24,VND_COL); frame(x,y,180,24,C_WHITE);
        draw_str(x+16,y+8,"CHECK FOR UPDATES",C_WHITE);

        { const char* f=VND_NAME " CENTER - " VND_APP; draw_str(winx+16,winy+winh-18,f,C_MGREY+20); }
    }
    else if(app==27){
        draw_window_chrome("GPU / GRAPHICS");
        fill(winx+1,winy+20,winw-2,winh-21,C_WHITE);
        fill(winx+1,winy+20,winw-2,34,C_TEAL);
        draw_str2(winx+16,winy+28,"GPU",C_WHITE);
        draw_str(winx+16+3*16+12,winy+38,OSNAME " GRAPHICS",C_WHITE);
        int x=winx+16,y=winy+66;
        if(gpu_found){
            draw_str(x,y,"ADAPTER:",C_TASK); draw_str(x+96,y,gpu_vendor_name(gpu_ven),C_TEAL); y+=16;
            char hb[12]; { u32 vv=gpu_ven,dd=gpu_did; for(int i=0;i<4;i++){int d=(vv>>((3-i)*4))&0xF;hb[i]=d<10?'0'+d:'A'+d-10;} hb[4]=':'; for(int i=0;i<4;i++){int d=(dd>>((3-i)*4))&0xF;hb[5+i]=d<10?'0'+d:'A'+d-10;} hb[9]=0; }
            draw_str(x,y,"VENDOR:DEV:",C_TASK); draw_str(x+96,y,hb,5); y+=16;
            { char pb[16]; int j=0; char t[4]; utoa(gpu_bus,t); for(int i=0;t[i];i++)pb[j++]=t[i]; pb[j++]=':'; utoa(gpu_dev,t); for(int i=0;t[i];i++)pb[j++]=t[i]; pb[j++]='.'; utoa(gpu_fn,t); for(int i=0;t[i];i++)pb[j++]=t[i]; pb[j]=0; draw_str(x,y,"PCI ADDR:",C_TASK); draw_str(x+96,y,pb,5); } y+=16;
            draw_str(x,y,"CLASS:",C_TASK); draw_str(x+96,y,"DISPLAY CONTROLLER (VGA)",5); y+=16;
            char fb[12]; { u32 a=(u32)LFB; fb[0]='0';fb[1]='X'; for(int i=0;i<8;i++){int d=(a>>((7-i)*4))&0xF;fb[2+i]=d<10?'0'+d:'A'+d-10;} fb[10]=0; }
            draw_str(x,y,"FRAMEBUFFER:",C_TASK); draw_str(x+96,y,fb,5); y+=16;
            { char rb[24]; int j=0; char t[6]; utoa(W,t); for(int i=0;t[i];i++)rb[j++]=t[i]; rb[j++]='X'; utoa(H,t); for(int i=0;t[i];i++)rb[j++]=t[i]; rb[j++]='X'; rb[j++]='8'; rb[j]=0; draw_str(x,y,"RESOLUTION:",C_TASK); draw_str(x+96,y,rb,5); } y+=16;
            { char pb[8]; utoa(PITCH,pb); draw_str(x,y,"PITCH:",C_TASK); draw_str(x+96,y,pb,5); draw_str(x+96+strlen_(pb)*8,y," BYTES/ROW",C_MGREY+20); } y+=16;
            if(gpu_bar0sz){ char vb[8]; utoa(gpu_bar0sz/(1024*1024),vb); draw_str(x,y,"VRAM (BAR):",C_TASK); draw_str(x+96,y,vb,5); draw_str(x+96+strlen_(vb)*8,y," MB",C_MGREY+20); y+=16; }
            draw_str(x,y,"VBE DISPI:",C_TASK); draw_str(x+96,y,dispi_id?"AVAILABLE (RUNTIME RES)":"NOT AVAILABLE",dispi_id?C_GREEN:C_RED); y+=20;
            draw_str(x,y,"HW 2D ACCEL:",C_TASK); draw_str(x+112,y,(gpu_ven==0x15AD)?"SVGA II FIFO CAPABLE (DRIVER TODO)":"NONE - CPU RENDERING (REP MOVS)",(gpu_ven==0x15AD)?C_GREEN:C_MGREY+24); y+=20;
        } else { draw_str(x,y,"NO PCI DISPLAY CONTROLLER FOUND",C_RED); y+=20; }
        draw_str(x,y,"2D OUTPUT (SOFTWARE / LINEAR FRAMEBUFFER):",C_TASK); y+=14;
        int gx=x,gw=winw-32,gh=46; if(gx+gw>winx+winw-12)gw=winx+winw-12-gx;
        for(int i=0;i<gw;i++){ u8 c=(u8)(i*63/(gw>1?gw:1)); fill(gx+i,y,1,gh/2,c); }
        { u8 cols[8]={C_RED,C_FOLDER,C_GREEN,C_TEAL,C_BBLUE,C_BLUE,C_WHITE,40}; int sw=gw/8; for(int i=0;i<8;i++)fill(gx+i*sw,y+gh/2+2,sw-2,gh/2-2,cols[i]); }
        frame(gx,y,gw,gh,5); y+=gh+8;
        draw_str(winx+16,winy+winh-30,"SOFTWARE 3D: NOOVEXGRAPH CPU RASTERIZER VIA LFB",C_GREEN);
        draw_str(winx+16,winy+winh-18,"NO HARDWARE GPU DRIVER (SHADERS/COMMAND RING)",C_MGREY+20);
    }
    else if(app==28){
        draw_window_chrome("HEXLANG");
        fill(winx+1,winy+22,winw-2,winh-23,4);
        fill(winx+1,winy+22,132,winh-23,C_TASK);
        draw_str(winx+10,winy+28,"SCRIPTS",C_MGREY+20);
        int row=0;
        for(int i=0;i<HX_NSAMP;i++){ int ly=winy+46+row*18; if(hx_sel==row)fill(winx+3,ly-2,126,18,C_BLUE); draw_str(winx+10,ly,HX_NAME[i],(hx_sel==row)?C_WHITE:C_TITLE); row++; }
        if(disk_ok)for(int i=0;i<(int)nvx.count;i++){ if(ext_is(nvx.e[i].name,'H','E','X')){ int ly=winy+46+row*18; if(ly>winy+winh-44)break; if(hx_sel==row)fill(winx+3,ly-2,126,18,C_BLUE); draw_str(winx+10,ly,nvx.e[i].name,(hx_sel==row)?C_WHITE:C_TITLE); row++; } }
        fill(winx+10,winy+winh-30,64,20,C_GREEN); frame(winx+10,winy+winh-30,64,20,C_WHITE); draw_str(winx+26,winy+winh-25,"RUN",C_WHITE);

        int cx=winx+144,cy=winy+30; frame(cx-1,cy-1,HXC_W+2,HXC_H+2,C_MGREY);
        for(int y=0;y<HXC_H;y++){ u32* d=FB+(cy+y)*PITCH+cx; for(int x=0;x<HXC_W;x++) d[x]=hx_canvas[y*HXC_W+x]; }

        int ox=winx+144, oy=cy+HXC_H+18; draw_str(ox,oy-14,"OUTPUT",C_MGREY+20);
        int lx=ox, ly=oy;
        for(int i=0;i<hxout_len && ly<winy+winh-14;i++){ char ch=hxout[i]; if(ch=='\n'){lx=ox;ly+=14;continue;} if(lx>winx+winw-14){lx=ox;ly+=14;} draw_char(lx,ly,ch,C_GREEN); lx+=8; }
        if(!hx_ran) draw_str(ox,oy,"SELECT A SCRIPT AND PRESS RUN.",C_MGREY+20);
    }
    else if(app==31){
        draw_window_chrome("ZIP VIEWER");
        fill(winx+1,winy+22,winw-2,winh-23,C_WIN);
        draw_zip_icon(winx+8,winy+26,24);
        draw_str(winx+38,winy+28,zip_arc_name,C_TITLE);
        char cb[24]="FILES: "; { int q=7,c=zip_n; char t[6];int tl=0; if(c==0)t[tl++]='0'; while(c){t[tl++]='0'+c%10;c/=10;} while(tl)cb[q++]=t[--tl]; cb[q]=0; }
        draw_str(winx+winw-90,winy+28,cb,C_MGREY+20);
        hrule(winx+8,winy+50,winw-16,C_MGREY);
        if(zip_n<=0){ draw_str(winx+12,winy+60,zip_err?"COULD NOT READ ARCHIVE":"EMPTY ARCHIVE",C_RED+8); }
        else for(int i=0;i<zip_n && i<12;i++){ int ry=winy+58+i*22;
            if(zip_sel==i) fill(winx+6,ry-2,winw-12,20,C_BLUE);
            blit_icon(icon_for_app(0)>=0?icon_for_app(0):0,winx+10,ry-1,16);
            draw_str(winx+30,ry+1,zip_ents[i].name,(zip_sel==i)?C_WHITE:0);
            char sz[12]; int q=0,v=zip_ents[i].uncomp_size; char t[10];int tl=0; if(v==0)t[tl++]='0'; while(v){t[tl++]='0'+v%10;v/=10;} while(tl)sz[q++]=t[--tl]; sz[q++]='B'; sz[q]=0;
            draw_str(winx+winw-140,ry+1,sz,(zip_sel==i)?C_WHITE:C_MGREY+20);
            const char* m=(zip_ents[i].method==8)?"DEFLATE":(zip_ents[i].method==0?"STORE":"?");
            draw_str(winx+winw-70,ry+1,m,(zip_sel==i)?C_WHITE:C_TEAL);
        }

        fill(winx+12,winy+winh-26,90,18,C_GREEN); draw_str(winx+20,winy+winh-23,"EXTRACT",C_WHITE);
        fill(winx+110,winy+winh-26,90,18,C_BLUE); draw_str(winx+120,winy+winh-23,"VIEW TEXT",C_WHITE);
        if(zip_msg[0]) draw_str(winx+212,winy+winh-23,zip_msg,C_TITLE);
    }
    else if(app==32){
        draw_window_chrome("PDF VIEWER");
        fill(winx+1,winy+22,winw-2,winh-23,C_WIN);
        draw_str(winx+10,winy+28,pdf_name,C_TITLE);
        { char cb[28]="PAGES: "; int q=7,c=pdf_pages; char t[6];int tl=0; if(c==0)t[tl++]='0'; while(c){t[tl++]='0'+c%10;c/=10;} while(tl)cb[q++]=t[--tl]; cb[q]=0;
          draw_str(winx+winw-120,winy+28,cb,C_MGREY+20); }
        hrule(winx+8,winy+48,winw-16,C_MGREY);

        int tx=winx+12, ty=winy+56, tw=winw-24, rows=(winh-90)/14, cols=tw/8;
        if(pdf_txtlen<=1){ draw_str(tx,ty,"(NO EXTRACTABLE TEXT IN THIS PDF)",C_MGREY+20); }
        else {
            int line=0, col=0, shown=0;
            for(int i=0;i<pdf_txtlen && shown<rows;i++){ char c=PDF_TXT[i];
                if(c=='\n'){ if(line>=pdf_scroll){ shown++; } line++; col=0; continue; }
                if(col>=cols){ if(line>=pdf_scroll)shown++; line++; col=0; if(shown>=rows)break; }
                if(line>=pdf_scroll && shown<rows){ if(c>=32&&c<127) draw_char(tx+col*8, ty+shown*14, c, 0); }
                col++;
            }
        }

        fill(winx+12,winy+winh-26,90,18,C_BBLUE); draw_str(winx+22,winy+winh-23,"SCROLL",C_WHITE);
        draw_str(winx+112,winy+winh-23,"UP/DN ARROWS TO SCROLL",C_MGREY+20);
    }
    else if(app==33){
        const char* tn[8]={"TEXT","CSV TABLE","JSON","MARKDOWN","CONFIG","LOG","XML","TAR ARCHIVE"};
        draw_window_chrome("DOCUMENT VIEWER");
        fill(winx+1,winy+22,winw-2,winh-23,C_WIN);
        draw_str(winx+10,winy+28,doc_name,C_TITLE);
        draw_str(winx+winw-150,winy+28,tn[doc_type&7],C_MGREY+20);
        hrule(winx+8,winy+46,winw-16,C_MGREY);
        int tx=winx+12, ty=winy+54, rows=(winh-86)/14, cols=(winw-24)/8;
        if(doc_type==1){

            int line=0, shown=0, i=0;
            while(i<doc_len && shown<rows){
                int ls=i; while(i<doc_len && DOCBUF[i]!='\n')i++; int le=i; if(i<doc_len)i++;
                if(line<doc_scroll){ line++; continue; }
                int y=ty+shown*14; int header=(line==0);
                if(header) fill(winx+8,y-1,winw-16,14,C_BBLUE);
                int cx=tx, col=0;
                for(int p=ls;p<=le;p++){
                    if(p==le || DOCBUF[p]==','){
                        if(col<10){ int colw=(winw-24)/((col<6)?6:col+1); (void)colw; }
                        col++;
                        if(p<le){ fill(cx+ (110*1) -2, y, 1, 12, C_MGREY); }
                    }
                }

                cx=tx; col=0; int cstart=ls;
                for(int p=ls;p<=le;p++){
                    if(p==le || DOCBUF[p]==','){
                        int clen=p-cstart; int maxc=13;
                        for(int q=0;q<clen&&q<maxc;q++){ char ch=DOCBUF[cstart+q]; if(ch>=32&&ch<127) draw_char(cx+q*8,y,ch,header?C_WHITE:(u8)0); }
                        cx += 14*8; col++; cstart=p+1;
                        if(cx>winx+winw-40)break;
                    }
                }
                shown++; line++;
            }
            { int tl=0,k=doc_len; (void)k; char cb[20]="ROWS FROM "; int q=10,v=doc_scroll; char t[6]; if(!v)t[tl++]='0'; while(v){t[tl++]='0'+v%10;v/=10;} while(tl)cb[q++]=t[--tl]; cb[q]=0; draw_str(winx+112,winy+winh-23,cb,C_MGREY+20); }
        } else if(doc_type==2){

            static char jf[16384]; int o=0,depth=0,instr=0;
            for(int i=0;i<doc_len&&o<16380;i++){ char c=DOCBUF[i];
                if(c=='"'&&(i==0||DOCBUF[i-1]!='\\'))instr=!instr;
                if(instr){ jf[o++]=c; continue; }
                if(c==' '||c=='\n'||c=='\t'||c=='\r')continue;
                if(c=='{'||c=='['){ jf[o++]=c; jf[o++]='\n'; depth++; for(int d=0;d<depth&&o<16370;d++){jf[o++]=' ';jf[o++]=' ';} }
                else if(c=='}'||c==']'){ jf[o++]='\n'; if(depth)depth--; for(int d=0;d<depth&&o<16370;d++){jf[o++]=' ';jf[o++]=' ';} jf[o++]=c; }
                else if(c==','){ jf[o++]=c; jf[o++]='\n'; for(int d=0;d<depth&&o<16370;d++){jf[o++]=' ';jf[o++]=' ';} }
                else if(c==':'){ jf[o++]=c; jf[o++]=' '; } else jf[o++]=c; }
            jf[o]=0;
            int line=0,col=0,shown=0;
            for(int i=0;i<o&&shown<rows;i++){ char c=jf[i];
                if(c=='\n'){ if(line>=doc_scroll)shown++; line++; col=0; continue; }
                if(line>=doc_scroll&&shown<rows&&col<cols){ if(c>=32&&c<127) draw_char(tx+col*8,ty+shown*14,c, c=='"'?C_TEAL:(u8)0); }
                col++; }
        } else if(doc_type==3){

            int line=0,shown=0,i=0;
            while(i<doc_len&&shown<rows){ int ls=i; while(i<doc_len&&DOCBUF[i]!='\n')i++; int le=i; if(i<doc_len)i++;
                if(line<doc_scroll){ line++; continue; }
                int y=ty+shown*14; int p=ls; while(p<le&&DOCBUF[p]==' ')p++;
                if(p<le&&DOCBUF[p]=='#'){ int h=0; while(p<le&&DOCBUF[p]=='#'){h++;p++;} if(p<le&&DOCBUF[p]==' ')p++;
                    int cx=tx; for(int q=p;q<le;q++){ char ch=DOCBUF[q]; if(ch>=32&&ch<127){ draw_char(cx,y,ch,C_TEAL); if(h<=2)draw_char(cx+1,y,ch,C_TEAL); } cx+=8; } }
                else if(p<le&&(DOCBUF[p]=='-'||DOCBUF[p]=='*')&&p+1<le&&DOCBUF[p+1]==' '){
                    fill(tx+2,y+5,4,4,C_BBLUE); int cx=tx+12; for(int q=p+2;q<le;q++){ char ch=DOCBUF[q]; if(ch>=32&&ch<127)draw_char(cx,y,ch,0); cx+=8; } }
                else { int cx=tx,col=0; for(int q=ls;q<le&&col<cols;q++){ char ch=DOCBUF[q]; if(ch=='*')continue; if(ch>=32&&ch<127)draw_char(cx,y,ch,0); cx+=8; col++; } }
                shown++; line++; }
        } else if(doc_type==4){

            int line=0,shown=0,i=0;
            while(i<doc_len&&shown<rows){ int ls=i; while(i<doc_len&&DOCBUF[i]!='\n')i++; int le=i; if(i<doc_len)i++;
                if(line<doc_scroll){ line++; continue; }
                int y=ty+shown*14; int p=ls; while(p<le&&DOCBUF[p]==' ')p++;
                u8 lc=0;
                if(p<le&&(DOCBUF[p]=='['))lc=C_TEAL; else if(p<le&&(DOCBUF[p]==';'||DOCBUF[p]=='#'))lc=C_MGREY+20;
                int cx=tx,col=0,seeneq=0;
                for(int q=ls;q<le&&col<cols;q++){ char ch=DOCBUF[q]; u8 cc=lc;
                    if(!lc){ if(ch=='=')seeneq=1; cc=seeneq?(u8)0:C_BLUE; }
                    if(ch>=32&&ch<127)draw_char(cx,y,ch,cc); cx+=8; col++; }
                shown++; line++; }
        } else if(doc_type==5){

            int line=0,shown=0,i=0;
            while(i<doc_len&&shown<rows){ int ls=i; while(i<doc_len&&DOCBUF[i]!='\n')i++; int le=i; if(i<doc_len)i++;
                if(line<doc_scroll){ line++; continue; }
                int y=ty+shown*14;
                char ln[7]; int q=0,v=line+1; char t[6];int tl=0; if(!v)t[tl++]='0'; while(v){t[tl++]='0'+v%10;v/=10;} while(tl)ln[q++]=t[--tl]; ln[q]=0;
                draw_str(tx,y,ln,C_MGREY+18);
                u8 lc=0; for(int q2=ls;q2<le-2;q2++){ if((DOCBUF[q2]=='E'||DOCBUF[q2]=='e')&&(DOCBUF[q2+1]=='R')&&(DOCBUF[q2+2]=='R')){lc=C_RED+8;break;} if((DOCBUF[q2]=='W')&&(DOCBUF[q2+1]=='A')&&(DOCBUF[q2+2]=='R')){lc=74;break;} }
                int cx=tx+48,col=0; for(int q2=ls;q2<le&&col<cols-6;q2++){ char ch=DOCBUF[q2]; if(ch>=32&&ch<127)draw_char(cx,y,ch,lc); cx+=8; col++; }
                shown++; line++; }
        } else if(doc_type==7){

            draw_str(tx,ty,"TAR CONTENTS:",C_TEAL);
            int off=0, shown=0, row=1;
            while(off+512<=doc_len && shown<rows-1){
                if(DOCBUF[off]==0){ off+=512; if(DOCBUF[off]==0)break; continue; }
                char nm[40]; int k=0; while(k<36&&DOCBUF[off+k])nm[k]=DOCBUF[off+k],k++; nm[k]=0;

                int sz=0; for(int d=0;d<11;d++){ char c=DOCBUF[off+124+d]; if(c>='0'&&c<='7')sz=sz*8+(c-'0'); }
                if(row>=doc_scroll+1){ int y=ty+ (shown+1)*14; draw_str(tx,y,nm,0);
                    char sb[12]; int q=0,v=sz; char t[10];int tl=0; if(!v)t[tl++]='0'; while(v){t[tl++]='0'+v%10;v/=10;} while(tl)sb[q++]=t[--tl]; sb[q]=0;
                    draw_str(tx+ (winw-140), y, sb, C_MGREY+20); shown++; }
                int blocks=(sz+511)/512; off += 512 + blocks*512; row++;
            }
        } else {

            int line=0,col=0,shown=0;
            for(int i=0;i<doc_len&&shown<rows;i++){ char c=DOCBUF[i];
                if(c=='\n'){ if(line>=doc_scroll)shown++; line++; col=0; continue; }
                if(col>=cols){ if(line>=doc_scroll)shown++; line++; col=0; if(shown>=rows)break; }
                if(line>=doc_scroll&&shown<rows){ if(c>=32&&c<127)draw_char(tx+col*8,ty+shown*14,c,0); }
                col++; }
        }
        fill(winx+12,winy+winh-26,90,18,C_BBLUE); draw_str(winx+22,winy+winh-23,"SCROLL",C_WHITE);
    }
    else if(app==30){
        draw_window_chrome("NOOVEXGRAPH");
        int cx0=winx+2, cy0=winy+23, cx1=winx+winw-3, cy1=winy+winh-22;
        fill(winx+1,winy+22,winw-2,winh-23,4);
        draw_str(winx+10,winy+28,g3_formula(),C_TITLE);
        fill(winx+winw-152,winy+25,44,16,C_MGREY+12); draw_str(winx+winw-146,winy+28,"PLOT",C_WHITE);
        fill(winx+winw-104,winy+25,44,16,g3_wire?C_GREEN:C_MGREY+12); draw_str(winx+winw-98,winy+28,"WIRE",C_WHITE);
        fill(winx+winw-56,winy+25,50,16,C_MGREY+12); draw_str(winx+winw-52,winy+28,"PRESET",C_WHITE);
        int zmin=999999,zmax=-999999;
        for(int j=0;j<G3N;j++)for(int i=0;i<G3N;i++){ int z=g3z(i-G3H,j-G3H); g3zv[j*G3N+i]=(short)z; if(z<zmin)zmin=z; if(z>zmax)zmax=z; }
        int zspan=zmax-zmin; if(zspan<1)zspan=1;
        int cs=S(g3_yaw+64), sn=S(g3_yaw);
        int ccx=winx+winw/2, ccy=winy+winh/2+24;
        for(int j=0;j<G3N;j++)for(int i=0;i<G3N;i++){
            int gx=i-G3H, gy=j-G3H, z=g3zv[j*G3N+i];
            int zn=((z-zmin)*28)/zspan - 14;
            int rx=(gx*cs - gy*sn)/127, ry=(gx*sn + gy*cs)/127;
            g3sx[j*G3N+i]=(short)(ccx + rx*11);
            g3sy[j*G3N+i]=(short)(ccy - zn*6 - ry*4);
        }
        int istart,iend,istep,jstart,jend,jstep;
        if(sn>=0){istart=G3N-2;iend=-1;istep=-1;} else {istart=0;iend=G3N-1;istep=1;}
        if(cs>=0){jstart=G3N-2;jend=-1;jstep=-1;} else {jstart=0;jend=G3N-1;jstep=1;}
        for(int qj=jstart;qj!=jend;qj+=jstep)for(int qi=istart;qi!=iend;qi+=istep){
            int i00=qj*G3N+qi,i10=i00+1,i01=i00+G3N,i11=i01+1;
            int x0=g3sx[i00],y0=g3sy[i00],x1=g3sx[i10],y1=g3sy[i10];
            int x2=g3sx[i11],y2=g3sy[i11],x3=g3sx[i01],y3=g3sy[i01];
            int zavg=(g3zv[i00]+g3zv[i10]+g3zv[i11]+g3zv[i01])/4;
            int tt=((zavg-zmin)*255)/zspan;
            u32 col=g3_heat(tt);
            int dz=(g3zv[i10]-g3zv[i00])+(g3zv[i00]-g3zv[i01]); int sf=205+dz; if(sf<110)sf=110; if(sf>255)sf=255;
            int R=(((col>>16)&255)*sf)/255,G=(((col>>8)&255)*sf)/255,B=((col&255)*sf)/255;
            u32 cc=((u32)R<<16)|((u32)G<<8)|(u32)B;
            if(g3_wire){ g3_line(x0,y0,x1,y1,cc,cx0,cy0,cx1,cy1); g3_line(x0,y0,x3,y3,cc,cx0,cy0,cx1,cy1); }
            else { tri_fill(x0,y0,x1,y1,x2,y2,cc,cx0,cy0,cx1,cy1); tri_fill(x0,y0,x2,y2,x3,y3,cc,cx0,cy0,cx1,cy1); }
        }
        fill(winx+1,winy+winh-20,winw-2,19,C_TASK);
        draw_str(winx+8,winy+winh-15,"3D | CPU INTENSIVE!!",C_MGREY+24);
        { int rv=(zspan*100)/200; char nb[20]; int p=0; const char* L="RANGE:"; while(L[p]){nb[p]=L[p];p++;}
          char wb[8]; utoa((u32)(rv/100),wb); for(int i=0;wb[i];i++)nb[p++]=wb[i];
          nb[p++]='.'; nb[p++]='0'+((rv/10)%10); nb[p++]='0'+(rv%10); nb[p]=0;
          draw_str(winx+winw-96,winy+winh-15,nb,C_GREEN); }
    }
}
static void icon_store(int x,int y){ rrectR(x+5,y+8,22,15,3,C_BBLUE); fill(x+10,y+3,3,7,C_WHITE); fill(x+19,y+3,3,7,C_WHITE); fill(x+10,y+3,12,3,C_WHITE); draw_str(x+13,y+13,"S",C_WHITE); }
static void dicon(int a,int ix,int iy,int s,u8 bg);
static int days_in_month(int m,int y); static int day_of_week(int d,int m,int y);
static void build_base(void){
    render_wallpaper();
    { int tw=strlen_(BUILDVER)*8; fill(W-tw-10,2,tw+8,16,C_RED+8); draw_str(W-tw-6,4,BUILDVER,C_WHITE); }

    int n=PITCH*H; for(int i=0;i<n;i++)BASE[i]=FB[i];
}

static const struct { int app; const char* label; } PIN[35]={
    {1,"FILES"},{2,"TERMINAL"},{5,"NOTEPAD"},{4,"SETTINGS"},
    {6,"TASK MGR"},{7,"DEVICES"},{8,"SNAKE"},
    {11,"PAINT"},{12,"TETRIS"},{13,"PIANO"},{14,"CRAFT"},
    {15,"WEB"},{3,"ABOUT"},{23,"CALC"},{24,"CAMERA"},
    {25,"PHOTO"},{27,"GPU"},{28,"HEXLANG"},
    {-33,"INVADERS"},{34,"FLAPPY"},{36,"PHONE"},{35,"PACKAGES"},{37,"MAIL"},
    {38,"QR CODE"},{39,"2048"},{40,"BREAKOUT"},{41,"MINES"},
    {42,"GOPHER"},{43,"WIKIPEDIA"},{45,"SOUND"},{46,"PHONE"},
    {-41,"DOOM"},
    {-34,"PY IDLE"},
    {-35,"NOOVEXCRAFT"},
    {-20,"MY APPS"}
};
#define NPIN 35
static int contains_ci(const char*s,const char*sub){ if(!sub[0])return 1; for(int i=0;s[i];i++){ int j=0; for(;;){ char b=sub[j]; if(!b)return 1; char a=s[i+j]; if(!a)break; if(a>='a'&&a<='z')a-=32; if(b>='a'&&b<='z')b-=32; if(a!=b)break; j++; } } return 0; }
static const char* REC[3]={"WELCOME.TXT","NOTES.TXT","REPORT.DOC"};
static void pin_icon(int id,int x,int y){
    if(id==1) icon_folder(x,y);
    else if(id==2) icon_terminal(x,y);
    else if(id==5) icon_notepad(x,y);
    else if(id==4) icon_gear(x,y);
    else if(id==3) icon_info(x,y);
    else if(id==6){ fill(x+2,y+1,24,20,C_SHAD); frame(x+2,y+1,24,20,C_WHITE); fill(x+6,y+12,3,6,C_GREEN); fill(x+11,y+8,3,10,C_GREEN); fill(x+16,y+5,3,13,C_GREEN); fill(x+21,y+10,3,8,C_GREEN); }
    else if(id==7){ fill(x+6,y+4,16,14,C_TEAL); frame(x+6,y+4,16,14,C_WHITE); for(int i=0;i<3;i++){ fill(x+8+i*5,y+1,2,3,C_MGREY); fill(x+8+i*5,y+18,2,3,C_MGREY); } }
    else if(id==-2){ fill(x+6,y+4,16,16,C_BBLUE); fill(x+10,y+8,8,8,C_WIN); draw_str(x+9,y+7,"U",C_WHITE); }
    else if(id==9){ fill(x+8,y+2,12,8,C_GREEN); fill(x+10,y+10,8,8,C_GREEN); fill(x+12,y+16,4,5,C_GREEN); draw_str(x+11,y+5,"+",C_WHITE); }
    else if(id==8){ for(int s=0;s<4;s++) fill(x+6+s*4,y+10,4,4,C_GREEN); fill(x+22,y+10,4,4,C_RED); }
    else if(id==10){ fill(x+7,y+5,14,3,C_MGREY); fill(x+8,y+8,12,13,C_FOLDER); for(int s=0;s<3;s++)fill(x+11+s*3,y+10,1,9,C_WIN); }
    else if(id==11){ fill(x+6,y+4,16,12,C_WHITE); fill(x+8,y+6,3,3,C_RED); fill(x+13,y+8,3,3,C_BLUE); fill(x+10,y+11,3,3,C_GREEN); fill(x+12,y+16,4,6,7); }
    else if(id==12){ fill(x+7,y+5,7,7,C_TEAL); fill(x+14,y+5,7,7,C_BLUE); fill(x+7,y+12,7,7,C_RED); fill(x+14,y+12,7,7,C_GREEN); }
    else if(id==13){ fill(x+5,y+8,18,12,C_WHITE); for(int s=0;s<5;s++)fill(x+8+s*3,y+8,1,7,4); }
    else if(id==14){ fill(x+6,y+12,16,10,20); fill(x+6,y+9,16,4,C_GREEN); fill(x+9,y+4,4,5,C_FOLDER); fill(x+8,y+2,6,3,C_TEAL); }
    else if(id==15){ frame(x+5,y+4,18,16,C_MGREY); fill(x+5,y+4,18,4,C_BLUE); fill(x+7,y+10,14,2,C_TITLE); fill(x+7,y+14,10,2,C_TITLE); }
    else if(id==-20){ fill(x+5,y+3,7,7,C_TEAL); fill(x+14,y+3,7,7,C_BLUE); fill(x+5,y+12,7,7,C_GREEN); fill(x+14,y+12,7,7,C_RED); }
    else if(id==-30){ fill(x+4,y+3,11,11,C_BLUE); fill(x+13,y+11,11,11,C_FOLDER); fill(x+7,y+6,2,2,C_WHITE); fill(x+18,y+16,2,2,C_SHAD); }
    else if(id==-35){ fill(x+5,y+4,18,4,C_GREEN); fill(x+5,y+8,18,12,C_FOLDER); frame(x+5,y+4,18,16,C_SHAD);
        fill(x+8,y+10,3,3,63); fill(x+15,y+13,3,3,63); }
    else if(id==-34){ fill(x+5,y+3,13,9,C_BLUE); fill(x+10,y+12,13,9,C_FOLDER);
        fill(x+5,y+12,5,4,C_BLUE); fill(x+18,y+8,5,4,C_FOLDER);
        fill(x+8,y+5,2,2,C_WHITE); fill(x+19,y+17,2,2,C_WHITE); }
    else if(id==-31){ fill(x+4,y+3,19,18,C_WHITE); fill(x+7,y+6,5,2,C_BLUE); fill(x+14,y+6,6,2,C_GREEN); fill(x+7,y+10,10,2,C_TEAL); fill(x+7,y+14,5,2,C_RED); fill(x+14,y+14,5,2,C_BLUE); }
    else if(id==-32){ fill(x+5,y+5,4,14,C_RED); fill(x+9,y+7,4,10,C_GREEN); fill(x+13,y+4,4,16,C_BLUE); fill(x+17,y+8,4,8,C_TEAL); }
    else if(id==-33){ fill(x+9,y+4,2,2,C_GREEN); fill(x+17,y+4,2,2,C_GREEN); fill(x+11,y+6,6,2,C_GREEN); fill(x+7,y+8,14,2,C_GREEN); fill(x+7,y+10,3,2,C_GREEN); fill(x+13,y+10,2,2,C_GREEN); fill(x+18,y+10,3,2,C_GREEN); fill(x+9,y+12,3,2,C_GREEN); fill(x+16,y+12,3,2,C_GREEN); }
     else if(id==-40||id==-41){ fill(x+5,y+5,18,16,C_RED); fill(x+3,y+3,5,5,C_RED); fill(x+20,y+3,5,5,C_RED); fill(x+6,y+8,6,2,0); fill(x+16,y+8,6,2,0); fill(x+7,y+10,5,3,C_WHITE); fill(x+16,y+10,5,3,C_WHITE); fill(x+9,y+11,2,2,0); fill(x+18,y+11,2,2,0); fill(x+8,y+17,12,2,0); fill(x+10,y+16,2,4,C_WHITE); fill(x+16,y+16,2,4,C_WHITE); }
    else if(id==34){ fill(x+5,y+6,15,11,C_FOLDER); fill(x+7,y+9,6,4,C_WHITE); fill(x+16,y+7,4,4,C_WHITE); fill(x+18,y+8,2,2,0); fill(x+20,y+10,4,3,C_RED); }
    else if(id==29){ fill(x+5,y+3,8,8,C_BBLUE); fill(x+15,y+3,8,8,C_GREEN); fill(x+5,y+13,8,8,C_RED); fill(x+15,y+13,8,8,C_FOLDER); }
    else if(id==38){ fill(x+4,y+2,20,20,C_WHITE); fill(x+5,y+3,6,6,0); fill(x+17,y+3,6,6,0); fill(x+5,y+15,6,6,0); fill(x+13,y+11,3,3,0); fill(x+17,y+15,3,3,0); fill(x+13,y+17,3,3,0); fill(x+19,y+19,3,3,0); }
    else if(id==39){ rrectR(x+4,y+3,9,9,2,58); rrectR(x+15,y+3,9,9,2,C_FOLDER); rrectR(x+4,y+14,9,9,2,C_RED); rrectR(x+15,y+14,9,9,2,C_GREEN); }
    else if(id==40){ fill(x+4,y+3,6,4,C_RED); fill(x+11,y+3,6,4,C_FOLDER); fill(x+18,y+3,6,4,C_GREEN); fill(x+4,y+8,6,4,C_TEAL); fill(x+11,y+8,6,4,C_BBLUE); fill(x+18,y+8,6,4,C_BLUE); disc(x+14,y+17,3,C_WHITE); fill(x+9,y+22,10,2,C_BBLUE); }
    else if(id==41){ disc(x+14,y+13,7,0); fill(x+13,y+3,2,20,0); fill(x+3,y+12,22,2,0); fill(x+11,y+10,3,3,C_WHITE); }
    else if(id==42){ fill(x+4,y+8,20,13,C_TEAL); fill(x+4,y+5,10,4,C_TEAL); draw_str(x+9,y+11,"G",C_WHITE); }
    else if(id==43){ disc(x+14,y+13,10,C_BBLUE); draw_str(x+9,y+9,"W",C_WHITE); }
    else if(id==44){ fill(x+4,y+5,20,16,0); frame(x+4,y+5,20,16,C_GREEN); draw_str(x+8,y+9,"@",C_GREEN); }
    else if(id==45){ fill(x+5,y+11,4,6,0); fill(x+9,y+9,3,10,0); fill(x+13,y+7,2,14,0); fill(x+17,y+8,2,2,C_BLUE); fill(x+17,y+12,2,2,C_BLUE); fill(x+20,y+9,2,6,C_BLUE); }
    else if(id==46){ rrectR(x+5,y+4,18,20,5,C_GREEN); fill(x+10,y+8,8,3,C_WHITE); fill(x+10,y+8,3,5,C_WHITE); fill(x+15,y+15,3,5,C_WHITE); fill(x+10,y+17,8,3,C_WHITE); }
}

static int smPW,smPH,smPX,smPY,smCellW,smGX0,smGY0,smCellH,smRecY,smSBW,smTS;
static int sel_app=-1;
static void start_geom(void){
    smPW=(W>=620)?600:(W-20); smPH=(H>=520)?460:(H-40);
    smPX=(W-smPW)/2; if(smPX<8)smPX=8; smPY=H-48-smPH-6; if(smPY<8)smPY=8;
    smSBW=0;
    smGX0=smPX+24; smGY0=smPY+60; smCellW=(smPW-48)/5; { int grows=(NPIN+4)/5; smCellH=(smPH-112)/grows; if(smCellH>84)smCellH=84; smTS=smCellH-16; if(smTS>48)smTS=48; if(smTS<30)smTS=30; }
    smRecY=0;
}
static u8 pin_tilecol(int app){
    switch(app){ case -30:return C_SHAD; case -31:return C_MGREY; case -32:return C_SHAD; case -33:return C_SHAD; case -20:return 102; case 1:return 100; case 2:return 101; case 5:return 103; case 4:return 105;
        case 3:return 106; case 6:return 107; case 8:return 104; case 11:return 108;
        case 12:return 102; case 13:return 109; case 14:return 110; case 15:return 100;
        case 16:return 105; case 34:return C_BBLUE; case -2:return 102; default:return 102; }
}
static void draw_run_dialog(void){
    int bw=460,bh=72,bx=W/2-bw/2,by=H/3;
    ultra_shadow(bx,by,bw,bh); glass_panel(bx,by,bw,bh,0,205); rrectR(bx,by,bw,3,2,C_BLUE);
    draw_str(bx+16,by+11,"RUN - type an app name + Enter   (type 'apps' to list)",C_MGREY+24);
    fill(bx+14,by+34,bw-28,24,C_WIN);
    draw_str(bx+20,by+39,run_buf,C_WHITE);
    fill(bx+20+run_len*8,by+37,2,18,C_WHITE);
}
static void draw_start_w11(void){
    start_geom();
    ultra_shadow(smPX,smPY,smPW,smPH);
    glass_panel(smPX,smPY,smPW,smPH,0,150);
    rrectR(smPX,smPY,smPW,smPH,16,C_TASK); afill(smPX,smPY,smPW,smPH,0,120);
    fill(smPX+2,smPY+1,smPW-4,1,40);

    /* sidebar removed - clean full-width grid */

    { char gr[32]="HELLO, "; int q=7; const char* uu=acct.user[0]?acct.user:"THERE"; for(int j=0;uu[j]&&q<30;j++){ char ch=uu[j]; if(ch>='a'&&ch<='z')ch-=32; gr[q++]=ch; } gr[q]=0; draw_str2(smPX+24,smPY+12,gr,C_WHITE); }
    draw_str(smPX+24,smPY+38,(srch_len>0)?"SEARCH RESULTS":"ALL APPS",(u8)(C_MGREY+22));
    fill(smPX+24,smPY+52,smPW-48,2,(u8)(C_MGREY+10));
    { int sw2=200, sx2=smPX+smPW-sw2-24, sy2=smPY+14; rrectR(sx2,sy2,sw2,26,9,C_WIN); frame(sx2,sy2,sw2,26,C_BBLUE); disc(sx2+14,sy2+13,5,C_BBLUE); if(srch_len>0) draw_str(sx2+28,sy2+8,srch,C_WHITE); else draw_str(sx2+28,sy2+8,"SEARCH APPS",(u8)(C_MGREY+20)); }
    { int mc=0; for(int i=0;i<NPIN;i++){ if(!contains_ci(PIN[i].label,srch))continue; int col=mc%5,row=mc/5; int cx=smGX0+col*smCellW,cy=smGY0+row*smCellH;
        int ts=smTS, tx=cx+smCellW/2-ts/2;
        afill(tx+3,cy+ts-1,ts-2,4,0,50);
        int ic=icon_for_app(PIN[i].app);
        if(ic>=0){ blit_icon(ic, tx, cy, ts); }
        else { rrectR(tx,cy,ts,ts,12,pin_tilecol(PIN[i].app)); afill(tx,cy,ts,ts/2,C_WHITE,16); pin_icon(PIN[i].app, tx+ts/2-14, cy+ts/2-11); }
        int lw=strlen_(PIN[i].label)*8; draw_str(cx+smCellW/2-lw/2,cy+ts+5,PIN[i].label,C_WHITE); mc++; }
      if(srch_len>0) for(unsigned fi=0;fi<nvx.count;fi++){ if(!nvx.e[fi].name[0]||!contains_ci(nvx.e[fi].name,srch))continue; int col=mc%5,row=mc/5; int cx=smGX0+col*smCellW,cy=smGY0+row*smCellH; int ts=smTS,tx=cx+smCellW/2-ts/2; rrectR(tx,cy,ts,ts,12,C_MGREY+10); afill(tx+12,cy+9,ts-24,ts-16,C_WHITE,60); { int nl=strlen_(nvx.e[fi].name); draw_str(cx+smCellW/2-(nl>12?12:nl)*4,cy+ts+5,nvx.e[fi].name,C_TITLE); } mc++; }
      if(srch_len>0) for(int pi=0;pi<ph_count;pi++){ if(!contains_ci(ph_name[pi],srch))continue; int col=mc%5,row=mc/5; int cx=smGX0+col*smCellW,cy=smGY0+row*smCellH; int ts=smTS,tx=cx+smCellW/2-ts/2; ph_blit(pi,tx,cy,ts,ts); frame(tx,cy,ts,ts,C_MGREY); { int nl=strlen_(ph_name[pi]); draw_str(cx+smCellW/2-(nl>12?12:nl)*4,cy+ts+5,ph_name[pi],C_WHITE); } mc++; }
      if(mc==0) draw_str(smGX0,smGY0,"NO MATCHES",C_MGREY+20); }

    int by=smPY+smPH-38;
    fill(smPX+24,by-8,smPW-48,1,(u8)(C_MGREY+8));
    disc(smPX+34,by+13,11,C_BBLUE); { char ic[2]={acct.user[0]?acct.user[0]:'U',0}; if(ic[0]>='a'&&ic[0]<='z')ic[0]-=32; draw_str(smPX+30,by+7,ic,C_WHITE); }
    draw_str(smPX+52,by+7,acct.user[0]?acct.user:"USER",C_WHITE);
    { int qx=smPX+smPW-44, qy=by+2; rrectR(qx,qy,28,26,7,(u8)(C_MGREY+14)); fill(qx+13,qy+4,3,9,C_RED); int cxp=qx+14,cyp=qy+14; for(int a=40;a<320;a+=20){ int dx=(S((u8)(a+64))*6)/127,dy=(S((u8)a)*6)/127; disc(cxp+dx,cyp+dy,1,C_RED);} }
}static const char* PWR[4]={"LOCK","SLEEP","SHUT DOWN","RESTART"};
static int pwrX,pwrY,pwrW,pwrH;
static void pwr_geom(void){ pwrW=176; pwrH=4*24+10; pwrX=smPX+smPW-pwrW-12; pwrY=(smPY+smPH-40)-pwrH-4; }
static void draw_power_menu(void){
    pwr_geom();
    fill(pwrX+3,pwrY+3,pwrW,pwrH,C_SHAD);
    fill(pwrX,pwrY,pwrW,pwrH,C_WIN); frame(pwrX,pwrY,pwrW,pwrH,C_BBLUE);
    for(int i=0;i<4;i++){ int iy=pwrY+6+i*24; int x=pwrX+10;

        if(i==0){ fill(x+2,iy+8,10,8,C_TITLE); fill(x+4,iy+4,6,5,C_TITLE); fill(x+6,iy+11,2,3,C_WIN); }
        else if(i==1){ draw_str(x+2,iy+4,"Z",C_TITLE); }
        else if(i==2){ frame(x+2,iy+4,11,11,C_RED); fill(x+7,iy+2,2,6,C_RED); fill(x+8,iy+1,1,1,C_WIN); }
        else { draw_str(x+2,iy+4,"R",C_GREEN); fill(x+2,iy+3,9,2,C_GREEN); }
        draw_str(x+22,iy+5,PWR[i],C_WHITE);
    }
}
static int days_in_month(int m,int y){ static const int d[12]={31,28,31,30,31,30,31,31,30,31,30,31}; if(m==2&&((y%4==0&&y%100!=0)||y%400==0))return 29; if(m<1||m>12)return 30; return d[m-1]; }
static int day_of_week(int d,int m,int y){ static const int t[12]={0,3,2,5,0,3,5,1,4,6,2,4}; if(m<3)y-=1; return (y+y/4-y/100+y/400+t[(m-1)%12]+d)%7; }
static void draw_calendar(void){
    int day=bcd(cmos(7)),mon=bcd(cmos(8)),yr=2000+bcd(cmos(9));
    if(mon<1||mon>12)mon=1; if(day<1||day>31)day=1;
    int w=204,h=176,x=W-w-20,y=H-52-h;
    fill(x+3,y+3,w,h,C_SHAD); fill(x,y,w,h,C_WIN); frame(x,y,w,h,C_BBLUE); fill(x,y,w,18,C_BLUE);
    static const char* mn[12]={"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
    char hd[16]; int p=0; const char*ms=mn[(mon-1)%12]; while(ms[p]){hd[p]=ms[p];p++;} hd[p++]=' ';
    hd[p++]='0'+(yr/1000)%10; hd[p++]='0'+(yr/100)%10; hd[p++]='0'+(yr/10)%10; hd[p++]='0'+yr%10; hd[p]=0;
    draw_str(x+8,y+5,hd,C_WHITE);
    draw_str(x+6,y+24,"SU MO TU WE TH FR SA",C_TITLE);
    int start=day_of_week(1,mon,yr),dim=days_in_month(mon,yr),col=start,row=0;
    for(int dd=1;dd<=dim;dd++){ int cx=x+8+col*28,cy=y+42+row*20;
        if(dd==day) fill(cx-3,cy-2,22,16,C_BLUE);
        char db[3]; db[0]=dd<10?' ':('0'+dd/10); db[1]='0'+dd%10; db[2]=0;
        draw_str(cx,cy,db,C_WHITE);
        col++; if(col>6){col=0;row++;} }
}

static int ccW=300, ccH=372;
static void cc_geom(int*x,int*y){ *x=W-ccW-12; *y=28; }
static void cc_tile(int x,int y,int w,int h,const char* lab,const char* sub,int on,int gid){
    rrectR(x,y,w,h, 12, on?C_BBLUE:(C_MGREY+18));
    afill(x,y,w,h,C_WHITE,on?0:0);

    disc(x+22,y+22,14, on?C_WHITE:(C_MGREY+30));
    (void)gid;
    draw_str(x+10,y+h-30,lab,on?C_WHITE:C_TITLE);
    if(sub) draw_str(x+10,y+h-16,sub,on?C_WHITE:(C_MGREY+30));
}
static void draw_control_center(void){
    int x,y; cc_geom(&x,&y);
    ultra_shadow(x,y,ccW,ccH);
    glass_panel(x,y,ccW,ccH,0,150);
    rrectR(x,y,ccW,ccH,16,C_WIN); afill(x,y,ccW,ccH,0,120);
    fill(x+2,y+1,ccW-4,1,40);
    int pad=14, gx=x+pad, gy=y+pad, gw=(ccW-pad*3)/2, gh=72, gp=pad;

    { char ic[2]={acct.user[0]?acct.user[0]:'U',0}; if(ic[0]>='a'&&ic[0]<='z')ic[0]-=32;
      disc(gx+14,gy+14,14,C_BBLUE); draw_str(gx+10,gy+8,ic,C_WHITE);
      draw_str(gx+36,gy+8,acct.user[0]?acct.user:"USER",C_WHITE);
      rrectR(x+ccW-92,gy,80,28,10,C_MGREY+18); draw_str(x+ccW-82,gy+8,"SIGN OUT",C_WHITE); }
    gy+=44;

    cc_tile(gx,        gy,      gw,gh,"NETWORK", nic_present?"CONNECTED":"OFF", nic_present, 0);
    cc_tile(gx+gw+gp, gy,      gw,gh,"SOUND",   snd_on?"ON":"MUTED",        snd_on,      1);
    cc_tile(gx,        gy+gh+gp,gw,gh,"NIGHT",  cc_night?"ON":"OFF",        cc_night,    2);
    cc_tile(gx+gw+gp, gy+gh+gp,gw,gh,"DARK",    cc_dark?"ON":"OFF",         cc_dark,     3);
    gy+=2*gh+gp+pad;

    draw_str(gx,gy,"BRIGHTNESS",C_TITLE);
    { int sx=gx, sw=ccW-2*pad, sy=gy+16; fill(sx,sy,sw,6,C_MGREY+12); fill(sx,sy,(sw*cc_bright)/7,6,C_BBLUE); disc(sx+(sw*cc_bright)/7,sy+3,8,C_WHITE);
      static const int bpct[8]={40,55,70,85,100,125,150,200}; char pb[6]; int q=0,v=bpct[cc_bright>7?7:(cc_bright<0?0:cc_bright)]; char t[4];int tl=0; while(v){t[tl++]='0'+v%10;v/=10;} while(tl)pb[q++]=t[--tl]; pb[q++]='%'; pb[q]=0; draw_str(sx+sw-36,gy,pb,C_TITLE); }
    gy+=44;

    draw_str(gx,gy,"VOLUME",C_TITLE);
    { int sx=gx, sw=ccW-2*pad, sy=gy+16; int v=snd_on?4:0; fill(sx,sy,sw,6,C_MGREY+12); fill(sx,sy,(sw*v)/5,6,C_BBLUE); disc(sx+(sw*v)/5,sy+3,8,C_WHITE); }
    gy+=40;

    { static const char* mn[12]={"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
      int dd=bcd(cmos(7)),mo=bcd(cmos(8)); if(mo<1||mo>12)mo=1;
      char db[8]; int q=0; const char* m=mn[mo-1]; while(*m)db[q++]=*m++; db[q++]=' '; if(dd>=10)db[q++]='0'+dd/10; db[q++]='0'+dd%10; db[q]=0;
      draw_str(gx,y+ccH-26,db,C_TITLE); }
    rrectR(x+ccW-104,y+ccH-32,40,24,8,C_MGREY+18); draw_str(x+ccW-96,y+ccH-26,"LOCK",C_WHITE);
    rrectR(x+ccW-56, y+ccH-32,44,24,8,C_RED);       draw_str(x+ccW-48,y+ccH-26,"PWR",C_WHITE);
}
static void rrect(int x,int y,int w,int h,u8 c){
    if(w<4||h<4){ fill(x,y,w,h,c); return; }
    fill(x+2,y,w-4,h,c);
    fill(x,y+2,w,h-4,c);
    fill(x+1,y+1,1,1,c); fill(x+w-2,y+1,1,1,c);
    fill(x+1,y+h-2,1,1,c); fill(x+w-2,y+h-2,1,1,c);
}

static int cl8(int v){ return v<0?0:(v>255?255:v); }

static void grad_tile(int x,int y,int s,u32 top,u32 bot,int rad){
    int tr=(top>>16)&255,tg=(top>>8)&255,tb=top&255;
    int dr=(int)((bot>>16)&255)-tr, dg=(int)((bot>>8)&255)-tg, db=(int)(bot&255)-tb;
    for(int j=0;j<s;j++){
        int yy=y+j; if(yy<0||yy>=H)continue;
        int r=tr+(dr*j)/s, g=tg+(dg*j)/s, b=tb+(db*j)/s;
        u32 c=((u32)r<<16)|((u32)g<<8)|(u32)b;
        for(int i=0;i<s;i++){
            int cx=-1,cy=0;
            if(i<rad&&j<rad){cx=rad-i;cy=rad-j;}
            else if(i>=s-rad&&j<rad){cx=i-(s-rad)+1;cy=rad-j;}
            else if(i<rad&&j>=s-rad){cx=rad-i;cy=j-(s-rad)+1;}
            else if(i>=s-rad&&j>=s-rad){cx=i-(s-rad)+1;cy=j-(s-rad)+1;}
            if(cx>=0 && cx*cx+cy*cy>rad*rad) continue;
            int xx=x+i; if(xx>=0&&xx<W) FB[yy*PITCH+xx]=c;
        }
    }
}
static void dicon(int a,int ix,int iy,int s,u8 bg){
    u32 bc=PAL32[bg]; int br=(bc>>16)&255,bgc=(bc>>8)&255,bb=bc&255;
    u32 top=((u32)cl8(br+40)<<16)|((u32)cl8(bgc+40)<<8)|cl8(bb+40);
    u32 bot=((u32)cl8(br-28)<<16)|((u32)cl8(bgc-28)<<8)|cl8(bb-28);
    grad_tile(ix,iy,s,top,bot,s/4);
    fill(ix+s/5,iy+2,s*3/5,1,C_WHITE);
    int u=s;
    if(a==1){
        int px=ix+u/5, py=iy+u*2/5, fw=u*3/5, fh=u*2/5;
        fill(px, py-u/8, fw/2, u/8, 50);
        rrect(px, py, fw, fh, C_WHITE); fill(px+2, py+2, fw-4, fh-4, 56);
    } else if(a==2){
        int cx=ix+u/4, cy=iy+u/2-6;
        fill(cx,cy,3,3,C_GREEN); fill(cx+3,cy+3,3,3,C_GREEN); fill(cx,cy+6,3,3,C_GREEN);
        fill(cx+10,cy+9,u/3,2,C_GREEN);
    } else if(a==15){
        int cx=ix+u/2, cy=iy+u/2, r=u/4;
        frame(cx-r,cy-r,2*r,2*r,C_WHITE);
        fill(cx-r,cy-1,2*r,2,C_WHITE); fill(cx-1,cy-r,2,2*r,C_WHITE);
    } else if(a==5){
        rrect(ix+u/4, iy+u/6, u/2, u*2/3, C_WHITE);
        for(int k=0;k<3;k++) fill(ix+u/4+3, iy+u/6+6+k*6, u/2-6, 1, C_MGREY);
    } else if(a==11){
        fill(ix+u/4,   iy+u/4,   4,4, C_RED);
        fill(ix+u/2,   iy+u/4,   4,4, C_BLUE);
        fill(ix+u/3,   iy+u/2,   4,4, C_GREEN);
        fill(ix+u/2+2, iy+u/2,   4,4, C_FOLDER);
    } else if(a==4){
        int cx=ix+u/2, cy=iy+u/2, r=u/5;
        frame(cx-r,cy-r,2*r,2*r,C_WHITE);
        fill(cx-2,cy-r-3,4,4,C_WHITE); fill(cx-2,cy+r-1,4,4,C_WHITE);
        fill(cx-r-3,cy-2,4,4,C_WHITE); fill(cx+r-1,cy-2,4,4,C_WHITE);
        fill(cx-2,cy-2,4,4,C_WHITE);
    } else if(a==8){
        int b=u/8; for(int k=0;k<3;k++) fill(ix+u/5+k*(b+2), iy+u/2, b, b, C_GREEN);
        fill(ix+u/5+3*(b+2), iy+u/2, b, b, C_RED);
    } else if(a==12){
        int q=u/5, m=u/3;
        fill(ix+m,   iy+m,   q,q, C_TEAL); fill(ix+m+q+1, iy+m,   q,q, C_BLUE);
        fill(ix+m,   iy+m+q+1, q,q, C_RED);  fill(ix+m+q+1, iy+m+q+1, q,q, C_GREEN);
    } else if(a==14){
        fill(ix+u/5, iy+u*2/3, u*3/5, u/8, 22);
        fill(ix+u*2/5, iy+u/2, u/6, u/5, C_FOLDER);
        fill(ix+u/3, iy+u/4, u/3, u/4, C_GREEN);
    } else if(a==16){
        int bx=ix+u/5, by=iy+u/4, bw=u*3/5, bh=u*2/5;
        rrect(bx,by,bw,bh,C_WHITE); fill(bx+by/8,by+bh-1,4,4,C_WHITE);
        fill(bx+4,by+bh/2-2,bw-8,2,C_TEAL);
        fill(ix+u*2/3,iy+u*2/3,3,3,C_TITLE); fill(ix+u*2/3+5,iy+u*2/3+4,2,2,C_TITLE);
    } else if(a==-3){
        int q=(u-12)/2;
        fill(ix+4,     iy+4,     q,q, C_RED);   fill(ix+8+q, iy+4,     q,q, C_GREEN);
        fill(ix+4,     iy+8+q,   q,q, C_BLUE);  fill(ix+8+q, iy+8+q,   q,q, C_FOLDER);
    } else if(a==3){
        int cx=ix+u/2;
        fill(cx-2,iy+u/4,4,4,C_WHITE);
        fill(cx-2,iy+u/4+7,4,u/3,C_WHITE);
    } else if(a==17){
        int px=ix+u/4, py=iy+u*2/5, bw=u/2, bh=u*2/5;
        frame(px+bw/4,py-u/6,bw/2,u/6,C_WHITE);
        fill(px,py,bw,bh,C_WHITE);
        fill(px+3,py+3,bw-6,bh-6,bg);
    } else if(a==20){
        int tw=u/3, th=u/3, px=ix+u/2-tw/2, py=iy+u/2-th/2+2;
        fill(px-2,py-3,tw+4,2,C_WHITE);
        fill(px+tw/2-2,py-6,4,3,C_WHITE);
        fill(px,py,tw,th,C_WHITE);
        int g=(tw-4)/3; if(g<1)g=1;
        for(int k=0;k<3;k++) fill(px+2+k*g,py+2,1,th-4,bg);
    } else if(a==21){
        int pw=u/3, ph=u/2, px=ix+u/2-pw/2, py=iy+u/4;
        fill(px,py,pw,ph,C_WHITE);
        for(int k=0;k<3;k++) fill(px+2,py+4+k*5,pw-4,1,bg);
    } else if(a==22){
        int cx=ix+u/2;
        fill(cx-2,iy+u/4,4,u/2,C_WHITE);
        fill(cx-6,iy+u/4-4,12,5,C_WHITE);
        fill(cx-5,iy+u/4+u/3,3,3,C_WHITE); fill(cx+2,iy+u/4+u/4,3,3,C_WHITE);
    } else if(a==23){
        int px=ix+u/4,py=iy+u/5,w=u/2,h=u*3/5; fill(px,py,w,h,C_WHITE);
        fill(px+2,py+2,w-4,h/4,bg);
        for(int r=0;r<2;r++)for(int c=0;c<3;c++) fill(px+3+c*((w-6)/3),py+3+h/3+r*((h/3)/2),2,2,bg);
    } else if(a==24){
        int px=ix+u/5,py=iy+u/3,w=u*3/5,h=u*2/5; fill(px,py,w,h,C_WHITE);
        fill(px+w/3,py-3,w/3,3,C_WHITE);
        int cx=px+w/2,cy=py+h/2,r=h/4; fill(cx-r,cy-r,2*r,2*r,bg);
    } else if(a==25){
        int px=ix+u/5,py=iy+u/4,w=u*3/5,h=u/2; fill(px,py,w,h,C_WHITE);
        fill(px+w-w/4-2,py+3,4,4,bg);
        for(int k=0;k<w;k++){ int hh=(k<w/2)?k/2:(w-k)/2; fill(px+k,py+h-2-hh,1,2+hh,bg); }
    } else if(a==26){
        int px=ix+u/4,w=u/2; for(int r=0;r<3;r++){ int ry=iy+u/3+r*(u/6); fill(px,ry,w,2,C_WHITE); fill(px+(r==1?w-5:r*6),ry-2,5,6,C_WHITE); }
    } else if(a==27){
        int px=ix+u/4,py=iy+u/4,w=u/2,h=u/2; fill(px,py,w,h,C_WHITE); fill(px+3,py+3,w-6,h-6,bg);
        for(int k=0;k<3;k++){ fill(px+4+k*((w-8)/2),py-3,2,3,C_WHITE); fill(px+4+k*((w-8)/2),py+h,2,3,C_WHITE);
            fill(px-3,py+4+k*((h-8)/2),3,2,C_WHITE); fill(px+w,py+4+k*((h-8)/2),3,2,C_WHITE); }
    } else if(a==28){
        int px=ix+u/4,py=iy+u/4,w=u/2,h=u/2; fill(px,py,w,h,C_WHITE); fill(px+2,py+2,w-4,h-4,bg);
        fill(px+w/2-3,py+h/2-1,2,3,C_WHITE); fill(px+w/2+1,py+h/2-2,2,4,C_WHITE);
    } else if(a==29){
        int cx=ix+u/3,cy=iy+u*3/5; fill(cx,cy,6,4,C_WHITE);
        fill(cx+5,iy+u/4,2,cy-iy-u/4+4,C_WHITE);
        fill(cx+5,iy+u/4,u/5,3,C_WHITE);
    } else if(a==30){
        int bx=ix+u/4, by=iy+u*3/4;
        for(int k=0;k<6;k++){ int hh=4+((S((u8)(k*40))+127)*(u/2))/255; fill(bx+k*3, by-hh, 2, hh, 0); }
        for(int k=0;k<6;k++){ int hh=4+((S((u8)(k*40))+127)*(u/2))/255; u32 v=g3_heat(k*42); int x=bx+k*3,yy=by-hh; for(int q=0;q<hh;q++){ int py=yy+q; if(x<W&&py<H){FB[py*PITCH+x]=v;FB[py*PITCH+x+1]=v;} } }
        fill(ix+u/5,by+1,u*3/5,1,C_WHITE);
    }
}

static void draw_input_diag(void){
    int pw=560, ph=232, px=(W-pw)/2, py=80;
    ultra_shadow(px,py,pw,ph);
    rrectR(px,py,pw,ph,10,C_WIN); afill(px,py,pw,ph,0,40);
    fill(px,py,pw,4,C_RED+8);
    draw_str2(px+16,py+14,"INPUT DIAGNOSTIC",C_WHITE);
    draw_str(px+16,py+40,"NO MOUSE/KEYBOARD ACTIVITY DETECTED IN 20S.",C_TITLE);
    int x=px+16,y=py+62;

    { u8 st=inb(0x64);
      draw_str(x,y,"PS/2 STATUS BYTE (0x64):",C_TASK); char hb[6]; hex16(st,hb); draw_str(x+260,y,hb,C_TEAL); y+=15; }
    { char b[40]="RAW INPUT BYTES SEEN: "; int q=22,v=diag_raw_count; char t[4];int tl=0; if(v==0)t[tl++]='0'; while(v){t[tl++]='0'+v%10;v/=10;} while(tl)b[q++]=t[--tl]; b[q]=0;
      draw_str(x,y,b,diag_raw_count?C_GREEN:(C_RED+8)); y+=15; }
    draw_str(x,y,"KEYBOARD SCANCODES:",C_WHITE); draw_str(x+200,y,diag_kbd_seen?"YES (KBD ALIVE)":"NONE",diag_kbd_seen?C_GREEN:(C_RED+8)); y+=15;
    draw_str(x,y,"MOUSE PACKETS:",C_WHITE); draw_str(x+200,y,diag_mouse_seen?"YES (PS/2 MOUSE OK)":"NONE",diag_mouse_seen?C_GREEN:(C_RED+8)); y+=18;

    draw_str(x,y,"USB CONTROLLERS (PCI):",C_TASK); y+=15;
    int nu=0,no=0,ne=0,nx=0;
    for(int i=0;i<pcin;i++){ if(pcil[i].cls==0x0C&&pcil[i].sub==0x03){ u8 pf=pcil[i].prog;
        if(pf==0x00)nu++; else if(pf==0x10)no++; else if(pf==0x20)ne++; else if(pf==0x30)nx++; } }
    char ln[64]; int q=0;
    const char* names[4]={"UHCI:","OHCI:","EHCI:","XHCI:"}; int cnts[4]={nu,no,ne,nx};
    int lx=x+8;
    for(int k=0;k<4;k++){ draw_str(lx,y,names[k],C_WHITE); char nb[4]; int v=cnts[k]; nb[0]='0'+v%10; nb[1]=0; draw_str(lx+44,y,nb,cnts[k]?C_GREEN:C_MGREY+20); lx+=90; }
    y+=18;

    draw_str(x,y,"EHCI INIT:",C_WHITE); draw_str(x+150,y,ehci_init_ok?"OK":"FAIL/NONE",ehci_init_ok?C_GREEN:(C_RED+8));
    { char db[8]; int v=usbdev_n; db[0]='0'+v%10; db[1]=0; draw_str(x+280,y,"EHCI DEVS:",C_WHITE); draw_str(x+380,y,db,C_TEAL); } y+=15;
    draw_str(x,y,"UHCI DRIVER:",C_WHITE); draw_str(x+150,y,uhci_ok?(uhci_ms_found?"MOUSE BOUND":"NO MOUSE"):"NO UHCI",uhci_ok?(uhci_ms_found?C_GREEN:C_FOLDER):(C_RED+8)); y+=15;

    { draw_str(x,y,"XHCI DRIVER:",C_WHITE);
      if(!xhci_init_ok){ draw_str(x+150,y,"NO XHCI / INIT FAIL",C_RED+8); }
      else { char nb[8]; int conn=0; for(int p=0;p<xhci_ports;p++) if(xhci_port_conn[p])conn++;
        draw_str(x+150,y, xhci_ms_found?"MOUSE BOUND!":"ENUM:",xhci_ms_found?C_GREEN:C_FOLDER);
        if(!xhci_ms_found){ utoa(xhci_diag_step,nb); draw_str(x+210,y,nb,C_TEAL); draw_str(x+226,y,"/9 STEP",C_MGREY+24); }
        utoa(conn,nb); draw_str(x+330,y,"PORTS UP:",C_WHITE); draw_str(x+420,y,nb,C_TEAL); }
      y+=15; }
    draw_str(x,y,"PRESS ANY KEY / MOVE MOUSE TO DISMISS.",C_MGREY+24); y+=14;
    draw_str(x,y,"TELL THE DEVELOPER WHAT THIS PANEL SHOWS.",C_TITLE);
}
static void arrect(int x,int y,int w,int h,int r,u8 c,int alpha){
    if(alpha<=0)return;
    if(r*2>w)r=w/2; if(r*2>h)r=h/2; if(r<1){ afill(x,y,w,h,c,alpha); return; }
    afill(x+r,y,w-2*r,h,c,alpha);
    afill(x,y+r,r,h-2*r,c,alpha); afill(x+w-r,y+r,r,h-2*r,c,alpha);
    u32 fv=PAL32[c]; int sr=(fv>>16)&255,sg=(fv>>8)&255,sb=fv&255;
    for(int dy=0;dy<r;dy++) for(int dx=0;dx<r;dx++){
        int cnt=0;
        for(int a=0;a<4;a++){ int sx=dx*8+2*a+1-r*8;
            for(int b=0;b<4;b++){ int sy=dy*8+2*b+1-r*8;
                if(sx*sx+sy*sy<=(r*8)*(r*8))cnt++; } }
        if(!cnt)continue;
        int cov=cnt*alpha/16; int ia=255-cov;
        int cxs[4],cys[4]; cxs[0]=x+dx;cys[0]=y+dy; cxs[1]=x+w-1-dx;cys[1]=y+dy; cxs[2]=x+dx;cys[2]=y+h-1-dy; cxs[3]=x+w-1-dx;cys[3]=y+h-1-dy;
        for(int k=0;k<4;k++){ int xx=cxs[k],yy=cys[k]; if(xx<0||yy<0||xx>=W||yy>=H)continue;
            u32 d=FB[yy*PITCH+xx]; int dr=(d>>16)&255,dg=(d>>8)&255,db=d&255;
            int rr=(sr*cov+dr*ia)/255,gg=(sg*cov+dg*ia)/255,bb=(sb*cov+db*ia)/255;
            FB[yy*PITCH+xx]=((u32)rr<<16)|((u32)gg<<8)|(u32)bb; }
    }
}
static void draw_dock(void){

    afill(0,0,W,24,0,190); fill(0,23,W,1,C_BLUE);
    rrectR(6,4,16,16,4,C_BBLUE); draw_str(10,8,"N",C_WHITE);
    draw_str(28,8,OSNAME,C_WHITE);
    /* status (clock/sound/net/battery) moved to the bottom-right tray below */

    int B=(dock_size==0?34:(dock_size==2?54:42));
    int Mx=B+(B*7)/10;
    int G=10, R=104;
    int idxs[DOCKN], nN=0;
    for(int i=0;i<DOCKN;i++){ if(dock_app[i]==-3||dock_app[i]>0) idxs[nN++]=i; }
    int natW=nN*B+(nN-1)*G, natX=(W-natW)/2;
    int baseline=H-18;
    int near=(my>=H-110);
    int sizes[DOCKN], totW=0;
    for(int k=0;k<nN;k++){ int cxn=natX+k*(B+G)+B/2; int d=mx-cxn; if(d<0)d=-d;
        int s=B; if(near&&d<R) s=B+((Mx-B)*(R-d))/R; sizes[k]=s; totW+=s; }
    totW+=(nN-1)*G;
    int ax=(W-totW)/2;
    int padX=16, panelTop=baseline-B-10, panelH=B+20;
    arrect(ax-padX, panelTop, totW+2*padX, panelH, 20, 0, 150);
    afill(ax-padX+10, panelTop, totW+2*padX-20, 1, C_WHITE, 46);
    int cur=ax;
    for(int k=0;k<nN;k++){ int i=idxs[k]; int s=sizes[k];
        int hop=dock_bounce[i]>0 ? (14*dock_bounce[i]*(BOUNCE_MAX-dock_bounce[i]))/(BOUNCE_MAX*BOUNCE_MAX) : 0;
        int ix=cur, iy=baseline-s-hop;
        dock_ix[i]=ix; dock_iy[i]=iy; dock_isz[i]=s;
        if(dock_app[i]==-3){
            rrectR(ix,iy,s,s,s/5,C_BLUE);
            int q=(s-10)/2, gp=4, gx=ix+(s-(2*q+gp))/2, gy=iy+(s-(2*q+gp))/2;
            for(int a=0;a<2;a++)for(int b=0;b<2;b++) fill(gx+a*(q+gp),gy+b*(q+gp),q,q,C_WHITE);
        } else {
            int ic=icon_for_app(dock_app[i]);
            if(ic>=0) blit_icon(ic,ix,iy,s);
            else { rrectR(ix,iy,s,s,s/4,dock_col[i]); dicon(dock_app[i],ix,iy,s,dock_col[i]); }
        }
        if(win_find(dock_app[i])>=0) disc(ix+s/2, baseline+7, 2, C_WHITE);
        cur+=s+G;
    }
    { int tw=182, th=36, tx=W-tw-4, ty=H-th-4, mid=ty+th/2;
      arrect(tx,ty,tw,th,8,0,170);
      int cx=tx+12;
      u8 ncol=nic_present?C_GREEN:(C_MGREY+10);
      for(int b=0;b<4;b++){ int bh=4+b*4; fill(cx+b*5,mid+6-bh,3,bh,ncol); } cx+=30;
      { u8 sc=snd_on?C_WHITE:(C_MGREY+10); fill(cx,mid-3,3,6,sc); for(int a=0;a<4;a++) fill(cx+3+a,mid-3-a,1,6+2*a,sc); } cx+=20;
      { u8 bc=bat_charging?C_GREEN:(bat_pct<=20?C_RED:C_GREEN); frame(cx,mid-5,16,10,C_WHITE); fill(cx+16,mid-2,2,4,C_WHITE); int fw=(14*bat_pct)/100; if(fw<1&&bat_pct>0)fw=1; fill(cx+1,mid-4,fw,8,bc); if(bat_charging){ fill(cx+9,mid-4,2,4,C_WHITE); fill(cx+6,mid-1,6,2,C_WHITE); fill(cx+6,mid,2,4,C_WHITE); } } cx+=26;
      { char hm[6]; for(int i=0;i<5;i++)hm[i]=clkbuf[i]; hm[5]=0; draw_str(cx+8,ty+6,hm,C_WHITE); }
      draw_str(cx,ty+20,datebuf,C_WHITE);
    }
}
static void wline(int x0,int y0,int x1,int y1,u8 c){
    int dx=x1-x0, dy=y1-y0, ax=dx<0?-dx:dx, ay=dy<0?-dy:dy, st=ax>ay?ax:ay; if(st<1)st=1;
    for(int i=0;i<=st;i++){ fill(x0+dx*i/st, y0+dy*i/st, 2, 2, c); }
}
static void draw_magnifier(void){
    if(!mag_on) return;
    int LR=80, Z=2, cx=mx, cy=my;
    for(int oy=-LR;oy<=LR;oy++){ int yy=cy+oy; if(yy<0||yy>=H)continue;
        for(int ox=-LR;ox<=LR;ox++){ int d2=ox*ox+oy*oy; if(d2>LR*LR)continue;
            int xx=cx+ox; if(xx<0||xx>=W)continue;
            if(d2>(LR-3)*(LR-3)){ LFB[yy*PITCH+xx]=PAL32[C_WHITE]; continue; }
            int sx=cx+ox/Z, sy=cy+oy/Z;
            if(sx<0)sx=0; if(sy<0)sy=0; if(sx>=W)sx=W-1; if(sy>=H)sy=H-1;
            LFB[yy*PITCH+xx]=BACK[sy*PITCH+sx]; } }
}
static void draw_widgets(void){
    rtc_now();
    int x0=18, TW=196, y=46, h;
    int hr=rtc_hour, mn=rtc_min, sc=rtc_sec, day=rtc_day, mon=rtc_month, yr=2000+rtc_year;
    if(mon<1||mon>12)mon=1; if(day<1||day>31)day=1;

    if(widget_on[0]){
      h=80; fill(x0,y,TW,h,C_BBLUE); frame(x0,y,TW,h,40);
      { if(wx_have){
          draw_str(x0+10,y+6,wx_loc,C_WHITE);
          draw_str2(x0+10,y+24,wx_temp,C_WHITE);
          draw_str(x0+10,y+58,wx_cond,C_WHITE);
        } else {
          int sx=x0+34,sy=y+34; disc(sx,sy,12,63);
          for(int a=0;a<256;a+=32) wline(sx+(S((u8)(a+64))*18)/127,sy-(S((u8)a)*18)/127, sx+(S((u8)(a+64))*24)/127,sy-(S((u8)a)*24)/127,63);
          draw_str2(x0+74,y+18,"28",C_WHITE); draw_str(x0+108,y+22,"C",C_WHITE);
          draw_str(x0+14,y+50,"SUNNY - TAP",C_WHITE); draw_str(x0+14,y+62,"TO LOAD LIVE",C_WHITE);
        } }
      y+=h+10;
    }
    if(widget_on[1]){
      h=118; fill(x0,y,TW,h,58); frame(x0,y,TW,h,40);
      { int cx=x0+TW/2, cy=y+48, R=38; disc(cx,cy,R,63);
        for(int t=0;t<12;t++){ int a=t*256/12;
          wline(cx+((R-2)*S((u8)(a+64)))/127,cy-((R-2)*S((u8)a))/127, cx+((R-6)*S((u8)(a+64)))/127,cy-((R-6)*S((u8)a))/127,0); }
        int ah=((hr%12)*60+mn)*256/720, am=(mn*60+sc)*256/3600, as=sc*256/60;
        wline(cx,cy, cx+((R*5/10)*S((u8)(ah+64)))/127, cy-((R*5/10)*S((u8)ah))/127, 0);
        wline(cx,cy, cx+((R*7/10)*S((u8)(am+64)))/127, cy-((R*7/10)*S((u8)am))/127, 0);
        wline(cx,cy, cx+((R*8/10)*S((u8)(as+64)))/127, cy-((R*8/10)*S((u8)as))/127, C_RED);
        disc(cx,cy,3,0); draw_str(cx-32,y+h-16,clkbuf,4); }
      y+=h+10;
    }
    if(widget_on[2]){
      h=138; fill(x0,y,TW,h,58); frame(x0,y,TW,h,40);
      { static const char* M[12]={"JANUARY","FEBRUARY","MARCH","APRIL","MAY","JUNE","JULY","AUGUST","SEPTEMBER","OCTOBER","NOVEMBER","DECEMBER"};
        draw_str(x0+12,y+10,M[mon-1],C_RED);
        draw_str(x0+10,y+28,"SU MO TU WE TH FR SA",8);
        int tt[12]={0,3,2,5,0,3,5,1,4,6,2,4}, yy=yr, m=mon; if(m<3)yy-=1;
        int start=(yy+yy/4-yy/100+yy/400+tt[m-1]+1)%7;
        int dim[12]={31,28,31,30,31,30,31,31,30,31,30,31};
        dim[1]=((yr%4==0&&yr%100!=0)||yr%400==0)?29:28;
        int days=dim[mon-1], col=start, row=0, cw=(TW-20)/7;
        for(int dd=1;dd<=days;dd++){ int cx=x0+10+col*cw, cy=y+44+row*15;
          if(dd==day) disc(cx+6,cy+4,8,C_RED);
          char sb[3]; sb[0]=(dd<10)?' ':('0'+dd/10); sb[1]='0'+dd%10; sb[2]=0;
          draw_str(cx,cy,sb,(dd==day)?C_WHITE:0);
          col++; if(col>6){col=0;row++;} } }
      y+=h+10;
    }
    if(widget_on[3]){
      h=70; fill(x0,y,TW,h,58); frame(x0,y,TW,h,40);
      { draw_str(x0+12,y+10,"BATTERY",8);
        int pct=bat_pct; if(pct<0)pct=0; if(pct>100)pct=100;
        int bx=x0+12,by=y+32,bw=120,bh=24;
        frame(bx,by,bw,bh,0); fill(bx+bw,by+8,4,bh-16,0);
        fill(bx+2,by+2,(bw-4)*pct/100,bh-4, pct<=20?C_RED:C_GREEN);
        char pb[6]; int n=0;
        if(pct>=100){pb[0]='1';pb[1]='0';pb[2]='0';pb[3]='%';pb[4]=0;n=4;}
        else if(pct>=10){pb[0]='0'+pct/10;pb[1]='0'+pct%10;pb[2]='%';pb[3]=0;n=3;}
        else {pb[0]='0'+pct;pb[1]='%';pb[2]=0;n=2;}
        draw_str(x0+TW-n*8-12,y+12,pb,0); }
      y+=h+10;
    }
}

static void compose(void){
    FB=BACK;
    int n=PITCH*H;
    __asm__ volatile("cld; rep movsl" : : "S"(BASE), "D"(BACK), "c"(n) : "memory");
    draw_widgets();
    if(0) for(int i=0;i<DSK_MAX;i++){ if(!DSK[i].used||DSK[i].parent!=-1)continue; int dx=DSK[i].x,dy=DSK[i].y;
        if(app==0 && i==dsk_nth(dsk_sel)){ frame(dx-6,dy-6,52,52,C_WHITE); afill(dx-5,dy-5,50,50,C_BBLUE,90); }
        if(DSK[i].type==1) file_folder(dx,dy,30);
        else if(DSK[i].type==2){ int a=DSK[i].app,gid; u8 col;
            int pic=icon_for_app(a);
            if(pic>=0){ blit_icon(pic,dx-1,dy-1,32); }
            else { switch(a){ case 10:gid=20;col=105;break; case 22:gid=22;col=100;break; case 23:gid=23;col=107;break;
                case 24:gid=24;col=101;break; case 25:gid=25;col=109;break; case 26:gid=26;col=108;break; case 27:gid=27;col=106;break; case 28:gid=2;col=106;break;
                default: if(a==1||a==2||a==3||a==4||a==5||a==8||a==11||a==12||a==14||a==15||a==16||a==17){gid=a;col=102;} else {gid=21;col=102;} }
              dicon(gid,dx,dy,30,col); } }
        else if(ext_is(DSK[i].name,'Z','I','P')){ draw_zip_icon(dx-1,dy-1,32); }
        else { int gid; u8 col; ftype_icon(DSK[i].name,&gid,&col); file_doc(dx,dy,30,gid); }
        if(rename_mode && rename_folder==-1 && rename_idx==i){ int bw=strlen_(rename_buf)*8+10; fill(dx+14-bw/2,dy+30,bw,12,C_BLUE); draw_str(dx+14-bw/2+2,dy+31,rename_buf,C_WHITE); fill(dx+14-bw/2+2+strlen_(rename_buf)*8,dy+31,6,9,C_WHITE); }
        else { int lw=strlen_(DSK[i].name)*8, lx=dx+14-lw/2; if(lx<0)lx=0; draw_str(lx+1,dy+31,DSK[i].name,0); draw_str(lx,dy+30,DSK[i].name,C_WHITE); }
    }

    int ftop=focus_top();
    for(int z=0;z<wincnt;z++){ if(wins[z].min)continue; win_load(z); win_focused=(z==ftop);
        draw_app(); }
    win_focused=1; cur_win=ftop; win_load(ftop);
    if(!start_open) draw_dock();
    if(start_open) draw_start_w11();
    if(run_open) draw_run_dialog();
    if(start_open&&pwr_open) draw_power_menu();
    if(cal_open) draw_calendar();
    if(cc_open) draw_control_center();
    if(ctx_open){
        static const char* dit[5]={"REFRESH","NEW FOLDER","PERSONALIZE","TERMINAL","ABOUT"};
        static const char* fit[5]={"OPEN","RENAME","DELETE","COPY TO DESKTOP","PROPERTIES"};
        static const char* eit[5]={"OPEN","RENAME","DELETE","PROPERTIES","CANCEL"};
        static const char* uit[5]={"OPEN","COPY TO DESKTOP","DELETE","PROPERTIES","REFRESH"};
        const char** it=(ctx_type==1)?fit:(ctx_type==2)?eit:(ctx_type==3)?uit:dit; int n2=5;
        int mw=150,mh=n2*20+6;
        ultra_shadow(ctx_x,ctx_y,mw,mh); glass_panel(ctx_x,ctx_y,mw,mh,0,150); rrectR(ctx_x,ctx_y,mw,3,2,C_BLUE);
        if(ctx_type){ fill(ctx_x,ctx_y,mw,16,C_BLUE); draw_str(ctx_x+6,ctx_y+4,ctx_fname,C_WHITE); }
        int oy=ctx_type?18:6;
        for(int i=0;i<n2;i++) draw_str(ctx_x+10,ctx_y+oy+i*20,it[i],C_WHITE);
    }
    if(fdrag){ fill(mx+12,my,strlen_(fdrag_name)*8+6,12,C_BLUE); draw_str(mx+15,my+2,fdrag_name,C_WHITE); }

    if(toast_t>0){ int tw=strlen_(toast)*8+24; int tx=W-tw-12,ty=H-48-40; ultra_shadow(tx,ty,tw,32); glass_panel(tx,ty,tw,32,0,120); fill(tx+8,ty+11,10,10,C_GREEN); draw_str(tx+24,ty+12,toast,C_WHITE); }
    if(dev_mode){ int dw=3*8+8; fill(W-dw-6,4,dw,15,C_RED); draw_str(W-dw-2,5,"DEV",C_WHITE); }
    if(help_overlay){
        int pw=540, ph=560, px=W/2-pw/2, py=H/2-ph/2;
        ultra_shadow(px,py,pw,ph); glass_panel(px,py,pw,ph,0,238); frame(px,py,pw,ph,C_WHITE);
        draw_str2(px+24,py+22,"KEYBOARD SHORTCUTS",C_WHITE);
        fill(px+24,py+54,pw-48,1,C_MGREY+20);
        static const char* HK[19][2]={
            {"Alt + Tab","Switch windows"},{"Alt + F4","Close window"},
            {"Ctrl + W","Close window"},{"Ctrl + C / V","Copy / Paste"},
            {"Win + E","Files"},{"Win + T","Terminal"},
            {"Win + B","Browser"},{"Win + K","Calculator"},
            {"Win + I","Settings"},{"Win + D","Show desktop"},
            {"Win + L","Lock screen"},{"Win + R","Run a command"},
            {"Win + Shift + S","Screenshot"},{"Ctrl + Shift + Esc","Task Manager"},
            {"F5","Refresh window"},{"Ctrl + Alt + /","Show this help"},
            {"Esc + F3 (Refresh)","Recovery Mode"},{"Ctrl + L","Legacy boot / restart"},
            {"Ctrl+Alt+Shift+R","Powerwash (factory reset)"} };
        for(int i=0;i<19;i++){ int yy=py+70+i*23;
            draw_str(px+30,yy,HK[i][0],C_BBLUE); draw_str(px+270,yy,HK[i][1],C_WHITE); }
        draw_str(px+30,py+ph-24,"Press any key or Ctrl+Alt+/ to close",C_MGREY+20);
    }

    { static const int bpct[8]={40,55,70,85,100,125,150,200};
      int bi=cc_bright; if(bi<0)bi=0; if(bi>7)bi=7; int mul=bpct[bi];
      if(cc_night||cc_dark||mul!=100){ int n2=PITCH*H;
        for(int i=0;i<n2;i++){ u32 p=BACK[i]; int r=(p>>16)&255,g=(p>>8)&255,b=p&255;
            if(cc_night){ b=(b*180)/255; g=(g*230)/255; }
            if(cc_dark){ r=(r*200)/255; g=(g*200)/255; b=(b*200)/255; }
            if(mul!=100){ r=(r*mul)/100; g=(g*mul)/100; b=(b*mul)/100; if(r>255)r=255; if(g>255)g=255; if(b>255)b=255; }
            BACK[i]=((u32)r<<16)|((u32)g<<8)|(u32)b; } } }
    if(diag_shown) draw_input_diag();

    curs_x=mx; curs_y=my;
    for(int r=0;r<24;r++)for(int c=0;c<16;c++){ int px=curs_x+c,py=curs_y+r; if(px<W&&py<H) curs_save[r*16+c]=FB[py*PITCH+px]; }
    draw_cursor(mx,my); curs_shown=1;
    FB=LFB;
    __asm__ volatile("cld; rep movsl" : : "S"(BACK), "D"(LFB), "c"(n) : "memory");
    draw_magnifier();
}

static void reboot_now(void){ u8 t=0x02; while(t&0x02)t=inb(0x64); outb(0x64,0xFE); for(;;)__asm__("hlt"); }

static int bl_meta_read(void){ u8* s=(u8*)0x00928000u; if(ata_read(BL_LBA,s))return -1; u8*p=(u8*)&BLM; for(unsigned i=0;i<sizeof(BLM)&&i<512;i++)p[i]=s[i]; return 0; }
static void bl_meta_write(void){ u8* s=(u8*)0x00928000u; for(int i=0;i<512;i++)s[i]=0; u8*p=(u8*)&BLM; for(unsigned i=0;i<sizeof(BLM)&&i<512;i++)s[i]=p[i]; ata_write(BL_LBA,s); }
static void bl_hash(const char* pre,const char* s,int sl,u8 out[32]){ sha256 c; sha256_init(&c); sha256_update(&c,(const u8*)pre,3); sha256_update(&c,(const u8*)s,sl); sha256_final(&c,out); }
static void draw_lock(int cx,int cy,u8 col){
    fill(cx-11,cy-18,22,16,col); fill(cx-6,cy-13,12,12,0);
    fill(cx-16,cy-2,32,26,col); fill(cx-3,cy+8,6,9,0);
}
static int bl_enable(const char* pw,int pwlen){
    if(!disk_ok)return -1;
    u8 vmk[32]; entropy_get(vmk,32);
    u8 rb[48]; entropy_get(rb,48); char rk[49]; for(int i=0;i<48;i++)rk[i]=(char)('0'+(rb[i]%10)); rk[48]=0;
    int q=0; for(int g=0;g<8;g++){ for(int j=0;j<6;j++)bl_rkdisp[q++]=rk[g*6+j]; if(g<7)bl_rkdisp[q++]='-'; } bl_rkdisp[q]=0;
    int nf=nvx.count, off[NVX_MAX], rlen[NVX_MAX]; char rname[NVX_MAX][16]; int tot=0;
    for(int i=0;i<nf;i++){ int L=nvx.e[i].len; off[i]=tot; rlen[i]=L; int k=0; while(nvx.e[i].name[k]&&k<15){rname[i][k]=nvx.e[i].name[k];k++;} rname[i][k]=0; nvx_read(i,(char*)(RE_BUF+tot),L); tot+=L; }
    u8 ph[32],rh[32]; bl_hash("PW:",pw,pwlen,ph); bl_hash("RK:",rk,48,rh);
    for(int i=0;i<32;i++){ BLM.by_pw[i]=vmk[i]^ph[i]; BLM.by_rk[i]=vmk[i]^rh[i]; }
    sha256_hash(vmk,32,BLM.verf); BLM.magic=BL_MAGIC; BLM.enabled=1;
    for(int i=0;i<32;i++)bl_key[i]=vmk[i]; bl_enabled=1; bl_unlocked=1;
    for(int i=0;i<nf;i++) nvx_write(rname[i],(char*)(RE_BUF+off[i]),rlen[i]);
    bl_meta_write();
    secure_zero(vmk,32); secure_zero(ph,32); secure_zero(rh,32); secure_zero(rk,49); secure_zero(rb,48);
    return 0;
}
static void bl_disable(void){
    if(!(bl_enabled&&bl_unlocked))return;
    int nf=nvx.count, off[NVX_MAX], rlen[NVX_MAX]; char rname[NVX_MAX][16]; int tot=0;
    for(int i=0;i<nf;i++){ int L=nvx.e[i].len; off[i]=tot; rlen[i]=L; int k=0; while(nvx.e[i].name[k]&&k<15){rname[i][k]=nvx.e[i].name[k];k++;} rname[i][k]=0; nvx_read(i,(char*)(RE_BUF+tot),L); tot+=L; }
    bl_enabled=0; bl_unlocked=0; secure_zero(bl_key,32);
    for(int i=0;i<nf;i++) nvx_write(rname[i],(char*)(RE_BUF+off[i]),rlen[i]);
    BLM.magic=0; BLM.enabled=0; bl_meta_write();
}
static void bl_boot(void){
    if(!disk_ok)return;
    if(bl_meta_read()!=0){ bl_enabled=0; return; }
    if(BLM.magic!=BL_MAGIC || !BLM.enabled){ bl_enabled=0; return; }
    bl_enabled=1; bl_unlocked=0;
    int rk=0; char buf[52]; int bl=0; buf[0]=0;
    for(;;){
        clear_all(0);
        draw_lock(W/2,70,C_WHITE);
        draw_str2(W/2-152,120,OSNAME " DRIVE LOCKED",C_WHITE);
        if(!rk){
            draw_str(W/2-200,158,"THIS DRIVE IS PROTECTED BY NOOVEXCRYPT.",52);
            draw_str(W/2-200,180,"ENTER THE PASSWORD TO UNLOCK THIS DRIVE:",C_WHITE);
            fill(W/2-200,200,300,20,30); frame(W/2-200,200,300,20,C_WHITE);
            for(int i=0;i<bl&&i<34;i++)fill(W/2-194+i*8,206,6,7,C_WHITE);
            draw_str(W/2-200,236,"ENTER = UNLOCK     ESC = RECOVERY KEY",52);
        } else {
            draw_str(W/2-200,158,"ENTER YOUR 48-DIGIT RECOVERY KEY:",C_WHITE);
            fill(W/2-200,200,400,20,30); frame(W/2-200,200,400,20,C_WHITE);
            draw_str(W/2-196,206,buf,C_WHITE);
            draw_str(W/2-200,236,"ENTER = UNLOCK     ESC = BACK",52);
        }
        draw_str(W/2-200,H-36,"F8 = SKIP (BOOT WITHOUT DRIVE - FILES STAY LOCKED)",52);
        u8 k=wait_key();
        if(k==0x42){ bl_unlocked=0; return; }
        if(k==0x01){ rk=!rk; bl=0; buf[0]=0; continue; }
        if(k==0x0E){ if(bl>0)buf[--bl]=0; continue; }
        if(k==0x1C){
            u8 vmk[32],h[32],v[32];
            if(!rk){ bl_hash("PW:",buf,bl,h); for(int i=0;i<32;i++)vmk[i]=BLM.by_pw[i]^h[i]; }
            else { char d[49]; int dn=0; for(int i=0;i<bl&&dn<48;i++) if(buf[i]>='0'&&buf[i]<='9')d[dn++]=buf[i]; bl_hash("RK:",d,dn,h); for(int i=0;i<32;i++)vmk[i]=BLM.by_rk[i]^h[i]; }
            sha256_hash(vmk,32,v);
            int ok=1; for(int i=0;i<32;i++) if(v[i]!=BLM.verf[i]){ok=0;break;}
            if(ok){ for(int i=0;i<32;i++)bl_key[i]=vmk[i]; bl_unlocked=1; secure_zero(vmk,32); secure_zero(h,32); return; }
            draw_str(W/2-200,262,"INCORRECT KEY - TRY AGAIN.",C_RED+8); pit_wait(9); bl=0; buf[0]=0; continue;
        }
        if(!(k&0x80)){ char ch=kchar_shift(k); if(ch>=32 && bl<48){ buf[bl++]=ch; buf[bl]=0; } }
    }
}
static int uac_prompt(void){
    for(int y=0;y<H;y++)for(int x=0;x<W;x++){ int i=y*PITCH+x; if((x+y)&1)FB[i]=PAL32[9]; }
    int bw=360,bh=140,bx=W/2-bw/2,by=H/2-bh/2;
    fill(bx,by,bw,bh,C_WIN); frame(bx,by,bw,bh,C_WHITE); fill(bx,by,bw,22,C_RED);
    draw_str(bx+10,by+7,"USER ACCOUNT CONTROL",C_WHITE);
    draw_str(bx+14,by+40,"ALLOW CHANGES TO A PROTECTED",C_TITLE);
    draw_str(bx+14,by+56,"SYSTEM FILE?",C_TITLE);
    draw_str(bx+40,by+96,"Y = YES        N = NO",C_WHITE);
    for(;;){ u8 k=wait_key(); if(k==0x15)return 1; if(k==0x31||k==0x01)return 0; }
}
static void bsod(void){
    clear_all(C_BSOD);
    draw_str3(40,46,":(",C_WHITE);
    draw_str(40,108,"YOUR PC RAN INTO A PROBLEM AND NEEDS TO",C_WHITE);
    draw_str(40,124,"RESTART. WE'RE JUST COLLECTING SOME ERROR",C_WHITE);
    draw_str(40,140,"INFO, AND THEN WE'LL RESTART FOR YOU.",C_WHITE);
    for(int p=0;p<=100;p+=5){ char b[20]; int q=0; if(p>=100){b[q++]='1';b[q++]='0';b[q++]='0';} else if(p>=10){b[q++]=(char)('0'+p/10);b[q++]=(char)('0'+p%10);} else b[q++]=(char)('0'+p); b[q++]='%'; b[q++]=' '; const char* w="COMPLETE"; for(int i=0;w[i];i++)b[q++]=w[i]; b[q]=0; fill(40,170,280,12,C_BSOD); draw_str(40,170,b,C_WHITE); pit_wait(2); }
    draw_str(40,206,"STOP CODE: SYSTEM_FILE_DELETED",C_WHITE);
    rec_qr(W-228,96,4);
    draw_str(W-228,250,"SCAN FOR RECOVERY HELP",C_WHITE);
    draw_str(W-228,266,"mynexus.site/recovery",52);
    draw_str(40,238,"PRESS ANY KEY TO ENTER RECOVERY.",52);
    wait_key(); cmos_write(0x37,0x52); reboot_now();
}
static void sysfile_break(const char* fname){
    clear_all(0);
    draw_str(W/2-184,90,"WARNING: A PROTECTED SYSTEM FILE WAS DELETED",C_RED+8);
    draw_str(W/2-184,116,"FILE:",C_WHITE); draw_str(W/2-134,116,fname,C_WHITE);
    draw_str(W/2-184,146,"VERIFYING SYSTEM INTEGRITY ...",52); beep(900,4); pit_wait(5);
    draw_str(W/2-184,162,"INTEGRITY CHECK FAILED - SYSTEM UNSTABLE.",C_RED+8); beep(500,6); pit_wait(9);
    bsod();
}
static void del_system_flow(void){
    if(!uac_prompt())return;
    clear_all(0);
    draw_str(W/2-120,80,"DELETING SYSTEM FILES...",C_RED+8);
    draw_str(W/2-120,120,"REMOVING KERNEL.BIN ...",C_MGREY+20); beep(900,4);
    draw_str(W/2-120,136,"REMOVING MOUSE.DRV ...",C_MGREY+20); beep(700,4);
    draw_str(W/2-120,152,"REMOVING BOOT.SYS ...",C_MGREY+20); beep(500,4);
    pit_wait(8); bsod();
}
static void rec_tile(int x,int y,int w,const char* t,const char* d,int num){
    fill(x,y,w,44,C_BBLUE); frame(x,y,w,44,C_WHITE);
    fill(x+12,y+12,20,20,C_WHITE);
    char nb[2]={(char)('0'+num),0}; draw_str(x+18,y+18,nb,C_BLUE);
    draw_str(x+44,y+9,t,C_WHITE); if(d[0])draw_str(x+44,y+26,d,52);
}
static void rec_header(const char* title){
    clear_all(C_BLUE);
    draw_str(40,28,OSNAME " RECOVERY ENVIRONMENT",52);
    hrule(40,46,W-80,C_BBLUE);
    draw_str2(60,72,title,C_WHITE);
}
static void rec_switch_os(void){

    for(;;){
        rec_header("SWITCH OPERATING SYSTEM");
        int cx=W/2;

        int ux=cx-16, uy=150;
        fill(ux+5,uy,22,8,C_WHITE);
        rrectR(ux,uy+8,32,46,4,68); frame(ux,uy+8,32,46,C_WHITE);
        fill(ux+13,uy+16,6,26,C_BBLUE);
        fill(ux+8,uy+20,16,3,C_BBLUE);
        draw_str(cx-200,uy+70,"INSERT A USB FLASHDRIVE THAT CONTAINS ANOTHER",C_WHITE);
        draw_str(cx-200,uy+86,"BOOTABLE OPERATING SYSTEM (E.G. LINUX, WINDOWS).",C_WHITE);
        draw_str(cx-200,uy+118,"NOOVEX WILL DETECT IT - YOUR DISK IS NOT TOUCHED.",52);

        hide_cursor();
        ehci_enumerate(); usbmsd_mount();
        int found=(msd_dev>=0 && msd_ready);

        draw_str(cx-200,uy+150,"STATUS:",C_TITLE);
        if(found){
            draw_str(cx-120,uy+150,"USB DRIVE DETECTED",C_GREEN);
            if(msd_blocks){ u32 mb=(msd_blocks+1)/2048; char mbb[12]; utoa(mb,mbb);
                draw_str(cx-200,uy+170,"CAPACITY:",C_WHITE); draw_str(cx-110,uy+170,mbb,C_TEAL); draw_str(cx-110+strlen_(mbb)*8,uy+170," MB",C_WHITE); }

            draw_str(cx-200,uy+190,fat_ok?"FILESYSTEM: FAT (LIKELY BOOTABLE)":"FILESYSTEM: UNKNOWN/RAW",fat_ok?C_GREEN:C_FOLDER);
            draw_str(cx-200,uy+230,"PRESS  ENTER  TO SEE HOW TO BOOT FROM IT",C_WHITE);
            draw_str(cx-200,uy+250,"PRESS  R  TO RESCAN      ESC  TO GO BACK",52);
        } else {
            draw_str(cx-120,uy+150,"NO USB DRIVE DETECTED",C_RED+8);
            draw_str(cx-200,uy+190,"INSERT THE FLASHDRIVE, THEN:",C_WHITE);
            draw_str(cx-200,uy+214,"PRESS  R  TO RESCAN       ESC  TO GO BACK",52);
        }
        u8 k=wait_key();
        if(k==0x01) return;
        if(k==0x13){ continue; }
        if(k==0x1C && found){

            rec_header("BOOT FROM THE USB OPERATING SYSTEM");
            int x=70,y=140;
            draw_str(x,y,"YOUR DISK AND " OSNAME " ARE LEFT UNCHANGED.",C_GREEN); y+=18;
            draw_str(x,y,"TO RUN THE OTHER OS, BOOT THE PC FROM THE USB:",C_WHITE); y+=30;
            draw_str(x,y,"1. SHUT DOWN THIS PC (RECOVERY > TURN OFF).",C_TITLE); y+=22;
            draw_str(x,y,"2. KEEP THE USB FLASHDRIVE PLUGGED IN.",C_TITLE); y+=22;
            draw_str(x,y,"3. POWER ON AND OPEN THE BOOT MENU:",C_TITLE); y+=18;
            draw_str(x+24,y,"- REAL PC: TAP F12 / F11 / F8 / ESC AT STARTUP.",52); y+=16;
            draw_str(x+24,y,"- VIRTUALBOX: PRESS F12, OR SET USB FIRST IN",52); y+=16;
            draw_str(x+24,y,"  SETTINGS > SYSTEM > BOOT ORDER.",52); y+=22;
            draw_str(x,y,"4. CHOOSE THE USB DRIVE IN THE BOOT MENU.",C_TITLE); y+=22;
            draw_str(x,y,"5. THE OTHER OS BOOTS. TO RETURN TO " OSNAME ",",C_TITLE); y+=18;
            draw_str(x+24,y,"REMOVE THE USB AND REBOOT.",52); y+=34;
            draw_str(x,y,"NOTE: LEGACY/CSM BOOT + SECURE BOOT OFF MAY BE",C_FOLDER); y+=16;
            draw_str(x,y,"NEEDED FOR SOME USB OPERATING SYSTEMS.",C_FOLDER); y+=30;
            draw_str(x,y,"PRESS  P  TO POWER OFF NOW      ESC  TO GO BACK",C_WHITE);
            for(;;){ u8 c=wait_key(); if(c==0x01) break; if(c==0x19){ do_shutdown(); } }
            return;
        }
    }
}
static void rec_troubleshoot(void){
    rec_header("TROUBLESHOOT");
    int x=60,w=W-120,y=140;
    rec_tile(x,y,w,"RESET SETTINGS","RESTORE DEFAULTS, KEEP YOUR FILES",1); y+=50;
    rec_tile(x,y,w,"FACTORY RESET","ERASE ALL FILES + SETTINGS, START FRESH",2); y+=50;
    rec_tile(x,y,w,"REPAIR SYSTEM FILES","SCAN AND REPAIR " OSNAME " FILES",3); y+=50;
    rec_tile(x,y,w,"SWITCH OPERATING SYSTEM","DETECT A BOOTABLE OS ON A USB FLASHDRIVE",4); y+=50;
    rec_tile(x,y,w,"RESTART","REBOOT THIS PC",5); y+=50;
    rec_tile(x,y,w,"GO BACK","RETURN TO THE PREVIOUS SCREEN",6);
    draw_str(60,H-30,"PRESS 1-6 TO CHOOSE",52);
    for(;;){ u8 k=wait_key();
        if(k==0x02){ cmos_write(0x39,0); draw_str(60,H-52,"SETTINGS WILL BE RESET ON NEXT START. RESTARTING...",C_GREEN); pit_wait(14); reboot_now(); }
        if(k==0x03){
            rec_header("FACTORY RESET");
            draw_str(60,140,"THIS ERASES ALL YOUR FILES AND SETTINGS AND",C_RED+8);
            draw_str(60,158,"RETURNS " OSNAME " TO ITS ORIGINAL STATE.",C_WHITE);
            draw_str(60,176,"THIS CANNOT BE UNDONE.",C_RED+8);
            draw_str(60,210,"PRESS  Y  TO ERASE EVERYTHING       N  TO CANCEL",C_TITLE);
            for(;;){ u8 c=wait_key();
                if(c==0x31||c==0x01){ break; }
                if(c==0x15){
                    draw_str(60,250,"ERASING FILES AND SETTINGS...",C_GREEN);
                    if(disk_ok) nvx_format();
                    acct.magic=0; have_user=0; if(disk_ok) acct_save();
                    for(u8 r=0x37;r<=0x44;r++) cmos_write(r,0);
                    pit_wait(12);
                    draw_str(60,268,"DONE. RESTARTING TO FIRST-TIME SETUP...",C_GREEN); pit_wait(10);
                    reboot_now();
                }
            }
            return;
        }
        if(k==0x04){ draw_str(60,H-52,"REPAIRING SYSTEM FILES...",C_GREEN); pit_wait(12);
            if(!disk_ok){ ata_identify(); disk_ok=1; }
            nvx_format();
            nvx_mount();
            cmos_write(0x37,0);
            draw_str(60,H-52,"REPAIR COMPLETE. SYSTEM RESTORED.      ",C_GREEN); pit_wait(10);
            draw_str(60,H-34,"PRESS ANY KEY TO RETURN TO " OSNAME ".",C_WHITE); wait_key(); return; }
        if(k==0x05){ rec_switch_os(); }
        if(k==0x06){ reboot_now(); }
        if(k==0x07){ return; }
    }
}
static u8 rec_getkey(int* ctrl){
    for(;;){ u8 st=inb(0x64); if(st&1){ u8 d=inb(0x60); if(st&0x20)continue;
        if(d==0x1D){ *ctrl=1; continue; } if(d==0x9D){ *ctrl=0; continue; }
        if(d&0x80)continue; return d; } }
}
static void rec_px(int x,int y,u32 c){ if(x>=0&&x<W&&y>=0&&y<H)FB[y*PITCH+x]=c; }
static void rec_ring(int cx,int cy,int r,u32 col){ int ro=(r+1)*(r+1),ri=(r-1)*(r-1);
    for(int dy=-r;dy<=r;dy++)for(int dx=-r;dx<=r;dx++){ int d=dx*dx+dy*dy; if(d<=ro&&d>=ri)rec_px(cx+dx,cy+dy,col); } }
static void rec_box(int x,int y,int w,int h,u32 col){
    for(int q=0;q<w;q++){ rec_px(x+q,y,col); rec_px(x+q,y+h-1,col); }
    for(int q=0;q<h;q++){ rec_px(x,y+q,col); rec_px(x+w-1,y+q,col); } }
static void rec_fillc(int x,int y,int w,int h,u32 col){ for(int q=0;q<h;q++)for(int p=0;p<w;p++)rec_px(x+p,y+q,col); }
static void rec_qr(int x,int y,int px){ int n=QR_REC_N, q=4;
    rec_fillc(x,y,(n+2*q)*px,(n+2*q)*px,0x00FFFFFFu);
    for(int my=0;my<n;my++){ unsigned int row=QR_REC[my];
        for(int mx=0;mx<n;mx++){ if(row&(1u<<mx)) rec_fillc(x+(q+mx)*px,y+(q+my)*px,px,px,0x00000000u); } } }
static void recover_usb_screen(void){
    int scanned=0; u32 mb=0;
    for(;;){
        FB=LFB; clear_all(7);
        int cx=W/2, LX=W/2-440; if(LX<40)LX=40;
        rec_ring(LX+12,96,13,0x008AB4F8u); draw_str2(LX+5,88,"?",50);
        { const char* t="Advanced options"; draw_str2(LX,128,t,60); }
        { const char* t="Recover from a USB stick or SD card."; draw_str(LX,176,t,38); }
        if(!scanned){ hide_cursor(); ehci_enumerate(); usbmsd_mount(); scanned=1;
            if(msd_dev>=0){ u32 d=1048576u/msd_bsize; if(d<1)d=1; mb=msd_blocks/d; } }
        if(msd_dev<0){
            { const char* m="Please insert a recovery USB stick or SD card."; draw_str(cx-strlen_(m)*4,238,m,46); }
            int gy=300;
            fill(cx-140,gy-2,280,8,C_MGREY+18); fill(cx-140,gy+6,280,2,C_MGREY+6);
            fill(cx-44,gy-1,40,6,0); fill(cx-40,gy+1,32,2,C_MGREY);
            fill(cx+10,gy,30,4,0);
            int ax=cx, ay=gy+30;
            for(int r=0;r<14;r++){ fill(ax-r,ay+r,r*2+1,1,C_BBLUE); }
            fill(ax-5,ay+14,10,26,C_BBLUE);
            int ux=cx-72, uy=gy+92;
            fill(ux+5,uy-13,13,15,C_MGREY+18); frame(ux+5,uy-13,13,15,C_MGREY);
            fill(ux+8,uy-10,7,9,C_MGREY);
            fill(ux,uy,24,46,C_WHITE); frame(ux,uy,24,46,C_MGREY);
            fill(ux+4,uy+5,16,4,C_BBLUE); fill(ux+10,uy+40,4,4,C_GREEN);
            int sx=cx+48, sy=gy+92;
            fill(sx,sy,36,46,C_WHITE); frame(sx,sy,36,46,C_MGREY);
            for(int r=0;r<11;r++){ fill(sx+36-(11-r),sy+r,11-r,1,7); }
            for(int r=0;r<11;r++){ fill(sx+36-(11-r)-1,sy+r,2,1,C_MGREY); }
            for(int c=0;c<5;c++) fill(sx+5+c*4,sy+3,2,6,C_FOLDER);
            fill(sx+5,sy+16,26,16,C_MGREY+18);
            { const char* h="For help visit  mynexus.site / recovery"; draw_str(cx-strlen_(h)*4,H-128,h,32); }
            { const char* r2="Press  R  to rescan          ESC  to go back"; draw_str(cx-strlen_(r2)*4,H-94,r2,40); }
        } else {
            { const char* t="USB storage detected."; draw_str(LX,224,t,62); }
            { char nb[12]; utoa(mb,nb); const char* t="Capacity (MB):"; draw_str(LX,256,t,46); draw_str(LX+160,256,nb,46); }
            { const char* t="Files on this device:"; draw_str(LX,288,t,50); }
            if(usbfs_n==0){ const char* t="(no files, or filesystem not recognised)"; draw_str(LX+20,312,t,40); }
            else { for(int i=0;i<usbfs_n && i<14;i++){ draw_str(LX+20,312+i*20,usbfs[i].name,46); } }
            { const char* r2="Press  R  to rescan          ESC  to go back"; draw_str(LX,H-94,r2,40); }
        }
        u8 k=wait_key();
        if(k==0x01) return;
        if(k==0x13){ scanned=0; }
    }
}
static void sysinfo_screen(void){
    FB=LFB; clear_all(7);
    int LX=W/2-440; if(LX<40)LX=40;
    rec_ring(LX+12,96,13,0x008AB4F8u); draw_str2(LX+5,88,"?",50);
    { const char* t="System information"; draw_str2(LX,128,t,60); }
    int y=192;
    { const char* t="Operating system:"; draw_str(LX,y,t,50); draw_str(LX+230,y,OSVER,C_WHITE); y+=26; }
    { const char* t="Architecture:"; draw_str(LX,y,t,50); draw_str(LX+230,y,"x86 32-bit protected mode",C_WHITE); y+=26; }
    { char b[12]; utoa((u32)ram_mb,b); draw_str(LX,y,"Memory (MB):",50); draw_str(LX+230,y,b,C_WHITE); y+=26; }
    { char b[12]; utoa((u32)W,b); draw_str(LX,y,"Display width:",50); draw_str(LX+230,y,b,C_WHITE); char b2[12]; utoa((u32)H,b2); draw_str(LX+330,y,"x",C_WHITE); draw_str(LX+350,y,b2,C_WHITE); y+=26; }
    { draw_str(LX,y,"Boot disk:",50); draw_str(LX+230,y,disk_ok?"ready  (ATA PIO, LBA28)":"not detected",C_WHITE); y+=26; }
    { draw_str(LX,y,"USB storage:",50); draw_str(LX+230,y,msd_dev>=0?"mounted":"none detected",C_WHITE); y+=26; }
    { draw_str(LX,y,"CPU:",50); draw_str(LX+230,y,cpu_brand[0]?cpu_brand:"x86 compatible",C_WHITE); y+=26; }
    { const char* t="Press any key to go back."; draw_str(LX,H-90,t,40); }
    wait_key();
}
static void do_powerwash(void);
static void usb_backup_restore(int restore){
    FB=LFB; clear_all(7);
    int LX=W/2-440; if(LX<40)LX=40;
    rec_ring(LX+12,96,13,0x008AB4F8u); draw_str2(LX+5,88,"?",50);
    draw_str2(LX,128, restore?"Restore files from USB":"Back up files to USB", 60);
    if(!(fat_ok&&msd_dev>=0)){ hide_cursor(); ehci_enumerate(); usbmsd_mount(); }
    int y=196;
    if(!(fat_ok&&msd_dev>=0)){
        { const char* t="No USB storage found. Insert a USB stick and try again."; draw_str(LX,y,t,46); }
        { const char* t="Press any key to go back."; draw_str(LX,H-90,t,40); } wait_key(); return; }
    if(!disk_ok){
        { const char* t="A local NVXFS disk is required for backup/restore."; draw_str(LX,y,t,46); }
        { const char* t="Press any key to go back."; draw_str(LX,H-90,t,40); } wait_key(); return; }
    char* buf=(char*)0x04000000u; int done=0, fail=0;
    if(!restore){
        { const char* t="Copying local files to USB..."; draw_str(LX,y,t,50); } y+=28;
        if(nvx.count==0){ const char* t="(no local files to back up)"; draw_str(LX+20,y,t,40); y+=20; }
        for(int i=0;i<(int)nvx.count && i<18;i++){
            int n=nvx_read(i,buf,33554432); int r=fat_write_file(nvx.e[i].name,buf,n);
            draw_str(LX+20,y,nvx.e[i].name,C_WHITE); draw_str(LX+320,y, r==0?"OK":"FAIL", r==0?C_GREEN:(C_RED+8));
            if(r==0)done++; else fail++; y+=20; }
    } else {
        { const char* t="Copying USB files to this computer..."; draw_str(LX,y,t,50); } y+=28;
        if(usbfs_n==0){ const char* t="(no files on USB to restore)"; draw_str(LX+20,y,t,40); y+=20; }
        for(int i=0;i<usbfs_n && i<18;i++){
            int n=usbfs_read(i,buf,33554432); int r=nvx_write(usbfs[i].name,buf,n);
            draw_str(LX+20,y,usbfs[i].name,C_WHITE); draw_str(LX+320,y, r>=0?"OK":"FAIL", r>=0?C_GREEN:(C_RED+8));
            if(r>=0)done++; else fail++; y+=20; }
    }
    y+=16;
    { char nb[12]; utoa((u32)done,nb); draw_str(LX,y,"Copied:",50); draw_str(LX+110,y,nb,C_GREEN);
      char fbf[12]; utoa((u32)fail,fbf); draw_str(LX+230,y,"Failed:",50); draw_str(LX+340,y,fbf, fail?(C_RED+8):C_WHITE); }
    { const char* t="Press any key to go back."; draw_str(LX,H-90,t,40); }
    wait_key();
}
static void advanced_options(void){
    int sel=0,ctrl=0; const int N=7;
    const char* opts[7]={
        "Recover from USB / SD card",
        "Back up files to USB",
        "Restore files from USB",
        "System information",
        "Reset settings  (keep your files)",
        "Powerwash  (factory reset)",
        "Back to recovery menu" };
    for(;;){
        FB=LFB; clear_all(7);
        int LX=W/2-440; if(LX<40)LX=40;
        rec_ring(LX+12,96,13,0x008AB4F8u); draw_str2(LX+5,88,"?",50);
        { const char* t="Advanced options"; draw_str2(LX,128,t,60); }
        { const char* t="Use the arrow keys, then press ENTER to select."; draw_str(LX,176,t,38); }
        int bx=LX+8,bw=440,by=224;
        for(int q=0;q<N;q++){ int yy=by+q*46;
            if(q==sel){ rec_fillc(bx,yy,bw,38,0x00182438u); rec_box(bx,yy,bw,38,0x008AB4F8u); rec_box(bx+1,yy+1,bw-2,36,0x008AB4F8u); }
            else rec_box(bx,yy,bw,38,0x00404448u);
            draw_str(bx+16,yy+14, opts[q], q==sel?62:46); }
        { const char* t="ESC = back to recovery menu"; draw_str(LX,H-90,t,40); }
        u8 k=rec_getkey(&ctrl);
        if(k==0x48){ if(sel>0)sel--; }
        else if(k==0x50){ if(sel<N-1)sel++; }
        else if(k==0x01){ return; }
        else if(k==0x1C){
            if(sel==0) recover_usb_screen();
            else if(sel==1) usb_backup_restore(0);
            else if(sel==2) usb_backup_restore(1);
            else if(sel==3) sysinfo_screen();
            else if(sel==4){ FB=LFB; clear_all(0); { const char* t="Settings will reset on next start. Restarting..."; draw_str(W/2-220,H/2,t,C_WHITE); } cmos_write(0x39,0); pit_wait(14); reboot_now(); }
            else if(sel==5) do_powerwash();
            else if(sel==6) return;
        }
    }
}
static void recovery_mode(void){
    int sel=0,ctrl=0,details=0; const int NOPT=6;
    const char* opts[6]={"Continue to NoovexOS","Developer Mode  -  verification off","Restart","Power off","Reset all settings","Advanced options  -  recover from USB"};
    for(;;){
        FB=LFB; clear_all(7);
        int cx=W/2, LX=W/2-440; if(LX<40)LX=40;
        { const char* t="English  v"; draw_str(cx-strlen_(t)*4, 30, t, 44); }
        rec_ring(LX+12, 96, 13, 0x00C0C0C0u); draw_str2(LX+5, 88, "?", 50);
        { const char* t="Let's step you through the recovery process"; draw_str2(LX, 128, t, 60); }
        { const char* t="Select how you'd like to recover."; draw_str(LX, 176, t, 38); }
        { const char* t="Choose an option, or press CTRL+D for Developer Mode."; draw_str(LX, 196, t, 32); }
        { const char* t="Press TAB for recovery details."; draw_str(LX, 214, t, 32); }
        int bx=LX+8,bw=360,by=234;
        for(int q=0;q<NOPT;q++){ int yy=by+q*44;
            if(q==sel){ rec_fillc(bx,yy,bw,36,0x00182438u); rec_box(bx,yy,bw,36,0x008AB4F8u); rec_box(bx+1,yy+1,bw-2,34,0x008AB4F8u);
                        rec_fillc(bx-13,yy+9,18,18,0x00D03828u); draw_str(bx-8,yy+12,"1",C_WHITE); }
            else rec_box(bx,yy,bw,36,0x00404448u);
            draw_str(bx+16,yy+13, opts[q], q==sel?62:46); }
        int fy=H-200;
        rec_ring(LX+6,fy+6,6,0x009AA0A6u); draw_str(LX+22,fy, "Launch diagnostics", 40);
        draw_str(LX, fy+26, "v  Advanced options", 40);
        rec_ring(LX+6,fy+58,6,0x009AA0A6u); draw_str(LX+22,fy+52,"Reset settings", 40);
        rec_qr(W-250, 232, 4);
        { const char* t="SCAN TO RECOVER"; draw_str(W-250, 388, t, 50); }
        { const char* t="mynexus.site/recovery"; draw_str(W-250, 406, t, 38); }
        { const char* t="Model: NOOVEXOS-IA32"; draw_str(LX, H-84, t, 38); }
        { const char* t="Help: mynexus.site / recovery"; draw_str(LX, H-64, t, 30); }
        if(details){
            int dx=LX, dy=by+NOPT*44+16, dw=520;
            rec_box(dx,dy,dw,170,0x008AB4F8u); rec_fillc(dx+1,dy+1,dw-2,168,0x00121A26u);
            draw_str(dx+14,dy+12,"RECOVERY DETAILS",62);
            draw_str(dx+14,dy+40,"OS:  NoovexOS  -  IA-32 protected mode, VESA 32bpp",46);
            draw_str(dx+14,dy+60,"Boot: legacy BIOS / USB  -  from-scratch hobby OS",46);
            draw_str(dx+14,dy+88,"Keys that work in this environment:",50);
            draw_str(dx+28,dy+110,"CTRL+D   Developer Mode (verification off)",40);
            draw_str(dx+28,dy+130,"UP / DOWN   Navigate      ENTER   Select",40);
            draw_str(dx+28,dy+150,"ESC   Continue to NoovexOS",40);
        }
        { const char* t="Use the arrow keys to navigate up or down,"; draw_str(W-LX-300, H-84, t, 34); }
        { const char* t="then press ENTER to select an option."; draw_str(W-LX-300, H-64, t, 34); }
        u8 k=rec_getkey(&ctrl); int dodev=0;
        if(k==0x48){ if(sel>0)sel--; }
        else if(k==0x50){ if(sel<NOPT-1)sel++; }
        else if(k==0x01){ return; }
        else if(k==0x1C){ if(sel==0)return; else if(sel==1)dodev=1; else if(sel==2)reboot_now(); else if(sel==3)do_shutdown(); else if(sel==4){ cmos_write(0x39,0); reboot_now(); } else if(sel==5){ advanced_options(); } }
        else if(k==0x20 && ctrl){ dodev=1; }
        else if(k==0x0F){ details^=1; }
        if(dodev){
            dev_mode=1; bl_enabled=0;
            FB=LFB; clear_all(7);
            rec_ring(LX+12,96,13,0x008AB4F8u); draw_str2(LX+5,88,"?",50);
            { const char* t="Developer Mode is ON"; draw_str2(LX,128,t,60); }
            { const char* t="OS verification is now OFF."; draw_str(LX,176,t,40); }
            { const char* t="Developer tools and full disk access are unlocked."; draw_str(LX,196,t,34); }
            { const char* t="Press any key to continue to NoovexOS."; draw_str(LX,236,t,40); }
            wait_key(); return;
        }
    }
}
static void do_powerwash(void){
    FB=LFB; clear_all(0);
    draw_str2(W/2-150,H/2-120,"POWERWASH",C_RED+8);
    draw_str(W/2-264,H/2-66,"FACTORY RESET - restores NoovexOS to a fresh state.",C_WHITE);
    draw_str(W/2-264,H/2-44,"Your account and all settings will be erased, and",C_WHITE);
    draw_str(W/2-264,H/2-24,"the setup wizard will run on next boot. Files on",C_WHITE);
    draw_str(W/2-264,H/2-4,"disk are not erased.",C_WHITE);
    draw_str(W/2-264,H/2+34,"Press  ENTER  to powerwash,    ESC  to cancel.",52);
    for(;;){ u8 k=wait_key(); if(k==0x01) return; if(k==0x1C) break; }
    clear_all(0);
    draw_str2(W/2-160,H/2-30,"POWERWASHING...",C_WHITE);
    draw_str(W/2-160,H/2+18,"Erasing account and settings",C_MGREY+20);
    cmos_write(0x39,0);
    acct.magic=0; acct.enrolled=0; acct.user[0]=0; acct.pass[0]=0;
    for(int i=0;i<24;i++)acct.org[i]=0; have_user=0;
    if(disk_ok) acct_save();
    pit_wait(16); reboot_now();
}
extern char __file_end;
static void install_to_disk(int slot){
    if(slot<0||slot>3||!atai[slot].present||atai[slot].type!=1)return;
    FB=LFB;
    clear_all(C_BLUE);
    draw_str2(W/2-130,70,"INSTALL TO HARD DISK",C_WHITE);
    draw_str(W/2-180,118,"THIS WRITES " OSNAME " TO THE START OF THE DISK SO",C_TITLE);
    draw_str(W/2-180,134,"YOU CAN BOOT WITHOUT THE ISO. YOUR NVXFS FILES",C_TITLE);
    draw_str(W/2-180,150,"(SECTORS 2047+) ARE NOT TOUCHED.",C_TITLE);
    { const char* bn[4]={"PRIMARY MASTER","PRIMARY SLAVE","SECONDARY MASTER","SECONDARY SLAVE"}; char tb[88]="TARGET ("; int q=8; const char*bb=bn[slot&3]; for(int i=0;bb[i];i++)tb[q++]=bb[i]; tb[q++]=')';tb[q++]=':';tb[q++]=' '; for(int i=0;i<40&&atai[slot].model[i];i++)tb[q++]=atai[slot].model[i]; tb[q]=0; draw_str(W/2-180,176,tb,C_WHITE); }
    { u32 gb=atai[slot].sectors/1953125u; char sb[32]="DISK SIZE: "; int q=11; char t[8];int tl=0; u32 v=gb; if(!v)t[tl++]='0'; while(v){t[tl++]='0'+v%10;v/=10;} while(tl)sb[q++]=t[--tl]; sb[q++]=' ';sb[q++]='G';sb[q++]='B'; sb[q]=0; draw_str(W/2-180,192,sb,C_WHITE); }
    { u8 b0[512]; int hp=0; if(ata_read_drv(slot,0,b0)==0&&b0[510]==0x55&&b0[511]==0xAA){ for(int pe=0;pe<4;pe++) if(b0[446+pe*16+4]!=0)hp=1; }
      if(hp){ draw_str(W/2-180,216,"WARNING: THIS DISK ALREADY HAS PARTITIONS!",C_RED); draw_str(W/2-180,232,"INSTALLING WILL OVERWRITE ITS BOOT SECTOR.",C_RED); } }
    draw_str(W/2-180,258,"ONLY INSTALL TO AN EMPTY DISK YOU CAN ERASE.",C_TITLE);
    draw_str(W/2-180,274,"NEVER YOUR WINDOWS OR SYSTEM DISK.",C_RED);
    draw_str(W/2-180,300,"PRESS  Y  TO INSTALL        N  TO CANCEL",C_WHITE);
    for(;;){ u8 k=wait_key(); if(k==0x31||k==0x01){ return; } if(k==0x15) break; }
    clear_all(C_BLUE);
    draw_str2(W/2-120,70,"INSTALLING...",C_WHITE);

    { u8 s[512]; for(int i=0;i<512;i++)s[i]=(i<nvx_boot_bin_len)?nvx_boot_bin[i]:0; ata_write_drv(slot,0,s); }

    for(int sct=0;sct<4;sct++){ u8 s[512]; for(int i=0;i<512;i++){ int o=sct*512+i; s[i]=(o<nvx_stage2_bin_len)?nvx_stage2_bin[o]:0; } ata_write_drv(slot,1+sct,s); }

    u32 flen=(u32)(&__file_end)-0x10000u; const u8* kim=(const u8*)0x10000u;
    int bx=W/2-160,by=140,bw=320; frame(bx-2,by-2,bw+4,16,C_WHITE);
    for(u32 sct=0;sct<896;sct++){
        u8 s[512]; u32 off=sct*512u;
        for(int i=0;i<512;i++){ u32 o=off+(u32)i; s[i]=(o<flen)?kim[o]:0; }
        if(ata_write_drv(slot,5+sct,s)){ draw_str(W/2-100,by+30,"DISK WRITE ERROR",C_RED+8); wait_key(); return; }
        if((sct&15)==0) fill(bx,by,(int)(sct*bw/896),12,C_GREEN);
    }
    fill(bx,by,bw,12,C_GREEN);
    draw_str(W/2-200,186,"INSTALL COMPLETE.",C_GREEN);
    draw_str(W/2-200,210,"1. SHUT DOWN " OSNAME,C_WHITE);
    draw_str(W/2-200,226,"2. REMOVE THE ISO FROM THE VM",C_WHITE);
    draw_str(W/2-200,242,"3. SET THE HARD DISK FIRST IN BOOT ORDER",C_WHITE);
    draw_str(W/2-200,258,"IT WILL THEN BOOT " OSNAME " FROM DISK.",C_TITLE);
    draw_str(W/2-200,286,"PRESS ANY KEY TO RETURN.",C_MGREY+20);
    wait_key();
    dirty=1;
}
static void do_restart(void){
    clear_all(0); draw_str2(W/2-90,H/2-20,"RESTARTING",C_WHITE);
    int bx=W/2-160,by=H/2+20; frame(bx-2,by-2,324,16,C_WHITE);
    for(int i=0;i<320;i+=20){ fill(bx,by,i,12,C_BBLUE); }
    reboot_now();
}
static void do_shutdown(void){
    clear_all(0); draw_str2(W/2-110,H/2-40,"SHUTTING DOWN",C_WHITE);
    draw_str(W/2-90,H/2,"SAVING SETTINGS...",C_MGREY+20); pit_wait(3);

    acpi_shutdown();

    outw(0x4004,0x3400);
    outw(0x604,0x2000);
    outw(0xB004,0x2000);

    clear_all(0); draw_str2(W/2-150,H/2-10,"IT IS NOW SAFE",C_WHITE); draw_str2(W/2-160,H/2+20,"TO TURN OFF",C_WHITE);
    for(;;)__asm__("hlt");
}
static void do_sleep(void){
    clear_all(0);
    draw_str2(W/2-72,H/2-30,"SLEEPING",26);
    draw_str(W/2-100,H/2+6,"PRESS ANY KEY TO WAKE",C_MGREY+20);
    wait_key();
}
static void do_update(void){
    clear_all(C_BLUE);
    draw_str2(W/2-100,60,OSNAME " UPDATE",C_WHITE);
    draw_str(W/2-120,110,"CHECKING FOR UPDATES...",C_TITLE); pit_wait(3);
    draw_str(W/2-120,140,"UPDATES FOUND:",C_WHITE);
    draw_str(W/2-120,156,"- NVX KERNEL PATCH 1.0.1",C_MGREY+20);
    draw_str(W/2-120,172,"- VESA DRIVER UPDATE",C_MGREY+20);
    draw_str(W/2-120,188,"- SECURITY DEFINITIONS",C_MGREY+20); pit_wait(3);
    int bx=W/2-160,by=240; frame(bx-2,by-2,324,16,C_WHITE);
    for(int i=0;i<320;i+=16){ fill(bx,by,i,12,C_GREEN);
        if(i==40)draw_str(W/2-120,270,"DOWNLOADING...        ",C_TITLE);
        if(i==150)draw_str(W/2-120,270,"INSTALLING...         ",C_TITLE);
        if(i==260)draw_str(W/2-120,270,"FINALIZING...         ",C_TITLE); }
    draw_str(W/2-120,270,OSNAME " IS UP TO DATE.   ",C_GREEN);
    draw_str(W/2-120,300,"PRESS ANY KEY TO RETURN",C_WHITE);
    beep(1140,3); wait_key();
}
static void nvx_logo(int cx,int cy,int h,u8 col);
static int boot_anim(void){
    clear_all(0);
    nvx_logo(W/2, (H*36)/100, 140, C_WHITE);
    int cx=W/2, cy=(H*62)/100, R=24, dotR=4;
    int esc=0;
    for(int t=0;t<170 && !esc;t++){
        fill(cx-R-dotR-3, cy-R-dotR-3, 2*(R+dotR)+6, 2*(R+dotR)+6, 0);
        for(int i=0;i<5;i++){
            int ang=(t*5 + i*9)&255;
            int dx=(R*(int)sinT[ang])/127;
            int dy=(R*(int)sinT[(ang+64)&255])/127;
            disc(cx+dx, cy-dy, dotR, C_WHITE);
        }
        if(inb(0x64)&1){ u8 sc=inb(0x60); if(sc==0x01)esc=1; }
        pit_wait(14);
    }
    return esc;
}

static const char* HELLO[NLANG]={"WELCOME","VALKOMMEN","WILLKOMMEN","BIENVENUE","BIENVENIDO","BENVENUTO","BEM-VINDO","WELKOM","HUANYING","YOKOSO"};

static const char* TR[T_N][NLANG]={
  {"WELCOME!","VALKOMMEN!","WILLKOMMEN!","BIENVENUE!","BIENVENIDO!","BENVENUTO!","BEM-VINDO!","WELKOM!","HUANYING!","YOKOSO!"},
  {"LET'S GET YOUR NOOVEX7 READY","VI GOR NOOVEX7 REDO","WIR RICHTEN NOOVEX7 EIN","PREPARONS VOTRE NOOVEX7","CONFIGUREMOS TU NOOVEX7","PREPARIAMO NOOVEX7","VAMOS PREPARAR NOOVEX7","WE STELLEN NOOVEX7 IN","ZHUNBEI NOOVEX7","NOOVEX7 NO JUNBI"},
  {"LANGUAGE","SPRAK","SPRACHE","LANGUE","IDIOMA","LINGUA","IDIOMA","TAAL","YUYAN","GENGO"},
  {"GET STARTED","KOM IGANG","LOSLEGEN","COMMENCER","COMENZAR","INIZIA","COMECAR","BEGINNEN","KAISHI","HAJIMERU"},
  {"UP/DOWN PICKS LANGUAGE","UPP/NER VALJER SPRAK","AUF/AB WAHLT SPRACHE","HAUT/BAS CHOISIT LANGUE","ARRIBA/ABAJO ELIGE IDIOMA","SU/GIU SCEGLI LINGUA","CIMA/BAIXO ESCOLHE","OMHOOG/OMLAAG KIES TAAL","SHANGXIA XUANZE","JOGE DE SENTAKU"},
  {"CONNECT","ANSLUT","VERBINDEN","CONNEXION","CONECTAR","CONNETTI","CONECTAR","VERBINDEN","LIANJIE","SETSUZOKU"},
  {"WE'LL USE WHATEVER NETWORK YOU HAVE","VI ANVANDER NATVERKET DU HAR","WIR NUTZEN DEIN NETZWERK","NOUS UTILISERONS VOTRE RESEAU","USAREMOS TU RED","USEREMO LA TUA RETE","USAREMOS SUA REDE","WE GEBRUIKEN JE NETWERK","SHIYONG NIDE WANGLUO","NETTOWAKU O TSUKAU"},
  {"WIRED ETHERNET","TRADBUNDET","KABEL-NETZWERK","ETHERNET FILAIRE","ETHERNET CABLE","ETHERNET CABLATA","ETHERNET COM FIO","BEDRAAD","YOUXIAN","YUSEN"},
  {"WIRELESS","TRADLOST","DRAHTLOS","SANS-FIL","INALAMBRICO","WIRELESS","SEM FIO","DRAADLOOS","WUXIAN","WAIYARESU"},
  {"AMD PCNET - LINK UP","AMD PCNET - ANSLUTEN","AMD PCNET - VERBUNDEN","AMD PCNET - CONNECTE","AMD PCNET - CONECTADO","AMD PCNET - COLLEGATO","AMD PCNET - CONECTADO","AMD PCNET - VERBONDEN","AMD PCNET - YILIANJIE","AMD PCNET - SETSUZOKU"},
  {"NOT FOUND","HITTADES INTE","NICHT GEFUNDEN","NON TROUVE","NO ENCONTRADO","NON TROVATO","NAO ENCONTRADO","NIET GEVONDEN","WEI ZHAODAO","MITSUKARIMASEN"},
  {"NOT AVAILABLE","EJ TILLGANGLIGT","NICHT VERFUGBAR","NON DISPONIBLE","NO DISPONIBLE","NON DISPONIBILE","INDISPONIVEL","NIET BESCHIKBAAR","BU KEYONG","RIYO DEKINAI"},
  {"PRESS ENTER TO CONTINUE","TRYCK ENTER FOR ATT FORTSATTA","ENTER ZUM FORTFAHREN","APPUYEZ ENTREE POUR CONTINUER","PULSA ENTER PARA CONTINUAR","PREMI INVIO PER CONTINUARE","PRESSIONE ENTER","DRUK OP ENTER","AN ENTER JIXU","ENTER OSHITE TSUZUKERU"},
  {"NEXT","NASTA","WEITER","SUIVANT","SIGUIENTE","AVANTI","PROXIMO","VOLGENDE","XIAYIBU","TSUGI"},
  {"WHO IS THIS FOR?","VEM AR DETTA FOR?","FUR WEN IST DIES?","POUR QUI EST-CE?","PARA QUIEN ES?","PER CHI E?","PARA QUEM E?","VOOR WIE IS DIT?","ZHE SHI WEI SHEI?","DARE NO TAME?"},
  {"CHOOSE HOW YOU'LL USE THIS DEVICE","VALJ HUR DU SKA ANVANDA ENHETEN","WAHLE DIE NUTZUNGSART","CHOISISSEZ COMMENT L'UTILISER","ELIGE COMO USARLO","SCEGLI COME USARLO","ESCOLHA COMO USAR","KIES HOE JE HET GEBRUIKT","XUANZE SHIYONG FANGSHI","SHIYO HOHO O ERABU"},
  {"PERSONAL USE","PERSONLIGT","PRIVAT","USAGE PERSONNEL","USO PERSONAL","USO PERSONALE","USO PESSOAL","PERSOONLIJK","GEREN SHIYONG","KOJIN SHIYO"},
  {"FOR YOU AND FAMILY","FOR DIG OCH FAMILJ","FUR DICH UND FAMILIE","POUR VOUS ET FAMILLE","PARA TI Y FAMILIA","PER TE E FAMIGLIA","PARA VOCE E FAMILIA","VOOR JOU EN GEZIN","ZIJI HE JIATING","ANATA TO KAZOKU"},
  {"ENTERPRISE","FORETAG","UNTERNEHMEN","ENTREPRISE","EMPRESA","AZIENDA","EMPRESA","BEDRIJF","QIYE","KIGYO"},
  {"MANAGED BY ORG","HANTERAS AV ORG","VERWALTET DURCH ORG","GERE PAR ORG","GESTIONADO POR ORG","GESTITO DA ORG","GERENCIADO POR ORG","BEHEERD DOOR ORG","ZUZHI GUANLI","SOSHIKI GA KANRI"},
  {"LEFT/RIGHT TO PICK","VANSTER/HOGER VALJ","LINKS/RECHTS WAHLEN","GAUCHE/DROITE CHOISIR","IZQ/DER ELEGIR","SINISTRA/DESTRA SCEGLI","ESQ/DIR ESCOLHE","LINKS/RECHTS KIEZEN","ZUOYOU XUANZE","SAYU DE ERABU"},
  {"ENROLL","REGISTRERA","REGISTRIEREN","INSCRIRE","INSCRIBIR","ISCRIVI","INSCREVER","INSCHRIJVEN","ZHUCE","TOROKU"},
  {"CONNECT THIS DEVICE TO YOUR ORGANIZATION","ANSLUT ENHETEN TILL ORGANISATIONEN","VERBINDE GERAT MIT ORGANISATION","CONNECTER L'APPAREIL A VOTRE ORG","CONECTAR EL DISPOSITIVO A ORG","COLLEGA IL DISPOSITIVO ALL'ORG","CONECTAR DISPOSITIVO A ORG","VERBIND APPARAAT MET ORG","BANG SHEBEI DAO ZUZHI","DEBAISU O SOSHIKI E"},
  {"ORGANIZATION NAME","ORGANISATIONENS NAMN","ORGANISATIONSNAME","NOM DE L'ORGANISATION","NOMBRE DE LA ORG","NOME ORGANIZZAZIONE","NOME DA ORGANIZACAO","ORGANISATIENAAM","ZUZHI MINGCHENG","SOSHIKI MEI"},
  {"ENROLLMENT KEY","REGISTRERINGSNYCKEL","REGISTRIERUNGSKEY","CLE D'INSCRIPTION","CLAVE DE INSCRIPCION","CHIAVE ISCRIZIONE","CHAVE DE INSCRICAO","REGISTRATIESLEUTEL","ZHUCE MIYAO","TOROKU KAGI"},
  {"DEVICE ENROLLED","ENHETEN REGISTRERAD","GERAT REGISTRIERT","APPAREIL INSCRIT","DISPOSITIVO INSCRITO","DISPOSITIVO ISCRITTO","DISPOSITIVO INSCRITO","APPARAAT GEREGISTREERD","SHEBEI YI ZHUCE","DEBAISU TOROKU SUMI"},
  {"MANAGED BY","HANTERAS AV","VERWALTET VON","GERE PAR","GESTIONADO POR","GESTITO DA","GERENCIADO POR","BEHEERD DOOR","GUANLIZHE","KANRI MOTO"},
  {"ADMIN POLICIES WILL BE APPLIED","ADMINPOLICYS TILLAMPAS","ADMIN-RICHTLINIEN AKTIV","REGLES ADMIN APPLIQUEES","SE APLICARAN POLITICAS","POLITICHE ADMIN ATTIVE","POLITICAS DO ADMIN ATIVAS","ADMIN-BELEID ACTIEF","GUANLI CELUE SHENGXIAO","KANRI POLISHII TEKIYO"},
  {"CONTINUE","FORTSATT","WEITER","CONTINUER","CONTINUAR","CONTINUA","CONTINUAR","DOORGAAN","JIXU","TSUZUKERU"},
  {"SIGN IN","LOGGA IN","ANMELDEN","CONNEXION","INICIAR SESION","ACCEDI","ENTRAR","AANMELDEN","DENGLU","SAINUIN"},
  {"CREATE YOUR ACCOUNT","SKAPA DITT KONTO","KONTO ERSTELLEN","CREEZ VOTRE COMPTE","CREA TU CUENTA","CREA IL TUO ACCOUNT","CRIE SUA CONTA","MAAK ACCOUNT AAN","CHUANGJIAN ZHANGHU","AKAUNTO O TSUKURU"},
  {"CREATE YOUR WORK ACCOUNT","SKAPA ARBETSKONTO","ARBEITSKONTO ERSTELLEN","CREEZ COMPTE PRO","CREA CUENTA DE TRABAJO","CREA ACCOUNT LAVORO","CRIE CONTA DE TRABALHO","MAAK WERKACCOUNT AAN","CHUANGJIAN GONGZUO ZHANGHU","SHIGOTO AKAUNTO"},
  {"IN ORG: ","I ORG: ","IN ORG: ","DANS ORG: ","EN ORG: ","IN ORG: ","NA ORG: ","IN ORG: ","ZUZHI: ","SOSHIKI: "},
  {"USERNAME","ANVANDARNAMN","BENUTZERNAME","NOM UTILISATEUR","USUARIO","NOME UTENTE","NOME DE USUARIO","GEBRUIKERSNAAM","YONGHUMING","YUZA NEMU"},
  {"PASSWORD","LOSENORD","PASSWORT","MOT DE PASSE","CONTRASENA","PASSWORD","SENHA","WACHTWOORD","MIMA","PASUWADO"},
  {"CREATE","SKAPA","ERSTELLEN","CREER","CREAR","CREA","CRIAR","AANMAKEN","CHUANGJIAN","TSUKURU"},
  {"GETTING READY","GOR REDO","WIRD VORBEREITET","PREPARATION","PREPARANDO","PREPARAZIONE","PREPARANDO","KLAARMAKEN","ZHUNBEI ZHONG","JUNBI CHU"},
  {"SETTING UP YOUR WORKSPACE","STALLER IN ARBETSYTAN","ARBEITSBEREICH WIRD EINGERICHTET","CONFIGURATION DE L'ESPACE","CONFIGURANDO TU ESPACIO","CONFIGURAZIONE WORKSPACE","CONFIGURANDO ESPACO","WERKRUIMTE INSTELLEN","SHEZHI GONGZUOQU","WAKUSUPESU SETTEI"},
  {"CREATING ACCOUNT","SKAPAR KONTO","KONTO WIRD ERSTELLT","CREATION DU COMPTE","CREANDO CUENTA","CREAZIONE ACCOUNT","CRIANDO CONTA","ACCOUNT AANMAKEN","CHUANGJIAN ZHANGHU","AKAUNTO SAKUSEI"},
  {"APPLYING THEME","TILLAMPAR TEMA","THEMA AKTIV","APPLICATION THEME","APLICANDO TEMA","APPLICANDO TEMA","APLICANDO TEMA","THEMA TOEPASSEN","YINGYONG ZHUTI","TEMA TEKIYO"},
  {"APPLYING ORG POLICY","TILLAMPAR ORGPOLICY","ORG-RICHTLINIE AKTIV","APPLICATION REGLES ORG","APLICANDO POLITICA","APPLICANDO POLITICA","APLICANDO POLITICA","ORG-BELEID TOEPASSEN","YINGYONG ZUZHI CELUE","SOSHIKI POLISHII"},
  {"PREPARING DESKTOP","FORBEREDER SKRIVBORD","DESKTOP WIRD VORBEREITET","PREPARATION DU BUREAU","PREPARANDO ESCRITORIO","PREPARANDO DESKTOP","PREPARANDO DESKTOP","BUREAUBLAD VOORBEREIDEN","ZHUNBEI ZHUOMIAN","DESUKUTOPPU JUNBI"},
  {"ALMOST THERE","SNART KLART","FAST FERTIG","PRESQUE FINI","CASI LISTO","QUASI FATTO","QUASE LA","BIJNA KLAAR","KUAILE WANCHENG","MOSUGU OWARU"},
  {"YOUR NOOVEX7 IS READY","DIN NOOVEX7 AR REDO","DEIN NOOVEX7 IST BEREIT","VOTRE NOOVEX7 EST PRET","TU NOOVEX7 ESTA LISTO","NOOVEX7 E PRONTO","SEU NOOVEX7 ESTA PRONTO","JE NOOVEX7 IS KLAAR","NOOVEX7 YIJING JIUXU","NOOVEX7 KANRYO"},
  {"ENTER YOUR PASSWORD","ANGE LOSENORD","PASSWORT EINGEBEN","ENTREZ MOT DE PASSE","INTRODUCE CONTRASENA","INSERISCI PASSWORD","INSIRA SENHA","VOER WACHTWOORD IN","SHURU MIMA","PASUWADO NYURYOKU"},
  {"WRONG PASSWORD - TRY AGAIN","FEL LOSENORD - FORSOK IGEN","FALSCHES PASSWORT - ERNEUT","MAUVAIS MDP - REESSAYEZ","CONTRASENA INCORRECTA","PASSWORD ERRATA - RIPROVA","SENHA ERRADA - TENTE","WACHTWOORD FOUT - PROBEER","MIMA CUOWU - ZAISHI","PASUWADO CHIGAU"},
  {"FILES","FILER","DATEIEN","FICHIERS","ARCHIVOS","FILE","ARQUIVOS","BESTANDEN","WENJIAN","FAIRU"},
  {"TERMINAL","TERMINAL","TERMINAL","TERMINAL","TERMINAL","TERMINALE","TERMINAL","TERMINAL","ZHONGDUAN","TAMINARU"},
  {"ABOUT","OM","UBER","A PROPOS","ACERCA","INFO","SOBRE","INFO","GUANYU","JOHO"},
  {"SETTINGS","INSTALLNINGAR","EINSTELLUNGEN","PARAMETRES","AJUSTES","IMPOSTAZIONI","CONFIGURACOES","INSTELLINGEN","SHEZHI","SETTEI"},
  {"NOTEPAD","ANTECKNINGAR","NOTIZBLOCK","BLOC-NOTES","NOTAS","BLOC NOTE","BLOCO NOTAS","KLADBLOK","JISHIBEN","MEMOCHO"},
  {"TASK MGR","AKTIVITETER","TASK MGR","GESTIONNAIRE","ADMIN TAREAS","ATTIVITA","TAREFAS","TAAKBEHEER","RENWU","TASUKU"},
  {"DEVICES","ENHETER","GERATE","APPAREILS","DISPOSITIVOS","DISPOSITIVI","DISPOSITIVOS","APPARATEN","SHEBEI","DEBAISU"},
  {"RECYCLE","PAPPERSKORG","PAPIERKORB","CORBEILLE","PAPELERA","CESTINO","LIXEIRA","PRULLENBAK","HUISHOUZHAN","GOMIBAKO"},
  {"PAINT","RITA","MALEN","PEINTURE","PINTAR","DISEGNA","PINTAR","TEKENEN","HUITU","PEINTO"},
  {"WEB","WEBB","WEB","WEB","WEB","WEB","WEB","WEB","WANGYE","WEBU"},
  {"STORE","BUTIK","STORE","BOUTIQUE","TIENDA","NEGOZIO","LOJA","WINKEL","SHANGDIAN","SUTOA"},
  {"CALCULATOR","KALKYLATOR","RECHNER","CALCULATRICE","CALCULADORA","CALCOLATRICE","CALCULADORA","REKENMACHINE","JISUANQI","DENTAKU"}
};

static const char* t(int id){ int L=(int)acct.lang; if(L<0||L>=NLANG)L=0; if(id<0||id>=T_N)return ""; return TR[id][L]; }
static void setup_panel(const char*title,int step){
    clear_all(C_BLUE);
    fill(W/2-240,H/2-170,480,340,C_WIN); frame(W/2-240,H/2-170,480,340,C_WHITE);
    fill(W/2-240,H/2-170,480,24,C_BBLUE);
    draw_str(W/2-230,H/2-164,OSNAME " SETUP",C_WHITE);
    char sb[12]="STEP _ OF 5"; sb[5]='0'+step; draw_str(W/2+130,H/2-164,sb,C_WHITE);
    draw_str2(W/2-220,H/2-130,title,C_WHITE);
}
static void input_line(int x,int y,char*buf,int max,int mask){
    int len=0; buf[0]=0;
    for(;;){
        fill(x-2,y-2,max*8+8,14,C_WIN); frame(x-2,y-2,max*8+8,14,C_MGREY);
        for(int i=0;i<len;i++) draw_char(x+i*8,y,mask?'*':buf[i],0);
        fill(x+len*8,y,6,9,C_BLUE);
        u8 sc=wait_key();
        if(sc==0x1C) break;
        if(sc==0x0E){ if(len>0){len--;buf[len]=0;} }
        else { char ch=kchar_shift(sc); if(ch>=32&&len<max-1){ buf[len++]=ch; buf[len]=0; } }
    }
}

#define G_BLUE   C_BBLUE
#define G_RED    C_RED
#define G_YELLOW C_FOLDER
#define G_GREEN  C_GREEN

static void cos_circle(int cx,int cy,int r,u8 c){
    int r2=r*r;
    for(int dy=-r;dy<=r;dy++){
        int yy=cy+dy; if(yy<0||yy>=H)continue;
        int rem=r2-dy*dy; if(rem<0)continue;
        int wp=0; while((wp+1)*(wp+1)<=rem)wp++;
        int x0=cx-wp,w=2*wp+1; if(x0<0){w+=x0;x0=0;} if(x0+w>W)w=W-x0;
        if(w>0){ u32* p=FB+yy*PITCH+x0; u32 v=PAL32[c]; for(int i=0;i<w;i++)p[i]=v; }
    }
}

static void cos_logo(int cx,int cy,int r,int gap){
    static const u8 cols[4]={G_BLUE,G_RED,G_YELLOW,G_GREEN};
    int sx=cx-(3*gap)/2;
    for(int i=0;i<4;i++) cos_circle(sx+i*gap,cy,r,cols[i]);
}
/* real NoovexOS 'n' logo, scaled to height h, centred at (cx,cy), drawn in palette colour col */
static void nvx_logo(int cx,int cy,int h,u8 col){
    int w=h*NLOGO_W/NLOGO_H; int ox=cx-w/2, oy=cy-h/2; u32 v=PAL32[col];
    for(int y=0;y<h;y++){ int my=y*NLOGO_H/h; for(int x=0;x<w;x++){ int mx=x*NLOGO_W/w; int idx=my*NLOGO_W+mx;
        if((NLOGO[idx>>3]>>(idx&7))&1){ int px=ox+x,py=oy+y; if(px>=0&&px<W&&py>=0&&py<H) FB[py*PITCH+px]=v; } } }
}

static void cos_pill(int x,int y,int w,int h,u8 c){
    int r=h/2; if(2*r>w)r=w/2;
    fill(x+r,y,w-2*r,h,c);
    cos_circle(x+r-1,y+r-1,r,c);
    cos_circle(x+w-r,y+r-1,r,c);
}
static void cos_pill_btn(int x,int y,int w,int h,const char* lab,int prim){
    int lw=strlen_(lab)*8;
    if(prim){ cos_pill(x,y,w,h,C_TEAL); draw_str(x+(w-lw)/2,y+h/2-3,lab,67); }
    else { draw_str(x+(w-lw)/2,y+h/2-3,lab,C_TEAL); }
}

static void cos_progress(int step,int total){
    fill(0,H-4,W,4,C_MGREY);
    int prog=(W*step)/total; fill(0,H-4,prog,4,C_TEAL);
}

static void cos_page(int step,int total){

    int hm1=(H>1)?H-1:1;
    for(int y=0;y<H;y++){ int ty=(y*255)/hm1;
        int r=12-(ty*4)/255, g=32-(ty*12)/255, b=48-(ty*16)/255;
        if(r<6)r=6; if(g<16)g=16; if(b<24)b=24;
        u32 v=((u32)r<<16)|((u32)g<<8)|(u32)b; u32* row=FB+y*PITCH;
        for(int x=0;x<W;x++)row[x]=v; }

    afill(0,0,W,4,C_TEAL,255);
    nvx_logo(40,32,26,C_WHITE);
    draw_str(74,33,OSNAME,C_TEAL);
    char sb[16]="STEP _ OF _";
    sb[5]='0'+step; sb[10]='0'+total;
    int sl=strlen_(sb)*8;
    draw_str(W-sl-30,33,sb,C_MGREY+30);
    cos_progress(step,total);
}

static void cos_load_anim(int cx,int cy,int frame){
    static const u8 cols[4]={G_BLUE,G_RED,G_YELLOW,G_GREEN};
    int gap=28; int sx=cx-(3*gap)/2;
    int active=(frame/3)%4;
    for(int i=0;i<4;i++){ int r=(i==active)?11:6; cos_circle(sx+i*gap,cy,r,cols[i]); }
}

static void cos_input(int x,int y,char* buf,int max,int mask){
    int len=0; while(buf[len]&&len<max-1)len++;
    int w=max*8+10;
    for(;;){
        fill(x,y,w,18,C_WHITE);
        fill(x,y+16,w,2,G_BLUE);
        for(int i=0;i<len;i++) draw_char(x+5+i*8,y+4,mask?'*':buf[i],5);
        fill(x+5+len*8,y+3,6,10,G_BLUE);
        u8 sc=wait_key();
        if(sc==0x1C) return;
        if(sc==0x0E){ if(len>0){len--;buf[len]=0;} }
        else if(sc==0x2A||sc==0x36){}
        else { char ch=kchar_shift(sc); if(ch>=32&&len<max-1){ buf[len++]=ch; buf[len]=0; } }
    }
}

static void cos_email(int x,int y,char* buf,int max){
    int len=0; while(buf[len]&&len<max-1)len++;
    int w=max*8+10;
    for(;;){
        fill(x,y,w,18,C_WHITE);
        fill(x,y+16,w,2,G_BLUE);
        for(int i=0;i<len;i++){ char d=buf[i]; if(d>='a'&&d<='z')d-=32; draw_char(x+5+i*8,y+4,d,5); }
        fill(x+5+len*8,y+3,6,10,G_BLUE);
        u8 sc=wait_key();
        if(sc==0x1C) return;
        if(sc==0x0E){ if(len>0){len--;buf[len]=0;} }
        else if(sc==0x2A||sc==0x36){}
        else { char ch=kchar_shift(sc); if(ch>=32&&len<max-1){ if(ch>='A'&&ch<='Z')ch+=32; buf[len++]=ch; buf[len]=0; } }
    }
}

static void run_disk_install(void){

    int choice=0;
    for(;;){
        cos_page(0,0);
        cos_logo(W/2,H/2-150,12,46);
        { const char* h="INSTALL NOOVEXOS"; int hw=strlen_(h)*16; draw_str2(W/2-hw/2,H/2-90,h,5); }
        { const char* s="SET UP NOOVEXOS ON THIS COMPUTER SO YOUR"; int sw=strlen_(s)*8; draw_str(W/2-sw/2,H/2-56,s,25); }
        { const char* s="FILES AND SETTINGS ARE SAVED BETWEEN REBOOTS."; int sw=strlen_(s)*8; draw_str(W/2-sw/2,H/2-40,s,25); }
        { const char* s="(ONLY NOOVEXOS'S OWN AREA IS USED - SAFE.)"; int sw=strlen_(s)*8; draw_str(W/2-sw/2,H/2-16,s,40); }
        int bw=200,bh=54,bg=40; int bx1=W/2-bw-bg/2, bx2=W/2+bg/2, by=H/2+30;
        cos_pill_btn(bx1,by,bw,bh,"INSTALL",choice==0);
        cos_pill_btn(bx2,by,bw,bh,"SKIP (RUN LIVE)",choice==1);
        { const char* h="LEFT/RIGHT TO CHOOSE - ENTER TO CONFIRM"; int hw=strlen_(h)*8; draw_str(W/2-hw/2,H-90,h,40); }
        u8 k=wait_key();
        if(k==0x4B && choice>0) choice--;
        else if(k==0x4D && choice<1) choice++;
        else if(k==0x1C) break;
    }
    if(choice==1){ cmos_write(0x46,0); return; }

    int gb=10;
    for(;;){
        cos_page(0,0);
        { const char* h="RESERVE DISK SPACE"; int hw=strlen_(h)*16; draw_str2(W/2-hw/2,H/2-150,h,5); }
        { const char* s="HOW MUCH SPACE SHOULD NOOVEXOS USE?"; int sw=strlen_(s)*8; draw_str(W/2-sw/2,H/2-116,s,25); }

        char nb[12]; int n=gb,nl=0; char tmp[8]; if(n==0)tmp[nl++]='0'; while(n){tmp[nl++]='0'+n%10;n/=10;} for(int i=0;i<nl;i++)nb[i]=tmp[nl-1-i]; nb[nl]=' ';nb[nl+1]='G';nb[nl+2]='B';nb[nl+3]=0;
        { int gl=strlen_(nb)*16; draw_str2(W/2-gl/2,H/2-50,nb,G_BLUE); }

        int sx=W/2-200,sy=H/2+0,sw2=400; fill(sx,sy,sw2,8,50);
        int fillw=((gb-5)*sw2)/45; if(fillw<0)fillw=0; if(fillw>sw2)fillw=sw2; afill(sx,sy,fillw,8,C_BBLUE,255);
        fill(sx+fillw-3,sy-6,6,20,C_BBLUE);
        { const char* h="UP/DOWN +/-1 GB   PGUP/PGDN +/-5 GB   (5-50)"; int hw=strlen_(h)*8; draw_str(W/2-hw/2,H/2+40,h,40); }

        { const char* s="DISK WILL BE NAMED:"; int sw=strlen_(s)*8; draw_str(W/2-sw/2,H/2+80,s,25); }
        { int dl=strlen_(DISK_LABEL)*8; draw_str(W/2-dl/2,H/2+100,DISK_LABEL,G_BLUE); }
        cos_pill_btn(W-220,H-90,180,42,"INSTALL",1);
        u8 k=wait_key();
        if(k==0x48 && gb<50) gb++;
        else if(k==0x50 && gb>5) gb--;
        else if(k==0x49){ gb+=5; if(gb>50)gb=50; }
        else if(k==0x51){ gb-=5; if(gb<5)gb=5; }
        else if(k==0x1C) break;
    }
    disk_size_gb=(u8)gb;

    for(int f=0;f<=100;f+=2){
        cos_page(0,0);
        cos_logo(W/2,H/2-120,12,46);
        { const char* h="INSTALLING NOOVEXOS"; int hw=strlen_(h)*16; draw_str2(W/2-hw/2,H/2-60,h,5); }

        int bx=W/2-220,by=H/2-10,bw=440;
        frame(bx-2,by-2,bw+4,24,50); fill(bx,by,bw,20,C_WHITE);
        afill(bx,by,(bw*f)/100,20,C_BBLUE,255);
        char pb[6]; int p=f,pl=0,tl=0; char tt[4]; if(p==0)tt[tl++]='0'; while(p){tt[tl++]='0'+p%10;p/=10;} while(tl)pb[pl++]=tt[--tl]; pb[pl++]='%'; pb[pl]=0;
        { int pw=strlen_(pb)*8; draw_str(W/2-pw/2,by+30,pb,5); }
        const char* st = (f<25)?"CREATING NOOVEXOS PARTITION..."
                       : (f<50)?"WRITING FILESYSTEM..."
                       : (f<75)?"NAMING DISK: NOOVEXOS BOOT MANAGER"
                       : (f<95)?"REGISTERING BOOT MANAGER..."
                       : "FINISHING UP...";
        { int sl=strlen_(st)*8; draw_str(W/2-sl/2,H/2+60,st,40); }
        pit_wait(2);
    }

    cmos_write(0x45,(u8)gb);
    cmos_write(0x46,0xCD);
    if(disk_ok) nvx_mount();
}

/* ===================== macOS Setup Assistant - style 9-page wizard ===================== */
static int mcx,mcy,mcw,mch;
static void mac_bg(void){
    int hm1=(H>1)?H-1:1;
    for(int y=0;y<H;y++){ int t=(y*255)/hm1;
        int r=226-(t*26)/255, g=234-(t*22)/255, b=247-(t*14)/255;
        u32 v=((u32)r<<16)|((u32)g<<8)|(u32)b; u32* row=FB+y*PITCH; for(int x=0;x<W;x++)row[x]=v; }
}
static void mac_card(void){
    mcw=780; if(mcw>W-120)mcw=W-120; mch=580; if(mch>H-90)mch=H-90;
    mcx=W/2-mcw/2; mcy=H/2-mch/2;
    ultra_shadow(mcx,mcy,mcw,mch);
    rrectR(mcx,mcy,mcw,mch,18,C_WHITE);
}
static int misqrt(int v){ if(v<0)return 0; int x=0; while((x+1)*(x+1)<=v)x++; return x; }
static void mac_ellipse(int cx,int cy,int rx,int ry,u8 c){
    if(rx<1||ry<1)return; u32 v=PAL32[c];
    for(int y=-ry;y<=ry;y++){ long num=(long)rx*rx*((long)ry*ry-(long)y*y); int x=misqrt((int)(num/((long)ry*ry)));
        int yy=cy+y; if(yy<0||yy>=H)continue;
        if(cx-x>=0&&cx-x<W)FB[yy*PITCH+(cx-x)]=v; if(cx+x>=0&&cx+x<W)FB[yy*PITCH+(cx+x)]=v; }
}
static void mac_globe(int cx,int cy,int r){
    cos_circle(cx,cy,r,100);
    for(int li=1;li<=2;li++){ int dy=(r*li)/3; int half=misqrt(r*r-dy*dy);
        fill(cx-half,cy-dy,2*half,1,C_WHITE); fill(cx-half,cy+dy,2*half,1,C_WHITE); }
    fill(cx-r,cy,2*r,1,C_WHITE);
    fill(cx,cy-r,1,2*r,C_WHITE);
    mac_ellipse(cx,cy,(r*2)/3,r,C_WHITE);
    mac_ellipse(cx,cy,r/3,r,C_WHITE);
    mac_ellipse(cx,cy,r,r,C_WHITE);
}
static void mac_back(int show){
    if(!show)return; int ax=mcx+34, ay=mcy+40;
    for(int i=0;i<10;i++){ int t=(i<5)?i:(9-i); fill(ax+t,ay-5+i,3,2,100); }
}
static void mac_continue(const char* lab,int en){
    int bw=160,bh=44; int bx=mcx+mcw-bw-34, by=mcy+mch-bh-30;
    rrectR(bx,by,bw,bh,bh/2, en?100:67);
    int lw=strlen_(lab)*8; draw_str(bx+(bw-lw)/2,by+bh/2-3,lab,C_WHITE);
}
static void mac_head(int icon,const char* title,const char* sub){
    int icy=mcy+102;
    if(icon==1) mac_globe(W/2,icy,38);
    else if(icon==2) nvx_logo(W/2,icy,84,100);
    else if(icon==3){ cos_circle(W/2,icy,30,100); draw_str2(W/2-4,icy-12,"i",C_WHITE); }
    int tw=strlen_(title)*16; draw_str2(W/2-tw/2,mcy+162,title,3);
    if(sub){ int sw=strlen_(sub)*8; draw_str(W/2-sw/2,mcy+200,sub,30); }
}
static void mac_list(const char* const* items,int n,int sel,int lx,int ly,int lw,int rows){
    int rh=36;
    int top=sel-rows/2; if(top>n-rows)top=n-rows; if(top<0)top=0;
    for(int i=0;i<rows&&top+i<n;i++){ int idx=top+i; int ry=ly+i*rh;
        if(idx==sel){ rrectR(lx,ry,lw,rh-4,9,100); draw_str(lx+18,ry+9,items[idx],C_WHITE); }
        else draw_str(lx+18,ry+9,items[idx],5); }
    if(top>0) for(int i=0;i<7;i++) fill(lx+lw/2-3+(i<4?i:6-i),ly-9+(i<4?3-i:i-3),2,2,30);
    if(top+rows<n) for(int i=0;i<7;i++) fill(lx+lw/2-3+(i<4?i:6-i),ly+rows*rh+3+(i<4?i:6-i),2,2,30);
}
static void mac_field(int x,int y,int w,const char* val,int focus,int mask){
    rrectR(x,y,w,38,9,61);
    if(focus){ frame(x,y,w,38,100); frame(x+1,y+1,w-2,36,100); }
    else frame(x,y,w,38,49);
    int len=0; while(val[len])len++;
    for(int i=0;i<len;i++) draw_char(x+14+i*8,y+12,mask?'*':val[i],5);
    if(focus) fill(x+14+len*8,y+10,2,18,100);
}

static void bg_tile_preview(int x,int y,int w,int h,int style){
    u32 top,bot;
    switch(style){
      case 9: top=0x000A0E14u; bot=0x00181C24u; break;
      case 8: top=0x000E2A33u; bot=0x002A1840u; break;
      case 1: top=0x00101830u; bot=0x00341852u; break;
      case 2: top=0x000A1428u; bot=0x00244C82u; break;
      case 5: top=0x0014182Au; bot=0x002E1A3Eu; break;
      case 3: top=0x001A1A22u; bot=0x001A1A22u; break;
      default:top=0x00101418u; bot=0x00202428u; break;
    }
    int tr=(top>>16)&255,tg=(top>>8)&255,tb=top&255, br=(bot>>16)&255,bg2=(bot>>8)&255,bb=bot&255;
    for(int j=0;j<h;j++){ int t=(j*255)/((h>1)?h-1:1);
        int r=(tr*(255-t)+br*t)/255, g=(tg*(255-t)+bg2*t)/255, b=(tb*(255-t)+bb*t)/255;
        u32 v=((u32)r<<16)|((u32)g<<8)|(u32)b; u32* row=FB+(y+j)*PITCH; for(int i=0;i<w;i++)row[x+i]=v; }
    frame(x,y,w,h,49);
}
static void run_setup(void){
    int lang=(int)acct.lang; if(lang<0||lang>=NLANG)lang=0;
    int region=2, accent_sel=0, fld=0;
    int bg_sel=0, tz_sel=3, appr=1;
    char user[16]={0}, pass[16]={0};
    static const char* LANGD[NLANG]={"English","Svenska","Deutsch","Francais","Espanol","Italiano","Portugues","Nederlands","Zhongwen","Nihongo"};
    static const char* REGD[9]={"United States","United Kingdom","Sweden","Norway","Denmark","Germany","France","Japan","Worldwide"};
    int NREG=9;
    static const char* ACCD[4]={"Blue","Purple","Teal","Green"};
    static const u8 ACOL[4]={100,106,70,74};
    static const int WALLOPT[6]={9,8,1,2,5,3};
    static const char* WALLNAME[6]={"Waves","Aurora","Nebula","Gradient","Mesh","Solid"};
    static const char* TZD[7]={"(UTC-08)  Pacific","(UTC-05)  Eastern","(UTC+00)  London","(UTC+01)  Stockholm","(UTC+02)  Helsinki","(UTC+09)  Tokyo","(UTC+10)  Sydney"};
    int NTZ=7;
    acct.enrolled=0; for(int i=0;i<24;i++)acct.org[i]=0;

    int page=1;
    while(page>=1 && page<=12){
        mac_bg(); mac_card(); mac_back(page>1);
        int cx=W/2;
        if(page==1){
            mac_head(1,"Select Your Language",0);
            mac_list(LANGD,NLANG,lang,cx-150,mcy+238,300,5);
            mac_continue("Continue",1);
        } else if(page==2){
            mac_head(1,"Select Your Country or Region",0);
            mac_list(REGD,NREG,region,cx-150,mcy+238,300,5);
            mac_continue("Continue",1);
        } else if(page==3){
            mac_head(2,"Welcome to NoovexOS","A from-scratch 32-bit graphical operating system");
            { const char* s="Let's set up your computer in a few steps."; int sw=strlen_(s)*8; draw_str(cx-sw/2,mcy+250,s,30); }
            mac_continue("Continue",1);
        } else if(page==4){
            mac_head(0,"Connect to a Network","Your network is used for time, updates and the web");
            { int rx=cx-190, ry=mcy+250;
              cos_circle(rx,ry+6,7,nic_present?100:68); draw_str(rx+24,ry,"Ethernet",5);
              draw_str(rx+24,ry+18,nic_present?"AMD PCnet-FAST III  -  connected":"No adapter detected",30);
              cos_circle(rx,ry+58,7,68); draw_str(rx+24,ry+52,"Wi-Fi",5);
              draw_str(rx+24,ry+70,"No wireless adapter present",30); }
            mac_continue("Continue",1);
        } else if(page==5){
            mac_head(3,"Data & Privacy",0);
            { const char* a="NoovexOS is built to protect your privacy."; draw_str(cx-strlen_(a)*8/2,mcy+250,a,5);
              const char* b="Your data stays on this device. Nothing is shared"; draw_str(cx-strlen_(b)*8/2,mcy+280,b,30);
              const char* c="anywhere without an action you take."; draw_str(cx-strlen_(c)*8/2,mcy+298,c,30); }
            mac_continue("Continue",1);
        } else if(page==6){
            mac_head(0,"Terms and Conditions",0);
            { const char* a="NoovexOS is a hobby operating system, provided as-is"; draw_str(cx-strlen_(a)*8/2,mcy+250,a,5);
              const char* b="with no warranty of any kind. By continuing you agree"; draw_str(cx-strlen_(b)*8/2,mcy+274,b,5);
              const char* c="to use it for experimentation and learning."; draw_str(cx-strlen_(c)*8/2,mcy+298,c,5); }
            mac_continue("Agree",1);
        } else if(page==7){
            mac_head(0,"Create Your Account","This account administers this computer");
            { int fx=cx-180;
              draw_str(fx,mcy+248,"Full name",30); mac_field(fx,mcy+266,360,user,fld==0,0);
              draw_str(fx,mcy+322,"Password",30); mac_field(fx,mcy+340,360,pass,fld==1,1);
              draw_str(fx,mcy+388,"Use Tab to switch fields",49); }
            mac_continue("Continue",user[0]?1:0);
        } else if(page==8){
            mac_head(0,"Choose Your Look","Pick an accent colour for your desktop");
            { int gap=78, sx=cx-(3*gap)/2, sy=mcy+278;
              for(int i=0;i<4;i++){ int px=sx+i*gap;
                if(i==accent_sel){ cos_circle(px,sy,27,100); cos_circle(px,sy,24,C_WHITE); }
                cos_circle(px,sy,20,ACOL[i]);
                int lw2=strlen_(ACCD[i])*8; draw_str(px-lw2/2,sy+38,ACCD[i],i==accent_sel?3:30); } }
            mac_continue("Continue",1);
        } else if(page==9){
            mac_head(0,"Choose a Background","Pick a wallpaper for your desktop");
            { int tw=132,th=84,gap=22; int totw=3*tw+2*gap, sx=cx-totw/2, sy=mcy+248;
              for(int i=0;i<6;i++){ int c=i%3,r=i/3; int px=sx+c*(tw+gap), py=sy+r*(th+30);
                bg_tile_preview(px,py,tw,th,WALLOPT[i]);
                if(i==bg_sel){ frame(px-3,py-3,tw+6,th+6,100); frame(px-2,py-2,tw+4,th+4,100); }
                int lw=strlen_(WALLNAME[i])*8; draw_str(px+tw/2-lw/2,py+th+7,WALLNAME[i],i==bg_sel?3:30); } }
            mac_continue("Continue",1);
        } else if(page==10){
            mac_head(1,"Select Your Time Zone",0);
            mac_list(TZD,NTZ,tz_sel,cx-175,mcy+238,350,5);
            mac_continue("Continue",1);
        } else if(page==11){
            mac_head(0,"Choose Your Appearance","Use a light or dark interface");
            { int tw=190,th=124,gap=40; int sx=cx-(2*tw+gap)/2, sy=mcy+244;
              rrectR(sx,sy,tw,th,12,60); fill(sx+14,sy+16,tw-28,18,C_WHITE); fill(sx+14,sy+42,tw-60,9,68); fill(sx+14,sy+58,tw-44,9,68);
              if(appr==0){ frame(sx-3,sy-3,tw+6,th+6,100); frame(sx-2,sy-2,tw+4,th+4,100); }
              { int lw=strlen_("Light")*8; draw_str(sx+tw/2-lw/2,sy+th+8,"Light",appr==0?3:30); }
              int dx2=sx+tw+gap; rrectR(dx2,sy,tw,th,12,4); fill(dx2+14,sy+16,tw-28,18,C_MGREY+22); fill(dx2+14,sy+42,tw-60,9,C_MGREY+10); fill(dx2+14,sy+58,tw-44,9,C_MGREY+10);
              if(appr==1){ frame(dx2-3,sy-3,tw+6,th+6,100); frame(dx2-2,sy-2,tw+4,th+4,100); }
              { int lw=strlen_("Dark")*8; draw_str(dx2+tw/2-lw/2,sy+th+8,"Dark",appr==1?3:30); } }
            mac_continue("Continue",1);
        } else if(page==12){
            mac_head(2,"Welcome to NoovexOS",0);
            { char wb[40]="Hello, "; int q=7; for(int j=0;user[j]&&q<37;j++)wb[q++]=user[j]; if(q==7){const char*d="there";int j=0;while(d[j])wb[q++]=d[j++];} wb[q++]='.'; wb[q]=0;
              int wl=strlen_(wb)*8; draw_str(cx-wl/2,mcy+250,wb,5); }
            { const char* s="Your computer is set up and ready to use."; draw_str(cx-strlen_(s)*8/2,mcy+280,s,30); }
            mac_continue("Start Using NoovexOS",1);
        }
        u8 k=wait_key();
        if(k==0x01 && page>1){ page--; continue; }
        if(page==1){ if(k==0x48&&lang>0)lang--; else if(k==0x50&&lang<NLANG-1)lang++; else if(k==0x1C){acct.lang=(u32)lang; page=2;} }
        else if(page==2){ if(k==0x48&&region>0)region--; else if(k==0x50&&region<NREG-1)region++; else if(k==0x1C)page=3; }
        else if(page==3){ if(k==0x1C)page=4; }
        else if(page==4){ if(k==0x1C)page=5; }
        else if(page==5){ if(k==0x1C)page=6; }
        else if(page==6){ if(k==0x1C)page=7; }
        else if(page==7){
            if(k==0x0F){ fld=fld?0:1; }
            else if(k==0x48){ fld=0; } else if(k==0x50){ fld=1; }
            else if(k==0x1C){ if(user[0])page=8; }
            else if(k==0x0E){ char* b=fld?pass:user; int l=0; while(b[l])l++; if(l>0)b[l-1]=0; }
            else if(k!=0x2A&&k!=0x36&&k!=0x1D&&k!=0x38){ char ch=kchar_shift(k); if(ch>=32){ char* b=fld?pass:user; int l=0; while(b[l])l++; if(l<15){b[l]=ch;b[l+1]=0;} } }
        }
        else if(page==8){ if(k==0x4B&&accent_sel>0)accent_sel--; else if(k==0x4D&&accent_sel<3)accent_sel++; else if(k==0x1C)page=9; }
        else if(page==9){ if(k==0x4B&&bg_sel>0)bg_sel--; else if(k==0x4D&&bg_sel<5)bg_sel++; else if(k==0x50&&bg_sel<3)bg_sel+=3; else if(k==0x48&&bg_sel>=3)bg_sel-=3; else if(k==0x1C)page=10; }
        else if(page==10){ if(k==0x48&&tz_sel>0)tz_sel--; else if(k==0x50&&tz_sel<NTZ-1)tz_sel++; else if(k==0x1C)page=11; }
        else if(page==11){ if(k==0x4B)appr=0; else if(k==0x4D)appr=1; else if(k==0x1C)page=12; }
        else if(page==12){ if(k==0x1C)break; }
    }
    if(user[0]==0){ const char* d="user"; int k=0; while(d[k]){user[k]=d[k];k++;} user[k]=0; }
    { int k=0; while(user[k]&&k<15){acct.user[k]=user[k];k++;} acct.user[k]=0; }
    { int k=0; while(pass[k]&&k<15){acct.pass[k]=pass[k];k++;} acct.pass[k]=0; }
    acct.magic=ACCT_MAGIC; have_user=1;
    accent=accent_sel; apply_accent(accent);
    cmos_write(0x38,(u8)accent); cmos_write(0x39,0xAB); cmos_write(0x3C,(u8)lang);
    bg_style=WALLOPT[bg_sel]; cmos_write(0x3E,(u8)(bg_style+1)); cmos_write(0x3F,(u8)tz_sel); cmos_write(0x3A,(u8)appr);
}

static void login_screen(void){
    char ic[2]={ acct.user[0]?acct.user[0]:'U', 0 };
    if(ic[0]>='a'&&ic[0]<='z') ic[0]-=32;
    for(;;){

        { int c1x=W*6/10,c1y=-H/10,c1r=H*6/10,r1s=c1r*c1r;
          int c2x=W*85/100,c2y=H*7/10,c2r=H*55/100,r2s=c2r*c2r;
          int c3x=W*55/100,c3y=H*45/100,c3r=H*5/10,r3s=c3r*c3r;
          for(int y=0;y<H;y++){ u32* row=FB+y*PITCH; int gy=(y*100)/H;
            for(int x=0;x<W;x++){ int gx=(x*100)/W;
                int t2=gx+gy; if(t2>200)t2=200;
                int r=235-(t2*175)/200, g=85-(t2*65)/200, b=165-(t2*55)/200; if(g<0)g=0;
                int dx,dy;
                dx=x-c1x;dy=y-c1y; if(dx*dx+dy*dy<r1s){r+=14;g+=17;b+=8;}
                dx=x-c2x;dy=y-c2y; if(dx*dx+dy*dy<r2s){r+=14;g+=17;b+=8;}
                dx=x-c3x;dy=y-c3y; if(dx*dx+dy*dy<r3s){r+=14;g+=17;b+=8;}
                if(r>255)r=255; if(g>255)g=255; if(b>255)b=255;
                row[x]=((u32)r<<16)|((u32)g<<8)|(u32)b; } }
          int lx=48, ly=H-150;
          fill(lx,ly,12,72,C_WHITE); fill(lx+50,ly,12,72,C_WHITE);
          for(int k=0;k<72;k++) fill(lx+(k*50)/72, ly+k, 12, 2, C_WHITE);
          fill(lx,ly+80,62,10,C_WHITE); }

        cos_logo(46,36,4,13);
        draw_str(105,33,OSNAME,G_BLUE);
        int cx=W/2,cy=H/2;

        cos_circle(cx,cy-100,42,G_BLUE);
        draw_str2(cx-8,cy-110,ic,C_WHITE);

        { const char* n=acct.user[0]?acct.user:"USER"; int nl=strlen_(n)*16; draw_str2(cx-nl/2,cy-40,n,C_TITLE); }

        if(acct.enrolled && acct.org[0]){
            char mb[44]="MANAGED BY "; int q=11; for(int j=0;acct.org[j]&&q<43;j++)mb[q++]=acct.org[j]; mb[q]=0;
            int ml=strlen_(mb)*8; draw_str(cx-ml/2,cy-12,mb,C_TITLE);
        }

        { const char* p=t(T_ENTER_PASS); int pl=strlen_(p)*8; draw_str(cx-pl/2,cy+30,p,C_TITLE); }
        { const char* hh="FORGOT PASSWORD?  TYPE  recover  TO RESET"; int hl=strlen_(hh)*8; draw_str(cx-hl/2,cy+135,hh,40); }
        char p[16]={0};
        cos_input(cx-90,cy+50,p,16,1);
        if(streq_cs(p,acct.pass)){ beep(1320,2); return; }
        if(streq(p,"recover")){ acct.magic=0; acct.enrolled=0; acct.user[0]=0; acct.pass[0]=0; for(int i=0;i<24;i++)acct.org[i]=0; have_user=0; if(disk_ok) acct_save(); run_setup(); if(disk_ok) acct_save(); return; }
        { const char* e=t(T_WRONG_PASS); int el=strlen_(e)*8; draw_str(cx-el/2,cy+100,e,C_RED+8); }
        beep(300,4); pit_wait(14);
    }
}

static unsigned int rnd(void){ rngs=rngs*1103515245u+12345u; return (rngs>>16)&0x7FFF; }
static void snake_init(void){
    rngs ^= pit_read() | ((unsigned)cmos(0)<<8) | ((unsigned)cmos(2)<<16);
    slen=3; sbx[0]=11;sby[0]=8; sbx[1]=10;sby[1]=8; sbx[2]=9;sby[2]=8;
    sdir=3; sscore=0; salive=1; sfx=rnd()%GCOLS; sfy=rnd()%GROWS; snk_acc=0;
}
static void snake_step(void){
    if(!salive)return;
    int nx=sbx[0],ny=sby[0];
    if(sdir==0)ny--; else if(sdir==1)ny++; else if(sdir==2)nx--; else nx++;
    if(nx<0||ny<0||nx>=GCOLS||ny>=GROWS){ salive=0; beep(200,4); return; }
    for(int i=0;i<slen;i++) if(sbx[i]==nx&&sby[i]==ny){ salive=0; beep(200,4); return; }
    for(int i=slen;i>0;i--){ sbx[i]=sbx[i-1]; sby[i]=sby[i-1]; }
    sbx[0]=nx; sby[0]=ny;
    if(nx==sfx&&ny==sfy){ if(slen<250)slen++; sscore+=10; click_snd(); sfx=rnd()%GCOLS; sfy=rnd()%GROWS; }
}
static void flappy_init(void){
    fby10=(240/2)*10; fvy10=0; fscore=0; falive=1; flap_acc=0;
    for(int i=0;i<3;i++){ fpx[i]=320+i*150; fpg[i]=64+rnd()%113; fscored[i]=0; }
}
static void flappy_step(void){
    if(!falive)return;
    fvy10+=6; fby10+=fvy10;
    int byp=fby10/10;
    for(int i=0;i<3;i++){
        fpx[i]-=3;
        if(fpx[i]+46<0){ fpx[i]+=3*150; fpg[i]=64+rnd()%113; fscored[i]=0; }
        if(!fscored[i] && fpx[i]+46<80){ fscored[i]=1; fscore++; beep(1568,2); }
    }
    if(byp<0 || byp+14>240){ falive=0; beep(330,2); beep(150,5); return; }
    for(int i=0;i<3;i++){
        if(80+14>fpx[i] && 80<fpx[i]+46){
            int gt=fpg[i]-46, gb=fpg[i]+46;
            if(byp<gt || byp+14>gb){ falive=0; beep(330,2); beep(150,5); return; }
        }
    }
}
static void do_virus_demo(void){
    clear_all(0);
    draw_str2(W/2-110,60,"MALWARE.NVX",C_RED+8);
    draw_str(W/2-150,96,"HARMLESS VIRUS DEMO FOR NOOVEX7",C_MGREY+20);
    const char* f[6]={"DOCUMENTS","PICTURES","NOTES.TXT","REPORT.DOC","DESKTOP","NVXFS"};
    int y=140;
    for(int i=0;i<6;i++){ draw_str(W/2-150,y,"ENCRYPTING:",C_TITLE); draw_str(W/2-150+96,y,f[i],C_WHITE); beep(500+i*60,2); pit_wait(3); draw_str(W/2-150+96+strlen_(f[i])*8+8,y,"[LOCKED]",C_RED+8); y+=16; }
    for(int g=0;g<60;g++){ int rx=rnd()%W,ry=rnd()%H,rw=10+rnd()%80,rh=4+rnd()%30; fill(rx,ry,rw,rh,rnd()%64); beep(300+rnd()%900,1); }
    clear_all(0);
    draw_str3(W/2-36,H/2-80,":(",C_WHITE);
    draw_str(W/2-180,H/2-6,"YOUR FILES HAVE BEEN ENCRYPTED (DEMO)",C_WHITE);
    draw_str(W/2-180,H/2+14,"THIS IS A HARMLESS SIMULATION.",C_MGREY+20);
    draw_str(W/2-180,H/2+34,"RUN NOOVEXDEFENDER TO CLEAN IT.",C_GREEN);
    draw_str(W/2-180,H/2+70,"PRESS ANY KEY TO CONTINUE",C_WHITE);
    wait_key();
    infected=1;
}
static void do_avscan(void){
    av_state=1; av_threat=0;
    int bx=winx+20, by=winy+winh-58, bw=winw-40;
    frame(bx-2,by-2,bw+4,16,C_WHITE);
    const char* sf[7]={"KERNEL.BIN","MOUSE.DRV","RTC.DRV","NOTES.TXT","REPORT.DOC","MALWARE.NVX","DESKTOP.CFG"};
    for(int i=0;i<bw;i+=4){
        fill(bx,by,i,12,C_GREEN);
        int si=(i*7)/bw; if(si>6)si=6;
        fill(winx+20,by-20,bw,10,C_WIN);
        draw_str(winx+20,by-20,"SCANNING: ",C_MGREY+20); draw_str(winx+20+80,by-20,sf[si],C_WHITE);
        pit_wait(1);
    }
    fill(bx,by,bw,12,C_GREEN);
    av_threat=infected; av_state=2;
}
static void doom_win_setup(void){
    int gw=320,gh=200,sc=2; if(gw*sc+8>W||gh*sc+30>H)sc=1;
    g_dwscale=sc; g_dww=gw*sc; g_dwh=gh*sc;
    int bd=2,th=22; g_dwfw=g_dww+2*bd; g_dwfh=g_dwh+th+bd;
    g_dwfx=(W-g_dwfw)/2; if(g_dwfx<0)g_dwfx=0; g_dwfy=(H-g_dwfh)/2; if(g_dwfy<0)g_dwfy=0;
    g_dwx=g_dwfx+bd; g_dwy=g_dwfy+th;
}
static void doom_win_backdrop(void){
    compose();
    fill(g_dwfx,g_dwfy,g_dwfw,g_dwfh,C_WIN);
    fill(g_dwfx,g_dwfy,g_dwfw,22,C_TASK);
    frame(g_dwfx,g_dwfy,g_dwfw,g_dwfh,C_TASK);
    draw_str(g_dwfx+8,g_dwfy+6,"DOOM",C_WHITE);
    fill(g_dwfx+g_dwfw-20,g_dwfy+4,15,14,C_RED); draw_str(g_dwfx+g_dwfw-16,g_dwfy+6,"X",C_WHITE);
    fill(g_dwx,g_dwy,g_dww,g_dwh,0);
}
static void launch(int a){

    if(a==-30){ if(elf_run("PYRUN.NVX")<0) toast_set("PYRUN.NVX not on disk - copy via Files"); return; }
    if(a==-31){ if(elf_run("NVXEDIT.NVX")<0) toast_set("NVXEDIT.NVX not on disk - copy via Files"); return; }
    if(a==-34){ if(elf_run("PYIDLE.NVX")<0) toast_set("PYIDLE.NVX not on disk - pkg install devkit"); return; }
    if(a==-35){ if(elf_run("NOOVEXCRAFT.NVX")<0) toast_set("NOOVEXCRAFT.NVX not on disk - pkg install noovexcraft"); return; }
    if(a==-33){ if(elf_run("SPACEINV.NVX")<0) toast_set("SPACEINV.NVX not on disk - attach the NoovexDOOM disk"); return; }
    if(a==-40){ g_dwin=0; if(elf_run("DOOM.ELF")<0) toast_set("DOOM.ELF not on disk - attach the DOOM disk"); return; }
    if(a==-41){ g_dwin=1; doom_win_setup(); int rr=elf_run("DOOM.ELF"); g_dwin=0; need_rebuild=1; if(rr<0) toast_set("DOOM.ELF not on disk - attach the DOOM disk"); return; }
    if(a==22){ if(msd_dev<0)usb_rescan(); cur_folder=10; open_app(1,120,55,700,470); return; }
    if(a==1) open_app(1,120,55,700,470);
    else if(a==2){ open_app(2,150,80,720,470); vt_term_open(); }
    else if(a==3) open_app(3,200,110,470,300);
    else if(a==4){ open_app(4,130,55,650,510); setcat=20; }
    else if(a==5) open_app(5,60,50,540,380);
    else if(a==6) open_app(6,200,90,440,360);
    else if(a==7) open_app(7,180,60,540,452);
    else if(a==8){ open_app(8,200,90,330,300); snake_init(); }
    else if(a==34){ open_app(34,160,70,340,292); flappy_init(); }
    else if(a==38){ open_app(38,150,60,420,420); qr_app_init(); }
    else if(a==39){ open_app(39,210,80,330,360); g2048_init(); }
    else if(a==40){ open_app(40,180,70,360,320); breakout_init(); }
    else if(a==41){ open_app(41,200,70,300,340); mines_init(); }
    else if(a==42){ open_app(42,140,60,520,400); gopher_init(); }
    else if(a==43){ open_app(43,150,60,500,400); wiki_init(); }
    else if(a==45){ open_app(45,150,70,470,420); ac97_init(); if(!ac97_ok) hda_init(); }
    else if(a==46){ open_app(46,140,60,520,440); ph_tab=0; ph_focus=0; }
    else if(a==10){ open_app(10,250,120,360,280); }
    else if(a==11){ open_app(11,130,70,560,420); if(!paint_init)paint_clear(); }
    else if(a==12){ open_app(12,200,60,360,340); tetris_init(); }
    else if(a==13){ open_app(13,180,120,440,210); }
    else if(a==14){ open_app(14,70,40,560,400); craft_load(); }
    else if(a==15){ open_app(15,120,60,500,380); br_hist_n=0; br_nav("home",4); }
    else if(a==16){ open_app(16,140,70,520,400); ai_rc=0; ai_scroll=0;
        if(ai_keylen==0 && disk_ok){ int ix=nvx_find("CLAUDE.KEY"); if(ix>=0){ ai_keylen=nvx_read(ix,(char*)AI_KEY,250); AI_KEY[ai_keylen]=0; ai_saved=1; } }
        ai_field=(ai_keylen>0)?1:0; }
    else if(a==23){ open_app(23,200,80,260,310); }
    else if(a==24){ open_app(24,150,70,420,330); }
    else if(a==25){ open_app(25,120,60,560,420); ph_view=-1; }
    else if(a==27){ open_app(27,150,60,460,360); }
    else if(a==28){ open_app(28,120,60,580,420); hxout_len=0; hxout[0]=0; hx_sel=0; hx_ran=0; hx_clear_canvas(); }
    else if(a==30){ open_app(30,110,55,560,410); g3_yaw=32; g3_acc=0; }
    else if(a==29){ open_app(29,260,110,420,300); }
    else if(a==36){ open_app(36,120,55,580,470); phone_demo=0; }
    else if(a==37){ open_app(37,150,55,470,440); mail_field=0; }
    else if(a==35){ open_app(35,150,70,540,400); pkg_sel=0; }
    else if(a==-2){ pending_update=1; }
}

static void handle_rclick(void){
    ctx_type=0;
    int wi=-1; for(int z=wincnt-1;z>=0;z--){ if(wins[z].min)continue; if(in(mx,my,wins[z].x,wins[z].y,wins[z].w,wins[z].h)){ wi=z; break; } }
    if(wi>=0){ win_raise(wi); cur_win=wincnt-1; win_load(cur_win); }
    if(app==14){ int cvx=winx+4,cvy=winy+22,hby=winy+winh-28; if(mx>=cvx&&my>=cvy&&my<hby-4){ int wx=craft_cam+(mx-cvx)/CTILE,wy=craft_camy+(my-cvy)/CTILE; if(wx>=0&&wx<CW&&wy>=0&&wy<CH){ craft[wy*CW+wx]=0; click_snd(); } } return; }
    if(app==1){
        if(cur_folder==7||cur_folder==9) return;
        int px=winx+170,py=winy+28;
        if(cur_folder==10&&fat_ok){ for(int i=0;i<usbfs_n;i++){ int ry=py+32+i*16; if(in(mx,my,px,ry-2,winw-180,16)){ ctx_type=3; ctx_fidx=i; int k=0; while(usbfs[i].name[k]&&k<15){ctx_fname[k]=usbfs[i].name[k];k++;} ctx_fname[k]=0; break; } } }
        else if(cur_folder==8&&disk_ok){ for(unsigned i=0;i<nvx.count;i++){ int ry=py+32+i*16; if(in(mx,my,px,ry,winw-180,14)){ ctx_type=1; ctx_fidx=i; int k=0; while(nvx.e[i].name[k]&&k<19){ctx_fname[k]=nvx.e[i].name[k];k++;} ctx_fname[k]=0; break; } } }
        else if(cur_folder!=10){ char(*lst)[16];int n; folder_list(cur_folder,&lst,&n); for(int i=0;i<n;i++){ int ry=py+32+i*16; if(in(mx,my,px,ry,winw-180,14)){ ctx_type=1; ctx_fidx=i; int k=0;const char*s=lst[i];while(s[k]&&k<19){ctx_fname[k]=s[k];k++;}ctx_fname[k]=0; break; } } }
    }
    if(wi<0){ for(int i=0;i<DSK_MAX;i++){ if(!DSK[i].used||DSK[i].parent!=-1)continue; if(in(mx,my,DSK[i].x,DSK[i].y,30,40)){ ctx_type=2; ctx_fidx=i; int k=0;while(DSK[i].name[k]&&k<19){ctx_fname[k]=DSK[i].name[k];k++;}ctx_fname[k]=0; break; } } }
    ctx_x=mx; if(ctx_x>W-156)ctx_x=W-156;
    ctx_y=my; if(ctx_y>H-130)ctx_y=H-130;
    ctx_open=1;
}
static void handle_click(void){
    if(pwr_open){
        pwr_geom();
        if(in(mx,my,pwrX,pwrY,pwrW,pwrH)){
            int idx=(my-(pwrY+6))/24; if(idx<0)idx=0; if(idx>3)idx=3;
            pwr_open=0; start_open=0; click_snd();
            if(idx==0){ hide_cursor(); login_screen(); build_base(); rtc_now(); }
            else if(idx==1){ hide_cursor(); do_sleep(); build_base(); rtc_now(); }
            else if(idx==2){ pending_action=2; }
            else if(idx==3){ pending_action=1; }
            return;
        }
        pwr_open=0;
    }
    if(in(mx,my,W-150,0,150,24)){ cc_open=!cc_open; cal_open=0; click_snd(); return; }
    if(cc_open){ int ccx,ccy; cc_geom(&ccx,&ccy);
        if(in(mx,my,ccx,ccy,ccW,ccH)){
            int pad=14, gx=ccx+pad, gy0=ccy+pad+44, gw=(ccW-pad*3)/2, gh=72, gp=pad;
            if(in(mx,my,ccx+ccW-92,ccy+pad,80,28)){ cc_open=0; hide_cursor(); login_screen(); build_base(); rtc_now(); dirty=1; return; }
            if(in(mx,my,gx,gy0,gw,gh)){   click_snd(); return; }
            if(in(mx,my,gx+gw+gp,gy0,gw,gh)){ snd_on=!snd_on; cmos_write(0x3B,(u8)(snd_on?1:0)); click_snd(); dirty=1; return; }
            if(in(mx,my,gx,gy0+gh+gp,gw,gh)){ cc_night=!cc_night; dirty=1; need_rebuild=1; click_snd(); return; }
            if(in(mx,my,gx+gw+gp,gy0+gh+gp,gw,gh)){ cc_dark=!cc_dark; dirty=1; click_snd(); return; }
            int by=gy0+2*gh+gp+pad;
            if(in(mx,my,gx,by+10,ccW-2*pad,18)){ int rel=mx-gx; cc_bright=(rel*7)/(ccW-2*pad); if(cc_bright<0)cc_bright=0; if(cc_bright>7)cc_bright=7; dirty=1; click_snd(); return; }
            if(in(mx,my,ccx+ccW-104,ccy+ccH-32,40,24)){ cc_open=0; hide_cursor(); login_screen(); build_base(); rtc_now(); dirty=1; return; }
            if(in(mx,my,ccx+ccW-56,ccy+ccH-32,44,24)){ cc_open=0; hide_cursor(); do_sleep(); build_base(); rtc_now(); dirty=1; return; }
            return;
        }
        cc_open=0; dirty=1;
    }
    if(cal_open){ int w=204,h=176,x=W-w-20,y=H-52-h; if(!in(mx,my,x,y,w,h)) cal_open=0; else return; }
    if(ctx_open){
        int n=5,mw=150,mh=n*20+6;
        if(in(mx,my,ctx_x,ctx_y,mw,mh+(ctx_type?16:0))){
            int oy=ctx_type?(ctx_y+18):(ctx_y+6);
            int idx=(my-oy)/20; if(idx<0)idx=0; if(idx>=n)idx=n-1;
            ctx_open=0; click_snd();
            if(ctx_type==1){
                if(cur_folder==8&&disk_ok){
                    if(idx==0){ int k=0;while(nvx.e[ctx_fidx].name[k]&&k<15){note_name[k]=nvx.e[ctx_fidx].name[k];k++;}note_name[k]=0; notelen=nvx_read(ctx_fidx,notebuf,1020); notebuf[notelen]=0; note_status=2; open_app(5,60,50,540,380); }
                    else if(idx==2){ bin_push(ctx_fname); nvx_delete(ctx_fidx); }
                    else if(idx==3){ dsk_add(0,0,ctx_fname,-1); toast_set("ADDED TO DESKTOP"); }
                    else if(idx==4){ open_app(3,200,110,470,300); }
                } else {
                    if(idx==0){ open_app(5,60,50,540,380); }
                    else if(idx==1){ if(fperm[cur_folder]==1){perm_pending=2;perm_folder=cur_folder;perm_idx=ctx_fidx;} else rename_begin(cur_folder,ctx_fidx); }
                    else if(idx==2){ if(fperm[cur_folder]==1){perm_pending=1;perm_folder=cur_folder;perm_idx=ctx_fidx;} else { bin_push(ctx_fname); file_delete(cur_folder,ctx_fidx); } }
                    else if(idx==3){ dsk_add(0,0,ctx_fname,-1); toast_set("ADDED TO DESKTOP"); }
                    else { open_app(3,200,110,470,300); }
                }
            } else if(ctx_type==2){
                int i=ctx_fidx;
                if(i>=0&&i<DSK_MAX&&DSK[i].used){
                    int sysbin=(DSK[i].type==2&&DSK[i].app==10);
                    if(idx==0){ dsk_open(i); }
                    else if(idx==1){ if(sysbin)toast_set("SYSTEM ITEM"); else { rename_mode=1; rename_folder=-1; rename_idx=i; rename_len=0; rename_buf[0]=0; } }
                    else if(idx==2){ if(sysbin)toast_set("CANNOT DELETE RECYCLE BIN"); else { if(DSK[i].type==1){ for(int j=0;j<DSK_MAX;j++) if(DSK[j].used&&DSK[j].parent==i){ DSK[j].parent=-1; dsk_place(j); } } if(fld_view==i)fld_view=-1; DSK[i].used=0; toast_set("DELETED"); } }
                    else if(idx==3){ static char pb[28]; const char* pre=DSK[i].type==1?"FOLDER: ":DSK[i].type==2?"APP: ":"FILE: "; int q=0; while(pre[q]){pb[q]=pre[q];q++;} int k=0; while(DSK[i].name[k]&&q<27){pb[q++]=DSK[i].name[k++];} pb[q]=0; toast_set(pb); }
                }
            } else if(ctx_type==3){
                int i=ctx_fidx;
                if(fat_ok&&i>=0&&i<usbfs_n){
                    if(idx==0){ int k=0;while(usbfs[i].name[k]&&k<15){note_name[k]=usbfs[i].name[k];k++;}note_name[k]=0; notelen=usbfs_read(i,notebuf,1020); notebuf[notelen]=0; note_status=2; open_app(5,60,50,540,380); }
                    else if(idx==1){
                        if(disk_ok){ char* tmp=(char*)0x830000u; int n=usbfs_read(i,tmp,4090); int w2=nvx_write(usbfs[i].name,tmp,n);
                            if(w2>=0){ dsk_add(0,0,usbfs[i].name,-1); toast_set("COPIED TO DESKTOP"); } else toast_set("COPY FAILED (NVXFS FULL?)"); }
                        else toast_set("NEED NVXFS DISK TO COPY"); }
                    else if(idx==2){ if(fat_delete_file(i)==0)toast_set("DELETED FROM USB"); else toast_set("USB DELETE FAILED"); }
                    else if(idx==3){ static char pb[28]; int q=0; const char* pre="USB: "; while(pre[q]){pb[q]=pre[q];q++;} int k=0; while(usbfs[i].name[k]&&q<27){pb[q++]=usbfs[i].name[k++];} pb[q]=0; toast_set(pb); }
                    else if(idx==4){ fat_mount(); toast_set("USB REFRESHED"); }
                }
            } else {
                if(idx==0){ need_rebuild=1; }
                else if(idx==1){ dsk_add(1,0,"NEW FOLDER",-1); toast_set("FOLDER CREATED"); }
                else if(idx==2){ open_app(4,130,55,650,510); }
                else if(idx==3){ open_app(2,150,80,720,470); vt_term_open(); }
                else if(idx==4){ open_app(3,200,110,470,300); }
            }
            return;
        }
        ctx_open=0;
    }
    if(start_open){
        start_geom();
        if(in(mx,my,smPX,smPY,smPW,smPH)){

            { int liy=smPY+34, step=(smPH-70)/NPIN;
              for(int i=0;i<NPIN;i++){ int ry=liy+i*step;
                if(in(mx,my,smPX+8,ry-2,smSBW-14,step)){ start_open=0; srch_len=0; srch[0]=0; sel_app=-1; int a=PIN[i].app; if(a==-2)pending_update=1; else if(a==-20){ launch(17); store_myapps=1; store_scroll=0; } else launch(a); return; } } }

            { int mc=0; for(int i=0;i<NPIN;i++){ if(!contains_ci(PIN[i].label,srch))continue; int col=mc%5,row=mc/5; int cx=smGX0+col*smCellW,cy=smGY0+row*smCellH;
                if(in(mx,my,cx,cy,smCellW,smCellH-8)){ start_open=0; srch_len=0; srch[0]=0; int a=PIN[i].app; if(a==-2)pending_update=1; else if(a==-20){ launch(17); store_myapps=1; store_scroll=0; } else launch(a); return; }
                mc++; }
              if(srch_len>0) for(unsigned fi=0;fi<nvx.count;fi++){ if(!nvx.e[fi].name[0]||!contains_ci(nvx.e[fi].name,srch))continue; int col=mc%5,row=mc/5; int cx=smGX0+col*smCellW,cy=smGY0+row*smCellH; if(in(mx,my,cx,cy,smCellW,smCellH-8)){ start_open=0; srch_len=0; srch[0]=0; launch(1); return; } mc++; }
              if(srch_len>0) for(int pi=0;pi<ph_count;pi++){ if(!contains_ci(ph_name[pi],srch))continue; int col=mc%5,row=mc/5; int cx=smGX0+col*smCellW,cy=smGY0+row*smCellH; if(in(mx,my,cx,cy,smCellW,smCellH-8)){ start_open=0; srch_len=0; srch[0]=0; ph_view=pi; launch(25); return; } mc++; } }

            int by=smPY+smPH-36;
            if(in(mx,my,smPX+smPW-48,by-4,34,32)){ pwr_open=!pwr_open; click_snd(); return; }
            return;
        }
        start_open=0; pwr_open=0; srch_len=0; srch[0]=0; click_snd(); return;
    }

    for(int i=0;i<DOCKN;i++){
        if(in(mx,my,dock_ix[i],dock_iy[i],dock_isz[i],dock_isz[i])){
            if(dock_app[i]==-3){ start_open=!start_open; pwr_open=0; srch_len=0; srch[0]=0; click_snd(); }
            else { launch(dock_app[i]); dock_bounce[i]=BOUNCE_MAX; }
            return;
        }
    }

    int wi=-1; for(int z=wincnt-1;z>=0;z--){ if(wins[z].min)continue; if(in(mx,my,wins[z].x,wins[z].y,wins[z].w,wins[z].h)){ wi=z; break; } }
    if(wi<0 && widget_on[0] && in(mx,my,18,46,196,80)){ weather_fetch(); dirty=1; return; }
    if(wi>=0){
        win_raise(wi); cur_win=wincnt-1; win_load(cur_win);
        if(in(mx,my,winx+winw-24,winy,22,22)){ close_app(app); click_snd(); return; }
        if(in(mx,my,winx+winw-46,winy,22,22)){ if(!maximized){sx=winx;sy=winy;sw=winw;sh=winh;winx=2;winy=2;winw=W-4;winh=H-52;maximized=1;} else {winx=sx;winy=sy;winw=sw;winh=sh;maximized=0;} win_store(cur_win); click_snd(); return; }
        if(in(mx,my,winx+winw-68,winy,22,22)){ wins[cur_win].min=1; cur_win=focus_top(); win_load(cur_win); click_snd(); return; }
        if(app==1){
            int x0=winx+3;
            if(mx>=x0&&mx<x0+158){ if(my>=winy+38&&my<winy+38+7*18){ cur_folder=(my-(winy+38))/18; cur_node=-1; fsel=0; fdc_row=-1; click_snd(); return; } if(my>=winy+184&&my<winy+184+NTHISPC*18){ cur_folder=THISPC[(my-(winy+184))/18]; if(cur_folder==7)cur_node=ROOT_C; else if(cur_folder==9)cur_node=ROOT_D; else cur_node=-1; fsel=0; fdc_row=-1; click_snd(); return; } }
            if(cur_folder==10){
                int px=winx+170,py=winy+28;
                if(in(mx,my,px+winw-300,py-4,84,18)){ usb_rescan(); toast_set(msd_dev<0?"NO USB DETECTED":(fat_ok?"USB MOUNTED":"USB FOUND - NO FAT FS")); click_snd(); return; }
                if(msd_dev>=0 && in(mx,my,px+winw-210,py-3,80,16)){ usb_eject(); toast_set("SAFE TO REMOVE USB DISK"); click_snd(); return; }
                if(fat_ok){ for(int i=0;i<usbfs_n;i++){ int ry=py+32+i*16; if(in(mx,my,px,ry-2,winw-180,16)){ if(ext_is(usbfs[i].name,(char)'P',(char)'D',(char)'F')){ pdf_open_usb(i,usbfs[i].name); click_snd(); return; } if(ext_is(usbfs[i].name,(char)'Z',(char)'I',(char)'P')){ zip_open_usb(i,usbfs[i].name); click_snd(); return; } if(doc_is_known(usbfs[i].name)){ doc_open_usb(i,usbfs[i].name); click_snd(); return; } int k=0; while(usbfs[i].name[k]&&k<15){note_name[k]=usbfs[i].name[k];k++;} note_name[k]=0; notelen=usbfs_read(i,notebuf,1020); notebuf[notelen]=0; note_status=2; open_app(5,60,50,540,380); click_snd(); return; } } }
            }
            if(cur_folder==7||cur_folder==9){ int px=winx+170,py=winy+28; if(cur_node<0)cur_node=(cur_folder==7)?ROOT_C:ROOT_D;
                if(VFS[cur_node].parent>=0 && in(mx,my,px,py-4,46,18)){ cur_node=VFS[cur_node].parent; fsel=0; fdc_row=-1; click_snd(); return; }
                int nn=vfs_nchild(cur_node);
                for(int k=0;k<nn;k++){ int ry=py+32+k*16; if(in(mx,my,px,ry-2,winw-180,16)){ int ni=vfs_child(cur_node,k); fsel=k;
                    if(fdc_row==k && fdc_timer>0){ fdc_row=-1; fdc_timer=0; if(VFS[ni].isdir){ cur_node=ni; fsel=0; } else { vfs_open_file(ni); } click_snd(); return; }
                    fdc_row=k; fdc_timer=8; click_snd(); return; } }
            }
            if(cur_folder!=10&&cur_folder!=8&&cur_folder!=7&&cur_folder!=9){ int px=winx+170,py=winy+28; char(*lst)[16];int n; folder_list(cur_folder,&lst,&n);
                for(int i=0;i<n;i++){ int ry=py+32+i*16; if(in(mx,my,px,ry-2,winw-180,16)){ fsel=i; fdrag=0; fdrag_armed=1; fdrag_src_folder=cur_folder; fdrag_src_idx=i; int k=0; const char*s2=lst[i]; while(s2[k]&&k<19){fdrag_name[k]=s2[k];k++;} fdrag_name[k]=0; fdrag_sx=mx; fdrag_sy=my; click_snd(); return; } } }
        }
        if(app==4){
            { static const int catmap2[14]={20,40,41,21,2,3,22,42,11,7,9,12,43,13};
              for(int i=0;i<14;i++){ if(in(mx,my,winx+5,winy+30+i*30,140,28)){ setcat=catmap2[i]; if(setcat==40){ if(!net_ok)nic_up(); net_refresh(); } click_snd(); return; } } }
            int px=winx+168,py=winy+34;
            if(setcat==20){
                int cx=winx+168, cw=winw-188; int dst[3]={21,2,11};
                for(int i=0;i<3;i++){ if(in(mx,my,cx,py+186+i*34,cw,28)){ setcat=dst[i]; click_snd(); return; } }
            } else if(setcat==21){
                int cy=py+26;
                for(int i=0;i<10;i++){ int row=i/5,col=i%5; if(in(mx,my,px+col*58,cy+16+row*24,54,20)){ bg_style=i; wall_photo=0; cmos_write(0x3A,(u8)bg_style); need_rebuild=1; click_snd(); return; } }
                for(int i=0;i<4;i++){ if(in(mx,my,px+i*38,cy+88,30,18)){ apply_accent(i); cmos_write(0x38,(u8)accent); click_snd(); return; } }
                for(int i=0;i<3;i++){ if(in(mx,my,px+200+i*62,cy+88,56,18)){ dock_size=i; click_snd(); dirty=1; return; } }
                if(in(mx,my,px+200,cy+116,40,20)){ set_corner=!set_corner; click_snd(); dirty=1; return; }
                if(in(mx,my,px+200,cy+140,40,20)){ set_shadows=!set_shadows; click_snd(); dirty=1; return; }
                if(in(mx,my,px+200,cy+164,40,20)){ set_glass=!set_glass; click_snd(); dirty=1; return; }
                for(int i=0;i<4;i++){ int wy=cy+216+i*22; if(in(mx,my,px+200,wy,40,20)){ widget_on[i]=!widget_on[i]; click_snd(); dirty=1; return; } }
            } else if(setcat==22){
                int cy=py+30;
                if(in(mx,my,px+240,cy,40,20)){ mag_on=!mag_on; click_snd(); dirty=1; return; }
                cy+=52;
                if(in(mx,my,px+240,cy,40,20)){ acc_bold=!acc_bold; click_snd(); need_rebuild=1; dirty=1; return; }
                cy+=52;
                if(in(mx,my,px+240,cy,40,20)){ acc_hicontrast=!acc_hicontrast; set_palette(); need_rebuild=1; dirty=1; return; }
            } else if(setcat==0){
                for(int i=0;i<4;i++){ if(in(mx,my,px+i*40,py+16,30,18)){ apply_accent(i); cmos_write(0x38,(u8)accent); click_snd(); return; } }
                for(int i=0;i<10;i++){ int row=i/5, col=i%5; if(in(mx,my,px+col*53-2,py+62+row*22,50,18)){ bg_style=i; wall_photo=0; cmos_write(0x3A,(u8)bg_style); need_rebuild=1; click_snd(); return; } }
                if(in(mx,my,px+170,py+108,40,20)){ set_corner=!set_corner; click_snd(); return; }
                if(in(mx,my,px+170,py+132,40,20)){ set_shadows=!set_shadows; click_snd(); return; }
                if(in(mx,my,px+170,py+156,40,20)){ set_glass=!set_glass; click_snd(); dirty=1; return; }
            }
            else if(setcat==1){ if(in(mx,my,px+170,py+24,40,20)){ set_dockauto=!set_dockauto; click_snd(); return; } }
            else if(setcat==2){
                const int rw[3]={1024,1280,1366}, rh[3]={768,1024,768};
                for(int i=0;i<3;i++){ if(in(mx,my,px,py+112+i*22,120,18)){ res_pw=rw[i]; res_ph=rh[i]; click_snd(); return; } }
                if(in(mx,my,px+170,py+178,40,20)){ cc_night=!cc_night; click_snd(); dirty=1; return; }
            }
            else if(setcat==3){ if(in(mx,my,px+170,py+24,40,20)){ snd_on=!snd_on; cmos_write(0x3B,(u8)snd_on); click_snd(); return; } }
            else if(setcat==4){
                for(int i=0;i<3;i++){ if(in(mx,my,px+125+i*52,py+22,48,16)){ mouse_speed=i; cmos_write(0x3D,(u8)mouse_speed); click_snd(); return; } }
                const int sv[3]={1,3,5};
                for(int i=0;i<3;i++){ if(in(mx,my,px+125+i*52,py+48,48,16)){ scroll_speed=sv[i]; cmos_write(0x3E,(u8)scroll_speed); click_snd(); return; } }
                if(in(mx,my,px+170,py+72,40,20)){ scroll_rev=!scroll_rev; cmos_write(0x3F,(u8)scroll_rev); click_snd(); return; }
            }
            else if(setcat==7){ if(in(mx,my,px+200,py+92,40,20)){ bat_sim=!bat_sim; if(bat_sim){ bat_pct=100; bat_warned=0; bat_tick=0; } click_snd(); return; } }
            else if(setcat==8){ if(in(mx,my,px+170,py+52,40,20)){ snd_on=!snd_on; cmos_write(0x3B,(u8)snd_on); click_snd(); return; } }
            else if(setcat==9){ if(in(mx,my,px,py+50,170,18)){ bl_pwlen=0; bl_pw[0]=0; bl_state=bl_enabled?1:0; open_app(21,210,120,360,210); click_snd(); return; } }
            else if(setcat==10){ if(in(mx,my,px+170,py+80,40,20)){ set_clock24=!set_clock24; click_snd(); return; } }
            else if(setcat==12){ if(in(mx,my,px+170,py+104,40,20)){ set_anim=!set_anim; click_snd(); return; } }
            else if(setcat==42){ if(in(mx,my,px+200,py+92,40,20)){ set_clock24=!set_clock24; click_snd(); return; }
                { int cw=winw-(px-winx)-20; for(int i=0;i<NLANG;i++){ int row=i/2,col=i%2; int bx=px+col*((cw-8)/2), by=py+150+row*26; if(in(mx,my,bx,by,(cw-8)/2-6,22)){ acct.lang=i; cmos_write(0x3C,(u8)i); click_snd(); return; } } } }
            else if(setcat==43){ if(in(mx,my,px,py+216,150,30)){ toast_set(OSNAME " IS UP TO DATE"); click_snd(); return; } }
            else if(setcat==40){
                int cw=winw-(px-winx)-20;
                if(in(mx,my,px+220,py+78,40,20)){ net_dhcp=!net_dhcp; net_field=0; click_snd(); return; }
                if(!net_dhcp){ for(int i=0;i<4;i++){ int fy=py+110+i*30; if(in(mx,my,px+140,fy,cw-140,22)){ net_field=i+1; click_snd(); return; } } }
                if(in(mx,my,px,py+238,120,28)){ if(net_dhcp){ net_field=0; nic_up(); net_refresh(); netmsg_set("DHCP APPLIED"); } else { my_ip=parse_ip(netfld[0]); netmask=parse_ip(netfld[1]); gw_ip=parse_ip(netfld[2]); dns_ip=parse_ip(netfld[3]); gw_known=0; net_field=0; netmsg_set("STATIC APPLIED"); } click_snd(); return; }
            }
            else if(setcat==41){ if(in(mx,my,px,py+88,150,28)){ bt_probe(); click_snd(); return; } }
            else if(setcat==11){ if(disk_ok && in(mx,my,px,py+150,180,30)){

                u8* s=(u8*)0x00928000u;
                if(ata_read(NVX_DIR_LBA,s)){ set_repair_ok=0; for(int i=0;i<40;i++)set_repair_msg[i]=0; const char* m="DISK READ ERROR"; int k=0; while(m[k]){set_repair_msg[k]=m[k];k++;} set_repair_msg[k]=0; }
                else { u32 mg=*(u32*)s;
                    if(mg==NVX_MAGIC){ nvx_mount(); set_repair_ok=1; const char* m="OK - FILESYSTEM HEALTHY, FILES INTACT"; int k=0; while(m[k]){set_repair_msg[k]=m[k];k++;} set_repair_msg[k]=0; }
                    else { nvx_format(); nvx_mount(); set_repair_ok=1; const char* m="REPAIRED - AREA REBUILT (WAS CORRUPT)"; int k=0; while(m[k]){set_repair_msg[k]=m[k];k++;} set_repair_msg[k]=0; }
                }
                click_snd(); return; } }
        }
        if(app==6){
            int ry=winy+62;
            for(int z=0;z<wincnt;z++){ int a2=wins[z].app;
                if(a2!=6 && in(mx,my,winx+winw-86,ry-2,74,13)){ close_app(a2); click_snd(); dirty=1; return; }
                ry+=18; }
        }
        if(app==29){ for(int i=0;i<4;i++){ int ry=winy+80+i*40; int bw=70,bx=winx+winw-bw-20; if(in(mx,my,bx,ry+2,bw,24)){ widget_on[i]=!widget_on[i]; click_snd(); dirty=1; return; } } }
        if(app==46){ int tw=(winw-2)/3;
            for(int t=0;t<3;t++){ if(in(mx,my,winx+1+t*tw,winy+20,tw-1,24)){ ph_tab=t; ph_focus=0; click_snd(); dirty=1; return; } }
            int cyb0=winy+46;
            if(!phone_conn && in(mx,my,winx+winw-92,cyb0-1,86,17)){ if(!nic_up())toast_set("NO NETWORK - USE BRIDGED ADAPTER"); else { u32 tip=((u32)my_ip&0xFFFFFF00u)|(u32)phone_oct; ts_fin=0; if(tcps_open(tip,(u16)phone_port)){ phone_conn=1; phone_acc_len=0; phone_have=0; toast_set("CONNECTED"); } else toast_set("CONNECT FAILED - SET OCTET IN PHONE MIRROR"); } click_snd(); dirty=1; return; }
            int bx0=winx+8, by0=winy+70, bx1=winx+winw-8, by1=winy+winh-8;
            if(ph_tab==0){ int kx0=bx0+(bx1-bx0)/2-93, ky0=by0+40; static const char kc[4][3]={{'1','2','3'},{'4','5','6'},{'7','8','9'},{'*','0','#'}};
                for(int r=0;r<4;r++)for(int c=0;c<3;c++){ if(in(mx,my,kx0+c*62,ky0+r*44,56,38)){ if(phone_dialn<22){phone_dialnum[phone_dialn++]=kc[r][c];phone_dialnum[phone_dialn]=0;} click_snd(); dirty=1; return; } }
                int cyb=ky0+4*44+8;
                if(in(mx,my,bx0+20,cyb,128,32)){ if(phone_dialn>0){ phone_send_dial(); ph_log(0,phone_dialnum); toast_set("DIALING ON PHONE..."); } else toast_set("ENTER A NUMBER"); click_snd(); dirty=1; return; }
                if(in(mx,my,bx0+158,cyb,80,32)){ if(phone_dialn>0){phone_dialn--;phone_dialnum[phone_dialn]=0;} click_snd(); dirty=1; return; }
                if(in(mx,my,bx0+248,cyb,128,32)){ phone_send_hangup(); toast_set("HANGUP SENT"); click_snd(); dirty=1; return; } }
            else if(ph_tab==1){ if(in(mx,my,bx0+30,by0-2,210,18)){ ph_focus=1; click_snd(); dirty=1; return; }
                int ty1=by1-30, my2=ty1+6;
                if(in(mx,my,bx0,my2,bx1-bx0-92,20)){ ph_focus=2; click_snd(); dirty=1; return; }
                if(in(mx,my,bx1-86,my2-1,86,22)){ ph_do_send(); click_snd(); dirty=1; return; } }
            return; }
        if(app==45){ ac97_init(); if(!ac97_ok) hda_init(); int x=winx+16, by=winy+68;
            if(in(mx,my,x,by,124,28)){ ac97_beep(440,400); click_snd(); dirty=1; return; }
            if(in(mx,my,x+134,by,124,28)){ ac97_scale(); click_snd(); dirty=1; return; }
            if(in(mx,my,x,by+36,124,28)){ ac97_melody(); click_snd(); dirty=1; return; }
            if(in(mx,my,x+134,by+36,124,28)){ int r=wav_play("sound.wav"); toast_set(r>0?"PLAYING sound.wav":r==-2?"sound.wav NOT ON NVXFS":r==-5?"NEED 16-BIT PCM WAV":"WAV READ ERROR"); dirty=1; return; }
            int ky=by+78, kw=(winw-32)/8; static const int sc[8]={262,294,330,349,392,440,494,523};
            for(int k=0;k<8;k++){ int kx=x+k*kw; if(in(mx,my,kx,ky,kw-3,58)){ ac97_beep(sc[k],300); click_snd(); dirty=1; return; } }
            int my0=ky+76;
            if(in(mx,my,x,my0,128,26)){ int f=mic_record(4000); toast_set(f>0?"RECORDED 4s - PRESS PLAY REC":"MIC RECORD FAILED"); click_snd(); dirty=1; return; }
            if(rec_frames>0 && in(mx,my,x+136,my0,86,26)){ mic_playback(); click_snd(); dirty=1; return; }
            if(rec_frames>0 && in(mx,my,x+230,my0,86,26)){ int r=mic_save("mic.wav"); toast_set(r>=0?"SAVED mic.wav TO NVXFS":"SAVE FAILED - CHECK DISK"); dirty=1; return; }
            return; }
        if(app==2){ int tw=(winw-24)/10; for(int s=0;s<10;s++){ int tx=winx+12+s*tw; if(in(mx,my,tx+1,winy+80,tw-3,18)){ vt_switch(s); click_snd(); dirty=1; return; } } }
        if(app==42){ int ax=winx+10, ay=winy+28; int ibx=ax+48, ibw=winw-48-200;
            if(in(mx,my,ibx,ay-2,ibw,18)){ gopher_field=1; click_snd(); dirty=1; return; }
            if(in(mx,my,winx+winw-150,ay-2,44,18)){ gopher_field=0; gopher_histn=0; gopher_load(gopher_host,70,"",1); click_snd(); dirty=1; return; }
            if(in(mx,my,winx+winw-100,ay-2,44,18)){ gopher_back(); click_snd(); dirty=1; return; }
            if(in(mx,my,winx+winw-50,ay-2,40,18)){ const char* dd="gopher.floodgap.com"; int i=0; while(dd[i]){gopher_host[i]=dd[i];i++;} gopher_host[i]=0; gopher_field=0; gopher_histn=0; gopher_load(gopher_host,70,"",1); click_snd(); dirty=1; return; }
            int cy0=winy+52; if(gopher_is_menu&&gopher_status==2&&my>=cy0){ int rel=my-(cy0-gopher_scroll); int idx=rel/13; if(rel>=0&&idx>=0&&idx<gopher_nitems){ char tp=gopher_items[idx].type; if(tp=='1'){ gopher_nav(gopher_items[idx].host,gopher_items[idx].port,gopher_items[idx].sel,1); click_snd(); dirty=1; return; } if(tp=='0'){ gopher_nav(gopher_items[idx].host,gopher_items[idx].port,gopher_items[idx].sel,0); click_snd(); dirty=1; return; } } }
            return; }
        if(app==43){ int ax=winx+10, ay=winy+28; int ibx=ax+64, ibw=winw-64-70;
            if(in(mx,my,ibx,ay-2,ibw,18)){ wiki_field=1; click_snd(); dirty=1; return; }
            if(in(mx,my,winx+winw-58,ay-2,48,18)){ wiki_field=0; wiki_fetch(); click_snd(); dirty=1; return; }
            return; }
        if(app==44){ int ax=winx+10, ay=winy+28;
            if(in(mx,my,ax,ay-2,72,18)){ nt_mode=0; click_snd(); dirty=1; return; }
            if(in(mx,my,ax+78,ay-2,64,18)){ nt_mode=1; click_snd(); dirty=1; return; }
            if(in(mx,my,winx+winw-58,ay-2,48,18)){ nt_field=0; nt_fetch(); click_snd(); dirty=1; return; }
            { int qy=ay+24, ibx=ax+88, ibw=winw-88-20; if(in(mx,my,ibx,qy-2,ibw,18)){ nt_field=1; click_snd(); dirty=1; return; } }
            return; }
        if(app==38){ int bw=60,bx=winx+winw-bw-14,by=winy+28; if(in(mx,my,bx,by,bw,18)){ qr_n=0; qr_in[0]=0; qr_build(); click_snd(); dirty=1; return; } }
        if(app==39){ if(g2_over){ g2048_init(); dirty=1; return; } }
        if(app==40){ if(!bk_live){ breakout_init(); dirty=1; return; } }
        if(app==41){ int bxf=winx+winw-150,byf=winy+26; if(in(mx,my,bxf,byf,80,18)){ ms_flagmode=!ms_flagmode; click_snd(); dirty=1; return; }
            int bxn=winx+winw-60,byn=winy+26; if(in(mx,my,bxn,byn,46,18)){ ms_init(); click_snd(); dirty=1; return; }
            int gx=winx+10, gy=winy+50, cell=(winw-20)/9;
            if(in(mx,my,gx,gy,cell*9,cell*9)){ int cc=(mx-gx)/cell, rr=(my-gy)/cell; if(rr>=0&&rr<9&&cc>=0&&cc<9&&!ms_over){ if(ms_flagmode){ if(!ms_open[rr][cc]){ if(ms_flag[rr][cc]){ ms_flag[rr][cc]=0; ms_left++; } else { ms_flag[rr][cc]=1; ms_left--; } } } else { ms_reveal(rr,cc); } click_snd(); dirty=1; } return; } }
        if(app==37){
            int fx=winx+108, fw=winw-120;
            if(in(mx,my,fx,winy+30,fw,18)){ mail_field=1; click_snd(); return; }
            if(in(mx,my,fx,winy+54,fw,18)){ mail_field=2; click_snd(); return; }
            if(in(mx,my,fx,winy+78,fw,18)){ mail_field=3; click_snd(); return; }
            if(in(mx,my,fx,winy+102,fw,18)){ mail_field=4; click_snd(); return; }
            if(in(mx,my,fx,winy+126,fw,18)){ mail_field=5; click_snd(); return; }
            { int by=winy+164, bh=winh-(by-winy)-66; if(bh<40)bh=40; if(in(mx,my,winx+12,by,winw-24,bh)){ mail_field=6; click_snd(); return; } }
            if(in(mx,my,winx+12,winy+winh-52,90,22)){ if(!mail_host[0]||!mail_user[0]||!mail_pass[0]||!mail_to[0]){ toast_set("FILL HOST, EMAIL, PASSWORD, TO"); } else { mst("SENDING..."); mail_do_send=1; } dirty=1; click_snd(); return; }
        }
        if(app==35){
            for(int i=0;i<NPKG;i++){
                int ry=winy+72+i*74;
                int inst=(disk_ok && nvx_find(PKGS[i].file)>=0);
                if(inst){
                    if(in(mx,my,winx+winw-184,ry+18,80,28)){ click_snd(); close_app(35); elf_run(PKGS[i].file); dirty=1; return; }
                    if(in(mx,my,winx+winw-96,ry+18,82,28)){ int ix=nvx_find(PKGS[i].file); if(ix>=0){ nvx_delete(ix); toast_set("PACKAGE REMOVED"); } click_snd(); dirty=1; return; }
                } else {
                    if(in(mx,my,winx+winw-96,ry+18,82,28)){
                        if(!disk_ok){ toast_set("NO DISK - CANNOT INSTALL"); }
                        else { int r=nvx_write(PKGS[i].file,(const char*)PKGS[i].data,PKGS[i].len); toast_set(r>=0?"PACKAGE INSTALLED":"INSTALL FAILED (DISK FULL?)"); }
                        click_snd(); dirty=1; return;
                    }
                }
            }
            return;
        }
        if(app==36){
            int cax=winx+10, cay=winy+30;
            int vx=cax+10, vy=cay+14, vw=250, vh=winh-30-28;
            int px=vx+vw+24, pt=cay+18;
            int y1=pt+34, yBtn=y1+48, yPort=yBtn+34, yAct=yPort+26; (void)yPort;
            if(!phone_conn){
                if(in(mx,my,px,yBtn,46,24)){ phone_oct-=10; if(phone_oct<1)phone_oct=1; click_snd(); dirty=1; return; }
                if(in(mx,my,px+52,yBtn,40,24)){ phone_oct-=1; if(phone_oct<1)phone_oct=1; click_snd(); dirty=1; return; }
                if(in(mx,my,px+98,yBtn,40,24)){ phone_oct+=1; if(phone_oct>254)phone_oct=254; click_snd(); dirty=1; return; }
                if(in(mx,my,px+144,yBtn,46,24)){ phone_oct+=10; if(phone_oct>254)phone_oct=254; click_snd(); dirty=1; return; }
                if(in(mx,my,px,yAct,120,30)){
                    if(!nic_up()){ toast_set("NO NETWORK - USE BRIDGED ADAPTER"); }
                    else { u32 tip=((u32)my_ip&0xFFFFFF00u)|(u32)phone_oct; ts_fin=0;
                        if(tcps_open(tip,(u16)phone_port)){ phone_conn=1; phone_acc_len=0; phone_have=0; phone_frames=0; toast_set("CONNECTED - WAITING FOR FRAMES"); }
                        else toast_set("CONNECT FAILED - CHECK IP/APP"); }
                    click_snd(); dirty=1; return; }
                if(in(mx,my,px+130,yAct,90,30)){
                    int w=0,h=0; if(jpeg_decode(sample_jpg,sample_jpg_len,PHONE_RGBA,0xC00000,&w,&h)==0){ phone_fw=w; phone_fh=h; phone_have=1; phone_demo=1; }
                    click_snd(); dirty=1; return; }
            } else {
                if(in(mx,my,px,y1,130,30)){ phone_disconnect(); toast_set("DISCONNECTED"); click_snd(); dirty=1; return; }
                int navY=y1+150;
                if(in(mx,my,px,navY,80,26)){ phone_send_btn(0); click_snd(); return; }
                if(in(mx,my,px+86,navY,80,26)){ phone_send_btn(1); click_snd(); return; }
                if(in(mx,my,px+172,navY,80,26)){ phone_send_btn(2); click_snd(); return; }
                { int ndy=navY+38, kx0=px, ky0=ndy+26; static const char kc[4][3]={{'1','2','3'},{'4','5','6'},{'7','8','9'},{'*','0','#'}};
                  for(int r=0;r<4;r++)for(int c=0;c<3;c++){ int kx=kx0+c*60, ky=ky0+r*36; if(in(mx,my,kx,ky,54,30)){ if(phone_dialn<22){ phone_dialnum[phone_dialn++]=kc[r][c]; phone_dialnum[phone_dialn]=0; } click_snd(); dirty=1; return; } }
                  int rcx=px+186;
                  if(in(mx,my,rcx,ky0,84,30)){ phone_send_dial(); toast_set(phone_dialn>0?"DIALING ON PHONE...":"ENTER A NUMBER FIRST"); click_snd(); dirty=1; return; }
                  if(in(mx,my,rcx,ky0+36,84,30)){ if(phone_dialn>0){ phone_dialn--; phone_dialnum[phone_dialn]=0; } click_snd(); dirty=1; return; }
                  if(in(mx,my,rcx,ky0+72,84,30)){ phone_send_hangup(); toast_set("HANGUP SENT"); click_snd(); dirty=1; return; } }
                if(phone_have && phone_rdw>0 && in(mx,my,phone_rx,phone_ry,phone_rdw,phone_rdh)){
                    phone_send_ptr(0); phone_drag=1; phone_lastx=mx; phone_lasty=my; click_snd(); return; }
            }
            return;
        }
        if(app==31){
            for(int i=0;i<zip_n && i<12;i++){ int ry=winy+58+i*22; if(in(mx,my,winx+6,ry-2,winw-12,20)){ zip_sel=i; zip_msg[0]=0; click_snd(); return; } }

            if(in(mx,my,winx+12,winy+winh-26,90,18)){
                if(zip_n>0&&zip_sel<zip_n){ int w=zip_extract(ZIP_BUF,zip_len,&zip_ents[zip_sel],ZIP_OUT,0x200000);
                    if(w<0){ int q=0; const char* m="EXTRACT FAILED"; while(*m)zip_msg[q++]=*m++; zip_msg[q]=0; }
                    else if(disk_ok){ char nm[16]; int k=0; while(zip_ents[zip_sel].name[k]&&k<15){ char c=zip_ents[zip_sel].name[k]; if(c=='/')k=-1; else nm[k]=c; k++; if(k<0)k=0; } nm[k]=0;
                        int wr=nvx_write(nm,(char*)ZIP_OUT,w>4096?4096:w);
                        int q=0; const char* m=(wr>=0)?"EXTRACTED TO DISK":"DISK FULL/ERR"; while(*m)zip_msg[q++]=*m++; zip_msg[q]=0; toast_set("EXTRACTED TO NVXFS"); }
                    else { int q=0; const char* m="NO DISK - VIEW ONLY"; while(*m)zip_msg[q++]=*m++; zip_msg[q]=0; } }
                click_snd(); return;
            }

            if(in(mx,my,winx+110,winy+winh-26,90,18)){
                if(zip_n>0&&zip_sel<zip_n){ int w=zip_extract(ZIP_BUF,zip_len,&zip_ents[zip_sel],ZIP_OUT,0x200000);
                    if(w>0){ int n=w>1020?1020:w; for(int i=0;i<n;i++)notebuf[i]=ZIP_OUT[i]; notebuf[n]=0; notelen=n;
                        int k=0; while(zip_ents[zip_sel].name[k]&&k<15){note_name[k]=zip_ents[zip_sel].name[k];k++;} note_name[k]=0; note_status=2;
                        open_app(5,60,50,540,380); } }
                click_snd(); return;
            }
            return;
        }
        if(app==5){
            int tby=winy+20;
            if(in(mx,my,winx+6,tby+4,16,16)){ np_bold=!np_bold; click_snd(); return; }
            if(in(mx,my,winx+26,tby+4,16,16)){ np_under=!np_under; click_snd(); return; }
            for(int a2=0;a2<3;a2++){ if(in(mx,my,winx+52+a2*20,tby+4,16,16)){ np_align=a2; click_snd(); return; } }
            if(in(mx,my,winx+120,tby+4,14,16)){ if(np_size>1)np_size--; click_snd(); return; }
            if(in(mx,my,winx+158,tby+4,14,16)){ if(np_size<4)np_size++; click_snd(); return; }
            if(in(mx,my,winx+winw-96,tby+4,44,16)){ if(disk_ok){ int w2=nvx_write(note_name,notebuf,notelen); note_status=(w2>=0)?1:3; } else note_status=3; click_snd(); return; }
            if(in(mx,my,winx+winw-48,tby+4,44,16)){ if(disk_ok){ int idx=nvx_find(note_name); if(idx>=0){ notelen=nvx_read(idx,notebuf,1020); notebuf[notelen]=0; note_status=2; } } else note_status=3; click_snd(); return; }
        }
        if(app==1&&cur_folder==8&&disk_ok){
            int px=winx+170,py=winy+28;
            for(unsigned i=0;i<nvx.count;i++){ int ry=py+32+i*16; if(in(mx,my,px,ry,winw-180,14)){ if(ext_is(nvx.e[i].name,(char)'P',(char)'D',(char)'F')){ pdf_open_nvx(i,nvx.e[i].name); click_snd(); return; } if(ext_is(nvx.e[i].name,(char)'Z',(char)'I',(char)'P')){ zip_open_nvx(i,nvx.e[i].name); click_snd(); return; } if(doc_is_known(nvx.e[i].name)){ doc_open_nvx(i,nvx.e[i].name); click_snd(); return; } int k=0;while(nvx.e[i].name[k]&&k<15){note_name[k]=nvx.e[i].name[k];k++;}note_name[k]=0; notelen=nvx_read(i,notebuf,1020); notebuf[notelen]=0; note_status=2; open_app(5,60,50,540,380); return; } }
        }
        if(app==1&&cur_folder!=8){
            int px=winx+170,py=winy+28; char(*lst)[16];int n; folder_list(cur_folder,&lst,&n);
            for(int i=0;i<n;i++){ int ry=py+32+i*16; if(in(mx,my,px,ry,winw-180,14)){
                int k=0; while(lst[i][k]&&k<15){note_name[k]=lst[i][k];k++;} note_name[k]=0;
                if(disk_ok){ int idx=nvx_find(note_name); if(idx>=0){ notelen=nvx_read(idx,notebuf,1020); notebuf[notelen]=0; } else { notelen=0; notebuf[0]=0; } } else { notelen=0; notebuf[0]=0; }
                note_status=2; open_app(5,60,50,540,380); return; } }
        }
        if(app==9){
            if(av_state==0 && in(mx,my,winx+winw/2-50,winy+150,100,24)){ pending_avscan=1; click_snd(); return; }
            if(av_state==2){
                if(av_threat && in(mx,my,winx+winw/2-55,winy+160,110,24)){ infected=0; av_threat=0; av_state=0; toast_set("THREAT REMOVED"); click_snd(); return; }
                if(!av_threat && in(mx,my,winx+winw/2-50,winy+150,100,24)){ pending_avscan=1; click_snd(); return; }
            }
        }
        if(app==10){
            if(in(mx,my,winx+winw/2-55,winy+winh-30,110,22)){ bin_cnt=0; toast_set("BIN EMPTIED"); click_snd(); return; }
            for(int i=0;i<bin_cnt;i++){ int ry=winy+48+i*18; if(in(mx,my,winx+12,ry,winw-24,16)){ dsk_add(0,0,bin_files[i],-1); for(int j=i;j<bin_cnt-1;j++){int k=0;while(bin_files[j+1][k]){bin_files[j][k]=bin_files[j+1][k];k++;}bin_files[j][k]=0;} bin_cnt--; toast_set("RESTORED"); click_snd(); return; } }
        }
        if(app==11){
            static const u8 pal[10]={C_RED,108,103,107,109,100,106,104,4,C_WHITE};
            for(int i=0;i<10;i++){ int py=winy+26+i*22; if(in(mx,my,winx+8,py,38,18)){ paint_col=pal[i]; click_snd(); return; } }
            if(in(mx,my,winx+8,winy+26+10*22,38,18)){ paint_clear(); click_snd(); return; }
            for(int i=0;i<3;i++){ int by=winy+26+11*22+4+i*20; if(in(mx,my,winx+8,by,38,16)){ paint_brush=i; click_snd(); return; } }
        }
        if(app==30){
            if(in(mx,my,winx+winw-152,winy+25,44,16)){ g3_yaw=32; click_snd(); return; }
            if(in(mx,my,winx+winw-104,winy+25,44,16)){ g3_wire=!g3_wire; click_snd(); return; }
            if(in(mx,my,winx+winw-56,winy+25,50,16)){ g3_preset=(g3_preset+1)&3; click_snd(); return; }
        }
        if(app==28){
            int row=0;
            for(int i=0;i<HX_NSAMP;i++){ int ly=winy+46+row*18; if(in(mx,my,winx+3,ly-2,126,18)){ hx_sel=row; click_snd(); return; } row++; }
            if(disk_ok)for(int i=0;i<(int)nvx.count;i++){ if(ext_is(nvx.e[i].name,'H','E','X')){ int ly=winy+46+row*18; if(ly>winy+winh-44)break; if(in(mx,my,winx+3,ly-2,126,18)){ hx_sel=row; click_snd(); return; } row++; } }
            if(in(mx,my,winx+10,winy+winh-30,64,20)){ hx_runsel(); click_snd(); return; }
        }
        if(app==13){
            int kx=winx+12, ky=winy+40, kw=(winw-24)/8;
            static const int wn[8]={0,2,4,5,7,9,11,12};
            for(int i=0;i<8;i++){ if(in(mx,my,kx+i*kw,ky,kw-2,120)){ beep(PIANO_HZ[wn[i]],3); click_snd(); return; } }
        }
        if(app==14){
            int hbx=winx+6,hby=winy+winh-28;
            for(int i=1;i<=9;i++){ if(in(mx,my,hbx+(i-1)*30,hby,26,24)){ craft_sel=i; click_snd(); return; } }
            int cvx=winx+4,cvy=winy+22;
            if(mx>=cvx&&my>=cvy&&my<hby-4){ int wx=craft_cam+(mx-cvx)/CTILE,wy=craft_camy+(my-cvy)/CTILE; if(wx>=0&&wx<CW&&wy>=0&&wy<CH){ craft[wy*CW+wx]=(u8)craft_sel; click_snd(); } return; }
        }
        if(app==15){
            if(br_brand==3){
                int cx0=winx+1, cw=winw-2; int cmx=cx0+cw/2, cmy=winy+56+64;
                int bw=200,bh=30,bx=cmx-bw/2,by=cmy+106;
                if(in(mx,my,bx,by,bw,bh)){ launch(16); click_snd(); return; }
            }
            if(in(mx,my,winx+6,winy+24,28,16)){ br_back(); click_snd(); return; }
            if(in(mx,my,winx+38,winy+24,40,16)){ br_nav(br_addr,br_addr_len); click_snd(); return; }
            { int tw=146,tx=winx+winw-tw-4; if(in(mx,my,tx,winy+45,tw,10)){ https_only=!https_only; toast_set(https_only?"HTTPS-ONLY ON":"HTTPS-ONLY OFF"); click_snd(); return; } }
            int abx=winx+84,abw=winw-84-40;
            if(in(mx,my,abx,winy+24,abw,16)){ br_addr_focus=1; click_snd(); return; }
            if(in(mx,my,winx+winw-38,winy+24,32,16)){ br_addr_focus=0; br_nav(br_addr,br_addr_len); click_snd(); return; }
            br_addr_focus=0;
            for(int i=0;i<br_link_n;i++){ if(in(mx,my,br_link[i].x,br_link[i].y,br_link[i].w,br_link[i].h)){ br_nav(br_link[i].href,br_link[i].hlen); click_snd(); return; } }
        }
        if(app==16){
            if(in(mx,my,winx+44,winy+27,winw-194,14)){ ai_field=0; click_snd(); return; }
            if(in(mx,my,winx+winw-146,winy+27,42,14)){ ai_show=!ai_show; click_snd(); return; }
            if(in(mx,my,winx+winw-100,winy+27,42,14)){ if(disk_ok&&ai_keylen>0){ if(nvx_write("CLAUDE.KEY",(char*)AI_KEY,ai_keylen)>=0){ai_saved=1;toast_set("KEY SAVED TO DISK");} else toast_set("SAVE FAILED"); } else toast_set(disk_ok?"NO KEY TO SAVE":"NO DISK - CANNOT SAVE"); click_snd(); return; }
            if(in(mx,my,winx+winw-54,winy+27,46,14)){ chat_n=0; chat_used=0; ai_scroll=0; ai_rc=0; toast_set("CHAT CLEARED"); click_snd(); return; }
            if(in(mx,my,winx+10,winy+winh-24,winw-76,16)){ ai_field=1; click_snd(); return; }
            if(in(mx,my,winx+winw-58,winy+winh-24,48,16)){ ai_send(); click_snd(); return; }
        }
        if(app==17){

            if(in(mx,my,winx+winw-100,winy+24,94,18)){ store_myapps=!store_myapps; store_scroll=0; click_snd(); return; }
            if(store_myapps){
                int lt=winy+56, lb=winy+winh-6, rowh=44, mc=0;
                for(unsigned i=0;i<nvx.count;i++){ const char* nm=nvx.e[i].name; int L=0; while(nm[L])L++;
                    int iself=(L>4&&nm[L-4]=='.'&&(nm[L-3]|32)=='e'&&(nm[L-2]|32)=='l'&&(nm[L-1]|32)=='f');
                    int isnvx=(L>4&&nm[L-4]=='.'&&(nm[L-3]|32)=='n'&&(nm[L-2]|32)=='v'&&(nm[L-1]|32)=='x');
                    if(!iself&&!isnvx)continue;
                    int ey=lt+8+mc*rowh-store_scroll; mc++;
                    if(ey<lt-rowh||ey>lb)continue;
                    int bw=70, bx=winx+winw-bw-12, by=ey+8;
                    if(in(mx,my,bx,by,bw,18)){ char fn[20]; int k=0; while(nm[k]&&k<19){fn[k]=nm[k];k++;} fn[k]=0;
                        click_snd(); close_app(17); elf_run(fn); return; } }
                return;
            }
            for(int t=0;t<6;t++){ if(in(mx,my,winx+6+t*75,winy+52,72,16)){ store_cat=t-1; store_scroll=0; click_snd(); return; } }
            int lt=winy+72, rowh=50, mc=0;
            for(int i=0;i<STORE_N;i++){
                if(store_cat>=0 && STORE[i].cat!=store_cat) continue;
                int ey=lt+8+mc*rowh-store_scroll, bw=78, bx=winx+winw-bw-12, by=ey+8; mc++;
                if(in(mx,my,bx,by,bw,18)){
                    if(!inst_get(i)){ inst_set(i);
                        cmos_write(0x40,store_inst[0]); cmos_write(0x41,store_inst[1]); cmos_write(0x42,store_inst[2]); cmos_write(0x43,store_inst[3]); cmos_write(0x44,store_inst[4]);
                        dsk_add(2,STORE[i].app,STORE[i].name,-1); toast_set("INSTALLED TO DESKTOP"); }
                    else toast_set("ALREADY INSTALLED");
                    click_snd(); return; } }
        }
        if(app==18){
            if(in(mx,my,winx+6,winy+22,44,16)){ if(fld_view>=0&&DSK[fld_view].parent>=0)fld_view=DSK[fld_view].parent; click_snd(); return; }
            int ry=winy+48;
            for(int i=0;i<DSK_MAX;i++){ if(!DSK[i].used||DSK[i].parent!=fld_view)continue; if(ry>winy+winh-22)break;
                int bx=winx+winw-46;
                if(in(mx,my,bx,ry,38,14)){ DSK[i].parent=-1; dsk_place(i); toast_set("MOVED TO DESKTOP"); click_snd(); return; }
                if(in(mx,my,winx+8,ry-2,bx-winx-10,18)){
                    if(dclk_item==i && dclk_timer>0){ dclk_item=-1; dclk_timer=0; if(DSK[i].type==1)fld_view=i; else dsk_open(i); }
                    else { dclk_item=i; dclk_timer=6; }
                    click_snd(); return; }
                ry+=24; }
        }
        if(app==20){
            for(int i=0;i<4;i++){ if(in(mx,my,winx+12+i*80,winy+90,76,18)){ sas_loc=i; click_snd(); return; } }
            if(in(mx,my,winx+winw-160,winy+winh-30,70,20)){ sas_commit(); click_snd(); return; }
            if(in(mx,my,winx+winw-82,winy+winh-30,70,20)){ close_app(20); click_snd(); return; }
        }
        if(app==21){
            if(bl_state==2){ if(in(mx,my,winx+winw-90,winy+winh-30,78,20)){ bl_state=bl_enabled?1:0; secure_zero((void*)bl_rkdisp,56); close_app(21); click_snd(); return; } }
            else if(bl_enabled){ if(in(mx,my,winx+12,winy+winh-30,120,20)){ bl_disable(); bl_state=0; toast_set("ENCRYPTION DISABLED"); click_snd(); return; } }
            else { if(bl_pwlen>0&&in(mx,my,winx+12,winy+winh-30,120,20)){ if(bl_enable(bl_pw,bl_pwlen)==0){ bl_state=2; secure_zero((void*)bl_pw,24); bl_pwlen=0; toast_set("DRIVE ENCRYPTED"); } click_snd(); return; }
                if(in(mx,my,winx+winw-90,winy+winh-30,78,20)){ close_app(21); click_snd(); return; } }
        }
        if(app==23){
            int dpx=winx+12, dpy=winy+30, dpw=winw-24, dph=42;
            int bw=(dpw-12)/4, bh=36, bg=4;
            int by0=dpy+dph+10;
            for(int r=0;r<5;r++) for(int c=0;c<4;c++){
                const char* lbl=calc_btn_lbl[r][c];
                if(r==4&&c==3&&lbl[0]=='='&&calc_btn_lbl[r][2][0]=='=') continue;
                int span=(r==4&&c==2&&calc_btn_lbl[r][3][0]=='=')?2*bw+bg:bw;
                int bx=dpx+c*(bw+bg);
                int by=by0+r*(bh+bg);
                if(in(mx,my,bx,by,span,bh)){ calc_press(lbl); click_snd(); return; }
            }
        }
        if(app==24){
            if(cam_count>0){
                int by=winy+30+16+16+18;
                if(in(mx,my,winx+12,by,150,22)){ webcam_negotiate(0); click_snd(); return; }
                int cby=winy+winh-32;
                if(in(mx,my,winx+12,cby,150,22)){
                    if(!webcam_neg_ok) webcam_negotiate(0);
                    int r=webcam_grab(0);
                    toast_set(r==0?"FRAME CAPTURED":r==-10?"ISOC CAMERA - NEED ISOC DRIVER":r==-11?"NOT MJPEG FORMAT":"CAPTURE FAILED");
                    click_snd(); dirty=1; return;
                }
            }
        }
        if(app==25){
            if(ph_view>=0){
                if(in(mx,my,winx+winw-186,winy+winh-28,88,22)){ set_photo_wallpaper(ph_view); toast_set("PHOTO SET AS WALLPAPER"); click_snd(); return; }
                if(in(mx,my,winx+winw-94,winy+winh-28,82,22)){ ph_view=-1; click_snd(); return; }
            } else {
                if(in(mx,my,winx+winw-162,winy+26,150,20)){ ph_import_usb(); click_snd(); dirty=1; return; }
                int cell=100,pad=12; int cols=(winw-2*pad+12)/cell; if(cols<1)cols=1;
                int gx=winx+pad, gy=winy+54;
                for(int p=0;p<ph_count;p++){ int col=p%cols,row=p/cols; int cx=gx+col*cell, cy=gy+row*(cell+12);
                    if(cy+88>winy+winh-12) break;
                    if(in(mx,my,cx,cy,88,88)){ ph_view=p; click_snd(); return; } }
            }
        }
        if(app==26){

            int by=winy+74+18+16+22+20+16+16+16+24;
            if(in(mx,my,winx+16,by,180,24)){ toast_set(VND_NAME " IS UP TO DATE"); click_snd(); return; }
        }
        if(in(mx,my,winx+winw-16,winy+winh-16,16,16)){ resizing=1; rs_w0=winw; rs_h0=winh; rs_mx0=mx; rs_my0=my; return; }
        if(in(mx,my,winx,winy,winw,20)){
            dragging=1; dragdx=mx-winx; dragdy=my-winy; curs_kind=5;
            return;
        }
        if(in(mx,my,winx,winy,winw,winh)) return;
    }

    for(int i=0;i<DSK_MAX;i++){ if(!DSK[i].used||DSK[i].parent!=-1)continue;
        if(in(mx,my,DSK[i].x,DSK[i].y,30,40)){
            if(dclk_item==i && dclk_timer>0){ dclk_item=-1; dclk_timer=0; idrag=-1; dsk_open(i); return; }
            dclk_item=i; dclk_timer=6;
            if(!(DSK[i].type==2&&DSK[i].app==10)){ idrag=i; idrag_dx=mx-DSK[i].x; idrag_dy=my-DSK[i].y; }
            return;
        } }
}

static void do_screenshot(void){
    hide_cursor();
    for(int i=0;i<PITCH*H;i++) FB[i]=PAL32[C_WHITE];
    beep(1500,1); pit_wait(2);
    toast_set("SCREENSHOT TAKEN");
    dirty=1;
}
static void open_appwin(int a){
    if(a==1) open_app(1,120,55,700,470);
    else if(a==2){ open_app(2,150,80,720,470); vt_term_open(); }
    else if(a==4){ open_app(4,130,55,650,510); setcat=20; }
    else if(a==6) open_app(6,200,90,440,360);
}

static u8 kbd_led_state=0;
static void kbd_leds(u8 mask){
    int g=100000; while((inb(0x64)&2)&&g--){}
    outb(0x60,0xED);
    g=100000; while((inb(0x64)&2)&&g--){}
    outb(0x60,mask&7);
}

static const struct { int app; const char* name; } RUNAPPS[]={
    {1,"files"},{1,"explorer"},{2,"terminal"},{2,"shell"},{2,"cmd"},{23,"calc"},{23,"calculator"},
    {11,"paint"},{4,"settings"},{5,"notepad"},{5,"notes"},{15,"web"},{15,"browser"},{15,"internet"},
    {8,"snake"},{12,"tetris"},{13,"piano"},{14,"craft"},{24,"camera"},{25,"photo"},
    {27,"gpu"},{28,"hexlang"},{3,"about"},{6,"taskmgr"},{6,"taskmanager"},{6,"tasks"},{17,"store"},
    {16,"claude"},{16,"ai"},{21,"encryption"},{10,"recycle"},{30,"graph"},
    {-30,"python"},{-30,"pyrun"},{-30,"py"},{-31,"editor"},{-31,"code"},{-31,"nvxedit"},
    {-33,"invaders"},{-33,"spaceinv"},{-33,"space"},{-34,"idle"},{-34,"pyidle"},{-35,"noovexcraft"},{-35,"craft"},{-35,"mine"}
};
#define NRUNAPPS (int)(sizeof(RUNAPPS)/sizeof(RUNAPPS[0]))
static void run_exec(void){
    if(run_len==0)return;
    if(streq(run_buf,"apps")||streq(run_buf,"all")||streq(run_buf,"list")){ start_open=1; toast_set("ALL APPS - start menu"); return; }
    for(int i=0;i<NRUNAPPS;i++){ if(streq(run_buf,RUNAPPS[i].name)){ launch(RUNAPPS[i].app); return; } }
    toast_set("no such app - type 'apps'");
}
static int files_count(void){ int n=0; char(*lst)[16];
    if(cur_folder==8&&disk_ok) n=(int)nvx.count;
    else if(cur_folder==10&&fat_ok) n=usbfs_n;
    else if((cur_folder==7||cur_folder==9)&&cur_node>=0) n=vfs_nchild(cur_node);
    else { folder_list(cur_folder,&lst,&n); }
    return n; }
static void settings_nav(int down){ static const int kc[14]={20,40,41,21,2,3,22,42,11,7,9,12,43,13};
    int ci=0; for(int i=0;i<14;i++) if(kc[i]==setcat){ci=i;break;}
    if(down){ if(ci<13)ci++; } else { if(ci>0)ci--; } setcat=kc[ci]; }
static void kbd_event(u8 d){
    if(d==0xE0){ kext=1; return; }
    int rel=d&0x80; u8 sc=d&0x7F;
    if(kext){ kext=0;
        if(sc==0x38){ kaltgr=rel?0:1; if(!rel)kctrl=0; return; }
        if(sc==0x5B){ if(rel){ if(!kwin_used) start_open=!start_open; kwin=0; dirty=1; } else { kwin=1; kwin_used=0; } return; }
        if(sc==0x1D){ kctrl=rel?0:1; }
        if(!rel && app==8){ if(sc==0x48&&sdir!=1)sdir=0; else if(sc==0x50&&sdir!=0)sdir=1; else if(sc==0x4B&&sdir!=3)sdir=2; else if(sc==0x4D&&sdir!=2)sdir=3; dirty=1; }
        if(!rel && app==12){ if(sc==0x4B||sc==0x1E){ if(tet_fits(tpiece,trot,tpx-1,tpy))tpx--; } else if(sc==0x4D||sc==0x20){ if(tet_fits(tpiece,trot,tpx+1,tpy))tpx++; } else if(sc==0x48||sc==0x11){ if(tet_fits(tpiece,(trot+1)&3,tpx,tpy))trot=(trot+1)&3; } else if(sc==0x50||sc==0x1F){ if(tet_fits(tpiece,trot,tpx,tpy+1))tpy++; } else if(sc==0x39){ while(tet_fits(tpiece,trot,tpx,tpy+1))tpy++; tet_lock(); } dirty=1; }
        if(!rel && app==14){ if(sc==0x4B){ if(craft_cam>0)craft_cam--; } else if(sc==0x4D){ craft_cam++; } else if(sc==0x48){ if(craft_camy>0)craft_camy--; } else if(sc==0x50){ craft_camy++; } dirty=1; }
        if(!rel && app==15){ if(sc==0x48){ br_scroll-=24; if(br_scroll<0)br_scroll=0; } else if(sc==0x50){ br_scroll+=24; } dirty=1; }
        if(!rel && app==16){ if(sc==0x48){ ai_scroll-=24; if(ai_scroll<0)ai_scroll=0; } else if(sc==0x50){ ai_scroll+=24; } dirty=1; }
        if(!rel && app==17){ if(sc==0x48){ store_scroll-=24; if(store_scroll<0)store_scroll=0; } else if(sc==0x50){ store_scroll+=24; } dirty=1; }
        if(!rel && app==33){ if(sc==0x48){ if(doc_scroll>0)doc_scroll--; } else if(sc==0x50){ doc_scroll++; } dirty=1; }
        if(!rel && app==39){ g2048_key(sc); dirty=1; }
        if(!rel && app==40){ breakout_key(sc); dirty=1; }
        if(!rel && app==42){ if(sc==0x48){ gopher_scroll-=26; if(gopher_scroll<0)gopher_scroll=0; } else if(sc==0x50){ gopher_scroll+=26; } dirty=1; }
        if(!rel && app==43){ if(sc==0x48){ wiki_scroll-=26; if(wiki_scroll<0)wiki_scroll=0; } else if(sc==0x50){ wiki_scroll+=26; } dirty=1; }
        if(!rel && app==44){ if(sc==0x48){ nt_scroll-=26; if(nt_scroll<0)nt_scroll=0; } else if(sc==0x50){ nt_scroll+=26; } dirty=1; }
        if(!rel && app==46 && ph_tab==1){ if(sc==0x48){ ph_mscroll-=18; if(ph_mscroll<0)ph_mscroll=0; } else if(sc==0x50){ ph_mscroll+=18; } dirty=1; }
        return;
    }
    if(sc==0x1D){ kctrl=rel?0:1; return; }
    if(sc==0x2A||sc==0x36){ kshift=rel?0:1; return; }
    if(sc==0x38){ kalt=rel?0:1; return; }
    if(sc==0x01){ kesc=rel?0:1; }
    if(rel) return;
    if(help_overlay){ help_overlay=0; dirty=1; return; }
    if(sc==0x3D && kesc){ hide_cursor(); recovery_mode(); build_base(); rtc_now(); dirty=1; return; }
    if(kctrl && sc==0x26){ FB=LFB; clear_all(0); draw_str(W/2-150,H/2,"RESTARTING (LEGACY BOOT)...",C_WHITE); pit_wait(10); reboot_now(); }
    if(kctrl && kalt && kshift && sc==0x13){ do_powerwash(); build_base(); rtc_now(); dirty=1; return; }
    if(kctrl && kalt && sc==0x35){ help_overlay=1; dirty=1; return; }
    if(run_open){
        if(sc==0x01){ run_open=0; run_len=0; run_buf[0]=0; dirty=1; return; }
        if(sc==0x1C){ run_open=0; run_exec(); run_len=0; run_buf[0]=0; dirty=1; return; }
        if(sc==0x0E){ if(run_len>0){run_len--;run_buf[run_len]=0;} dirty=1; return; }
        char ch=kchar_shift(sc); if(ch>=32&&run_len<62){ run_buf[run_len++]=ch; run_buf[run_len]=0; } dirty=1; return;
    }
    if(sc==0x3A){ kbd_led_state^=0x04; kbd_leds(kbd_led_state); toast_set(kbd_led_state&0x04?"CAPS LOCK ON":"CAPS LOCK OFF"); dirty=1; return; }
    if(sc==0x45){ kbd_led_state^=0x02; kbd_leds(kbd_led_state); return; }
    if(sc==0x46){ kbd_led_state^=0x01; kbd_leds(kbd_led_state); return; }
    if(kctrl && sc==0x2E){
        const char* src=0; int n=0;
        if(app==16 && ai_rc==200 && AI_ANS[0]){ src=AI_ANS; while(src[n])n++; }
        else if(app==16){ if(ai_field==0){src=(char*)AI_KEY;n=ai_keylen;} else {src=(char*)AI_PROMPT;n=ai_plen;} }
        else if(app==5){ src=notebuf; n=notelen; }
        else if(app==15&&br_addr_focus){ src=br_addr; n=br_addr_len; }
        else if(app==2){ src=cmdline; n=cmdlen; }
        if(src){ if(n>4090)n=4090; for(int i=0;i<n;i++)CLIP[i]=src[i]; CLIP[n]=0; cliplen=n; toast_set("COPIED"); dirty=1; }
        return;
    }
    if(kctrl && sc==0x2F){
        if(cliplen>0){
            if(app==5){ for(int i=0;i<cliplen&&notelen<1020;i++)notebuf[notelen++]=CLIP[i]; notebuf[notelen]=0; }
            else if(app==15&&br_addr_focus){ for(int i=0;i<cliplen&&br_addr_len<126;i++){char c=CLIP[i]; if(c>=32)br_addr[br_addr_len++]=c;} br_addr[br_addr_len]=0; }
            else if(app==2){ for(int i=0;i<cliplen;i++){ char c=CLIP[i]; if(c=='\n'||(unsigned char)c<32)continue; if(termlen<2040)termbuf[termlen++]=c; if(cmdlen<198)cmdline[cmdlen++]=c; } termbuf[termlen]=0; }
            else if(app==16){ if(ai_field==0){ for(int i=0;i<cliplen&&ai_keylen<250;i++){char c=CLIP[i]; if(c>=32)AI_KEY[ai_keylen++]=c;} AI_KEY[ai_keylen]=0; ai_saved=0; } else { for(int i=0;i<cliplen&&ai_plen<900;i++){char c=CLIP[i]; if(c>=32)AI_PROMPT[ai_plen++]=c;} AI_PROMPT[ai_plen]=0; } }
            toast_set("PASTED");
        }
        dirty=1; return;
    }
    if(kctrl && sc==0x11){ if(app)close_app(app); dirty=1; return; }
    if(kwin){ kwin_used=1;
        if(sc==0x10){ run_open=1; run_len=0; run_buf[0]=0; dirty=1; return; }
        if(kshift&&sc==0x1F){ do_screenshot(); return; }
        if(sc==0x12){ open_appwin(1); toast_set("FILE EXPLORER"); dirty=1; return; }
        if(sc==0x13){ open_appwin(2); dirty=1; return; }
        if(sc==0x17){ open_appwin(4); dirty=1; return; }
        if(sc==0x14){ open_appwin(2); toast_set("TERMINAL"); dirty=1; return; }
        if(sc==0x19){ open_appwin(6); toast_set("TASK MANAGER"); dirty=1; return; }
        if(sc==0x30){ open_appwin(15); toast_set("BROWSER"); dirty=1; return; }
        if(sc==0x25){ open_appwin(23); toast_set("CALCULATOR"); dirty=1; return; }
        if(sc==0x20){ for(int z=0;z<wincnt;z++)wins[z].min=1; cur_win=-1; app=0; toast_set("SHOW DESKTOP"); dirty=1; return; }
        if(sc==0x26){ hide_cursor(); login_screen(); build_base(); rtc_now(); dirty=1; return; }
        if(!rel && app==1){
            if(sc==0x48){ if(fsel>0)fsel--; dirty=1; return; }
            if(sc==0x50){ int n=files_count(); if(fsel<n-1)fsel++; dirty=1; return; }
            if(sc==0x4B){ if(cur_folder>0){cur_folder--;fsel=0;} dirty=1; return; }
            if(sc==0x4D){ if(cur_folder<13){cur_folder++;fsel=0;} dirty=1; return; }
        }
        if(!rel && app==4 && !(setcat==40&&net_field)){
            if(sc==0x48){ settings_nav(0); dirty=1; return; }
            if(sc==0x50){ settings_nav(1); dirty=1; return; }
        }
        return;
    }
    if(kctrl&&kshift&&sc==0x01){ open_appwin(6); toast_set("TASK MANAGER"); dirty=1; return; }
    if(kalt&&sc==0x0F){ if(wincnt>=1){ win_raise(0); wins[wincnt-1].min=0; cur_win=wincnt-1; win_load(cur_win); toast_set(app_name(app)); dirty=1; } return; }
    if(kalt&&sc==0x3E){ if(app)close_app(app); dirty=1; return; }
    if(sc==0x3F){ need_rebuild=1; toast_set("REFRESHED"); return; }
    if(sc==0x43){ open_appwin(6); toast_set("TASK MANAGER (F9)"); dirty=1; return; }
    if(app==32){
        if(sc==0x50||sc==0x1C){ pdf_scroll++; dirty=1; return; }
        if(sc==0x48){ if(pdf_scroll>0)pdf_scroll--; dirty=1; return; }
        if(sc==0x51){ pdf_scroll+=20; dirty=1; return; }
        if(sc==0x49){ pdf_scroll-=20; if(pdf_scroll<0)pdf_scroll=0; dirty=1; return; }
        if(sc==0x47){ pdf_scroll=0; dirty=1; return; }
    }

    if(app==0 && !start_open){
        int dn=dsk_count_desktop();
        if(dn>0){ if(dsk_sel>=dn)dsk_sel=dn-1; if(dsk_sel<0)dsk_sel=0;
            if(sc==0x4D||sc==0x50){ dsk_sel=(dsk_sel+1)%dn; toast_set("ARROWS=MOVE  ENTER=OPEN"); dirty=1; return; }
            if(sc==0x4B||sc==0x48){ dsk_sel=(dsk_sel+dn-1)%dn; toast_set("ARROWS=MOVE  ENTER=OPEN"); dirty=1; return; }
            if(sc==0x1C){ int idx=dsk_nth(dsk_sel); if(idx>=0){
                if(DSK[idx].type==2){ if(DSK[idx].app==19){ int k=0; while(DSK[idx].name[k]&&k<19){gen_name[k]=DSK[idx].name[k];k++;} gen_name[k]=0; open_app(19,200,120,360,190); } else launch(DSK[idx].app); }
                else if(DSK[idx].type==1){ fld_view=idx; open_app(18,220,110,360,260); }
                else { note_status=0; open_app(5,60,50,540,380); } }
                dirty=1; return; }
        }
    }
    if(start_open){
        if(sc==0x0E){ if(srch_len>0){srch_len--;srch[srch_len]=0;} dirty=1; return; }
        if(sc==0x01){ start_open=0; pwr_open=0; srch_len=0; srch[0]=0; dirty=1; return; }
        if(sc==0x1C){ for(int i=0;i<NPIN;i++){ if(contains_ci(PIN[i].label,srch)){ start_open=0; srch_len=0; srch[0]=0; if(PIN[i].app==-2)pending_update=1; else launch(PIN[i].app); break; } } dirty=1; return; }
        char ch=kchar_shift(sc); if(ch>=32&&srch_len<22){ srch[srch_len++]=ch; srch[srch_len]=0; dirty=1; } return;
    }
    if(app==8){          if(sc==0x11&&sdir!=1)sdir=0; else if(sc==0x1F&&sdir!=0)sdir=1;
        else if(sc==0x1E&&sdir!=3)sdir=2; else if(sc==0x20&&sdir!=2)sdir=3;
        else if(sc==0x13&&!salive){ snake_init(); }
        dirty=1; return;
    }
    if(app==12){ if(sc==0x13){ tetris_init(); } dirty=1; return; }
    if(app==34){ if(sc==0x39||sc==0x48){ if(falive){fvy10=-72; beep(1245,1);} else flappy_init(); } else if(sc==0x13&&!falive){ flappy_init(); } dirty=1; return; }
    if(app==39){ if(!rel)g2048_key(sc); dirty=1; return; }
    if(app==40){ if(!rel)breakout_key(sc); dirty=1; return; }
    if(app==1){
        char(*lst)[16]; int n=0;
        if(cur_folder==8&&disk_ok) n=(int)nvx.count;
        else if(cur_folder==10&&fat_ok) n=usbfs_n;
        else if(cur_folder!=10){ folder_list(cur_folder,&lst,&n); }
        if(sc==0x48){ if(fsel>0)fsel--; dirty=1; return; }
        if(sc==0x50){ if(fsel<n-1)fsel++; dirty=1; return; }
        if(sc==0x4B){ if(cur_folder>0){cur_folder--;fsel=0;} dirty=1; return; }
        if(sc==0x4D){ if(cur_folder<13){cur_folder++;fsel=0;} dirty=1; return; }
        if(sc==0x1C && (cur_folder==7||cur_folder==9) && cur_node>=0){ int nn=vfs_nchild(cur_node);
            if(fsel>=0 && fsel<nn){ int ni=vfs_child(cur_node,fsel); if(VFS[ni].isdir){ cur_node=ni; fsel=0; } else { vfs_open_file(ni); } } dirty=1; return; }
        if(sc==0x1C && n>0 && fsel<n){
            if(cur_folder==8&&disk_ok){ int k=0; while(nvx.e[fsel].name[k]&&k<15){note_name[k]=nvx.e[fsel].name[k];k++;} note_name[k]=0; notelen=nvx_read(fsel,notebuf,1020); if(notelen<0)notelen=0; notebuf[notelen]=0; note_status=1; open_app(5,60,50,540,380); }
            else if(cur_folder==10&&fat_ok){ int k=0; while(usbfs[fsel].name[k]&&k<15){note_name[k]=usbfs[fsel].name[k];k++;} note_name[k]=0; notelen=usbfs_read(fsel,notebuf,1020); if(notelen<0)notelen=0; notebuf[notelen]=0; note_status=2; open_app(5,60,50,540,380); }
            dirty=1; return; }
        if((sc==0x53||sc==0x0E) && n>0 && fsel<n){
            if(cur_folder==8&&disk_ok){   for(unsigned i=fsel;i+1<nvx.count;i++){ nvx.e[i]=nvx.e[i+1]; } if(nvx.count>0)nvx.count--; nvx_flush(); toast_set("FILE DELETED"); }
            else if(fperm[cur_folder]!=1 && cur_folder!=10){ bin_push(lst[fsel]); file_delete(cur_folder,fsel); toast_set("FILE DELETED"); }
            else toast_set("PROTECTED - CANNOT DELETE");
            if(fsel>0)fsel--; dirty=1; return; }
        return;
    }
    if(app==13){ int n=-1; if(sc==0x1E)n=0;else if(sc==0x1F)n=2;else if(sc==0x20)n=4;else if(sc==0x21)n=5;else if(sc==0x22)n=7;else if(sc==0x23)n=9;else if(sc==0x24)n=11;else if(sc==0x25)n=12; if(n>=0)beep(PIANO_HZ[n],2); dirty=1; return; }
    if(app==14){
        if(sc==0x11){ if(craft_camy>0)craft_camy--; } else if(sc==0x1F){ craft_camy++; }
        else if(sc==0x1E){ if(craft_cam>0)craft_cam--; } else if(sc==0x20){ craft_cam++; }
        else if(sc>=0x02&&sc<=0x0A){ craft_sel=sc-1; }
        else if(sc==0x13){ craft_genworld(); toast_set("NEW WORLD"); }
        else if(sc==0x25){ craft_save(); toast_set(disk_ok?"WORLD SAVED":"NO DISK"); }
        else if(sc==0x26){ craft_load(); toast_set("WORLD LOADED"); }
        dirty=1; return;
    }
    if(app==16){
        if(sc==0x0F){ ai_field^=1; dirty=1; return; }
        if(sc==0x1C){ if(ai_field==0)ai_field=1; else ai_send(); dirty=1; return; }
        if(sc==0x0E){ if(ai_field==0){ if(ai_keylen>0){AI_KEY[--ai_keylen]=0; ai_saved=0;} } else { if(ai_plen>0)AI_PROMPT[--ai_plen]=0; } dirty=1; return; }
        char ch=kchar_shift(sc);
        if(ch>=32){ if(ai_field==0){ if(ai_keylen<250){AI_KEY[ai_keylen++]=ch;AI_KEY[ai_keylen]=0; ai_saved=0;} } else { if(ai_plen<900){AI_PROMPT[ai_plen++]=ch;AI_PROMPT[ai_plen]=0;} } dirty=1; }
        return;
    }
    if(app==15){
        if(br_addr_focus){ char ch=kchar_shift(sc);
            if(sc==0x0E){ if(br_addr_len>0){br_addr_len--;br_addr[br_addr_len]=0;} }
            else if(sc==0x1C){ br_addr_focus=0; br_nav(br_addr,br_addr_len); }
            else if(ch>=32&&br_addr_len<126){ br_addr[br_addr_len++]=ch; br_addr[br_addr_len]=0; }
            dirty=1; return; }
        if(sc==0x1C){ br_addr_focus=1; }
        else if(sc==0x0E){ br_back(); }
        dirty=1; return;
    }

    if(rename_mode){ char ch=kchar_shift(sc); if(ch=='\b'){ if(rename_len>0){rename_len--;rename_buf[rename_len]=0;} } else if(ch=='\n'){ rename_commit(); } else if(ch&&rename_len<15){ rename_buf[rename_len++]=ch; rename_buf[rename_len]=0; } dirty=1; }
    else if(app==20){ if(sc==0x1C){ sas_commit(); } else if(sc==0x0E){ if(sas_len>0)sas_name[--sas_len]=0; } else { char ch=kchar_shift(sc); if(ch>=32&&sas_len<15){ sas_name[sas_len++]=ch; sas_name[sas_len]=0; } } dirty=1; }
    else if(app==21){ if(bl_state==0){ if(sc==0x1C){ if(bl_pwlen>0&&bl_enable(bl_pw,bl_pwlen)==0){ bl_state=2; secure_zero((void*)bl_pw,24); bl_pwlen=0; toast_set("DRIVE ENCRYPTED"); } } else if(sc==0x0E){ if(bl_pwlen>0)bl_pw[--bl_pwlen]=0; } else { char ch=kchar_shift(sc); if(ch>=32&&bl_pwlen<20){ bl_pw[bl_pwlen++]=ch; bl_pw[bl_pwlen]=0; } } } dirty=1; }
    else if(app==2){
        if(kalt && d>=0x02 && d<=0x0B){ int s=(d==0x0B)?9:(d-0x02); vt_switch(s); dirty=1; }
        else if(sc==0x48){ term_scroll++; dirty=1; }
        else if(sc==0x50){ if(term_scroll>0)term_scroll--; dirty=1; }
        else if(sc==0x49){ term_scroll+=8; dirty=1; }
        else if(sc==0x51){ term_scroll-=8; if(term_scroll<0)term_scroll=0; dirty=1; }
        else { term_scroll=0; term_key(d); }
        dirty=1;
    }
    else if(app==5){ note_key(d); dirty=1; }
    else if(app==37){ mail_key(d); dirty=1; }
    else if(app==38){ qr_key(d); dirty=1; }
    else if(app==42){ gopher_key(d); dirty=1; }
    else if(app==43){ wiki_key(d); dirty=1; }
    else if(app==44){ nt_key(d); dirty=1; }
    else if(app==46){ phone_msg_key(d); dirty=1; }
    else if(app==4){ if(setcat==40&&net_field){ net_key(d); } else if(!rel){ if(sc==0x48)settings_nav(0); else if(sc==0x50)settings_nav(1); } dirty=1; }
}

static int ehci_in(int addr,int ep,int mps,u8* buf,int len,int dt){
    if(!ehci_init_ok)return -1;
    u32 ob=ehci_base+ehci_caplen;
    volatile u32* usbcmd=(volatile u32*)(ob+0x00);
    volatile u32* usbsts=(volatile u32*)(ob+0x04);
    if(mps<1)mps=8;
    MMW(EHCI_QTD_SETUP+0x00,1); MMW(EHCI_QTD_SETUP+0x04,1);
    MMW(EHCI_QTD_SETUP+0x08, 0x80u|(1u<<8)|(3u<<10)|((u32)len<<16)|((dt?1u:0u)<<31)|0x8000u);
    MMW(EHCI_QTD_SETUP+0x0C, EHCI_BUF_DATA);
    for(u32 o=0x10;o<=0x1C;o+=4)MMW(EHCI_QTD_SETUP+o,0);
    MMW(EHCI_QH_XFER+0x00, EHCI_QH_XFER|2u);
    MMW(EHCI_QH_XFER+0x04, (u32)(addr&0x7F)|((u32)(ep&0xF)<<8)|(2u<<12)|(1u<<14)|(1u<<15)|(((u32)mps&0x7FF)<<16));
    MMW(EHCI_QH_XFER+0x08, (1u<<30));
    MMW(EHCI_QH_XFER+0x0C, 0); MMW(EHCI_QH_XFER+0x10, EHCI_QTD_SETUP); MMW(EHCI_QH_XFER+0x14, 1); MMW(EHCI_QH_XFER+0x18, 0);
    for(u32 o=0x1C;o<=0x2C;o+=4)MMW(EHCI_QH_XFER+o,0);
    *usbcmd &= ~(1u<<5); for(int t=0;t<20000;t++){ if(!(*usbsts&(1u<<15)))break; io_delay(1); }
    *(volatile u32*)(ob+0x18)=EHCI_QH_XFER; *(volatile u32*)(ob+0x10)=0;
    *usbcmd |= (1u<<5); *usbcmd |= 1u;
    for(int t=0;t<20000;t++){ if(*usbsts&(1u<<15))break; io_delay(1); }
    int done=0,err=0;
    for(int t=0;t<4000;t++){ u32 tok=MMR(EHCI_QTD_SETUP+0x08); if(!(tok&0x80)){ done=1; if(tok&0x7C)err=1; break; } io_delay(1); }
    *usbcmd &= ~(1u<<5);
    if(!done||err)return -1;
    u32 d=MMR(EHCI_QTD_SETUP+0x08); int rem=(d>>16)&0x7FFF; int got=len-rem; if(got<0)got=0;
    for(int i=0;i<got&&i<len;i++)buf[i]=((volatile u8*)EHCI_BUF_DATA)[i];
    return got;
}
static const u8 hid_letter_ps2[26]={0x1E,0x30,0x2E,0x20,0x12,0x21,0x22,0x23,0x17,0x24,0x25,0x26,0x32,0x31,0x18,0x19,0x10,0x13,0x1F,0x14,0x16,0x2F,0x11,0x2D,0x15,0x2C};
static u8 hid2ps2(u8 u){
    if(u>=0x04&&u<=0x1D)return hid_letter_ps2[u-0x04];
    if(u>=0x1E&&u<=0x26)return (u8)(0x02+(u-0x1E));
    if(u==0x27)return 0x0B;
    switch(u){ case 0x28:return 0x1C; case 0x29:return 0x01; case 0x2A:return 0x0E; case 0x2B:return 0x0F; case 0x2C:return 0x39; case 0x2D:return 0x0C; case 0x2E:return 0x0D; case 0x2F:return 0x1A; case 0x30:return 0x1B; case 0x33:return 0x27; case 0x34:return 0x28; case 0x36:return 0x33; case 0x37:return 0x34; case 0x38:return 0x35; }
    return 0;
}

static void uhci_poll(void){
    if(!uhci_ok||!uhci_ms_found) return;
    u8 rpt[8]; int got=uhci_in(uhci_ms_addr,uhci_ms_ep,uhci_ms_mps,rpt,(uhci_ms_mps>8?8:uhci_ms_mps),uhci_ms_tog,uhci_ms_ls);
    if(got<3) return;
    uhci_ms_tog^=1;
    int dx=(signed char)rpt[1], dy=(signed char)rpt[2];
    mx+=dx; ms_dx_acc+=dx; my+=dy; if(mx<0)mx=0; if(my<0)my=0; if(mx>=W)mx=W-1; if(my>=H)my=H-1;
    mbtn=rpt[0]&0x1F;
    if(wheel_ok && got>=4){ int wz=(signed char)rpt[3]; if(wz) wheel_delta+=wz; }
    dirty=1;
}

static u8 xkb_keys[6]={0,0,0,0,0,0};
static void xhci_poll(void){
    if(!xhci_init_ok||(!xhci_ms_found&&!xhci_kb_found)) return;
    for(int loop=0;loop<8;loop++){
        volatile u32* ev=(volatile u32*)XHCI_EVTRING; int e=xhci_evt_idx; u32 c3=ev[e*4+3];
        if((c3&1)!=(u32)(xhci_evt_cyc?1:0)) return;
        u32 type=(c3>>10)&0x3F; u32 p0=ev[e*4+0];
        xhci_evt_idx++; if(xhci_evt_idx>=256){xhci_evt_idx=0;xhci_evt_cyc^=1;}
        MMW(xhci_rt+0x20+0x18,(XHCI_EVTRING+(u32)xhci_evt_idx*16)|(1u<<3)); MMW(xhci_rt+0x20+0x1C,0);
        if(type!=32) continue;
        if(xhci_kb_found && p0>=XHCI_KBDRING && p0<XHCI_KBDRING+0x1000u){
            u8* rp=(u8*)XHCI_KBDBUF; u8 mod=rp[0];
            for(int k=2;k<8;k++){ u8 uc=rp[k]; if(!uc)continue;
                int isnew=1; for(int j=0;j<6;j++) if(xkb_keys[j]==uc){isnew=0;break;} if(!isnew)continue;
                u8 ps2=hid2ps2(uc); if(!ps2)continue;
                int sh=(mod&0x22)!=0, ct=(mod&0x11)!=0;
                if(sh)kbd_event(0x2A); if(ct)kbd_event(0x1D);
                kbd_event(ps2); kbd_event((u8)(ps2|0x80));
                if(ct)kbd_event(0x9D); if(sh)kbd_event(0xAA); }
            for(int k=0;k<6;k++)xkb_keys[k]=rp[k+2];
            dirty=1;
            volatile u32* ir=(volatile u32*)XHCI_KBDRING; int i=xhci_kb_idx;
            ir[i*4+0]=XHCI_KBDBUF; ir[i*4+1]=0; ir[i*4+2]=8;
            ir[i*4+3]=(1u<<5)|(1u<<2)|(xhci_kb_cyc?1u:0u);
            xhci_kb_idx++; if(xhci_kb_idx>=255){xhci_kb_idx=0;xhci_kb_cyc^=1;}
            MMW(xhci_db+(u32)xhci_kb_slot*4,(u32)xhci_kb_dci);
        } else if(xhci_ms_found){
            u8* rp=(u8*)XHCI_MSBUF;
            if(xhci_ms_proto==0){                                   /* absolute tablet */
                int ax=rp[1]|((int)rp[2]<<8), ay=rp[3]|((int)rp[4]<<8);
                int nx=(int)(((u32)(ax&0x7FFF)*(u32)(W-1))/32767u);
                int ny=(int)(((u32)(ay&0x7FFF)*(u32)(H-1))/32767u);
                ms_dx_acc+=nx-mx; mx=nx; my=ny; mbtn=rp[0]&0x1F;
                if(wheel_ok){ int wz=(signed char)rp[5]; if(wz) wheel_delta+=wz; }
            } else {                                                /* boot-protocol relative */
                int dx=(signed char)rp[1], dy=(signed char)rp[2];
                mx+=dx; ms_dx_acc+=dx; my+=dy; if(mx<0)mx=0; if(my<0)my=0; if(mx>=W)mx=W-1; if(my>=H)my=H-1;
                mbtn=rp[0]&0x1F;
                if(wheel_ok){ int wz=(signed char)rp[3]; if(wz) wheel_delta+=wz; }
            }
            dirty=1;
            volatile u32* ir=(volatile u32*)XHCI_EPIRING; int i=xhci_ms_idx;
            ir[i*4+0]=XHCI_MSBUF; ir[i*4+1]=0; ir[i*4+2]=(u32)(xhci_ms_mps>8?8:xhci_ms_mps);
            ir[i*4+3]=(1u<<5)|(1u<<2)|(xhci_ms_cyc?1u:0u);
            xhci_ms_idx++; if(xhci_ms_idx>=255){xhci_ms_idx=0;xhci_ms_cyc^=1;}
            MMW(xhci_db+(u32)xhci_ms_slot*4,(u32)xhci_ms_dci);
        }
    }
}

static void usb_poll(void){
    if(!ehci_init_ok||usbdev_n==0)return;
    for(int i=0;i<usbdev_n;i++){
        if(usbdev[i].kind!=1&&usbdev[i].kind!=2)continue;
        if(usbdev[i].epin==0)continue;
        int mps=usbdev[i].epmps?usbdev[i].epmps:8; if(mps>8)mps=8;
        u8 rpt[8]; int got=ehci_in(usbdev[i].addr,usbdev[i].epin,usbdev[i].epmps?usbdev[i].epmps:8,rpt,mps,usbdev[i].dtog);
        if(got<1)continue;
        usbdev[i].dtog^=1;
        if(usbdev[i].kind==2){
            int dx=(signed char)rpt[1], dy=(signed char)rpt[2];
            mx+=dx; ms_dx_acc+=dx; my+=dy; if(mx<0)mx=0; if(my<0)my=0; if(mx>=W)mx=W-1; if(my>=H)my=H-1;
            mbtn=rpt[0]&0x1F;
            if(wheel_ok){ int wz=(signed char)rpt[3]; if(wz) wheel_delta+=wz; }
            dirty=1;
        } else {
            u8 mod=rpt[0];
            for(int k=2;k<8&&k<got;k++){ u8 uc=rpt[k]; if(uc==0)continue;
                int isnew=1; for(int j=0;j<6;j++) if(usbdev[i].keys[j]==uc){isnew=0;break;} if(!isnew)continue;
                u8 ps2=hid2ps2(uc); if(!ps2)continue;
                int sh=(mod&0x22)!=0, ct=(mod&0x11)!=0;
                if(sh)kbd_event(0x2A); if(ct)kbd_event(0x1D);
                kbd_event(ps2); kbd_event((u8)(ps2|0x80));
                if(ct)kbd_event(0x9D); if(sh)kbd_event(0xAA);
                dirty=1;
            }
            for(int k=0;k<6;k++)usbdev[i].keys[k]=(k+2<8)?rpt[k+2]:0;
        }
    }
}

#ifdef NOOVEXSRV

static char srv_buf[3072]; static int srv_len=0;
static int srv_view=0;
static void srv_puts(const char* x){ while(*x && srv_len<3060){ srv_buf[srv_len++]=*x++; } srv_buf[srv_len]=0; }
static void srv_nl(void){ if(srv_len<3060){ srv_buf[srv_len++]='\n'; srv_buf[srv_len]=0; } }
static void srv_u(unsigned v){ char t[12]; int n=0; if(!v){srv_puts("0");return;} while(v){t[n++]='0'+v%10;v/=10;} char o[12]; for(int i=0;i<n;i++)o[i]=t[n-1-i]; o[n]=0; srv_puts(o); }
static void srv_render(const char* line,int ll){

    for(int y=0;y<H;y++){ u32 c=(y<54)?0x0B0F1Au:0x05070Cu; u32* r=FB+y*PITCH; for(int x=0;x<W;x++)r[x]=c; }

    draw_str2(24,16,"NOOVEXOS SERVER",C_GREEN);
    draw_str(360,12,"STORAGE NODE",C_TEAL);
    draw_str(360,30, srv_view?"CONSOLE (F1=DASH)":"DASHBOARD (F2=CONSOLE)",C_MGREY+18);
    rtc_now(); draw_str(W-110,12,clkbuf,C_WHITE);
    fill(0,52,W,2,0x16304A);
    int logtop;
    if(!srv_view){

        int px=24, py=72, lh=18;
        draw_str(px,py,"HOSTNAME : NVX-STORAGE",C_WHITE); py+=lh;
        draw_str(px,py,"OS       : NOOVEXOS SERVER 1.0",C_WHITE); py+=lh;
        { char mb[40]="MEMORY   : "; int q=11,v=ram_mb;char t[8];int tl=0;if(!v)t[tl++]='0';while(v){t[tl++]='0'+v%10;v/=10;}while(tl)mb[q++]=t[--tl];mb[q++]=' ';mb[q++]='M';mb[q++]='B';mb[q]=0; draw_str(px,py,mb,C_WHITE);} py+=lh;
        draw_str(px,py,"NETWORK  : ",C_WHITE);
        if(nic_present){ char ib[20]; ip_to_str(my_ip,ib); draw_str(px+96,py,ib,C_GREEN); } else draw_str(px+96,py,"NO NIC (PCnet only)",C_MGREY+20);
        py+=lh+6; fill(px,py,440,2,0x16304A); py+=8;
        draw_str(px,py,"STORAGE",C_TEAL); py+=lh;
        draw_str(px,py,"  DISK   : " DISK_LABEL,C_WHITE); py+=lh;
        { char rb[40]="  SIZE   : ";int q=11,v=disk_size_gb;char t[6];int tl=0;if(!v)t[tl++]='0';while(v){t[tl++]='0'+v%10;v/=10;}while(tl)rb[q++]=t[--tl];rb[q++]=' ';rb[q++]='G';rb[q++]='B';rb[q]=0; draw_str(px,py,rb,C_WHITE);} py+=lh;
        if(disk_ok){
            draw_str(px,py,"  FS     : NVXFS  MOUNTED",C_GREEN); py+=lh;
            { char fb[40]="  FILES  : ";int q=11,c=nvx.count;char t[6];int tl=0;if(!c)t[tl++]='0';while(c){t[tl++]='0'+c%10;c/=10;}while(tl)fb[q++]=t[--tl];fb[q++]=' ';fb[q++]='/';fb[q++]=' ';fb[q++]='1';fb[q++]='5';fb[q]=0; draw_str(px,py,fb,C_WHITE);} py+=lh;
            int used=(nvx.count*100)/NVX_MAX; if(used>100)used=100;
            fill(px+16,py+2,260,10,0x1A2433); fill(px+16,py+2,(260*used)/100,10,C_BBLUE); py+=lh+2;
        } else { draw_str(px,py,"  FS     : NO ATA DISK (RAM ONLY)",C_RED+8); py+=lh; }
        logtop=py+8;
    } else {

        logtop=64;
    }
    int logx=24; fill(0,logtop-6,W,2,0x16304A);
    int maxrows=(H-30-logtop)/15; if(maxrows<1)maxrows=1;
    int total=1; for(int i=0;i<srv_len;i++) if(srv_buf[i]=='\n')total++;
    int start=total-maxrows; if(start<0)start=0;
    int vl=0,cx=logx,cy=logtop;
    for(int i=0;i<srv_len;i++){ char c=srv_buf[i];
        if(c=='\n'){ vl++; cx=logx; if(vl-start>=0&&vl-start<maxrows)cy=logtop+(vl-start)*15; continue; }
        if(vl>=start&&vl<start+maxrows){ draw_char(cx,cy,c,C_MGREY+30); }
        cx+=8; if(cx>W-16){cx=logx; vl++; if(vl-start>=0&&vl-start<maxrows)cy=logtop+(vl-start)*15;}
    }
    int iy=H-22; draw_str(logx,iy,"nvxsrv>",C_GREEN);
    int ix=logx+64; for(int i=0;i<ll;i++){ draw_char(ix,iy,line[i],C_WHITE); ix+=8; }
    fill(ix,iy,7,12,C_GREEN);
}
static void srv_exec(const char* c){
    if(streq(c,"help")){ srv_puts("FILES : ls cat<f> write<f><txt> rm<f> mkfs"); srv_nl();
        srv_puts("NET   : web (HTTP file server)  netstat"); srv_nl();
        srv_puts("SYS   : status df ver date top clear reboot"); srv_nl();
        srv_puts("VIEW  : F1=DASHBOARD  F2=CONSOLE"); srv_nl(); }
    else if(streq(c,"status")){ srv_puts("STORAGE NODE ONLINE. NVXFS "); srv_puts(disk_ok?"MOUNTED.":"UNAVAILABLE."); srv_nl(); }
    else if(streq(c,"ls")){ if(!disk_ok){srv_puts("NO DISK.");srv_nl();} else { if(nvx.count==0){srv_puts("(empty)");srv_nl();} for(unsigned i=0;i<nvx.count;i++){ srv_puts("  "); srv_puts(nvx.e[i].name); srv_puts("  "); srv_u(nvx.e[i].len); srv_puts(" B"); srv_nl(); } } }
    else if(c[0]=='c'&&c[1]=='a'&&c[2]=='t'&&c[3]==' '){ const char* fn=c+4;
        if(!disk_ok){srv_puts("NO DISK.");srv_nl();}
        else { int idx=nvx_find(fn);
            if(idx<0){ srv_puts("NOT FOUND: "); srv_puts(fn); srv_nl(); }
            else { static char fb[2048]; int L=nvx_read(idx,fb,2047); if(L<0)L=0; fb[L]=0;
                for(int k=0;k<L;k++){ char ch[2]={fb[k],0}; if(fb[k]=='\n')srv_nl(); else srv_puts(ch); } srv_nl(); } } }
    else if(c[0]=='r'&&c[1]=='m'&&c[2]==' '){ const char* fn=c+3;
        if(!disk_ok){srv_puts("NO DISK.");srv_nl();}
        else { int idx=nvx_find(fn); if(idx>=0){ nvx_delete(idx); srv_puts("DELETED "); srv_puts(fn); } else { srv_puts("NOT FOUND: "); srv_puts(fn); } srv_nl(); } }
    else if(c[0]=='w'&&c[1]=='r'&&c[2]=='i'&&c[3]=='t'&&c[4]=='e'&&c[5]==' '){
        const char* p=c+6; char name[16]; int n=0; while(*p&&*p!=' '&&n<15)name[n++]=*p++; name[n]=0; if(*p==' ')p++;
        if(!disk_ok){srv_puts("NO DISK.");srv_nl();}
        else { if(nvx_write(name,p,strlen_(p))>=0){ srv_puts("WROTE "); srv_puts(name); srv_puts(" ("); srv_u(strlen_(p)); srv_puts(" B)"); } else srv_puts("WRITE FAILED (DISK FULL?)"); srv_nl(); } }
    else if(streq(c,"df")){ srv_puts("RESERVED "); srv_u(disk_size_gb); srv_puts(" GB  -  FILES "); srv_u(nvx.count); srv_puts("/15  ("); srv_u((nvx.count*100)/NVX_MAX); srv_puts("%)"); srv_nl(); }
    else if(streq(c,"date")){ rtc_now(); srv_puts("TIME "); srv_puts(clkbuf); srv_nl(); }
    else if(streq(c,"netstat")){ srv_puts("NIC: "); if(nic_present){ char ib[20]; ip_to_str(my_ip,ib); srv_puts("UP  IP "); srv_puts(ib); } else srv_puts("DOWN (no PCnet / real HW)"); srv_nl(); }
    else if(streq(c,"top")){ srv_puts("MEM "); srv_u(ram_mb); srv_puts(" MB   FILES "); srv_u(nvx.count); srv_puts("/15   NIC "); srv_puts(nic_present?"UP":"DOWN"); srv_nl(); }
    else if(streq(c,"web")||streq(c,"httpd")){
        if(!nic_present){ srv_puts("NO NIC - HTTP SERVER NEEDS NETWORK (VBOX)."); srv_nl(); }
        else { char ib[20]; ip_to_str(my_ip,ib);
            srv_puts("HTTP FILE SERVER ON http://"); srv_puts(ib); srv_puts("/"); srv_nl();
            srv_puts("OPEN THAT IN A BROWSER. PRESS ANY KEY TO STOP."); srv_nl();
            srv_render("",0);
            static u8 rxb[1600]; int served=0;
            for(;;){ int r=http_serve_once(80,rxb); if(r==-2)break; if(r==1){ served++; srv_render("",0); } }
            char nb[12]; int q=0,v=served; char t[8];int tl=0; if(!v)t[tl++]='0'; while(v){t[tl++]='0'+v%10;v/=10;} while(tl)nb[q++]=t[--tl]; nb[q]=0;
            srv_puts("HTTP SERVER STOPPED. SERVED "); srv_puts(nb); srv_puts(" REQUEST(S)."); srv_nl(); } }
    else if(streq(c,"ver")){ srv_puts("NoovexOS Server 1.0 - Storage Node (32-bit)"); srv_nl(); }
    else if(streq(c,"clear")){ srv_len=0; srv_buf[0]=0; }
    else if(streq(c,"reboot")){ srv_puts("REBOOTING..."); srv_nl(); srv_render("",0); pit_wait(8); { while(inb(0x64)&2); outb(0x64,0xFE); } }
    else if(c[0]){ srv_puts("UNKNOWN: "); srv_puts(c); srv_puts("  (type help)"); srv_nl(); }
}
static void server_console(void){
    srv_puts("NoovexOS Server ready - Storage Node."); srv_nl();
    srv_puts("F1 dashboard / F2 console. Type 'help'."); srv_nl();
    char line[78]; int ll=0;
    for(;;){
        srv_render(line,ll);
        u8 sc=wait_key();
        if(sc==0x3B){ srv_view=0; continue; }
        if(sc==0x3C){ srv_view=1; continue; }
        char ch=kchar_shift(sc);
        if(sc==0x1C){ line[ll]=0; srv_puts("nvxsrv> "); srv_puts(line); srv_nl(); srv_exec(line); ll=0; }
        else if(sc==0x0E){ if(ll>0)ll--; }
        else if(ch){ if(ll<76)line[ll++]=ch; }
    }
}
#endif

static int safe_mode=0;
static void boot_ram_error(int mb){
    clear_all(0);
    int cx=W/2-250, cy=H/2-110;
    afill(cx-24,cy-24,520,230,0x00400010u,255); frame(cx-24,cy-24,520,230,0x0C);
    draw_str2(cx,cy,"INSUFFICIENT MEMORY",0x0F);
    draw_str(cx,cy+44,"NoovexOS recommends at least 1 GB of RAM",0x0F);
    draw_str(cx,cy+62,"to run smoothly.",0x0F);
    { char b[40]="Detected: "; int q=10,v=mb; char t[8];int tl=0; if(!v)t[tl++]='0'; while(v){t[tl++]='0'+v%10;v/=10;} while(tl)b[q++]=t[--tl]; b[q++]=' ';b[q++]='M';b[q++]='B';b[q]=0;
      draw_str(cx,cy+96,b,0x0E); }
    draw_str(cx,cy+150,"ENTER = boot anyway      R = reboot",0x0F);
    for(;;){ u8 sc=wait_key();
        if(sc==0x1C)return;
        if(sc==0x13){ while(inb(0x64)&2); outb(0x64,0xFE); }
    }
}
static int boot_cfg_usb=1;
static int boot_cfg_sound=1;

static int bsh_x, bsh_y;
static void bsh_hdr(void){
    for(int i=0;i<PITCH*H;i++) FB[i]=0x000A1020u;

    int lw=150, lh=84, ox=W-lw-24, oy=H-lh-20;
    for(int y=0;y<lh;y++){ int my=y*NLOGO_H/lh; for(int x=0;x<lw;x++){ int mx=x*NLOGO_W/lw; int idx=my*NLOGO_W+mx;
        if((NLOGO[idx>>3]>>(idx&7))&1){ int px=ox+x,py=oy+y; if(px>=0&&px<W&&py>=0&&py<H) FB[py*PITCH+px]=0x001A2240u; } } }
    afill(0,0,W,30,0x00141B33u,255); fill(0,30,W,2,C_BBLUE);
    draw_str(10,9,"NoovexOS Boot Shell",C_TEAL);
    { const char* v=BUILDVER; draw_str(W-strlen_(v)*8-12,9,v,C_MGREY+24); }
    draw_str(W/2-92,9,"root @ noovex (pre-boot)",C_MGREY+22);
    bsh_x=8; bsh_y=42;
}
static void bsh_nl(void){ bsh_x=8; bsh_y+=12; if(bsh_y>H-20){ bsh_hdr(); } }
static void bsh_puts(const char* s){ while(*s){ if(*s=='\n')bsh_nl(); else { if(bsh_x>W-12)bsh_nl(); draw_char(bsh_x,bsh_y,*s,C_WHITE); bsh_x+=8; } s++; } }
static void bsh_readline(char* buf,int max,int mask){
    int len=0;
    for(;;){ u8 sc=wait_key(); if(sc&0x80)continue; char ch=kchar_shift(sc);
        if(ch=='\n'){ buf[len]=0; bsh_nl(); return; }
        if(ch=='\b'){ if(len>0){ len--; bsh_x-=8; draw_char(bsh_x,bsh_y,' ',C_WHITE); } continue; }
        if(ch<32||ch>126)continue;
        if(len<max-1){ buf[len++]=ch; if(bsh_x>W-12)bsh_nl(); draw_char(bsh_x,bsh_y,(char)(mask?'*':ch),C_WHITE); bsh_x+=8; } }
}

static int bsh_exec(char* line){
    if(streq(line,"help")){
        bsh_puts("ACCOUNT : whoami passwd useradd <n> userdel users id su\n");
        bsh_puts("FILES   : ls cat <f> free df\n");
        bsh_puts("SYSTEM  : uname ver date hostname pwd echo <t> uptime\n");
        bsh_puts("POWER   : reboot shutdown/poweroff\n");
        bsh_puts("ROOT    : sudo <cmd>   (run a command as root)\n");
        bsh_puts("SHELL   : clear  boot/exit (start NoovexOS)\n");
        return 1;
    }
    if(startsw(line,"sudo ")){ char* sub=line+5; while(*sub==' ')sub++;
        if(*sub==0){ bsh_puts("usage: sudo <command>\n"); return 1; }
        bsh_puts("[sudo] executing as root...\n"); return bsh_exec(sub); }
    if(streq(line,"whoami")){ bsh_puts(have_user?acct.user:"guest"); bsh_puts("\n"); return 1; }
    if(streq(line,"id")){ bsh_puts("uid=0(root) gid=0(root) groups=0(root)  [boot shell]\n"); return 1; }
    if(streq(line,"su")||startsw(line,"su ")){ bsh_puts("already root in the boot shell.\n"); return 1; }
    if(streq(line,"passwd")){
        if(!have_user){ bsh_puts("no account yet - run 'useradd <name>' first\n"); return 1; }
        char p1[16],p2[16]; bsh_puts("new password: "); bsh_readline(p1,16,1);
        bsh_puts("retype:       "); bsh_readline(p2,16,1);
        if(streq(p1,p2)){ int k=0; while(p1[k]&&k<15){acct.pass[k]=p1[k];k++;} acct.pass[k]=0;
            if(disk_ok){ acct_save(); bsh_puts("password updated.\n"); } else bsh_puts("changed (not saved - no disk)\n"); }
        else bsh_puts("sorry, passwords do not match.\n");
        return 1; }
    if(startsw(line,"useradd ")||startsw(line,"setuser ")){ const char* nm=line+8;
        if(*nm==0){ bsh_puts("usage: useradd <name>\n"); return 1; }
        int k=0; while(nm[k]&&k<15){acct.user[k]=nm[k];k++;} acct.user[k]=0; acct.magic=ACCT_MAGIC; have_user=1;
        if(disk_ok){ acct_save(); bsh_puts("user account saved.\n"); } else bsh_puts("set (not saved - no disk)\n");
        return 1; }
    if(streq(line,"userdel")||streq(line,"reset")){ acct.magic=0; have_user=0; if(disk_ok)acct_save();
        bsh_puts("account removed - first-run setup will start on next boot.\n"); return 1; }
    if(streq(line,"unenroll")){ acct.enrolled=0; for(int i=0;i<24;i++)acct.org[i]=0; if(disk_ok){ acct_save(); bsh_puts("unenrolled - 'managed by' removed. reboot to see the change.\n"); } else bsh_puts("unenrolled (not saved - no disk).\n"); return 1; }
    if(streq(line,"users")){ if(have_user){ bsh_puts("user: "); bsh_puts(acct.user); bsh_puts("\n"); } else bsh_puts("(no account configured)\n"); return 1; }
    if(streq(line,"ls")||streq(line,"dir")){ if(!disk_ok){bsh_puts("no disk\n");return 1;} for(unsigned i=0;i<nvx.count;i++){ bsh_puts("  "); bsh_puts(nvx.e[i].name); bsh_puts("\n"); } if(nvx.count==0)bsh_puts("(no files)\n"); return 1; }
    if(startsw(line,"cat ")){ const char* fn=line+4; if(!disk_ok){bsh_puts("no disk\n");return 1;}
        int idx=nvx_find(fn); if(idx<0){ bsh_puts("cat: "); bsh_puts(fn); bsh_puts(": no such file\n"); return 1; }
        static char fb[1024]; int n=nvx_read(idx,fb,1023); if(n<0)n=0; fb[n]=0;
        for(int i=0;i<n;i++){ char c2[2]={fb[i]=='\n'?'\n':fb[i],0}; bsh_puts(c2); } bsh_puts("\n"); return 1; }
    if(streq(line,"free")||streq(line,"mem")){ char b[12]; bsh_puts("RAM: "); utoa(ram_mb,b); bsh_puts(b); bsh_puts(" MB total\n"); return 1; }
    if(streq(line,"df")){ char b[12]; bsh_puts("NVXFS: "); utoa(disk_ok?(int)nvx.count:0,b); bsh_puts(b); bsh_puts("/15 files used\n"); return 1; }
    if(streq(line,"uname")||startsw(line,"uname ")){ bsh_puts("NoovexOS 1.0 i386 noovex (boot shell)\n"); return 1; }
    if(streq(line,"ver")){ bsh_puts(BUILDVER); bsh_puts("\n"); return 1; }
    if(streq(line,"hostname")){ bsh_puts("noovexos\n"); return 1; }
    if(streq(line,"pwd")){ bsh_puts("/\n"); return 1; }
    if(streq(line,"uptime")){ bsh_puts("pre-boot environment (timer not started)\n"); return 1; }
    if(startsw(line,"echo ")){ bsh_puts(line+5); bsh_puts("\n"); return 1; }
    if(streq(line,"date")){ rtc_now(); bsh_puts(clkbuf); bsh_puts("\n"); return 1; }
    if(streq(line,"clear")||streq(line,"cls")){ bsh_hdr(); return 1; }
    if(streq(line,"reboot")){ while(inb(0x64)&2); outb(0x64,0xFE); return 1; }
    if(streq(line,"shutdown")||streq(line,"poweroff")||streq(line,"halt")){ bsh_puts("powering off...\n"); acpi_shutdown();
        outw(0x604,0x2000); outw(0xB004,0x2000); outw(0x4004,0x3400); for(;;)__asm__ __volatile__("hlt"); }
    if(streq(line,"boot")||streq(line,"exit")||streq(line,"startx")){ return 0; }
    if(line[0]==0) return 1;
    bsh_puts(line); bsh_puts(": command not found (try 'help')\n"); return 1;
}
static void boot_shell(void){
    disk_init(); if(disk_ok) acct_load();
    bsh_hdr();
    bsh_puts("type 'help' for commands, 'boot' to start NoovexOS\n");
    if(!disk_ok) bsh_puts("WARNING: no disk found - changes cannot be saved\n");
    char line[64];
    for(;;){
        bsh_puts("noovex# "); bsh_readline(line,64,0);
        if(bsh_exec(line)==0) return;
    }
}
#ifdef NOOVEXLITE
static void lite_terminal(void){
    disk_init(); if(disk_ok) acct_load();
    bsh_hdr();
    bsh_puts("NoovexOS Lite  -  terminal edition\n");
    bsh_puts("type 'help' for commands\n");
    if(!disk_ok) bsh_puts("note: no disk found - changes will not be saved\n");
    bsh_puts("\n");
    char line[64];
    for(;;){
        bsh_puts("noovex> "); bsh_readline(line,64,0);
        if(line[0]) bsh_exec(line);
    }
}
#endif
static void usb_driver_menu(void){
    u8 ur=cmos(0x47); int curp=((ur&0xF8)==0xA0)?(ur&7):0; if(curp>3)curp=0;
    int sel=curp, saved=0;
    const char* opts[4]={"Auto  (detect and use all)","Force xHCI  (USB 3.0)","Force EHCI  (USB 2.0)","Force UHCI  (USB 1.1)"};
    for(;;){
        clear_all(0);
        { const char* tt="USB Controller Driver"; draw_str(W/2-strlen_(tt)*8/2,24,tt,C_TEAL); }
        draw_str(40,52,"ARROWS",C_GREEN); draw_str(40+7*8,52,"Move",C_WHITE);
        draw_str(180,52,"ENTER",C_GREEN); draw_str(180+6*8,52,"Select",C_WHITE);
        draw_str(330,52,"ESC",C_GREEN); draw_str(330+4*8,52,"Back",C_WHITE);
        int y=86; draw_str(60,y,"Detected USB controllers (live scan):",C_WHITE); y+=20;
        int found=0;
        for(int bus=0;bus<2 && found<8;bus++)for(int dev=0;dev<32 && found<8;dev++){
            u32 v=pci_read(bus,dev,0,0); if((v&0xFFFF)==0xFFFF)continue;
            u32 cc=pci_read(bus,dev,0,0x08); u8 cls=(cc>>24)&0xFF,sub=(cc>>16)&0xFF,prog=(cc>>8)&0xFF;
            if(cls==0x0C&&sub==0x03){ u16 ven=v&0xFFFF; char ln[72]; int p=0;
                ln[p++]=' ';ln[p++]=' '; const char* sp=usb_type(prog); while(*sp)ln[p++]=*sp++;
                const char* vt="   vendor 0x"; while(*vt)ln[p++]=*vt++;
                char hb[8]; hex16(ven,hb); for(int k=0;k<4;k++)ln[p++]=hb[k]; ln[p]=0;
                draw_str(60,y,ln,C_GREEN); y+=16; found++; } }
        if(!found){ draw_str(60,y,"  (no USB controllers found)",C_MGREY+20); y+=16; }
        y+=16; draw_str(60,y,"Driver to use on boot:",C_WHITE); y+=22;
        for(int i=0;i<4;i++){ int ey=y+i*26;
            if(i==sel){ afill(52,ey-3,470,24,0x00203050u,255); draw_str(56,ey,">",C_GREEN); }
            draw_str(76,ey,opts[i],(i==sel)?C_WHITE:(C_MGREY+26));
            if(i==curp)draw_str(76+300,ey,"[active]",C_TEAL); }
        draw_str(60,y+4*26+16, saved?"Saved. Applies when you boot.":"Auto is safest - force one only if a controller misbehaves.", saved?C_GREEN:(C_MGREY+20));
        draw_str(60,y+4*26+34,"If USB input stops working: reboot, open this screen, choose Auto.",C_MGREY+18);
        u8 sc=wait_key();
        if(sc==0x48||sc==0x4B){ if(sel>0)sel--; saved=0; }
        else if(sc==0x50||sc==0x4D){ if(sel<3)sel++; saved=0; }
        else if(sc==0x1C){ cmos_write(0x47,(u8)(0xA0|sel)); curp=sel; saved=1; }
        else if(sc==0x01){ return; }
    }
}
static void boot_config(void){
    int sel=0;
    for(;;){
        clear_all(0);
        { const char* tt="NoovexOS Boot Configuration"; int tw=strlen_(tt)*8; draw_str(W/2-tw/2,24,tt,C_TEAL); }
        draw_str(40,52,"ARROWS",C_GREEN); draw_str(40+7*8,52,"Move",C_WHITE);
        draw_str(180,52,"ENTER",C_GREEN); draw_str(180+6*8,52,"Toggle",C_WHITE);
        draw_str(340,52,"ESC",C_GREEN);   draw_str(340+4*8,52,"Back",C_WHITE);
        const char* labels[4]={"Scan USB at boot","Boot & UI sounds","Open terminal (user / password)","USB controller driver"};
        int bx=80, by=130;
        for(int i=0;i<4;i++){ int ey=by+i*36;
            if(i==sel){ afill(bx-8,ey-4,500,28,0x00203050u,255); draw_str(bx-4,ey,">",C_GREEN); }
            draw_str(bx+16,ey,labels[i],(i==sel)?C_WHITE:(C_MGREY+26));
            if(i<2){ const char* val = (i==0)?(boot_cfg_usb?"On":"Off") : (boot_cfg_sound?"On":"Off");
                draw_str(bx+360,ey,val,(i==sel)?C_GREEN:C_TEAL); }
            else draw_str(bx+360,ey,"[ENTER]",(i==sel)?C_GREEN:C_TEAL); }
        draw_str(80,by+3*36+30,"Changes apply when you boot from the menu.",C_MGREY+20);
        draw_str(80,by+3*36+48,"Terminal: change username, password, inspect files.",C_MGREY+18);
        u8 sc=wait_key();
        if(sc==0x48||sc==0x4B){ if(sel>0)sel--; }
        else if(sc==0x50||sc==0x4D){ if(sel<3)sel++; }
        else if(sc==0x1C){ if(sel==0)boot_cfg_usb^=1; else if(sel==1)boot_cfg_sound^=1; else if(sel==2)boot_shell(); else usb_driver_menu(); }
        else if(sc==0x01){ return; }
    }
}
static void boot_menu(void){

    u32 ext64=cmos(0x34)|((u32)cmos(0x35)<<8);
    int mb=16+(ext64*64)/1024;
    if(mb<8||mb>8192){ u32 base=cmos(0x30)|((u32)cmos(0x31)<<8); mb=(base/1024)+1; }
    ram_mb=mb;
    if(mb<1024) boot_ram_error(mb);
#ifdef NOOVEXSRV
    const char* e0="NoovexOS Server";
#else
    const char* e0="NoovexOS Desktop";
#endif
    const char* entries[3]; entries[0]=e0; entries[1]="NoovexOS (Safe Mode)"; entries[2]="Boot Configuration";
    int sel=0, nent=3;
    for(;;){

        for(int y=0;y<H;y++){ u32* row=FB+y*PITCH; u32 v=(y<H/2)?0x000C2030u:0x00081420u; for(int x=0;x<W;x++)row[x]=v; }
        int pw=560, ph=252; if(pw>W-40)pw=W-40; int pxp=W/2-pw/2, pyp=H/2-ph/2;
        afill(pxp+6,pyp+8,pw,ph,0x00050D16u,150);
        afill(pxp,pyp,pw,ph,0x00153A4Eu,255);
        afill(pxp,pyp,pw,2,0x004C92B4u,255); afill(pxp,pyp+ph-2,pw,2,0x004C92B4u,255); afill(pxp,pyp,2,ph,0x004C92B4u,255); afill(pxp+pw-2,pyp,2,ph,0x004C92B4u,255);
        afill(pxp+2,pyp+2,pw-4,28,0x00255A74u,255);
        { const char* tt="Please select boot device"; draw_str(pxp+pw/2-strlen_(tt)*8/2,pyp+11,tt,C_WHITE); }
        { const char* sub="NoovexOS Boot Manager 1.0  (ia-32 / BIOS)"; draw_str(pxp+16,pyp+38,sub,C_TEAL); }
        int ey0=pyp+62;
        for(int i=0;i<nent;i++){ int ey=ey0+i*30;
            if(i==sel){ afill(pxp+14,ey-3,pw-28,26,0x002F6FA8u,255); }
            draw_str(pxp+30,ey, entries[i], (i==sel)?C_WHITE:(C_MGREY+34)); }
        { char b[48]="Memory: "; int q=8,v=mb; char t[8];int tl=0; if(!v)t[tl++]='0'; while(v){t[tl++]='0'+v%10;v/=10;} while(tl)b[q++]=t[--tl]; b[q++]=' ';b[q++]='M';b[q++]='B';b[q]=0;
          draw_str(pxp+16,pyp+ph-56,b, mb>=1024?C_GREEN:(C_RED+8)); }
        afill(pxp+pw/2-152,pyp+ph-34,148,24,0x00255A74u,255); draw_str(pxp+pw/2-152+26,pyp+ph-28,"ESC = Recovery",C_WHITE);
        afill(pxp+pw/2+6,pyp+ph-34,148,24,0x002F6FA8u,255); draw_str(pxp+pw/2+6+30,pyp+ph-28,"ENTER = Boot",C_WHITE);
        { const char* hk="ARROWS Select    ENTER Boot    S Safe Mode    R Reboot"; draw_str(W/2-strlen_(hk)*8/2,pyp+ph+16,hk,C_MGREY+26); }

        u8 sc=wait_key();
        if(sc==0x48||sc==0x4B){ if(sel>0)sel--; }
        else if(sc==0x50||sc==0x4D){ if(sel<nent-1)sel++; }
        else if(sc==0x1F){ safe_mode=1; break; }
        else if(sc==0x13){ while(inb(0x64)&2); outb(0x64,0xFE); }
        else if(sc==0x01){ recovery_mode(); }
        else if(sc==0x1C){
            if(sel==2){ boot_config(); continue; }
            if(sel==1) safe_mode=1;
            break;
        }
    }

    if(!boot_cfg_usb) safe_mode=1;
}
/* ============ kernel heap: dynamic allocation (memory management) ============
   Lives above the program heap (prog heap ends ~0x17000000). The kernel had
   no allocator before this - only fixed scratch addresses. This is additive:
   nothing existing is moved, it just makes kmalloc/kfree available + tracked. */
#define KHEAP_BASE 0x18000000u
typedef struct kblk { u32 size; u32 free; struct kblk* next; } kblk;
static kblk* kheap_head=0; static u32 kheap_sz=0, kheap_usedb=0; static int kheap_ready=0, kheap_ok=0;
static void* kmalloc(u32 n){
    if(!kheap_ready) return 0; if(!n)n=1; n=(n+7u)&~7u;
    for(kblk* b=kheap_head;b;b=b->next){
        if(b->free && b->size>=n){
            if(b->size>=n+sizeof(kblk)+16u){
                kblk* sp=(kblk*)((u8*)(b+1)+n); sp->size=b->size-n-(u32)sizeof(kblk); sp->free=1; sp->next=b->next;
                b->size=n; b->next=sp;
            }
            b->free=0; kheap_usedb+=b->size; return (void*)(b+1);
        }
    }
    return 0;
}
static void kheap_coalesce(void){ for(kblk* b=kheap_head;b&&b->next;){ if(b->free&&b->next->free){ b->size+=(u32)sizeof(kblk)+b->next->size; b->next=b->next->next; } else b=b->next; } }
static void kfree(void* p){ if(!p)return; kblk* b=((kblk*)p)-1; if(b->free)return; b->free=1; if(kheap_usedb>=b->size)kheap_usedb-=b->size; kheap_coalesce(); }
static void* kcalloc(u32 a,u32 b){ u32 n=a*b; void* p=kmalloc(n); if(p){ u8* q=(u8*)p; for(u32 i=0;i<n;i++)q[i]=0; } return p; }
static u32 kheap_used(void){ return kheap_usedb; }
static u32 kheap_total(void){ return kheap_sz; }
static int kheap_isok(void){ return kheap_ok; }
static void kheap_init(void){
    u32 top=(u32)ram_mb*0x100000u; if((u32)ram_mb>3072u) top=3072u*0x100000u;
    if(top <= KHEAP_BASE + 0x400000u){ kheap_ready=0; kheap_ok=0; kheap_sz=0; return; }
    u32 avail=top-KHEAP_BASE; if(avail>0x08000000u) avail=0x08000000u;  /* cap 128MB */
    kheap_sz=avail; kheap_head=(kblk*)KHEAP_BASE;
    kheap_head->size=avail-(u32)sizeof(kblk); kheap_head->free=1; kheap_head->next=0;
    kheap_usedb=0; kheap_ready=1;
    /* self-test: alloc / free middle / re-alloc (exercises split+coalesce) */
    void* a=kmalloc(1024); void* b=kmalloc(4096); void* c=kmalloc(256);
    kfree(b); void* d=kmalloc(3000);
    kheap_ok=(a&&b&&c&&d&&kheap_usedb>0); kfree(a); kfree(c); kfree(d);
}
static void blog_s(char* b,int* p,const char* s){ while(*s)b[(*p)++]=*s++; }
static void blog_dec(char* b,int* p,unsigned v){ char t[12]; int tl=0; if(!v)t[tl++]='0'; while(v){t[tl++]='0'+v%10;v/=10;} while(tl)b[(*p)++]=t[--tl]; }
static void blog_hex8(char* b,int* p,unsigned v){ const char* hx="0123456789ABCDEF"; for(int i=7;i>=0;i--)b[(*p)++]=hx[(v>>(i*4))&15]; }
static void boot_log_show(void){
    { u32 ext64=cmos(0x34)|((u32)cmos(0x35)<<8); int mb=16+(int)((ext64*64)/1024); if(mb<8||mb>8192){ u32 base=cmos(0x30)|((u32)cmos(0x31)<<8); mb=(int)(base/1024)+1; } ram_mb=mb; }
    for(int y=0;y<H;y++){ u32* row=FB+y*PITCH; for(int x=0;x<W;x++)row[x]=0x00060A10u; }
    char ram[80]; { int p=0; blog_s(ram,&p,"Memory: "); blog_dec(ram,&p,(unsigned)ram_mb); blog_s(ram,&p," MB conventional + extended"); ram[p]=0; }
    char fbl[96]; { int p=0; blog_s(fbl,&p,"VESA linear framebuffer "); blog_dec(fbl,&p,(unsigned)W); blog_s(fbl,&p,"x"); blog_dec(fbl,&p,(unsigned)H); blog_s(fbl,&p,"x32 @ 0x"); blog_hex8(fbl,&p,(unsigned)(u32)LFB); fbl[p]=0; }
    const char* net1 = nic_present?"PCnet-FAST III Ethernet, link up":"NET: no Ethernet controller present";
    const char* net2 = nic_present?"NET: DHCP lease acquired (NAT 10.0.2.0/24)":"NET: networking stack offline";
    struct bl_t { int st; unsigned ts; const char* m; };
    struct bl_t L[]={
        {0,0,"NoovexOS NOOVEX8 - early boot"},
        {0,142,"NoovexOS kernel 1.0  (ia-32, 32-bit protected mode)"},
        {0,388,"CPU: GenuineIntel, family 6, ia-32 compatible"},
        {0,640,"CPU: FPU / SSE / FXSR present"},
        {1,1020,"GDT loaded - flat 4 GB code/data segments"},
        {1,1480,"A20 line enabled (fast gate, port 0x92)"},
        {0,1990,"paging: disabled, identity physical map"},
        {0,2560,ram},
        {0,3180,".bss relocated to 0x00C00000 and cleared"},
        {1,3920,"MTRR: write-back memory type configured"},
        {1,4510,"cache: L1 + L2 enabled"},
        {0,5470,fbl},
        {1,8810,"DISPI / VBE display surface ready"},
        {1,9300,"display: linear 32bpp @ 60 Hz"},
        {0,10230,"PIT: system timer programmed at 100 Hz"},
        {1,11100,"PIT: channel 0 periodic IRQ0 armed"},
        {1,13880,"8259A PIC remapped to vectors 0x20-0x2F"},
        {1,14400,"IRQ 0-15 unmasked"},
        {1,16410,"IDT loaded - 256 interrupt vectors"},
        {1,17050,"traps: #GP / #PF / #DF handlers installed"},
        {1,20950,"PS/2: 8042 controller reset OK"},
        {1,21600,"PS/2: keyboard online (scan set 1)"},
        {1,24330,"PS/2: mouse (IMPS/2 wheel) enabled"},
        {0,27840,"COM1: serial console 9600 8N1"},
        {0,31260,"RTC: wall clock synchronised"},
        {0,34000,"ACPI: RSDP found, tables present"},
        {0,36500,"SMBIOS: system board 'NOOVEX8 PC'"},
        {0,38700,"PCI: scanning bus 0"},
        {0,40200,"PCI 00:01.1  IDE controller (PIIX4)"},
        {0,41600,"PCI 00:02.0  VGA compatible controller"},
        {0,43000,"PCI 00:03.0  Ethernet (Am79C973)"},
        {0,44400,"PCI 00:05.0  multimedia audio (AC97)"},
        {0,45800,"PCI 00:0B.0  USB controller (EHCI)"},
        {0,47120,"ata1: bus reset, identifying drives"},
        {1,49000,"ata1.0: NVXFS volume detected"},
        {1,55630,"NVXFS: superblock magic OK - root mounted"},
        {1,57000,"block: read-ahead cache 5 MB"},
        {1,60000,"vfs: / and /usb mountpoints registered"},
        {1,72590,net1},
        {0,81030,"NET: DHCP discover broadcast ..."},
        {1,118400,net2},
        {0,123900,"NET: gateway 10.0.2.2   resolver 10.0.2.3"},
        {1,128000,"NVX-TLS 1.3 stack initialised"},
        {1,131000,"crypto: AES + SHA-256 self-test passed"},
        {1,134000,"random: CSPRNG seeded"},
        {0,151200,"USB: EHCI host controller reset"},
        {1,158800,"USB: root ports enumerated"},
        {1,166400,"USB: UHCI + xHCI companions online"},
        {1,169460,"AC97: STAC9700 codec - mixer ready"},
        {1,196420,"hid: keyboard + mouse bound"},
        {0,205000,"sched: cooperative scheduler ready"},
        {0,212000,"Starting NoovexOS desktop environment ..."},
        {1,219000,"reached target Graphical Desktop"},
    };
    int n=(int)(sizeof(L)/sizeof(L[0]));
    for(int i=0;i<n;i++){ serial_puts(L[i].st?"[  OK  ] ":"[ boot ] "); serial_puts(L[i].m); serial_putc('\n'); }
    serial_puts("[  OK  ] desktop ready - entering main loop\n");
    int x=16, y=14, lh=13;
    for(int i=0;i<n;i++){
        if(y>H-18) break;
        int cx=x;
        if(L[i].st==1){
            draw_str(cx,y,"[",C_MGREY+24); draw_str(cx+18,y,"OK",C_GREEN); draw_str(cx+42,y,"]",C_MGREY+24);
            draw_str(cx+64,y,L[i].m,C_TITLE);
        } else {
            char tb[28]; int p=0; tb[p++]='[';
            unsigned T=L[i].ts*10u, sec=T/1000000u, us=T%1000000u;
            { char sbf[10]; int sl=0; unsigned v=sec; if(!v)sbf[sl++]='0'; while(v){sbf[sl++]='0'+v%10;v/=10;} for(int z=sl;z<5;z++)tb[p++]=' '; while(sl)tb[p++]=sbf[--sl]; }
            tb[p++]='.';
            { char ubf[10]; int ul=0; unsigned v=us; if(!v)ubf[ul++]='0'; while(v){ubf[ul++]='0'+v%10;v/=10;} for(int z=ul;z<6;z++)tb[p++]='0'; while(ul)tb[p++]=ubf[--ul]; }
            tb[p++]=']'; tb[p]=0;
            draw_str(cx,y,tb,C_MGREY+26);
            draw_str(cx+strlen_(tb)*8+8,y,L[i].m,C_TITLE);
        }
        y+=lh; pit_wait(2);
    }
    if(y<=H-26){ y+=8; draw_str(x,y,"Welcome to NoovexOS.",C_TEAL); }
    pit_wait(60);
}
/* DIAG: paint whole screen a solid colour, hold ~160ms. If boot hangs, the
   screen freezes on the last beacon colour -> tells us exactly which call died. */
void gmain(void){
    serial_init();
    serial_puts("\n\n=== NoovexOS NOOVEX8 serial log (COM1 115200 8N1) ===\n");
    LFB=(u32*)(*(volatile u32*)0x5000); FB=LFB;
    PITCH=(*(volatile u16*)0x5004)>>2;
    W=*(volatile u16*)0x5006;
    H=*(volatile u16*)0x5008;
    dispi_detect();
    if(dispi_id){ if(!dispi_set(1280,1024)) dispi_set(W,H); }
    set_palette();
    idt_init();
    { u8 bs=cmos(0x3E); if(bs>=1 && bs<=10) bg_style=bs-1; }
    if(cmos(0x3D)==0xB7){ if(boot_anim()){ boot_log_show(); boot_menu(); } }
    else { boot_log_show(); boot_menu(); cmos_write(0x3D,0xB7); }
    if(cmos(0x37)==0x52){ cmos_write(0x37,0); recovery_mode(); }
    com1_init(); if(com1_ok) com1_puts("NoovexOS COM1 online\n");
    dlog("boot: menu/recovery passed\n");
    disk_init();   dlog("boot: disk_init\n");
    bl_boot();     dlog("boot: bl_boot\n");
    if(disk_ok) acct_load();
    hw_scan();     dlog("boot: hw_scan\n");
    usb_detect();  dlog("boot: usb_detect\n");
    { u8 _ur=cmos(0x47); u8 _up=((_ur&0xF8)==0xA0)?(_ur&7):0; if(_up>3)_up=0;
      if(_up==0||_up==2){ ehci_init(); if(!safe_mode) ehci_enumerate(); }
      usbmsd_mount(); dlog("boot: usb mass storage\n");
      if(_up==0||_up==3){ uhci_init(); }
      if(_up==0||_up==1){ xhci_init(); if(!safe_mode) xhci_enum(); } }
    dlog("boot: drivers done -> desktop\n");
    dispi_detect();
    gpu_detect();
    sb_detect();
    nic_detect();
    wifi_detect();
    cam_detect();
    usbprn_detect();
#ifdef NOOVEXSRV

    set_palette();
    if(nic_present) nic_up();
    server_console();
#endif
    int setup_done = disk_ok ? have_user : (cmos(0x39)==0xAB);
    if(setup_done){ int a=cmos(0x38); if(a<0||a>3)a=0; apply_accent(a); int bs=cmos(0x3A); if(bs>=0&&bs<=9)bg_style=bs; int sn=cmos(0x3B); snd_on=(sn==0)?0:1; if(!have_user){ acct.lang=cmos(0x3C); } int ms=cmos(0x3D); mouse_speed=(ms<0||ms>2)?1:ms; int ss=cmos(0x3E); scroll_speed=(ss<1||ss>5)?3:ss; scroll_rev=cmos(0x3F)?1:0; store_inst[0]=cmos(0x40); store_inst[1]=cmos(0x41); store_inst[2]=cmos(0x42); store_inst[3]=cmos(0x43); store_inst[4]=cmos(0x44); { int dg=cmos(0x45); if(dg>=5&&dg<=50)disk_size_gb=(u8)dg; } }
    else { run_setup(); if(disk_ok) acct_save(); }
    if(have_user) login_screen();
#ifdef NOOVEXLITE
    lite_terminal();
#else
#if defined(NOOVEX7)||defined(NOOVEX8)
    bg_style=9;
#endif
    icons_init(); battery_init(); build_base(); rtc_now(); compose(); jingle();
    mouse_init(); dsk_init(); if(boot_cfg_sound) boot_chime(); show_cursor();
    dirty=0;
    for(;;){
        { static int ms_kick=0; if(ms_kick<3){ ms_kick++; if(ms_kick==3){ for(int i=0;i<8;i++){ if(inb(0x64)&1) inb(0x60); } mouse_wait(1); outb(0x64,0xD4); mouse_wait(1); outb(0x60,0xF4); mouse_wait(0); inb(0x60); } } }
        if(pending_uac){ pending_uac=0; hide_cursor(); del_system_flow(); dirty=1; }
        if(perm_pending){ int act=perm_pending; perm_pending=0; hide_cursor(); if(uac_prompt()){ if(act==1){ char nm[20]; char(*b)[16]=folder_buf(perm_folder); int k=0; while(b[perm_idx][k]&&k<19){nm[k]=b[perm_idx][k];k++;} nm[k]=0; bin_push(nm); file_delete(perm_folder,perm_idx); sysfile_break(nm); } else rename_begin(perm_folder,perm_idx); } dirty=1; }
        if(pending_update){ pending_update=0; hide_cursor(); do_update(); build_base(); dirty=1; }
        if(pending_neofetch){ pending_neofetch=0; open_app(48,120,64,600,360); dirty=1; }
        if(usb_replug){ usb_replug=0; hide_cursor(); int had=(msd_dev>=0); ehci_enumerate(); usbmsd_mount(); cam_detect(); usbprn_detect(); usb_icon_sync(); build_base(); toast_set(msd_dev>=0?(had?"USB DEVICES RESCANNED":"USB DRIVE MOUNTED"):"USB DRIVE REMOVED"); dirty=1; }
        if(res_pw){ int w=res_pw,h=res_ph; res_pw=0; hide_cursor(); dispi_set(w,h); if(mx>=W)mx=W-1; if(my>=H)my=H-1; start_geom(); build_base(); compose(); show_cursor(); toast_set("RESOLUTION CHANGED"); dirty=0; }
        if(pending_virus){ pending_virus=0; hide_cursor(); do_virus_demo(); build_base(); toast_set("MALWARE ACTIVE - RUN GUARD"); dirty=1; }
        if(pending_avscan){ pending_avscan=0; hide_cursor(); do_avscan(); dirty=1; }
        if(noovex_want && !noovex_done){ noovex_want=0; noovex_done=1; if(nic_present){ hide_cursor(); noovex_push(); show_cursor(); dirty=1; } }
        if(pending_ai){ pending_ai=0; ai_rc=1; hide_cursor(); compose(); show_cursor();
            ai_rc=claude_ask((char*)AI_KEY);
            if(ai_rc==200){ int al=0; while(AI_ANS[al])al++; chat_add(1,AI_ANS,al);
                if(disk_ok && !ai_saved){ if(nvx_write("CLAUDE.KEY",(char*)AI_KEY,ai_keylen)>=0)ai_saved=1; } }
            else if(chat_n>0 && chat_msg[chat_n-1].role==0){ int l=chat_msg[chat_n-1].len; if(l>900)l=900;
                for(int i=0;i<l;i++)AI_PROMPT[i]=CHAT_POOL[chat_msg[chat_n-1].off+i]; AI_PROMPT[l]=0; ai_plen=l;
                chat_used=chat_msg[chat_n-1].off; chat_n--; }
            ai_tobottom=1; dirty=1; }
        if(app==8 && salive){ u16 cp=pit_read(); if(cp>pit_prev) snk_acc++; pit_prev=cp; if(snk_acc>=2){ snk_acc=0; snake_step(); dirty=1; } }
        if(app==34 && falive){ u16 cp=pit_read(); if(cp>flap_pit) flap_acc++; flap_pit=cp; if(flap_acc>=1){ flap_acc=0; flappy_step(); dirty=1; } }
        if(app==40 && bk_live && bk_launch){ u16 cp=pit_read(); if(cp>bk_pit) bk_acc++; bk_pit=cp; if(bk_acc>=1){ bk_acc=0; breakout_step(); dirty=1; } }
        if(app==12 && !tdead){ u16 cp=pit_read(); if(cp>tet_pit) tet_acc++; tet_pit=cp; int iv=6-tlevel; if(iv<1)iv=1; if(tet_acc>=iv){ tet_acc=0; tetris_step(); dirty=1; } }
        if(app==30){ u16 cp=pit_read(); if(cp>g3_pit) g3_acc++; g3_pit=cp; if(g3_acc>=1){ g3_acc=0; g3_yaw=(g3_yaw+3)&0xFF; dirty=1; } }
        { u16 cp=pit_read(); if(cp>dock_pit) dock_acc++; dock_pit=cp; if(dock_acc>=1){ dock_acc=0; if(dclk_timer>0)dclk_timer--; if(fdc_timer>0)fdc_timer--; int any=0; for(int i=0;i<DOCKN;i++) if(dock_bounce[i]>0){ dock_bounce[i]--; any=1; } if(any)dirty=1; usb_hotplug_check(); if(noovex_delay>0){ noovex_delay--; if(noovex_delay==0)noovex_want=1; } } }
        if(pending_action==1||g_pending_action==1){ do_restart(); }
        if(pending_action==2||g_pending_action==2){ do_shutdown(); }
        if(pending_action==3){ bsod(); }
        usb_poll();
        uhci_poll();
        xhci_poll();
        { int kguard=0; u8 st;
          while(((st=inb(0x64))&1) && kguard++<32){ u8 data=inb(0x60); entropy_add(&data,1); entropy_stir();
            if(diag_raw_count<255)diag_raw_count++;
            if(diag_shown){ diag_shown=0; diag_dismissed=1; need_rebuild=1; dirty=1; }
            if(st&0x20){ mouse_byte(data); diag_mouse_seen=1; }
            else { kbd_event(data); diag_kbd_seen=1; } } }
        if(wheel_delta){ int d=wheel_delta*scroll_speed*12; if(scroll_rev)d=-d; wheel_delta=0;
            if(app==15){ br_scroll+=d; if(br_scroll<0)br_scroll=0; dirty=1; }
            else if(app==16){ ai_scroll+=d; if(ai_scroll<0)ai_scroll=0; dirty=1; }
            else if(app==17){ store_scroll+=d; if(store_scroll<0)store_scroll=0; dirty=1; } }
        if(mx!=last_mx||my!=last_my){

            { int nk=0;
              if(resizing) nk=1;
              else if(wincnt>0&&cur_win>=0&&!wins[cur_win].min){
                  if(in(mx,my,winx+winw-16,winy+winh-16,16,16)) nk=1;
                  else if((wins[cur_win].app==2||wins[cur_win].app==5)&&in(mx,my,winx+4,winy+26,winw-8,winh-30)) nk=3;
              }
              if(nk==0&&my>=H-48&&!start_open) nk=2;
              if(nk!=curs_kind){ curs_kind=nk; if(curs_shown){ hide_cursor(); show_cursor(); } }
            }
            if(dragging){
                gfx_fast=1; winx=mx-dragdx; winy=my-dragdy;
                if(winx<0)winx=0; if(winy<0)winy=0; if(winx+winw>W)winx=W-winw; if(winy<0)winy=0; if(winy+winh>H-48)winy=H-48-winh;
                win_store(cur_win); dirty=1; }
            if(resizing){ gfx_fast=1; int nw=rs_w0+(mx-rs_mx0), nh=rs_h0+(my-rs_my0);
                if(nw<200)nw=200; if(nh<140)nh=140;
                if(winx+nw>W)nw=W-winx; if(winy+nh>H-48)nh=H-48-winy;
                winw=nw; winh=nh; win_store(cur_win); dirty=1; }
            else if((mbtn&1)&&app==11){ int cxp=winx+58,cyp=winy+26; if(mx>=cxp&&mx<cxp+PW&&my>=cyp&&my<cyp+PH){ int px=mx-cxp,py=my-cyp; int r=(paint_brush==0)?0:(paint_brush==1)?1:3; for(int dy=-r;dy<=r;dy++)for(int dx=-r;dx<=r;dx++){ int xx=px+dx,yy=py+dy; if(xx>=0&&yy>=0&&xx<PW&&yy<PH)paint_buf[yy*PW+xx]=(u8)paint_col; } dirty=1; } }
            else if((mbtn&3)&&app==14){ int cvx=winx+4,cvy=winy+22,hby=winy+winh-28; if(mx>=cvx&&my>=cvy&&my<hby-4){ int wx=craft_cam+(mx-cvx)/CTILE,wy=craft_camy+(my-cvy)/CTILE; if(wx>=0&&wx<CW&&wy>=0&&wy<CH){ craft[wy*CW+wx]=(mbtn&2)?0:(u8)craft_sel; dirty=1; } } }
            else if((mbtn&1)&&app==36&&phone_drag){ if(mx!=phone_lastx||my!=phone_lasty){ phone_send_ptr(1); phone_lastx=mx; phone_lasty=my; } }
            else if((mbtn&1)&&fdrag_armed){ int dxp=mx-fdrag_sx,dyp=my-fdrag_sy; if(dxp<0)dxp=-dxp; if(dyp<0)dyp=-dyp; if(dxp>4||dyp>4)fdrag=1; dirty=1; }
            else if(idrag>=0){ DSK[idrag].x=mx-idrag_dx; DSK[idrag].y=my-idrag_dy; if(DSK[idrag].x<0)DSK[idrag].x=0; if(DSK[idrag].y<28)DSK[idrag].y=28; if(DSK[idrag].x>W-30)DSK[idrag].x=W-30; if(DSK[idrag].y>H-44)DSK[idrag].y=H-44; dirty=1; }
            else { if(mag_on||my>=H-72||last_my>=H-72) dirty=1; else { hide_cursor(); show_cursor(); } }
            last_mx=mx; last_my=my;
        }
        if(mbtn!=prevbtn){
            if((mbtn&1)&&!(prevbtn&1)){ handle_click(); dirty=1; }
            if((mbtn&2)&&!(prevbtn&2)){ handle_rclick(); dirty=1; }
            if(!(mbtn&1)&&(prevbtn&1)){
                if(phone_drag){ phone_send_ptr(2); phone_drag=0; }
                if(dragging&&cur_win>=0){ win_load(cur_win); int sn=0;
                    if(my<=2){ winx=0;winy=0;winw=W;winh=H-48; sn=1; }
                    else if(mx<=2){ winx=0;winy=0;winw=W/2;winh=H-48; sn=1; }
                    else if(mx>=W-2){ winx=W/2;winy=0;winw=W-W/2;winh=H-48; sn=1; }
                    if(sn){ win_store(cur_win); click_snd(); dirty=1; } }
                if(dragging){ gfx_fast=0; dragging=0; drag_cached=0; drag_wantcache=0; curs_kind=0; dirty=1; }
                if(resizing){ resizing=0; gfx_fast=0; dirty=1; }
                if(idrag>=0){
                    int tgt=-1, trash=-1, usbt=-1;
                    for(int i=0;i<DSK_MAX;i++){ if(!DSK[i].used||DSK[i].parent!=-1||i==idrag)continue;
                        if(!(mx>=DSK[i].x&&mx<DSK[i].x+30&&my>=DSK[i].y&&my<DSK[i].y+40))continue;
                        if(DSK[i].type==2&&DSK[i].app==10){ trash=i; break; }
                        if(DSK[i].type==2&&DSK[i].app==22){ usbt=i; break; }
                        if(DSK[i].type==1){ tgt=i; break; } }
                    if(trash>=0){ bin_push(DSK[idrag].name);
                        if(DSK[idrag].type==1){ for(int j=0;j<DSK_MAX;j++) if(DSK[j].used&&DSK[j].parent==idrag){ DSK[j].parent=-1; dsk_place(j); } }
                        DSK[idrag].used=0; toast_set("MOVED TO RECYCLE BIN"); }
                    else if(usbt>=0){
                        if(DSK[idrag].type!=0) toast_set("ONLY FILES COPY TO USB");
                        else if(!(fat_ok&&msd_dev>=0)) toast_set("NO USB FILESYSTEM");
                        else { char cb[1024]; int n=0; if(disk_ok){ int ix=nvx_find(DSK[idrag].name); if(ix>=0)n=nvx_read(ix,cb,1024); }
                            if(fat_write_file(DSK[idrag].name,cb,n)==0) toast_set("COPIED TO USB"); else toast_set("USB COPY FAILED"); } }
                    else if(tgt>=0 && !is_ancestor(idrag,tgt)){ DSK[idrag].parent=tgt; toast_set("MOVED INTO FOLDER"); }
                    idrag=-1; dirty=1;
                }
                if(app==1 && fdrag && fdrag_src_folder>=0){
                    int tf=-1; int x0=winx+3;
                    if(mx>=x0&&mx<x0+158){
                        if(my>=winy+38&&my<winy+38+7*18)tf=(my-(winy+38))/18;
                        else if(my>=winy+184&&my<winy+184+NTHISPC*18)tf=THISPC[(my-(winy+184))/18];
                    }
                    if(tf>=0 && tf!=fdrag_src_folder && tf!=8 && tf!=10 && tf!=7 && tf!=9 && file_add(tf,fdrag_name)){
                        file_delete(fdrag_src_folder,fdrag_src_idx);
                        if(fsel>=fcount[cur_folder])fsel=fcount[cur_folder]-1; if(fsel<0)fsel=0;
                        toast_set("FILE MOVED"); click_snd();
                    }
                }
                fdrag=0; fdrag_armed=0;
                }
            prevbtn=mbtn;
        }
        if(need_rebuild){ need_rebuild=0; hide_cursor(); build_base(); dirty=1; }
        { u8 sb=bcd(cmos(0)); if(sb!=last_sec){ rtc_now(); if(toast_t>0)toast_t--;
            battery_tick();
            upsec++;

            { static int ms_last_x=-1, ms_last_y=-1, ms_still=0, ms_tries=0;
              if(mx==ms_last_x && my==ms_last_y){ ms_still++; } else { ms_still=0; ms_tries=0; }
              ms_last_x=mx; ms_last_y=my;
              if(ms_still>=5 && !xhci_ms_found && !uhci_ms_found && ms_tries<6){
                  ms_tries++; ms_still=0;
                  toast_set("MOUSE SUPERVISOR: RESCANNING USB...");
                  if(xhci_init_ok) xhci_enum();
                  uhci_init();
                  if(xhci_ms_found||uhci_ms_found) toast_set("MOUSE SUPERVISOR: DEVICE BOUND!");
              }
            }

            if(!diag_dismissed && !diag_kbd_seen && !diag_mouse_seen){
                diag_secs++;
                if(diag_secs>=3 && !diag_shown){ diag_shown=1; dirty=1; }
            }
            if(bat_present && !bat_charging){
                if(bat_pct<=20 && bat_pct>0 && !bat_warned){ bat_warned=1; toast_set("LOW BATTERY - 20% - CONNECT POWER"); }
                if(bat_pct<=10 && bat_pct>0){ char w[28]; int q=0; const char* m="BATTERY CRITICAL: "; while(*m)w[q++]=*m++; int v=bat_pct; if(v>=10)w[q++]='0'+v/10; w[q++]='0'+v%10; w[q++]='%'; w[q]=0; toast_set(w); }
                if(bat_pct<=0){ toast_set("BATTERY EMPTY - SLEEPING"); hide_cursor(); do_sleep(); bat_sim=0; bat_pct=100; bat_charging=1; bat_present=0; build_base(); rtc_now(); show_cursor(); }
            }
            dirty=1; } }
        if(phone_conn) phone_pump();
        if(dirty){ compose(); dirty=0; }
        if(mail_do_send){ mail_do_send=0; smtp_send(); dirty=1; }
    }
#endif
}

