/* HexLang: tiny, bounded scripting language for NoovexOS.
   Integer variables, print, let, if/end, while/end, and draw calls.
   Hard instruction budget so a script can never hang the OS.
   Pure C, callbacks for all I/O so it is host-testable. */
#ifndef NVX_HEXLANG_H
#define NVX_HEXLANG_H

#define HX_MAXLINES 512
#define HX_MAXVARS  48
#define HX_BUDGET   2000000

typedef struct {
    void (*put_str)(void*ctx,const char*s);
    void (*put_num)(void*ctx,int n);
    void (*nl)(void*ctx);
    void (*cls)(void*ctx,int col);
    void (*text)(void*ctx,int x,int y,const char*s,int col);
    void (*rect)(void*ctx,int x,int y,int w,int h,int col);
    void (*waitt)(void*ctx,int ticks);
    void* ctx;
} hx_env;

typedef struct {
    char names[HX_MAXVARS][9];
    int  vals[HX_MAXVARS];
    int  nvars;
} hx_state;

static int hx_streq(const char*a,const char*b){ while(*a&&*b){ if(*a!=*b)return 0; a++; b++; } return *a==*b; }
static int hx_issp(char c){ return c==' '||c=='\t'||c=='\r'; }

static int hx_varidx(hx_state*S,const char*name){
    for(int i=0;i<S->nvars;i++) if(hx_streq(S->names[i],name))return i;
    if(S->nvars>=HX_MAXVARS)return -1;
    int i=S->nvars++; int k=0; while(name[k]&&k<8){S->names[i][k]=name[k];k++;} S->names[i][k]=0; S->vals[i]=0; return i;
}

/* expression parser over a NUL-terminated string; integer math with * / before + - */
typedef struct { const char*p; hx_state*S; int err; } hx_px;
static void hx_skip(hx_px*x){ while(hx_issp(*x->p))x->p++; }
static int hx_parse_expr(hx_px*x);
static int hx_parse_atom(hx_px*x){
    hx_skip(x);
    char c=*x->p;
    if(c=='('){ x->p++; int v=hx_parse_expr(x); hx_skip(x); if(*x->p==')')x->p++; return v; }
    if(c=='-'){ x->p++; return -hx_parse_atom(x); }
    if(c>='0'&&c<='9'){ int v=0; while(*x->p>='0'&&*x->p<='9'){ v=v*10+(*x->p-'0'); x->p++; } return v; }
    if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'){
        char nm[9]; int k=0;
        while(((*x->p>='a'&&*x->p<='z')||(*x->p>='A'&&*x->p<='Z')||(*x->p>='0'&&*x->p<='9')||*x->p=='_')&&k<8){ nm[k++]=*x->p; x->p++; }
        nm[k]=0; int i=hx_varidx(x->S,nm); return (i<0)?0:x->S->vals[i];
    }
    x->err=1; return 0;
}
static int hx_parse_term(hx_px*x){
    int v=hx_parse_atom(x);
    for(;;){ hx_skip(x); char c=*x->p;
        if(c=='*'){ x->p++; v*=hx_parse_atom(x); }
        else if(c=='/'){ x->p++; int d=hx_parse_atom(x); v=(d==0)?0:v/d; }
        else if(c=='%'){ x->p++; int d=hx_parse_atom(x); v=(d==0)?0:v%d; }
        else break; }
    return v;
}
static int hx_parse_expr(hx_px*x){
    int v=hx_parse_term(x);
    for(;;){ hx_skip(x); char c=*x->p;
        if(c=='+'){ x->p++; v+=hx_parse_term(x); }
        else if(c=='-'){ x->p++; v-=hx_parse_term(x); }
        else break; }
    return v;
}
static int hx_eval(hx_state*S,const char*s){ hx_px x; x.p=s; x.S=S; x.err=0; return hx_parse_expr(&x); }

/* evaluate a condition like "a > b", "x == 3", "y" (nonzero) */
static int hx_cond(hx_state*S,const char*s){
    const char*op=0; int kind=0; /* 0 ==,1 !=,2 <,3 >,4 <=,5 >= */
    for(const char*p=s;*p;p++){
        if(p[0]=='='&&p[1]=='='){op=p;kind=0;break;}
        if(p[0]=='!'&&p[1]=='='){op=p;kind=1;break;}
        if(p[0]=='<'&&p[1]=='='){op=p;kind=4;break;}
        if(p[0]=='>'&&p[1]=='='){op=p;kind=5;break;}
        if(p[0]=='<'){op=p;kind=2;break;}
        if(p[0]=='>'){op=p;kind=3;break;}
    }
    if(!op) return hx_eval(S,s)!=0;
    char lhs[128]; int k=0; for(const char*p=s;p<op&&k<127;p++)lhs[k++]=*p; lhs[k]=0;
    const char*r=op+((kind==0||kind==1||kind==4||kind==5)?2:1);
    int a=hx_eval(S,lhs), b=hx_eval(S,r);
    switch(kind){case 0:return a==b;case 1:return a!=b;case 2:return a<b;case 3:return a>b;case 4:return a<=b;default:return a>=b;}
}

