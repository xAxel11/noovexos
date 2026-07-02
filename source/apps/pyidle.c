/* PYIDLE - Python IDLE for NoovexOS (devkit).
   Top pane: editable script. Bottom pane: output console.
   F5 run | ^S save SCRIPT.PY | ^O open SCRIPT.PY | ^N new | ESC stop script / quit.
   Embeds the pymini interpreter (compiled with -DIDE: it calls ide_out /
   polls ide_should_stop so a runaway loop can be stopped with ESC). */
#define NVX_LIBC_IMPL
#include "noovex.h"
#include "noovex_libc.h"
#include "font8x14.h"

#define CW 8
#define CH 15
#define MAXBUF 100000
#define MAXOUT 32768

typedef unsigned int u32t;
static u32t* BB; static u32t* LFB; static int PITCH,SW,SH;
static char buf[MAXBUF]; static int len=0,cur=0,topline=0,modified=0;
static char out[MAXOUT]; static int olen=0;
static int running=0, stop_req=0;
static char status[64]="READY";

/* ---- keymap (scancode set 1) ---- */
static const char KN[128]={
 [0x02]='1',[0x03]='2',[0x04]='3',[0x05]='4',[0x06]='5',[0x07]='6',[0x08]='7',[0x09]='8',[0x0A]='9',[0x0B]='0',[0x0C]='-',[0x0D]='=',
 [0x10]='q',[0x11]='w',[0x12]='e',[0x13]='r',[0x14]='t',[0x15]='y',[0x16]='u',[0x17]='i',[0x18]='o',[0x19]='p',[0x1A]='[',[0x1B]=']',
 [0x1E]='a',[0x1F]='s',[0x20]='d',[0x21]='f',[0x22]='g',[0x23]='h',[0x24]='j',[0x25]='k',[0x26]='l',[0x27]=';',[0x28]='\'',[0x29]='`',
 [0x2B]='\\',[0x2C]='z',[0x2D]='x',[0x2E]='c',[0x2F]='v',[0x30]='b',[0x31]='n',[0x32]='m',[0x33]=',',[0x34]='.',[0x35]='/',[0x39]=' ',
};
static const char KS[128]={
 [0x02]='!',[0x03]='@',[0x04]='#',[0x05]='$',[0x06]='%',[0x07]='^',[0x08]='&',[0x09]='*',[0x0A]='(',[0x0B]=')',[0x0C]='_',[0x0D]='+',
 [0x10]='Q',[0x11]='W',[0x12]='E',[0x13]='R',[0x14]='T',[0x15]='Y',[0x16]='U',[0x17]='I',[0x18]='O',[0x19]='P',[0x1A]='{',[0x1B]='}',
 [0x1E]='A',[0x1F]='S',[0x20]='D',[0x21]='F',[0x22]='G',[0x23]='H',[0x24]='J',[0x25]='K',[0x26]='L',[0x27]=':',[0x28]='"',[0x29]='~',
 [0x2B]='|',[0x2C]='Z',[0x2D]='X',[0x2E]='C',[0x2F]='V',[0x30]='B',[0x31]='N',[0x32]='M',[0x33]='<',[0x34]='>',[0x35]='?',[0x39]=' ',
};

/* ---- drawing ---- */
#define BG    0x14141C
#define BGOUT 0x0C0C12
#define BAR   0x283860
#define SEP   0x3A5080
#define TXT   0xD0D4D8
#define LNUM  0x4A4A60
#define OTXT  0x7FD98C
#define ERRC  0xFF7060
#define CURC  0xFFFFFF
static void pset(int x,int y,u32t c){ if((unsigned)x<(unsigned)SW&&(unsigned)y<(unsigned)SH)BB[y*SW+x]=c; }
static void frect(int x,int y,int w,int h,u32t c){ for(int j=0;j<h;j++)for(int i=0;i<w;i++)pset(x+i,y+j,c); }
static void dchar(int x,int y,char ch,u32t c){ if(ch<32||ch>126)return; const unsigned char* g=FONT814[ch-32];
    for(int r=0;r<14;r++)for(int col=0;col<8;col++) if(g[r]&(1<<col)) pset(x+col,y+r,c); }
static void dtext(int x,int y,const char* t,u32t c){ while(*t){ dchar(x,y,*t++,c); x+=CW; } }
static void flip(void){ for(int y=0;y<SH;y++){ u32t* d=LFB+y*PITCH; u32t* s=BB+y*SW; for(int x=0;x<SW;x++)d[x]=s[x]; } }

