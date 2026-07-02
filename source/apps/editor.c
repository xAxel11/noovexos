/* NoovexOS Code Editor (NVXEDIT) - syntax-highlighted code editor.
   Renders to a backbuffer, blits to the framebuffer via getfb (flicker-free).
   Keyboard via readraw + bundled US keymap (tracks shift/caps).
   ^S save, ^O open, ^N new, ESC quit. Arrows/Home/End/PgUp/PgDn navigate. */
#include "noovex.h"
#include "noovex_libc.h"
#include "font8x14.h"

#define CW 8
#define CH 15
#define MAXBUF (512*1024)

static u32* BB; static u32* LFB; static int PITCH, SW, SH;
static char* buf; static int len=0, cur=0;
static int topline=0;
static char fname[24]="UNTITLED.C";
static int modified=0;
static int rows, cols, gutter;

/* ---- US keymap (scancode set 1) ---- */
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

/* ---- colours ---- */
#define C_BG    0x191921
#define C_GUT   0x12121A
#define C_LNUM  0x4A4A60
#define C_TEXT  0xD0D4D8
#define C_KW    0x6AA0FF
#define C_TYPE  0x46C8B4
#define C_STR   0xE0A060
#define C_NUM   0xE08040
#define C_COM   0x5E8060
#define C_PRE   0xD080D0
#define C_CUR   0xFFFFFF
#define C_BAR   0x283860
#define C_BARTX 0xFFFFFF

/* ---- multi-language syntax config ----
   lc1/lc2 = line-comment chars (lc2=0 -> single char). bs/be = block comment start/end (bs1=0 -> none). */
typedef struct { const char* name; const char** kw; const char** ty; char lc1,lc2,bs1,bs2,be1,be2; } Lang;

static const char* KWC[]={"if","else","while","for","do","return","break","continue","switch","case",
 "default","goto","sizeof","static","const","volatile","extern","struct","union","enum","typedef",
 "register","inline","#include","#define","#ifndef","#ifdef","#endif","#if","#else","#pragma",0};
static const char* TYC[]={"int","char","short","long","float","double","void","unsigned","signed",
 "u8","u16","u32","u64","size_t","bool","FILE",0};

static const char* KWJS[]={"function","var","let","const","if","else","for","while","do","return",
 "break","continue","switch","case","default","new","delete","typeof","instanceof","this","import",
 "export","from","class","extends","super","async","await","yield","try","catch","finally","throw","in","of",0};
static const char* TYJS[]={"null","undefined","true","false","console","Math","JSON","Object","Array",
 "String","Number","Boolean","document","window",0};

static const char* KWPY[]={"def","class","if","elif","else","for","while","return","break","continue",
 "import","from","as","pass","lambda","yield","with","try","except","finally","raise","global","nonlocal",
 "in","is","not","and","or","del","assert","async","await","print",0};
static const char* TYPY[]={"None","True","False","self","len","range","int","str","float","list",
 "dict","set","tuple","bool","print","open",0};

static const char* KWLUA[]={"function","local","if","then","else","elseif","end","for","while","do",
 "return","break","repeat","until","in","and","or","not","goto",0};
static const char* TYLUA[]={"nil","true","false","print","pairs","ipairs","type","tostring","tonumber",
 "math","string","table","require","self",0};

static const char* KWRUST[]={"fn","let","mut","if","else","match","for","while","loop","return","break","continue",
 "struct","enum","impl","trait","pub","use","mod","const","static","ref","move","where","as","in","dyn","async",
 "await","unsafe","extern","crate","super","type",0};
static const char* TYRUST[]={"i8","i16","i32","i64","i128","u8","u16","u32","u64","u128","usize","isize","f32","f64",
 "bool","char","str","String","Vec","Option","Result","Box","self","Self","true","false","None","Some","Ok","Err",0};