/* copy a quoted string literal starting at *pp (which points at the opening quote) into out */
static const char* hx_qstr(const char*p,char*out,int cap){
    if(*p!='"')return 0; p++;
    int k=0; while(*p&&*p!='"'&&k<cap-1){ out[k++]=*p; p++; } out[k]=0;
    if(*p=='"')p++; return p;
}

/* read the first word (command) of a line */
static const char* hx_word(const char*p,char*out,int cap){
    while(hx_issp(*p))p++;
    int k=0; while(*p&&!hx_issp(*p)&&k<cap-1){ out[k++]=*p; p++; } out[k]=0; return p;
}
static const char* hx_int(const char*p,hx_state*S,int*v){
    while(hx_issp(*p))p++;
    char tok[64]; int k=0; while(*p&&!hx_issp(*p)&&*p!='"'&&k<63){ tok[k++]=*p; p++; } tok[k]=0;
    *v=hx_eval(S,tok); return p;
}

/* run a script. returns 0 ok, or negative; *errline set on error */
static int hx_run(const char*src,hx_env*env,int*errline){
    static char buf[HX_MAXLINES*0]; (void)buf;
    /* split into lines (in place copy to a static buffer) */
    static char text[16384];
    int tl=0; for(const char*s=src;*s&&tl<16383;s++)text[tl++]=*s; text[tl]=0;
    static char* lines[HX_MAXLINES]; int nl=0;
    char* cur=text; lines[nl++]=cur;
    for(char*p=text;*p;p++){ if(*p=='\n'){ *p=0; if(nl<HX_MAXLINES)lines[nl++]=p+1; } }
    /* match if/while -> end */
    static int match[HX_MAXLINES]; static int opener_is_while[HX_MAXLINES];
    static int stack[HX_MAXLINES]; int sp=0;
    for(int i=0;i<nl;i++){ match[i]=-1; opener_is_while[i]=0; }
    for(int i=0;i<nl;i++){
        char w[16]; hx_word(lines[i],w,16);
        if(hx_streq(w,"if")||hx_streq(w,"while")){ if(sp<HX_MAXLINES){ opener_is_while[i]=hx_streq(w,"while"); stack[sp++]=i; } }
        else if(hx_streq(w,"end")){ if(sp>0){ int o=stack[--sp]; match[o]=i; match[i]=o; } }
    }
    if(sp!=0){ if(errline)*errline=stack[sp-1]+1; return -1; } /* unmatched if/while */
    hx_state S; S.nvars=0;
    long budget=HX_BUDGET;
    int pc=0;
    while(pc<nl){
        if(--budget<=0){ if(errline)*errline=pc+1; return -2; } /* ran too long */
        char* line=lines[pc];
        char w[16]; const char* rest=hx_word(line,w,16);
        if(w[0]==0||w[0]=='#'){ pc++; continue; }
        if(hx_streq(w,"print")){
            while(hx_issp(*rest))rest++;
            if(*rest=='"'){ char s[256]; hx_qstr(rest,s,256); env->put_str(env->ctx,s); }
            else { int v=hx_eval(&S,rest); env->put_num(env->ctx,v); }
            env->nl(env->ctx); pc++; continue;
        }
        if(hx_streq(w,"let")){
            char nm[9]; const char* p=hx_word(rest,nm,9);
            while(hx_issp(*p))p++; if(*p=='=')p++;
            int v=hx_eval(&S,p); int idx=hx_varidx(&S,nm); if(idx>=0)S.vals[idx]=v; pc++; continue;
        }
        if(hx_streq(w,"if")){
            if(hx_cond(&S,rest)) pc++; else pc=match[pc]+1; continue;
        }
        if(hx_streq(w,"while")){
            if(hx_cond(&S,rest)) pc++; else pc=match[pc]+1; continue;
        }
        if(hx_streq(w,"end")){
            if(opener_is_while[match[pc]]) pc=match[pc]; else pc++; continue;
        }
        if(hx_streq(w,"cls")){ int v; hx_int(rest,&S,&v); env->cls(env->ctx,v); pc++; continue; }
        if(hx_streq(w,"wait")){ int v; hx_int(rest,&S,&v); env->waitt(env->ctx,v); pc++; continue; }
        if(hx_streq(w,"rect")){ int x,y,ww,hh,c; const char*p=rest;
            p=hx_int(p,&S,&x); p=hx_int(p,&S,&y); p=hx_int(p,&S,&ww); p=hx_int(p,&S,&hh); p=hx_int(p,&S,&c);
            env->rect(env->ctx,x,y,ww,hh,c); pc++; continue; }
        if(hx_streq(w,"text")){ int x,y; const char*p=hx_int(rest,&S,&x); p=hx_int(p,&S,&y);
            while(hx_issp(*p))p++; char s[256]; if(*p=='"')hx_qstr(p,s,256); else s[0]=0;
            env->text(env->ctx,x,y,s,7); pc++; continue; }
        /* unknown command -> error */
        if(errline)*errline=pc+1; return -3;
    }
    return 0;
}
#endif
