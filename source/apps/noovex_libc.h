/* ============================================================
   noovex_libc.h - freestanding libc shim for NoovexOS programs
   (link at 0x04000000, int 0x80 syscalls via noovex.h)

   Declarations are always visible. Definitions are emitted only
   where NVX_LIBC_IMPL is defined  -> put that in exactly ONE .c
   (single-file programs: define it in that file before #include).

   Provides: coalescing malloc/free/calloc/realloc, mem*, str*,
   ctype, atoi/atol/strtol/abs/labs, and a whole-file-in-RAM stdio
   (fopen/fread/fwrite/fseek/ftell/fclose backed by syscalls 11/12)
   plus printf-family (snprintf/vsnprintf/sprintf/sscanf).
   ============================================================ */
#ifndef NOOVEX_LIBC_H
#define NOOVEX_LIBC_H
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include "noovex.h"

/* heap region: VM is guaranteed >=1GB (boot refuses <1GB), program
   image lives <0x04400000, so a base at 0x05000000 never collides. */
#ifndef NVX_HEAP_BASE
#define NVX_HEAP_BASE 0x09000000u
#endif
#ifndef NVX_HEAP_SIZE
#define NVX_HEAP_SIZE (224u*1024u*1024u)
#endif

void*  malloc(size_t n);
void   free(void* p);
void*  calloc(size_t a, size_t b);
void*  realloc(void* p, size_t n);
size_t heap_used(void);

void*  memcpy(void* d, const void* s, size_t n);
void*  memmove(void* d, const void* s, size_t n);
void*  memset(void* d, int c, size_t n);
int    memcmp(const void* a, const void* b, size_t n);
void*  memchr(const void* s, int c, size_t n);

size_t strlen(const char* s);
char*  strcpy(char* d, const char* s);
char*  strncpy(char* d, const char* s, size_t n);
char*  strcat(char* d, const char* s);
char*  strncat(char* d, const char* s, size_t n);
int    strcmp(const char* a, const char* b);
int    strncmp(const char* a, const char* b, size_t n);
int    strcasecmp(const char* a, const char* b);
int    strncasecmp(const char* a, const char* b, size_t n);
char*  strchr(const char* s, int c);
char*  strrchr(const char* s, int c);
char*  strstr(const char* h, const char* n);

int    isspace(int c); int isdigit(int c); int isalpha(int c);
int    isalnum(int c); int isprint(int c); int toupper(int c); int tolower(int c);

int    atoi(const char* s);
long   atol(const char* s);
long   strtol(const char* s, char** end, int base);
int    abs(int x);
long   labs(long x);

int    snprintf(char* buf, size_t n, const char* fmt, ...);
int    vsnprintf(char* buf, size_t n, const char* fmt, va_list ap);
int    sprintf(char* buf, const char* fmt, ...);
int    vsprintf(char* buf, const char* fmt, va_list ap);
int    sscanf(const char* str, const char* fmt, ...);
int    printf(const char* fmt, ...);

/* whole-file-in-RAM stdio (backed by NVXFS syscalls 11/12) */
typedef struct { unsigned char* buf; long size, cap, pos; int w, dirty; char name[32]; } FILE;
FILE*  fopen(const char* name, const char* mode);
size_t fread(void* p, size_t sz, size_t n, FILE* f);
size_t fwrite(const void* p, size_t sz, size_t n, FILE* f);
int    fseek(FILE* f, long off, int whence);
long   ftell(FILE* f);
int    fclose(FILE* f);
int    feof(FILE* f);
int    fgetc(FILE* f);
char*  fgets(char* s, int n, FILE* f);
int    fputc(int c, FILE* f);
int    fputs(const char* s, FILE* f);
int    fprintf(FILE* f, const char* fmt, ...);
int    remove(const char* name);
void   exit(int code);
#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif
#ifndef NULL
#define NULL ((void*)0)
#endif

/* ====================== IMPLEMENTATION ====================== */
#ifdef NVX_LIBC_IMPL

typedef struct nx_blk { size_t size; int free; struct nx_blk* next; } nx_blk;
static nx_blk* nx_head = 0;
static size_t  nx_usedb = 0;
static int     nx_ready = 0;