static const char* KWJAVA[]={"class","interface","extends","implements","public","private","protected","static",
 "final","abstract","if","else","for","while","do","switch","case","default","return","break","continue","new",
 "this","super","try","catch","finally","throw","throws","import","package","synchronized","volatile","instanceof",
 "enum","assert",0};
static const char* TYJAVA[]={"int","long","short","byte","char","float","double","boolean","void","String","Object",
 "Integer","Long","Double","Boolean","List","Map","ArrayList","HashMap","true","false","null",0};

static const char* KWCS[]={"class","struct","interface","namespace","using","public","private","protected","internal",
 "static","readonly","const","if","else","for","foreach","while","do","switch","case","default","return","break",
 "continue","new","this","base","try","catch","finally","throw","get","set","async","await","override","virtual",
 "abstract","sealed","partial","enum","in","out","ref","params",0};
static const char* TYCS[]={"int","long","short","byte","char","float","double","decimal","bool","string","object",
 "void","var","List","Dictionary","true","false","null",0};

static const char* KWGO[]={"func","var","const","package","import","if","else","for","range","switch","case",
 "default","return","break","continue","go","defer","chan","select","struct","interface","map","type",
 "fallthrough","goto",0};
static const char* TYGO[]={"int","int8","int16","int32","int64","uint","uint8","uint16","uint32","uint64","byte",
 "rune","float32","float64","complex64","complex128","bool","string","error","uintptr","true","false","nil",0};

static const char* KWTS[]={"function","var","let","const","if","else","for","while","do","switch","case","default",
 "return","break","continue","new","delete","typeof","instanceof","this","import","export","from","as","class",
 "extends","implements","interface","type","enum","namespace","public","private","protected","readonly","static",
 "async","await","yield","try","catch","finally","throw","in","of",0};
static const char* TYTS[]={"number","string","boolean","any","never","unknown","object","void","null","undefined",
 "symbol","bigint","Array","Promise","Record","Map","Set","true","false",0};

static const Lang LANGS[]={
 {"C",      KWC,  TYC,  '/','/','/','*','*','/'},
 {"JS",     KWJS, TYJS, '/','/','/','*','*','/'},
 {"Python", KWPY, TYPY, '#', 0,  0, 0, 0, 0 },
 {"Lua",    KWLUA,TYLUA,'-','-', 0, 0, 0, 0 },
 {"Rust",   KWRUST,TYRUST,'/','/','/','*','*','/'},
 {"Java",   KWJAVA,TYJAVA,'/','/','/','*','*','/'},
 {"C#",     KWCS,  TYCS,  '/','/','/','*','*','/'},
 {"Go",     KWGO,  TYGO,  '/','/','/','*','*','/'},
 {"TS",     KWTS,  TYTS,  '/','/','/','*','*','/'},
};
static const Lang* LG=&LANGS[0];     /* active language */

static int streq_ext(const char* a,const char* b){ while(*a&&*b){ char x=*a,y=*b; if(x>='A'&&x<='Z')x+=32; if(y>='A'&&y<='Z')y+=32; if(x!=y)return 0; a++;b++; } return *a==*b; }
static void detect_lang(const char* nm){
    const char* dot=0; for(const char* p=nm;*p;p++)if(*p=='.')dot=p;
    LG=&LANGS[0];
    if(!dot)return;
    if(streq_ext(dot,".js"))LG=&LANGS[1];
    else if(streq_ext(dot,".py"))LG=&LANGS[2];
    else if(streq_ext(dot,".lua"))LG=&LANGS[3];
    else if(streq_ext(dot,".rs"))LG=&LANGS[4];
    else if(streq_ext(dot,".java"))LG=&LANGS[5];
    else if(streq_ext(dot,".cs"))LG=&LANGS[6];
    else if(streq_ext(dot,".go"))LG=&LANGS[7];
    else if(streq_ext(dot,".ts"))LG=&LANGS[8];
    else LG=&LANGS[0];   /* .c/.h/.cpp/etc -> C */
}
static int wordmatch(const char* w,int n,const char** list){ for(int i=0;list[i];i++){ const char* k=list[i]; int j=0; while(j<n&&k[j]&&k[j]==w[j])j++; if(j==n&&k[j]==0)return 1; } return 0; }