/* ---- interpreter hooks (pymini -DIDE) ---- */
void ide_out(const char* s){ while(*s&&olen<MAXOUT-1){ out[olen++]=*s++; } out[olen]=0; }
int ide_should_stop(void){
    if(stop_req) return 1;
    for(int g=0; g<8; g++){ int k=nvx_readraw(); if(k<0)break; if((k&0x7F)==0x01 && !(k&0x80)){ stop_req=1; return 1; } }
    return 0;
}

/* pull in the interpreter (py_run, vars, g_err, g_errmsg, nvars) */
#define IDE 1
#include "pymini.c"

/* ---- text buffer ops ---- */
static int line_start(int i){ while(i>0&&buf[i-1]!='\n')i--; return i; }
static int line_end(int i){ while(i<len&&buf[i]!='\n')i++; return i; }
static int cur_line(void){ int n=0; for(int i=0;i<cur;i++) if(buf[i]=='\n')n++; return n; }
static void ins(char c){ if(len>=MAXBUF-1)return; for(int i=len;i>cur;i--)buf[i]=buf[i-1]; buf[cur++]=c; len++; buf[len]=0; modified=1; }
static void del(void){ if(cur<=0)return; for(int i=cur-1;i<len-1;i++)buf[i]=buf[i+1]; cur--; len--; buf[len]=0; modified=1; }

/* ---- render ---- */
static void render(void){
    int rows_e=(SH*55/100)/CH; if(rows_e<5)rows_e=5;
    int ey0=24, ey1=ey0+rows_e*CH;
    int oy0=ey1+20, orows=(SH-oy0-22)/CH;
    frect(0,0,SW,SH,BG);
    frect(0,0,SW,22,BAR);
    dtext(8,4,"PYTHON IDLE - NOOVEX DEVKIT",0xFFFFFF);
    dtext(SW-45*CW,4,"F5 RUN  ^S SAVE  ^O OPEN  ^N NEW  ESC QUIT",0xB8C6E8);
    /* editor pane */
    int cl=cur_line();
    if(cl<topline)topline=cl; if(cl>=topline+rows_e)topline=cl-rows_e+1;
    int i=0,line2=0;
    while(line2<topline && i<len){ if(buf[i]=='\n')line2++; i++; }
    for(int r=0;r<rows_e && i<=len;r++){
        int ls=i, le=line_end(i);
        char nb[8]; int ln=topline+r+1, o=0; if(ln>=100)nb[o++]='0'+(ln/100)%10; if(ln>=10)nb[o++]='0'+(ln/10)%10; nb[o++]='0'+ln%10; nb[o]=0;
        dtext(6,ey0+r*CH,nb,LNUM);
        int x=6+4*CW;
        for(int k=ls;k<le;k++){ if(k==cur){ frect(x,ey0+r*CH,CW,CH,CURC); dchar(x,ey0+r*CH,buf[k],0x000000); } else dchar(x,ey0+r*CH,buf[k],TXT); x+=CW; if(x>SW-CW)break; }
        if(cur==le) frect(x,ey0+r*CH,2,CH,CURC);
        i=le+1; if(le>=len)break;
    }
    /* separator + output pane */
    frect(0,ey1+2,SW,16,SEP);
    dtext(8,ey1+3,running?"OUTPUT  [RUNNING - ESC STOPS]":"OUTPUT",0xFFFFFF);
    frect(0,oy0-2,SW,SH-oy0+2,BGOUT);
    { int nl=0; for(int k=0;k<olen;k++) if(out[k]=='\n')nl++;
      int skip=(nl>=orows)?(nl-orows+1):0;
      int k=0; while(skip>0&&k<olen){ if(out[k]=='\n')skip--; k++; }
      int x=8,y=oy0;
      for(;k<olen&&y<SH-20;k++){ char c=out[k];
          if(c=='\n'){ x=8; y+=CH; continue; }
          dchar(x,y,c,OTXT); x+=CW; if(x>SW-CW){x=8;y+=CH;} } }
    /* status */
    frect(0,SH-20,SW,20,BAR);
    dtext(8,SH-18,status,0xFFFFFF);
    { char pos[32]; const char* a="LN "; int o=0; while(*a)pos[o++]=*a++; int ln=cl+1; char t[8]; int tl=0; if(!ln)t[tl++]='0'; while(ln){t[tl++]='0'+ln%10;ln/=10;} while(tl)pos[o++]=t[--tl];
      a=modified?"  [MODIFIED]":"          "; while(*a)pos[o++]=*a++; pos[o]=0; dtext(SW-20*CW,SH-18,pos,0xB8C6E8); }
    flip();
}