static void nx_heap_init(void){
    nx_head = (nx_blk*)(uintptr_t)NVX_HEAP_BASE;
    nx_head->size = (size_t)NVX_HEAP_SIZE - sizeof(nx_blk);
    nx_head->free = 1; nx_head->next = 0;
    nx_usedb = 0; nx_ready = 1;
}
static void nx_coalesce(void){
    nx_blk* b = nx_head;
    while(b && b->next){
        if(b->free && b->next->free){ b->size += sizeof(nx_blk) + b->next->size; b->next = b->next->next; }
        else b = b->next;
    }
}
void* malloc(size_t n){
    if(!nx_ready) nx_heap_init();
    if(!n) n = 1; n = (n + 7u) & ~((size_t)7u);
    for(nx_blk* b = nx_head; b; b = b->next){
        if(b->free && b->size >= n){
            if(b->size >= n + sizeof(nx_blk) + 16u){
                nx_blk* s = (nx_blk*)((uint8_t*)(b + 1) + n);
                s->size = b->size - n - sizeof(nx_blk); s->free = 1; s->next = b->next;
                b->size = n; b->next = s;
            }
            b->free = 0; nx_usedb += b->size; return (void*)(b + 1);
        }
    }
    return 0;
}
void free(void* p){
    if(!p) return; nx_blk* b = ((nx_blk*)p) - 1; if(b->free) return;
    b->free = 1; if(nx_usedb >= b->size) nx_usedb -= b->size; nx_coalesce();
}
void* calloc(size_t a, size_t b){ size_t n = a * b; void* p = malloc(n); if(p) memset(p, 0, n); return p; }
void* realloc(void* p, size_t n){
    if(!p) return malloc(n); if(!n){ free(p); return 0; }
    nx_blk* b = ((nx_blk*)p) - 1; size_t old = b->size; if(old >= n) return p;
    void* q = malloc(n); if(!q) return 0; memcpy(q, p, old); free(p); return q;
}
size_t heap_used(void){ return nx_usedb; }

void* memcpy(void* d, const void* s, size_t n){ uint8_t* a=(uint8_t*)d; const uint8_t* b=(const uint8_t*)s; while(n--) *a++=*b++; return d; }
void* memmove(void* d, const void* s, size_t n){ uint8_t* a=(uint8_t*)d; const uint8_t* b=(const uint8_t*)s;
    if(a<b){ while(n--) *a++=*b++; } else { a+=n; b+=n; while(n--) *--a=*--b; } return d; }
void* memset(void* d, int c, size_t n){ uint8_t* a=(uint8_t*)d; while(n--) *a++=(uint8_t)c; return d; }
int   memcmp(const void* a, const void* b, size_t n){ const uint8_t* x=(const uint8_t*)a,*y=(const uint8_t*)b; while(n--){ if(*x!=*y) return *x-*y; x++; y++; } return 0; }
void* memchr(const void* s, int c, size_t n){ const uint8_t* p=(const uint8_t*)s; while(n--){ if(*p==(uint8_t)c) return (void*)p; p++; } return 0; }

size_t strlen(const char* s){ size_t n=0; while(s[n]) n++; return n; }
char*  strcpy(char* d, const char* s){ char* r=d; while((*d++=*s++)); return r; }
char*  strncpy(char* d, const char* s, size_t n){ char* r=d; while(n&&(*d=*s)){ d++; s++; n--; } while(n--) *d++=0; return r; }
char*  strcat(char* d, const char* s){ char* r=d; while(*d) d++; while((*d++=*s++)); return r; }
char*  strncat(char* d, const char* s, size_t n){ char* r=d; while(*d) d++; while(n&&*s){ *d++=*s++; n--; } *d=0; return r; }
int    strcmp(const char* a, const char* b){ while(*a&&*a==*b){ a++; b++; } return (unsigned char)*a-(unsigned char)*b; }
int    strncmp(const char* a, const char* b, size_t n){ while(n&&*a&&*a==*b){ a++; b++; n--; } if(!n) return 0; return (unsigned char)*a-(unsigned char)*b; }
static int lc(int c){ return (c>='A'&&c<='Z')?c+32:c; }
int    strcasecmp(const char* a, const char* b){ while(*a&&lc(*a)==lc(*b)){ a++; b++; } return lc((unsigned char)*a)-lc((unsigned char)*b); }
int    strncasecmp(const char* a, const char* b, size_t n){ while(n&&*a&&lc(*a)==lc(*b)){ a++; b++; n--; } if(!n) return 0; return lc((unsigned char)*a)-lc((unsigned char)*b); }
char*  strchr(const char* s, int c){ for(;;s++){ if(*s==(char)c) return (char*)s; if(!*s) return 0; } }
char*  strrchr(const char* s, int c){ const char* r=0; for(;;s++){ if(*s==(char)c) r=s; if(!*s) return (char*)r; } }
char*  strstr(const char* h, const char* n){ if(!*n) return (char*)h; for(; *h; h++){ const char* a=h,*b=n; while(*a&&*b&&*a==*b){ a++; b++; } if(!*b) return (char*)h; } return 0; }