static void pset(int x,int y,u32 c){ if((unsigned)x<(unsigned)SW&&(unsigned)y<(unsigned)SH)BB[y*SW+x]=c; }
static void frect(int x,int y,int w,int h,u32 c){ for(int j=0;j<h;j++)for(int i=0;i<w;i++)pset(x+i,y+j,c); }
static void dchar(int x,int y,char ch,u32 c){ if(ch<32||ch>126)return; const unsigned char* g=FONT814[ch-32];
    for(int r=0;r<14;r++)for(int col=0;col<8;col++) if(g[r]&(1<<col)) pset(x+col,y+r,c); }
static void dtext(int x,int y,const char* t,u32 c){ while(*t){ dchar(x,y,*t++,c); x+=CW; } }

static int is_idc(char c){ return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='#'; }

/* draw one logical line [s,e) at pixel (px,py); returns new block-comment state */
static int draw_line(int s,int e,int px,int py,int inblock){
    int x=px, i=s; const Lang* L=LG;
    while(i<e){
        char c=buf[i];
        if(inblock){ int st=i; while(i<e){ if(buf[i]==L->be1&&i+1<e&&buf[i+1]==L->be2){ i+=2; inblock=0; break; } i++; }
            for(int k=st;k<i;k++){ dchar(x,py,buf[k],C_COM); x+=CW; } continue; }
        if(c==L->lc1 && (L->lc2==0 || (i+1<e&&buf[i+1]==L->lc2))){ for(int k=i;k<e;k++){ dchar(x,py,buf[k],C_COM); x+=CW; } break; }
        if(L->bs1 && c==L->bs1 && i+1<e && buf[i+1]==L->bs2){ inblock=1; continue; }
        if(c=='"'||c=='\''){ char q=c; dchar(x,py,c,C_STR); x+=CW; i++;
            while(i<e){ dchar(x,py,buf[i],C_STR); x+=CW; if(buf[i]=='\\'&&i+1<e){ i++; dchar(x,py,buf[i],C_STR); x+=CW; i++; continue; } if(buf[i]==q){ i++; break; } i++; } continue; }
        if((c>='0'&&c<='9')){ while(i<e&&(is_idc(buf[i])||buf[i]=='.')){ dchar(x,py,buf[i],C_NUM); x+=CW; i++; } continue; }
        if(is_idc(c)&&!(c>='0'&&c<='9')){ int st=i; while(i<e&&is_idc(buf[i]))i++; int n=i-st;
            u32 col=C_TEXT; if(wordmatch(buf+st,n,L->kw))col=C_KW; else if(wordmatch(buf+st,n,L->ty))col=C_TYPE;
            for(int k=st;k<i;k++){ dchar(x,py,buf[k],col); x+=CW; } continue; }
        dchar(x,py,c,C_TEXT); x+=CW; i++;
    }
    return inblock;
}

/* advance block-comment state across a line WITHOUT drawing (for off-screen lines) */
static int scan_block(int s,int e,int inblock){
    int i=s; const Lang* L=LG;
    while(i<e){
        if(inblock){ if(buf[i]==L->be1&&i+1<e&&buf[i+1]==L->be2){i+=2;inblock=0;}else i++; continue; }
        if(buf[i]==L->lc1 && (L->lc2==0 || (i+1<e&&buf[i+1]==L->lc2)))break;
        if(L->bs1 && buf[i]==L->bs1 && i+1<e && buf[i+1]==L->bs2){inblock=1;i+=2;continue;}
        if(buf[i]=='"'||buf[i]=='\''){ char q=buf[i]; i++; while(i<e){ if(buf[i]=='\\'){i+=2;continue;} if(buf[i]==q){i++;break;} i++; } continue; }
        i++;
    }
    return inblock;
}