static void set_status(const char* s){ int i=0; while(s[i]&&i<63){status[i]=s[i];i++;} status[i]=0; }

static void run_script(void){
    olen=0; out[0]=0; stop_req=0;
    nvars=0; g_err=0;            /* fresh interpreter state each run */
    running=1; set_status("RUNNING SCRIPT..."); render();
    buf[len]=0;
    py_run(buf);
    running=0;
    ide_out("\n--- done ---\n");
    set_status(g_err?"SCRIPT ERROR - SEE OUTPUT":"SCRIPT FINISHED");
}

void _start(void){
    LFB=(u32t*)nvx_fb(); PITCH=nvx_fb_pitch(); SW=nvx_fb_w(); SH=nvx_fb_h();
    BB=(u32t*)malloc((long)SW*SH*4);
    if(!BB){ nvx_print("PYIDLE: out of memory\n"); nvx_exit(); }
    const char* demo="# Python IDLE on NoovexOS\nprint(\"hello from the devkit!\")\nfor i in range(1, 6):\n    print(i, \"squared =\", i * i)\n";
    { int k=0; while(demo[k]&&k<MAXBUF-1){buf[k]=demo[k];k++;} len=k; buf[len]=0; cur=len; }
    ide_out("Python IDLE ready. Press F5 to run.\n");
    int sh=0,ct=0,ext=0;
    render();
    for(;;){
        int k=nvx_readraw();
        if(k<0){ nvx_delay(8); continue; }
        int rel=(k&0x80)!=0; int sc=k&0x7F;
        if(sc==0x60||k==0xE0){ ext=1; continue; }   /* extended prefix */
        if(sc==0x2A||sc==0x36){ sh=!rel; continue; }
        if(sc==0x1D){ ct=!rel; continue; }
        if(rel){ ext=0; continue; }
        if(sc==0x01){ set_status("BYE"); render(); nvx_exit(); }
        else if(sc==0x3F){ run_script(); }                                    /* F5  */
        else if(ct&&sc==0x1F){ int r=nvx_filewrite("SCRIPT.PY",buf,len);      /* ^S  */
            set_status(r>=0?"SAVED SCRIPT.PY":"SAVE FAILED (no disk?)"); modified=0; }
        else if(ct&&sc==0x18){ int n=nvx_fileread("SCRIPT.PY",buf,MAXBUF-1);  /* ^O  */
            if(n>=0){ len=n; buf[len]=0; cur=0; topline=0; modified=0; set_status("OPENED SCRIPT.PY"); }
            else set_status("SCRIPT.PY NOT FOUND"); }
        else if(ct&&sc==0x31){ len=0; cur=0; topline=0; buf[0]=0; modified=0; set_status("NEW"); } /* ^N */
        else if(sc==0x0E){ del(); }                                           /* bksp */
        else if(sc==0x1C){ ins('\n'); }                                       /* enter*/
        else if(sc==0x0F){ ins(' ');ins(' ');ins(' ');ins(' '); }             /* tab  */
        else if(sc==0x4B){ if(cur>0)cur--; ext=0; }                           /* left */
        else if(sc==0x4D){ if(cur<len)cur++; ext=0; }                         /* right*/
        else if(sc==0x48){ int col=cur-line_start(cur); int ls=line_start(cur);/* up  */
            if(ls>0){ int pls=line_start(ls-1); int ple=ls-1; cur=pls+col; if(cur>ple)cur=ple; } ext=0; }
        else if(sc==0x50){ int col=cur-line_start(cur); int le=line_end(cur); /* down */
            if(le<len){ int nls=le+1; int nle=line_end(nls); cur=nls+col; if(cur>nle)cur=nle; } ext=0; }
        else if(sc==0x47){ cur=line_start(cur); ext=0; }                      /* home */
        else if(sc==0x4F){ cur=line_end(cur); ext=0; }                        /* end  */
        else { char c=sh?KS[sc]:KN[sc]; if(c) ins(c); }
        render();
    }
}