int isspace(int c){ return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\v'||c=='\f'; }
int isdigit(int c){ return c>='0'&&c<='9'; }
int isalpha(int c){ return (c>='A'&&c<='Z')||(c>='a'&&c<='z'); }
int isalnum(int c){ return isalpha(c)||isdigit(c); }
int isprint(int c){ return c>=0x20&&c<0x7f; }
int toupper(int c){ return (c>='a'&&c<='z')?c-32:c; }
int tolower(int c){ return (c>='A'&&c<='Z')?c+32:c; }

long strtol(const char* s, char** end, int base){
    while(isspace((unsigned char)*s)) s++;
    int neg=0; if(*s=='+'||*s=='-'){ neg=(*s=='-'); s++; }
    if((base==0||base==16)&&s[0]=='0'&&(s[1]=='x'||s[1]=='X')){ s+=2; base=16; }
    if(base==0){ base=(*s=='0')?8:10; }
    long v=0;
    for(;;){ int c=(unsigned char)*s,d;
        if(c>='0'&&c<='9') d=c-'0'; else if(c>='a'&&c<='z') d=c-'a'+10; else if(c>='A'&&c<='Z') d=c-'A'+10; else break;
        if(d>=base) break; v=v*base+d; s++; }
    if(end) *end=(char*)s; return neg?-v:v;
}
int  atoi(const char* s){ return (int)strtol(s,0,10); }
long atol(const char* s){ return strtol(s,0,10); }
int  abs(int x){ return x<0?-x:x; }
long labs(long x){ return x<0?-x:x; }

/* ---- printf core ---- */
static void pc_(char* b, size_t n, size_t* o, char c){ if(*o<n) b[*o]=c; (*o)++; }
static void ps_(char* b, size_t n, size_t* o, const char* s){ while(*s) pc_(b,n,o,*s++); }
static void pnum_(char* b, size_t n, size_t* o, unsigned long v, int base, int up, int width, int zero, int neg, int prec){
    char t[34]; int i=0; const char* dg= up?"0123456789ABCDEF":"0123456789abcdef";
    if(!v) t[i++]='0'; while(v){ t[i++]=dg[v%base]; v/=base; }
    while(i<prec && i<33) t[i++]='0';
    int len=i+(neg?1:0); int pad=width-len;
    if(!zero){ while(pad-->0) pc_(b,n,o,' '); }
    if(neg) pc_(b,n,o,'-');
    if(zero){ while(pad-->0) pc_(b,n,o,'0'); }
    while(i) pc_(b,n,o,t[--i]);
}
int vsnprintf(char* buf, size_t n, const char* fmt, va_list ap){
    size_t o=0; if(n==0) n=1;
    for(; *fmt; fmt++){
        if(*fmt!='%'){ pc_(buf,n,&o,*fmt); continue; }
        fmt++; int zero=0,width=0,lng=0,prec=-1;
        if(*fmt=='-'){ fmt++; } /* left-justify ignored, rare in DOOM */
        if(*fmt=='0'){ zero=1; fmt++; }
        while(isdigit((unsigned char)*fmt)){ width=width*10+(*fmt-'0'); fmt++; }
        if(*fmt=='.'){ fmt++; prec=0; while(isdigit((unsigned char)*fmt)){ prec=prec*10+(*fmt-'0'); fmt++; } }
        while(*fmt=='l'){ lng++; fmt++; }
        switch(*fmt){
            case 'd': case 'i': { long v= lng?va_arg(ap,long):(long)va_arg(ap,int);
                int neg=v<0; unsigned long u= neg?(unsigned long)(-v):(unsigned long)v; pnum_(buf,n,&o,u,10,0,width,zero,neg,prec); } break;
            case 'u': { unsigned long v= lng?va_arg(ap,unsigned long):(unsigned long)va_arg(ap,unsigned int); pnum_(buf,n,&o,v,10,0,width,zero,0,prec); } break;
            case 'x': { unsigned long v= lng?va_arg(ap,unsigned long):(unsigned long)va_arg(ap,unsigned int); pnum_(buf,n,&o,v,16,0,width,zero,0,prec); } break;
            case 'X': { unsigned long v= lng?va_arg(ap,unsigned long):(unsigned long)va_arg(ap,unsigned int); pnum_(buf,n,&o,v,16,1,width,zero,0,prec); } break;
            case 'p': { unsigned long v=(unsigned long)(uintptr_t)va_arg(ap,void*); ps_(buf,n,&o,"0x"); pnum_(buf,n,&o,v,16,0,0,0,0,-1); } break;
            case 'c': pc_(buf,n,&o,(char)va_arg(ap,int)); break;
            case 's': { const char* s=va_arg(ap,const char*); if(!s) s="(null)"; int l=(int)strlen(s); if(prec>=0&&l>prec)l=prec; int pad=width-l; while(pad-->0) pc_(buf,n,&o,' '); for(int k=0;k<l;k++) pc_(buf,n,&o,s[k]); } break;
            case '%': pc_(buf,n,&o,'%'); break;
            default: pc_(buf,n,&o,'%'); pc_(buf,n,&o,*fmt); break;
        }
    }
    buf[o<n?o:n-1]=0; return (int)o;
}
int vsprintf(char* buf, const char* fmt, va_list ap){ return vsnprintf(buf,(size_t)1<<30,fmt,ap); }
int snprintf(char* buf, size_t n, const char* fmt, ...){ va_list ap; va_start(ap,fmt); int r=vsnprintf(buf,n,fmt,ap); va_end(ap); return r; }
int sprintf(char* buf, const char* fmt, ...){ va_list ap; va_start(ap,fmt); int r=vsnprintf(buf,(size_t)1<<30,fmt,ap); va_end(ap); return r; }
int printf(const char* fmt, ...){ char tmp[512]; va_list ap; va_start(ap,fmt); int r=vsnprintf(tmp,sizeof tmp,fmt,ap); va_end(ap); nvx_print(tmp); return r; }

/* ---- minimal sscanf (handles %d %u %x %s %c, whitespace) ---- */
int vsscanf(const char* str, const char* fmt, va_list ap){
    int got=0;
    while(*fmt){
        if(isspace((unsigned char)*fmt)){ while(isspace((unsigned char)*str)) str++; fmt++; continue; }
        if(*fmt!='%'){ if(*str!=*fmt){ break; } str++; fmt++; continue; }
        fmt++;
        int suppress=0; if(*fmt=='*'){ suppress=1; fmt++; }
        int width=0; while(*fmt>='0'&&*fmt<='9'){ width=width*10+(*fmt-'0'); fmt++; }
        if(width<=0) width=0x7fffffff;
        switch(*fmt){
            case 'd': case 'u': case 'x': {
                int base=(*fmt=='x')?16:10; int sign=1; int w=width;
                while(isspace((unsigned char)*str)) str++;
                if(w>0&&*str=='-'){ sign=-1; str++; w--; } else if(w>0&&*str=='+'){ str++; w--; }
                long v=0; int any=0;
                while(w-->0){ int c=(unsigned char)*str,d;
                    if(c>='0'&&c<='9') d=c-'0'; else if(base==16&&c>='a'&&c<='f') d=c-'a'+10; else if(base==16&&c>='A'&&c<='F') d=c-'A'+10; else break;
                    v=v*base+d; str++; any=1; }
                if(!any) goto done; if(!suppress){ *va_arg(ap,int*)=(int)(sign*v); got++; } } break;
            case 's': {
                while(isspace((unsigned char)*str)) str++;
                char* d=suppress?0:va_arg(ap,char*); int any=0; int w=width;
                while(*str&&!isspace((unsigned char)*str)&&w-->0){ if(d)*d++=*str; str++; any=1; }
                if(d)*d=0; if(!any) goto done; if(!suppress)got++; } break;
            case 'c': { int w=(width==0x7fffffff)?1:width; char* d=suppress?0:va_arg(ap,char*);
                while(w-->0){ char ch=*str; if(ch) str++; if(d)*d++=ch; } if(!suppress)got++; } break;
            case '[': {
                fmt++; int neg=0; if(*fmt=='^'){ neg=1; fmt++; }
                unsigned char set[256]; for(int i=0;i<256;i++) set[i]=0;
                if(*fmt==']'){ set[(unsigned char)']']=1; fmt++; }
                while(*fmt&&*fmt!=']'){ set[(unsigned char)*fmt]=1; fmt++; }
                char* d=suppress?0:va_arg(ap,char*); int any=0; int w=width;
                while(*str&&w-->0){ unsigned char c=(unsigned char)*str; int in=set[c]; if(neg) in=!in; if(!in) break; if(d)*d++=c; str++; any=1; }
                if(d)*d=0; if(!any) goto done; if(!suppress)got++;
                if(*fmt==']') fmt++; continue; }
            default: goto done;
        }
        fmt++;
    }
done: return got;
}
int sscanf(const char* str, const char* fmt, ...){ va_list ap; va_start(ap,fmt); int r=vsscanf(str,fmt,ap); va_end(ap); return r; }

/* ---- whole-file stdio ---- */
#ifndef NVX_FILE_MAX
#define NVX_FILE_MAX 8
#endif
static FILE nx_files[NVX_FILE_MAX];
FILE* fopen(const char* name, const char* mode){
    int w = (mode && (mode[0]=='w'||mode[0]=='a'));
    FILE* f=0; for(int i=0;i<NVX_FILE_MAX;i++) if(!nx_files[i].buf){ f=&nx_files[i]; break; }
    if(!f) return 0;
    int k=0; while(name[k]&&k<31){ f->name[k]=name[k]; k++; } f->name[k]=0;
    f->pos=0; f->w=w; f->dirty=0;
    if(w){ f->cap=1<<20; f->buf=(unsigned char*)malloc(f->cap); if(!f->buf) return 0; f->size=0; }
    else {
        f->cap = 8*1024*1024; f->buf=(unsigned char*)malloc(f->cap);
        if(!f->buf) return 0;
        int n=nvx_fileread(name,(char*)f->buf,(int)f->cap);
        if(n<0){ free(f->buf); f->buf=0; return 0; }
        f->size=n;
    }
    return f;
}
size_t fread(void* p, size_t sz, size_t n, FILE* f){
    if(!f||!f->buf) return 0; long want=(long)(sz*n); long avail=f->size-f->pos; if(want>avail) want=avail;
    if(want<0) want=0; memcpy(p,f->buf+f->pos,(size_t)want); f->pos+=want; return sz?((size_t)want/sz):0;
}
size_t fwrite(const void* p, size_t sz, size_t n, FILE* f){
    if(!f||!f->buf) return 0; long want=(long)(sz*n);
    if(f->pos+want>f->cap){ long nc=f->cap*2; while(nc<f->pos+want) nc*=2; unsigned char* nb=(unsigned char*)realloc(f->buf,nc); if(!nb) return 0; f->buf=nb; f->cap=nc; }
    memcpy(f->buf+f->pos,p,(size_t)want); f->pos+=want; if(f->pos>f->size) f->size=f->pos; f->dirty=1; return n;
}
int fseek(FILE* f, long off, int whence){ if(!f) return -1;
    long b = whence==SEEK_CUR? f->pos : whence==SEEK_END? f->size : 0; f->pos=b+off; if(f->pos<0) f->pos=0; return 0; }
long ftell(FILE* f){ return f? f->pos : -1; }
int feof(FILE* f){ return f? (f->pos>=f->size) : 1; }
int fgetc(FILE* f){ if(!f||f->pos>=f->size) return -1; return f->buf[f->pos++]; }
char* fgets(char* s, int n, FILE* f){ if(!f||f->pos>=f->size||n<=0) return 0; int i=0; while(i<n-1&&f->pos<f->size){ char c=f->buf[f->pos++]; s[i++]=c; if(c=='\n') break; } s[i]=0; return s; }
int fputc(int c, FILE* f){ unsigned char ch=(unsigned char)c; return fwrite(&ch,1,1,f)?c:-1; }
int fputs(const char* s, FILE* f){ return (int)fwrite(s,1,strlen(s),f); }
int fprintf(FILE* f, const char* fmt, ...){ char tmp[512]; va_list ap; va_start(ap,fmt); int r=vsnprintf(tmp,sizeof tmp,fmt,ap); va_end(ap); fwrite(tmp,1,(size_t)r,f); return r; }
int fclose(FILE* f){ if(!f) return -1; if(f->w&&f->dirty) nvx_filewrite(f->name,(const char*)f->buf,(int)f->size); if(f->buf) free(f->buf); f->buf=0; return 0; }
int remove(const char* name){ return nvx_remove(name); }
void exit(int code){ (void)code; nvx_exit(); for(;;){} }

#endif /* NVX_LIBC_IMPL */
#endif /* NOOVEX_LIBC_H */