/* line navigation helpers */
static int line_start(int idx){ while(idx>0&&buf[idx-1]!='\n')idx--; return idx; }
static int line_end(int idx){ while(idx<len&&buf[idx]!='\n')idx++; return idx; }
static int cur_line(void){ int ln=0; for(int i=0;i<cur;i++)if(buf[i]=='\n')ln++; return ln; }
static int cur_col(void){ return cur-line_start(cur); }

static void ins(char c){ if(len>=MAXBUF-1)return; for(int i=len;i>cur;i--)buf[i]=buf[i-1]; buf[cur]=c; cur++; len++; modified=1; }
static void del_back(void){ if(cur<=0)return; for(int i=cur-1;i<len-1;i++)buf[i]=buf[i+1]; cur--; len--; modified=1; }
static void del_fwd(void){ if(cur>=len)return; for(int i=cur;i<len-1;i++)buf[i]=buf[i+1]; len--; modified=1; }

static void save_file(void){ buf[len]=0; int r=nvx_filewrite(fname,buf,len); if(r==0)modified=0; }
static void load_file(const char* nm){ int n=nvx_fileread(nm,buf,MAXBUF-1); if(n<0)n=0; len=n; cur=0; topline=0; modified=0;
    int k=0; while(nm[k]&&k<23){ fname[k]=nm[k]; k++; } fname[k]=0; detect_lang(fname); }

/* shift/caps state */
static int shift=0, caps=0, ctrl=0;

/* prompt mode for filename (open) */
static int prompt=0; static char pbuf[24]; static int plen=0;

