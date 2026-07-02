/* pymini.c - a small Python-subset interpreter (portable C).
   Supports: int/float/string values, variables, print(), arithmetic
   (+ - * / // % **), comparisons, and/or/not, if/elif/else, while,
   for VAR in range(...), pass/break/continue, # comments, and the
   builtins print/range/str/int/len/abs. Indentation-based blocks.
   NOT full Python - a basic runnable subset. */

#if defined(NOOVEX) || defined(IDE)
  #include "noovex.h"
  #include "noovex_libc.h"
#endif
#if defined(IDE)
  extern void ide_out(const char* s);
  extern int  ide_should_stop(void);
  static void OUT(const char* s){ ide_out(s); }
#elif defined(NOOVEX)
  static void OUT(const char* s){ nvx_print(s); }
#else
  #include <stdio.h>
  #include <string.h>
  #include <stdlib.h>
  static void OUT(const char* s){ fputs(s,stdout); }
#endif

/* ---------- values ---------- */
typedef struct { int t; double n; char s[128]; } Val;   /* t: 0=num 1=str */
static Val mknum(double x){ Val v; v.t=0; v.n=x; v.s[0]=0; return v; }
static Val mkstr(const char* x){ Val v; v.t=1; v.n=0; int i=0; while(x[i]&&i<127){v.s[i]=x[i];i++;} v.s[i]=0; return v; }
static Val mkbool(int b){ Val v; v.t=2; v.n=b?1:0; v.s[0]=0; return v; }

static void numfmt(double x,char* out){
    /* whole numbers print without a decimal, like Python ints */
    long long w=(long long)x;
    if((double)w==x){ /* integer */ char tmp[32]; int n=0; long long a=w; int neg=0; if(a<0){neg=1;a=-a;} if(a==0)tmp[n++]='0'; while(a){tmp[n++]='0'+(int)(a%10);a/=10;} int o=0; if(neg)out[o++]='-'; while(n)out[o++]=tmp[--n]; out[o]=0; }
    else { /* float: print with up to 6 sig decimals, trim */
        int neg=x<0; if(neg)x=-x; long long ip=(long long)x; double fp=x-ip; char tmp[48]; int o=0;
        char ib[24]; int ni=0; long long a=ip; if(a==0)ib[ni++]='0'; while(a){ib[ni++]='0'+(int)(a%10);a/=10;}
        if(neg)tmp[o++]='-'; while(ni)tmp[o++]=ib[--ni]; tmp[o++]='.';
        for(int d=0;d<6;d++){ fp*=10; int dig=(int)fp; tmp[o++]='0'+dig; fp-=dig; }
        while(o>0&&tmp[o-1]=='0')o--; if(tmp[o-1]=='.')o++; tmp[o]=0;
        int k=0; while(tmp[k]){out[k]=tmp[k];k++;} out[k]=0;
    }
}
static void val_str(Val v,char* out){ if(v.t==1){ int i=0; while(v.s[i]){out[i]=v.s[i];i++;} out[i]=0; } else if(v.t==2){ const char* b=v.n?"True":"False"; int i=0; while(b[i]){out[i]=b[i];i++;} out[i]=0; } else numfmt(v.n,out); }
static int truthy(Val v){ if(v.t==1)return v.s[0]!=0; return v.n!=0; }

/* ---------- environment ---------- */
typedef struct { char name[32]; Val v; } Var;
static Var vars[256]; static int nvars=0;
static Val* find_var(const char* nm){ for(int i=0;i<nvars;i++){ int eq=1; for(int k=0;k<32;k++){ if(vars[i].name[k]!=nm[k]){eq=0;break;} if(!nm[k])break; } if(eq)return &vars[i].v; } return 0; }
static void set_var(const char* nm,Val v){ Val* p=find_var(nm); if(p){*p=v;return;} if(nvars<256){ int k=0; while(nm[k]&&k<31){vars[nvars].name[k]=nm[k];k++;} vars[nvars].name[k]=0; vars[nvars].v=v; nvars++; } }

/* ---------- error ---------- */
static int g_err=0; static char g_errmsg[80];
static void err(const char* m){ if(!g_err){ g_err=1; int i=0; while(m[i]&&i<79){g_errmsg[i]=m[i];i++;} g_errmsg[i]=0; } }
/* cooperative stop check (IDE: poll ESC every 256 calls) */
static int chk_stop(void){
#if defined(IDE)
    static unsigned cnt=0; if((++cnt & 0xFF)!=0) return 0; if(ide_should_stop()){ err("Stopped by user"); return 1; } return 0;
#else
    return 0;
#endif
}