void _start(void){
    LFB=nvx_fb(); PITCH=nvx_fb_pitch(); SW=nvx_fb_w(); SH=nvx_fb_h();
    BB=(u32*)malloc(SW*SH*4); buf=(char*)malloc(MAXBUF);
    if(!BB||!buf){ nvx_clearrgb(0x200000); for(;;){} }
    /* default starter content */
    const char* d="#include \"noovex.h\"\n\n/* welcome to the NoovexOS code editor */\nvoid _start(void){\n    int x = 42;\n    nvx_print(\"hello, world\\n\");\n    nvx_exit();\n}\n";
    int k=0; while(d[k]){ buf[k]=d[k]; k++; } len=k; cur=0; detect_lang(fname);
    gutter=6*CW; cols=(SW-gutter)/CW; rows=(SH-2*CH)/CH;   /* leave a title + status row */

    for(;;){
        /* ---------- input ---------- */
        int sc=nvx_readraw();
        if(sc){
            int rel=sc&0x80, k2=sc&0x7F;
            if(k2==0x2A||k2==0x36){ shift=!rel; }
            else if(k2==0x3A){ if(!rel)caps=!caps; }
            else if(!rel){
                if(prompt){
                    if(k2==0x1C){ prompt=0; pbuf[plen]=0; if(plen)load_file(pbuf); }
                    else if(k2==0x01){ prompt=0; }
                    else if(k2==0x0E){ if(plen)plen--; }
                    else { char ch=shift?KS[k2]:KN[k2]; if(ch&&plen<22){ if(ch>='a'&&ch<='z')ch=ch-'a'+'A'; pbuf[plen++]=ch; } }
                } else {
                    /* Ctrl combos: detect Ctrl via scancode 0x1D held - track it */
                    if(k2==0x1F&&ctrl){ save_file(); }            /* ^S */
                    else if(k2==0x18&&ctrl){ prompt=1; plen=0; }  /* ^O */
                    else if(k2==0x31&&ctrl){ len=0; cur=0; topline=0; modified=0; const char* u="UNTITLED.C"; int j=0; while(u[j]){fname[j]=u[j];j++;} fname[j]=0; detect_lang(fname); } /* ^N */
                    else if(k2==0x01){ free(BB); free(buf); nvx_exit(); }   /* ESC */
                    else if(k2==0x0E){ del_back(); }
                    else if(k2==0x53){ del_fwd(); }
                    else if(k2==0x1C){ ins('\n'); }
                    else if(k2==0x0F){ ins(' '); ins(' '); ins(' '); ins(' '); }   /* tab = 4 spaces */
                    else if(k2==0x4B){ if(cur>0)cur--; }          /* left */
                    else if(k2==0x4D){ if(cur<len)cur++; }        /* right */
                    else if(k2==0x48){ int col=cur_col(); int ls=line_start(cur); if(ls>0){ int pls=line_start(ls-1); int ple=ls-1; cur=pls+col; if(cur>ple)cur=ple; } } /* up */
                    else if(k2==0x50){ int col=cur_col(); int le=line_end(cur); if(le<len){ int nls=le+1; int nle=line_end(nls); cur=nls+col; if(cur>nle)cur=nle; } } /* down */
                    else if(k2==0x47){ cur=line_start(cur); }     /* home */
                    else if(k2==0x4F){ cur=line_end(cur); }       /* end */
                    else { char ch=shift?KS[k2]:KN[k2]; char base=KN[k2];
                        if(base>='a'&&base<='z'){ int up=shift^caps; ch=up?base-'a'+'A':base; }
                        if(ch) ins(ch); }
                }
            }
            /* track Ctrl key (0x1D) make/release */
            if(k2==0x1D) ctrl=!rel;
        }

        /* ---------- scroll to cursor ---------- */
        int cl=cur_line();
        if(cl<topline)topline=cl;
        if(cl>=topline+rows)topline=cl-rows+1;

        /* ---------- render to backbuffer ---------- */
        for(int i=0;i<SW*SH;i++)BB[i]=C_BG;
        frect(0,0,gutter,SH,C_GUT);
        /* title bar */
        frect(0,0,SW,CH,0x202030);
        { char t[40]; sprintf(t," NVXEDIT  %s%s  [%s]",fname,modified?" *":"",LG->name); dtext(4,1,t,0x90C0FF); }

        /* render lines from the top of the file, drawing only the visible window
           (keeps block-comment highlighting correct when scrolled) */
        int s=0, ln=0, inblock=0, y=CH;
        while(s<=len && ln<topline+rows){
            int e=s; while(e<len&&buf[e]!='\n')e++;
            if(ln>=topline){
                char num[8]; sprintf(num,"%4d",ln+1); dtext(4,y,num,C_LNUM);
                inblock=draw_line(s,e,gutter,y,inblock);
                y+=CH;
            } else inblock=scan_block(s,e,inblock);
            if(e>=len)break;
            s=e+1; ln++;
        }

        /* cursor */
        { int ccl=cur_line(), ccol=cur_col();
          if(ccl>=topline&&ccl<topline+rows){ int cx=gutter+ccol*CW, cy=CH+(ccl-topline)*CH; frect(cx,cy,2,14,C_CUR); } }

        /* status bar */
        frect(0,SH-CH,SW,CH,C_BAR);
        if(prompt){ char p[40]; sprintf(p," OPEN FILE: %s_",pbuf); dtext(4,SH-CH+1,p,0xFFFF80); }
        else { char st[80]; sprintf(st," Ln %d, Col %d   ^S save  ^O open  ^N new  ESC quit",cur_line()+1,cur_col()+1); dtext(4,SH-CH+1,st,C_BARTX); }

        /* ---------- blit ---------- */
        for(int yy=0;yy<SH;yy++){ u32* srow=BB+yy*SW; u32* drow=LFB+yy*PITCH; for(int xx=0;xx<SW;xx++)drow[xx]=srow[xx]; }
    }
}