/* ---------- tokenizer (per expression) ---------- */
typedef struct { int t; double n; char s[128]; } Tok;   /* t: 0=num 1=str 2=name 3=op 9=end */
static Tok toks[256]; static int ntoks, tp;
static int isname0(char c){ return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'; }
static int isnamec(char c){ return isname0(c)||(c>='0'&&c<='9'); }
static void tokenize(const char* s){
    ntoks=0; int i=0;
    while(s[i]){
        char c=s[i];
        if(c==' '||c=='\t'){ i++; continue; }
        if(c=='#') break;
        if((c>='0'&&c<='9')||(c=='.'&&s[i+1]>='0'&&s[i+1]<='9')){
            double val=0; while(s[i]>='0'&&s[i]<='9'){ val=val*10+(s[i]-'0'); i++; }
            if(s[i]=='.'){ i++; double f=0.1; while(s[i]>='0'&&s[i]<='9'){ val+=(s[i]-'0')*f; f*=0.1; i++; } }
            toks[ntoks].t=0; toks[ntoks].n=val; ntoks++; continue;
        }
        if(c=='"'||c=='\''){ char q=c; i++; int o=0; char b[128];
            while(s[i]&&s[i]!=q){ if(s[i]=='\\'&&s[i+1]){ i++; char e=s[i]; char r=e; if(e=='n')r='\n'; else if(e=='t')r='\t'; else if(e=='\\')r='\\'; else if(e=='"')r='"'; else if(e=='\'')r='\''; else if(e=='0')r=0; if(o<127)b[o++]=r; i++; continue; } if(o<127)b[o++]=s[i]; i++; }
            if(s[i]==q)i++; b[o]=0; toks[ntoks].t=1; int k=0; while(b[k]){toks[ntoks].s[k]=b[k];k++;} toks[ntoks].s[k]=0; ntoks++; continue; }
        if(isname0(c)){ int o=0; char b[128]; while(isnamec(s[i])){ if(o<127)b[o++]=s[i]; i++; } b[o]=0; toks[ntoks].t=2; int k=0; while(b[k]){toks[ntoks].s[k]=b[k];k++;} toks[ntoks].s[k]=0; ntoks++; continue; }
        /* operators */
        char op[3]={0,0,0}; op[0]=c;
        if((c=='='&&s[i+1]=='=')||(c=='!'&&s[i+1]=='=')||(c=='<'&&s[i+1]=='=')||(c=='>'&&s[i+1]=='=')||(c=='/'&&s[i+1]=='/')||(c=='*'&&s[i+1]=='*')){ op[1]=s[i+1]; i+=2; }
        else i++;
        toks[ntoks].t=3; toks[ntoks].s[0]=op[0]; toks[ntoks].s[1]=op[1]; toks[ntoks].s[2]=0; ntoks++;
    }
    toks[ntoks].t=9; toks[ntoks].s[0]=0;
}
static int tok_is(const char* o){ return toks[tp].t==3 && toks[tp].s[0]==o[0] && toks[tp].s[1]==o[1]; }
static int tok_name(const char* nm){ if(toks[tp].t!=2)return 0; const char* s=toks[tp].s; int k=0; while(s[k]&&nm[k]&&s[k]==nm[k])k++; return s[k]==0&&nm[k]==0; }

/* floor-division modulo in double (avoids 64-bit __moddi3, matches Python sign) */
static double py_mod(double a,double b){ if(b==0)return 0; double q=a/b; long long f=(long long)q; if((double)f>q)f--; return a-(double)f*b; }

/* ---------- expression evaluator (recursive descent) ---------- */
static Val eval_or(void);
static Val eval_expr(void){ return eval_or(); }
static int module_const(const char* nm,Val* out);

static Val do_call(const char* name);  /* fwd */

static Val eval_atom(void){
    Tok* t=&toks[tp];
    if(t->t==0){ tp++; return mknum(t->n); }
    if(t->t==1){ tp++; return mkstr(t->s); }
    if(t->t==3 && t->s[0]=='(' && t->s[1]==0){ tp++; Val v=eval_expr(); if(tok_is("(")){} if(toks[tp].t==3&&toks[tp].s[0]==')')tp++; return v; }
    if(t->t==2){
        if(tok_name("True")){ tp++; return mkbool(1); }
        if(tok_name("False")){ tp++; return mkbool(0); }
        if(tok_name("None")){ tp++; return mknum(0); }
        char name[64]; int k=0; while(t->s[k]&&k<31){name[k]=t->s[k];k++;} name[k]=0; tp++;
        if(toks[tp].t==3 && toks[tp].s[0]=='.' && toks[tp].s[1]==0){ tp++; if(toks[tp].t==2){ int nl=0; while(name[nl])nl++; if(nl<62)name[nl++]='.'; int j=0; while(toks[tp].s[j]&&nl<63)name[nl++]=toks[tp].s[j++]; name[nl]=0; tp++; } }
        if(toks[tp].t==3 && toks[tp].s[0]=='(' && toks[tp].s[1]==0){ return do_call(name); }
        { Val cv; if(module_const(name,&cv))return cv; }
        Val* p=find_var(name); if(p)return *p; err("NameError"); return mknum(0);
    }
    err("SyntaxError"); tp++; return mknum(0);
}
static Val eval_power(void){ Val b=eval_atom(); if(tok_is("**")){ tp++; Val e=eval_power(); double r=1,base=b.n; int ex=(int)e.n; if(ex<0){r=0;} else for(int i=0;i<ex;i++)r*=base; return mknum(r); } return b; }
static Val eval_unary(void){ if(tok_is("-")){ tp++; Val v=eval_unary(); return mknum(-v.n); } if(tok_is("+")){ tp++; return eval_unary(); } return eval_power(); }
static Val eval_term(void){ Val a=eval_unary();
    while(tok_is("*")||tok_is("/")||tok_is("//")||tok_is("%")){ char op0=toks[tp].s[0],op1=toks[tp].s[1]; tp++; Val b=eval_unary();
        if(op0=='*'&&op1==0){ if(a.t==1&&b.t==0){ char out[128]; int o=0; int rep=(int)b.n; for(int r=0;r<rep;r++){ int k=0; while(a.s[k]&&o<127){out[o++]=a.s[k++];} } out[o]=0; a=mkstr(out); } else a=mknum(a.n*b.n); }
        else if(op0=='/'&&op1=='/'){ double q=a.n/b.n; long long f=(long long)q; if((double)f>q)f--; a=mknum((double)f); }
        else if(op0=='/'){ if(b.n==0){err("ZeroDivisionError");a=mknum(0);} else a=mknum(a.n/b.n); }
        else { if(b.n==0){err("ZeroDivisionError");a=mknum(0);} else a=mknum(py_mod(a.n,b.n)); }
    } return a; }
static Val eval_add(void){ Val a=eval_term();
    while(tok_is("+")||tok_is("-")){ char op=toks[tp].s[0]; tp++; Val b=eval_term();
        if(op=='+'){ if(a.t==1||b.t==1){ char sa[128],sb[128]; val_str(a,sa); val_str(b,sb); char out[256]; int o=0,k=0; while(sa[k]&&o<255)out[o++]=sa[k++]; k=0; while(sb[k]&&o<255)out[o++]=sb[k++]; out[o]=0; a=mkstr(out); } else a=mknum(a.n+b.n); }
        else a=mknum(a.n-b.n);
    } return a; }
static int val_eq(Val a,Val b){ if(a.t==1&&b.t==1){ int k=0; while(a.s[k]&&b.s[k]&&a.s[k]==b.s[k])k++; return a.s[k]==b.s[k]; } if(a.t==b.t)return a.n==b.n; return 0; }
static Val eval_cmp(void){ Val a=eval_add();
    while(toks[tp].t==3 && (tok_is("==")||tok_is("!=")||tok_is("<")||tok_is(">")||tok_is("<=")||tok_is(">="))){
        char o0=toks[tp].s[0],o1=toks[tp].s[1]; tp++; Val b=eval_add(); int r=0;
        if(o0=='='&&o1=='=')r=val_eq(a,b); else if(o0=='!'&&o1=='=')r=!val_eq(a,b);
        else if(o0=='<'&&o1==0)r=a.n<b.n; else if(o0=='>'&&o1==0)r=a.n>b.n;
        else if(o0=='<'&&o1=='=')r=a.n<=b.n; else if(o0=='>'&&o1=='=')r=a.n>=b.n;
        a=mkbool(r);
    } return a; }
static Val eval_not(void){ if(tok_name("not")){ tp++; Val v=eval_not(); return mkbool(truthy(v)?0:1); } return eval_cmp(); }
static Val eval_and(void){ Val a=eval_not(); while(tok_name("and")){ tp++; Val b=eval_not(); a=mkbool((truthy(a)&&truthy(b))?1:0); } return a; }
static Val eval_or(void){ Val a=eval_and(); while(tok_name("or")){ tp++; Val b=eval_and(); a=mkbool((truthy(a)||truthy(b))?1:0); } return a; }

/* ---------- module helpers: math, random, time, syscalls ---------- */
#if !defined(NOOVEX) && !defined(IDE)
  /* sandbox stubs (no NoovexOS syscalls available) */
  static unsigned _sbtk=0;
  static unsigned nvx_ticks_ms(void){ return (_sbtk+=16); }
  static int nvx_http_get(const char* u,char* b,int m){ (void)u; const char* z="[sandbox: no network]"; int i=0; while(z[i]&&i<m-1){b[i]=z[i];i++;} b[i]=0; return i; }
  static int nvx_remove(const char* n){ (void)n; return 0; }
#endif

static int streqf(const char* a,const char* b){ int i=0; while(a[i]&&b[i]){ if(a[i]!=b[i])return 0; i++; } return a[i]==b[i]; }

#define PY_PI 3.14159265358979323846
#define PY_E  2.71828182845904523536
#define PY_LN2 0.69314718055994530942
static double m_sqrt(double x){ if(x<=0)return 0; double r=x>1?x:1; for(int i=0;i<60;i++){ double nr=0.5*(r+x/r); if(nr==r)break; r=nr; } return r; }
static double m_floor(double x){ long long i=(long long)x; if((double)i>x)i--; return (double)i; }
static double m_ceil(double x){ long long i=(long long)x; if((double)i<x)i++; return (double)i; }
static double m_sin(double x){ while(x>PY_PI)x-=2*PY_PI; while(x<-PY_PI)x+=2*PY_PI; double x2=x*x,t=x,sm=x; for(int n=1;n<=9;n++){ t*=-x2/(double)((2*n)*(2*n+1)); sm+=t; } return sm; }
static double m_cos(double x){ return m_sin(x+PY_PI/2); }
static double m_tan(double x){ double c=m_cos(x); if(c<1e-12&&c>-1e-12)return 0; return m_sin(x)/c; }
static double m_log(double x){ if(x<=0)return 0; int e=0; while(x>=2){x*=0.5;e++;} while(x<1){x*=2;e--;} double y=(x-1)/(x+1),y2=y*y,t=y,sm=0; for(int n=0;n<25;n++){ sm+=t/(double)(2*n+1); t*=y2; } return 2*sm+e*PY_LN2; }
static double m_exp(double x){ int k=(int)(x/PY_LN2+(x<0?-0.5:0.5)); double r=x-k*PY_LN2,t=1,sm=1; for(int n=1;n<=18;n++){ t*=r/(double)n; sm+=t; } double p=1; if(k>=0)for(int i=0;i<k;i++)p*=2; else for(int i=0;i<-k;i++)p*=0.5; return sm*p; }
static double m_pow(double b,double e){ long long ie=(long long)e; if((double)ie==e){ double r=1; if(ie>=0)for(long long i=0;i<ie;i++)r*=b; else for(long long i=0;i<-ie;i++)r/=b; return r; } if(b<=0)return 0; return m_exp(e*m_log(b)); }

/* xorshift32 PRNG */
static unsigned rng_state=2463534242u;
static void r_seed(unsigned x){ rng_state=x?x:1; }
static unsigned r_next(void){ unsigned x=rng_state; x^=x<<13; x^=x>>17; x^=x<<5; rng_state=x; return x; }
static double r_random(void){ return (double)(r_next()>>8)*(1.0/16777216.0); }
static int r_randint(int a,int b){ if(b<a){int t=a;a=b;b=t;} unsigned range=(unsigned)(b-a)+1u; return a+(int)(r_next()%range); }
static double r_uniform(double a,double b){ return a+r_random()*(b-a); }

/* module attribute constants (math.pi, math.e, math.tau); 1 if found */
static int module_const(const char* nm,Val* out){
    if(streqf(nm,"math.pi")){ *out=mknum(PY_PI); return 1; }
    if(streqf(nm,"math.e")){ *out=mknum(PY_E); return 1; }
    if(streqf(nm,"math.tau")){ *out=mknum(2*PY_PI); return 1; }
    return 0;
}

/* range state for for-loops, returned via do_call when name=="range" used specially */
static int g_range_a,g_range_b,g_range_s,g_is_range;
static Val do_call(const char* name){
    tp++; /* consume '(' */
    Val args[16]; int na=0;
    if(!(toks[tp].t==3&&toks[tp].s[0]==')')){
        for(;;){ args[na++]=eval_expr(); if(na>=16)break; if(toks[tp].t==3&&toks[tp].s[0]==','){tp++;continue;} break; }
    }
    if(toks[tp].t==3&&toks[tp].s[0]==')')tp++;
    /* networking + module functions */
    if(streqf(name,"http_get")){ static char hbuf[60000]; int n=nvx_http_get(na>0?args[0].s:"",hbuf,(int)sizeof(hbuf)); if(n<0){ err("network error (http_get failed)"); return mknum(-1); } OUT(hbuf); if(n>0&&hbuf[n-1]!='\n')OUT("\n"); return mknum(n); }
    if(streqf(name,"math.sqrt"))return mknum(m_sqrt(args[0].n));
    if(streqf(name,"math.sin"))return mknum(m_sin(args[0].n));
    if(streqf(name,"math.cos"))return mknum(m_cos(args[0].n));
    if(streqf(name,"math.tan"))return mknum(m_tan(args[0].n));
    if(streqf(name,"math.floor"))return mknum(m_floor(args[0].n));
    if(streqf(name,"math.ceil"))return mknum(m_ceil(args[0].n));
    if(streqf(name,"math.log"))return mknum(m_log(args[0].n));
    if(streqf(name,"math.exp"))return mknum(m_exp(args[0].n));
    if(streqf(name,"math.pow"))return mknum(m_pow(args[0].n,args[1].n));
    if(streqf(name,"math.fabs")){ double x=args[0].n; return mknum(x<0?-x:x); }
    if(streqf(name,"random.randint"))return mknum((double)r_randint((int)args[0].n,(int)args[1].n));
    if(streqf(name,"random.random"))return mknum(r_random());
    if(streqf(name,"random.uniform"))return mknum(r_uniform(args[0].n,args[1].n));
    if(streqf(name,"random.seed")){ r_seed((unsigned)args[0].n); return mknum(0); }
    if(streqf(name,"time.time")||streqf(name,"time.monotonic"))return mknum((double)nvx_ticks_ms()/1000.0);
    if(streqf(name,"time.sleep")){ unsigned st0=nvx_ticks_ms(); unsigned ms=(unsigned)(args[0].n*1000.0); while((unsigned)(nvx_ticks_ms()-st0)<ms){ if(chk_stop())break; } return mknum(0); }
    if(streqf(name,"os.getcwd"))return mkstr("/");
    if(streqf(name,"os.remove")){ int rr=nvx_remove(na>0?args[0].s:""); if(rr<0)err("FileNotFoundError"); return mknum(0); }
    if(streqf(name,"os.listdir")){ err("os.listdir needs lists - coming soon"); return mknum(0); }
    /* builtins */
    int eqp=1; { const char* p="print"; int k=0; while(p[k]&&name[k]&&p[k]==name[k])k++; eqp=(p[k]==0&&name[k]==0); }
    if(eqp){ char line[1024]; int o=0; for(int i=0;i<na;i++){ if(i){line[o++]=' ';} char s[128]; val_str(args[i],s); int k=0; while(s[k]&&o<1022)line[o++]=s[k++]; } line[o++]='\n'; line[o]=0; OUT(line); return mknum(0); }
    if(name[0]=='r'&&name[1]=='a'&&name[2]=='n'&&name[3]=='g'&&name[4]=='e'&&name[5]==0){ g_is_range=1; g_range_s=1; if(na==1){g_range_a=0;g_range_b=(int)args[0].n;} else if(na>=2){g_range_a=(int)args[0].n;g_range_b=(int)args[1].n; if(na>=3)g_range_s=(int)args[2].n;} return mknum(0); }
    if(name[0]=='s'&&name[1]=='t'&&name[2]=='r'&&name[3]==0){ char s[128]; val_str(args[0],s); return mkstr(s); }
    if(name[0]=='i'&&name[1]=='n'&&name[2]=='t'&&name[3]==0){ return mknum((double)(long long)args[0].n); }
    if(name[0]=='l'&&name[1]=='e'&&name[2]=='n'&&name[3]==0){ if(args[0].t==1){ int n=0; while(args[0].s[n])n++; return mknum(n); } return mknum(0); }
    if(name[0]=='a'&&name[1]=='b'&&name[2]=='s'&&name[3]==0){ double x=args[0].n; return mknum(x<0?-x:x); }
    err("NameError: function"); return mknum(0);
}

/* ---------- line storage ---------- */
#define MAXLINES 2000
static char* L_text[MAXLINES]; static int L_indent[MAXLINES]; static int L_count=0;

/* find effective code length (strip trailing comment respecting strings) */
static void add_line(char* start,int rawlen){
    /* compute indent */
    int ind=0,i=0; while(i<rawlen&&(start[i]==' '||start[i]=='\t')){ ind+=(start[i]=='\t')?4:1; i++; }
    /* check if line has any code (not blank, not comment-only) */
    int j=i; int instr=0; char q=0; int hascode=0;
    while(j<rawlen){ char c=start[j]; if(instr){ if(c=='\\'){j+=2;continue;} if(c==q)instr=0; j++; continue; } if(c=='"'||c=='\''){instr=1;q=c;hascode=1;j++;continue;} if(c=='#')break; if(c!=' '&&c!='\t'&&c!='\r')hascode=1; j++; }
    if(!hascode)return;
    if(L_count>=MAXLINES)return;
    start[rawlen]=0;                       /* terminate (we own the buffer) */
    L_text[L_count]=start+i; L_indent[L_count]=ind; L_count++;
}

/* ---------- statement executor ---------- */
#define FL_NORMAL 0
#define FL_BREAK 1
#define FL_CONTINUE 2
static int exec_block(int indent,int* pc);

/* skip a body block (lines with indent > 'indent') */
static void skip_block(int indent,int* pc){ while(*pc<L_count && L_indent[*pc]>indent)(*pc)++; }

/* parse an assignment if the line is "NAME op= expr"; returns 1 if handled */
static int try_assign(const char* line){
    /* find a name then '=' (not '==') at top level (no parens/strings before) */
    int i=0; while(line[i]==' ')i++;
    if(!isname0(line[i]))return 0;
    int s=i; while(isnamec(line[i]))i++; int ne=i;
    while(line[i]==' ')i++;
    char op=0;
    if(line[i]=='='&&line[i+1]!='=' ){ op='='; }
    else if((line[i]=='+'||line[i]=='-'||line[i]=='*'||line[i]=='/'||line[i]=='%')&&line[i+1]=='='){ op=line[i]; }
    else return 0;
    char name[32]; int k=0; for(int p=s;p<ne&&k<31;p++)name[k++]=line[p]; name[k]=0;
    int rhs=i + (op=='=' ? 1 : 2);
    tokenize(line+rhs); tp=0; g_is_range=0; Val v=eval_expr();
    if(op!='='){ Val* cur=find_var(name); Val a=cur?*cur:mknum(0); if(op=='+'){ if(a.t==1||v.t==1){ char sa[128],sb[128]; val_str(a,sa); val_str(v,sb); char out[256]; int o=0,kk=0; while(sa[kk]&&o<255)out[o++]=sa[kk++]; kk=0; while(sb[kk]&&o<255)out[o++]=sb[kk++]; out[o]=0; v=mkstr(out);} else v=mknum(a.n+v.n);} else if(op=='-')v=mknum(a.n-v.n); else if(op=='*')v=mknum(a.n*v.n); else if(op=='/')v=mknum(v.n==0?0:a.n/v.n); else v=mknum(py_mod(a.n, v.n?v.n:1)); }
    set_var(name,v); return 1;
}

/* keyword match with word boundary */
static int kwmatch(const char* line,const char* kw){ int i=0; while(kw[i]){ if(line[i]!=kw[i])return 0; i++; } return !isnamec(line[i]); }
/* find a top-level ':' (not in strings/parens); -1 if none */
static int find_colon(const char* s){ int i=0,par=0,instr=0; char q=0; while(s[i]){ char c=s[i]; if(instr){ if(c=='\\'&&s[i+1]){i+=2;continue;} if(c==q)instr=0; i++; continue; } if(c=='"'||c=='\''){instr=1;q=c;i++;continue;} if(c=='(')par++; else if(c==')'){if(par>0)par--;} else if(c==':'&&par==0)return i; i++; } return -1; }

/* execute one simple statement (no compound); returns flow status */
static int exec_simple(const char* line){
    while(*line==' ')line++;
    if(kwmatch(line,"pass"))return FL_NORMAL;
    if(kwmatch(line,"break"))return FL_BREAK;
    if(kwmatch(line,"continue"))return FL_CONTINUE;
    if(kwmatch(line,"import")){ const char* p=line+6; while(*p==' ')p++; char md[32]; int k=0; while(isnamec(*p)&&k<31)md[k++]=*p++; md[k]=0;
        if(streqf(md,"math")||streqf(md,"random")||streqf(md,"time")||streqf(md,"os")||streqf(md,"sys"))return FL_NORMAL;
        err("ModuleNotFoundError"); return FL_NORMAL; }
    if(try_assign(line))return FL_NORMAL;
    tokenize(line); tp=0; g_is_range=0; eval_expr(); return FL_NORMAL;
}
/* execute an inline body (one or more ';'-separated simple statements) */
static int exec_inline(const char* code){
    char buf[512]; int i=0;
    while(code[i]){
        int par=0,instr=0; char q=0; int o=0;
        while(code[i]){ char c=code[i];
            if(instr){ if(c=='\\'&&code[i+1]){ if(o<510){buf[o++]=c;buf[o++]=code[i+1];} i+=2; continue; } if(c==q)instr=0; if(o<511)buf[o++]=c; i++; continue; }
            if(c=='"'||c=='\''){ instr=1; q=c; if(o<511)buf[o++]=c; i++; continue; }
            if(c=='(')par++; else if(c==')'){if(par>0)par--;} else if(c==';'&&par==0){ i++; break; }
            if(o<511)buf[o++]=c; i++; }
        buf[o]=0; int k=0; while(buf[k]==' ')k++;
        if(buf[k]){ int fl=exec_simple(buf); if(fl)return fl; if(g_err)return FL_NORMAL; }
    }
    return FL_NORMAL;
}

/* execute statements at the given indent starting at *pc */
static int exec_block(int indent,int* pc){
    while(*pc<L_count && L_indent[*pc]==indent && !g_err){
        char* line=L_text[*pc];
        if(kwmatch(line,"if")){
            int taken=0; int cp=find_colon(line);
            char sv=0; if(cp>=0){ sv=line[cp]; line[cp]=0; } tokenize(line+2); tp=0; Val c=eval_expr(); if(cp>=0)line[cp]=sv;
            int cond_t=truthy(c); const char* body=(cp>=0)?line+cp+1:""; while(*body==' ')body++;
            if(*body){ (*pc)++; if(cond_t){ taken=1; int f=exec_inline(body); if(f)return f; } }
            else { (*pc)++; int bind=(*pc<L_count)?L_indent[*pc]:indent+1; if(cond_t){ taken=1; int f=exec_block(bind,pc); if(f)return f; } else skip_block(indent,pc); }
            while(*pc<L_count && L_indent[*pc]==indent && !g_err){
                char* l2=L_text[*pc];
                if(kwmatch(l2,"elif")){ int cp2=find_colon(l2); int dotake=0;
                    if(!taken){ char s2=0; if(cp2>=0){s2=l2[cp2];l2[cp2]=0;} tokenize(l2+4); tp=0; Val cc=eval_expr(); if(cp2>=0)l2[cp2]=s2; dotake=truthy(cc); }
                    const char* b2=(cp2>=0)?l2+cp2+1:""; while(*b2==' ')b2++;
                    if(*b2){ (*pc)++; if(dotake){ taken=1; int f=exec_inline(b2); if(f)return f; } }
                    else { (*pc)++; int bind=(*pc<L_count)?L_indent[*pc]:indent+1; if(dotake){ taken=1; int f=exec_block(bind,pc); if(f)return f; } else skip_block(indent,pc); }
                } else if(kwmatch(l2,"else")){ int cp2=find_colon(l2); const char* b2=(cp2>=0)?l2+cp2+1:""; while(*b2==' ')b2++;
                    if(*b2){ (*pc)++; if(!taken){ int f=exec_inline(b2); if(f)return f; } }
                    else { (*pc)++; int bind=(*pc<L_count)?L_indent[*pc]:indent+1; if(!taken){ int f=exec_block(bind,pc); if(f)return f; } else skip_block(indent,pc); }
                    break;
                } else break;
            }
            continue;
        }
        if(kwmatch(line,"while")){
            int cp=find_colon(line); const char* body=(cp>=0)?line+cp+1:""; while(*body==' ')body++;
            if(*body){ (*pc)++; for(;;){ char sv=0; if(cp>=0){sv=line[cp];line[cp]=0;} tokenize(line+5); tp=0; Val c=eval_expr(); if(cp>=0)line[cp]=sv; if(!truthy(c)||g_err)break; if(chk_stop())break; int f=exec_inline(body); if(f==FL_BREAK)break; if(g_err)break; } }
            else { (*pc)++; int bind=(*pc<L_count)?L_indent[*pc]:indent+1; int bodystart=*pc;
                for(;;){ char sv=0; if(cp>=0){sv=line[cp];line[cp]=0;} tokenize(line+5); tp=0; Val c=eval_expr(); if(cp>=0)line[cp]=sv; if(!truthy(c)||g_err)break; if(chk_stop())break; int p2=bodystart; int f=exec_block(bind,&p2); if(f==FL_BREAK)break; if(g_err)break; }
                *pc=bodystart; skip_block(indent,pc); }
            continue;
        }
        if(kwmatch(line,"for")){
            int cp=find_colon(line); const char* p=line+3; while(*p==' ')p++;
            char var[32]; int k=0; while(isnamec(*p)&&k<31)var[k++]=*p++; var[k]=0;
            while(*p==' ')p++; if(p[0]=='i'&&p[1]=='n'&&!isnamec(p[2]))p+=2; while(*p==' ')p++;
            char sv=0; if(cp>=0){ sv=line[cp]; line[cp]=0; } tokenize(p); tp=0; g_is_range=0; eval_expr(); if(cp>=0)line[cp]=sv;
            int a=g_range_a,b=g_range_b,st=g_range_s; if(st==0)st=1; int isr=g_is_range;
            const char* body=(cp>=0)?line+cp+1:""; while(*body==' ')body++;
            if(*body){ (*pc)++; if(isr) for(int iv=a;(st>0)?(iv<b):(iv>b);iv+=st){ set_var(var,mknum(iv)); if(chk_stop())break; int f=exec_inline(body); if(f==FL_BREAK)break; if(g_err)break; } }
            else { (*pc)++; int bind=(*pc<L_count)?L_indent[*pc]:indent+1; int bodystart=*pc;
                if(isr) for(int iv=a;(st>0)?(iv<b):(iv>b);iv+=st){ set_var(var,mknum(iv)); if(chk_stop())break; int p2=bodystart; int f=exec_block(bind,&p2); if(f==FL_BREAK)break; if(g_err)break; }
                *pc=bodystart; skip_block(indent,pc); }
            continue;
        }
        { int f=exec_simple(line); (*pc)++; if(f)return f; }
    }
    return FL_NORMAL;
}

/* ---------- entry ---------- */
void py_run(char* src){
    nvars=0; L_count=0; g_err=0; g_errmsg[0]=0;
    int n=0; while(src[n])n++;
    int i=0; while(i<n){ int s=i; while(i<n&&src[i]!='\n')i++; int e=i; if(e>s&&src[e-1]=='\r')e--; add_line(src+s, e-s); if(i<n)i++; }
    int pc=0; exec_block(0,&pc);
    if(g_err){ OUT("\n*** "); OUT(g_errmsg); OUT(" ***\n"); }
}

#ifdef NOOVEX
static char prog[200000];
void _start(void){
    int n=nvx_fileread("SCRIPT.PY",prog,199999);
    if(n<0){
        nvx_print("SCRIPT.PY not found - running built-in demo.\nWrite your own Python in the editor and save as SCRIPT.PY.\n\n");
        const char* demo =
            "print(\"=== mini-Python on NoovexOS ===\")\n"
            "x = 7\n"
            "y = 3\n"
            "print(\"7 + 3 =\", x + y)\n"
            "print(\"7 * 3 =\", x * y)\n"
            "print(\"2 ** 10 =\", 2 ** 10)\n"
            "print(\"7 / 3 =\", x / y)\n"
            "name = \"Noovex\"\n"
            "print(name + \"OS\", \"is\", \"running!\")\n"
            "print(\"-\" * 20)\n"
            "print(\"squares:\")\n"
            "for i in range(1, 6):\n"
            "    print(i, \"squared =\", i * i)\n"
            "import math\n"
            "print(\"math.sqrt(2) =\", math.sqrt(2))\n"
            "print(\"math.pi =\", math.pi)\n"
            "import random\n"
            "random.seed(7)\n"
            "print(\"dice roll:\", random.randint(1, 6))\n"
            "print(\"FizzBuzz 1..15:\")\n"
            "for i in range(1, 16):\n"
            "    if i % 15 == 0: print(\"FizzBuzz\")\n"
            "    elif i % 3 == 0: print(\"Fizz\")\n"
            "    elif i % 5 == 0: print(\"Buzz\")\n"
            "    else: print(i)\n";
        int k=0; while(demo[k]){ prog[k]=demo[k]; k++; } prog[k]=0;
    } else { prog[n]=0; nvx_print("=== SCRIPT.PY ===\n\n"); }
    py_run(prog);
    nvx_print("\n=== done ===\n");
    nvx_exit();
}
#endif

#if !defined(NOOVEX) && !defined(IDE)
int main(int argc,char**argv){
    static char prog[200000]; int n=0;
    if(argc>1){ FILE* f=fopen(argv[1],"rb"); n=fread(prog,1,199999,f); fclose(f); }
    else { n=fread(prog,1,199999,stdin); }
    prog[n]=0; py_run(prog); return 0;
}
#endif
