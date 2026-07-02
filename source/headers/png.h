/* Minimal PNG decoder: zlib/DEFLATE inflate + PNG unfilter.
   Supports 8-bit color types 2 (RGB) and 6 (RGBA), no interlace.
   Pure C, no libc beyond what is passed in. Buffers supplied by caller.
   Returns 0 on success, negative on error. Output is RGBA (4 bytes/px). */
#ifndef NVX_PNG_H
#define NVX_PNG_H

typedef struct {
    const unsigned char *src; int slen; int spos;   /* compressed input */
    int bitbuf, bitcnt;
    unsigned char *out; int outcap; int outpos;      /* inflate output (filtered scanlines) */
} pngz;

static int pz_getbit(pngz*z){
    if(z->bitcnt==0){ if(z->spos>=z->slen) return -1; z->bitbuf=z->src[z->spos++]; z->bitcnt=8; }
    int b=z->bitbuf&1; z->bitbuf>>=1; z->bitcnt--; return b;
}
static int pz_getbits(pngz*z,int n){ int v=0; for(int i=0;i<n;i++){ int b=pz_getbit(z); if(b<0)return -1; v|=b<<i; } return v; }

/* Huffman table: canonical from code lengths */
typedef struct { short count[16]; short sym[288]; } pzhuff;
static int pz_build(pzhuff*h,const unsigned char*lengths,int n){
    for(int i=0;i<16;i++)h->count[i]=0;
    for(int i=0;i<n;i++)h->count[lengths[i]]++;
    h->count[0]=0;
    short offs[16]; offs[1]=0;
    for(int i=1;i<15;i++)offs[i+1]=offs[i]+h->count[i];
    for(int i=0;i<n;i++) if(lengths[i])h->sym[offs[lengths[i]]++]=i;
    return 0;
}
static int pz_decode(pngz*z,pzhuff*h){
    int code=0,first=0,idx=0;
    for(int len=1;len<=15;len++){
        int b=pz_getbit(z); if(b<0)return -1;
        code|=b;
        int cnt=h->count[len];
        if(code-first<cnt) return h->sym[idx+(code-first)];
        idx+=cnt; first+=cnt; first<<=1; code<<=1;
    }
    return -1;
}
static const unsigned short PZ_LBASE[29]={3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
static const unsigned char  PZ_LEXT[29]={0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
static const unsigned short PZ_DBASE[30]={1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
static const unsigned char  PZ_DEXT[30]={0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

static int pz_inflate_block(pngz*z,pzhuff*lh,pzhuff*dh){
    for(;;){
        int sym=pz_decode(z,lh); if(sym<0)return -1;
        if(sym==256)return 0;
        if(sym<256){ if(z->outpos>=z->outcap)return -2; z->out[z->outpos++]=(unsigned char)sym; }
        else{
            sym-=257; if(sym>=29)return -1;
            int e=pz_getbits(z,PZ_LEXT[sym]); if(e<0)return -1;
            int length=PZ_LBASE[sym]+e;
            int dsym=pz_decode(z,dh); if(dsym<0||dsym>=30)return -1;
            int de=pz_getbits(z,PZ_DEXT[dsym]); if(de<0)return -1;
            int dist=PZ_DBASE[dsym]+de;
            if(dist>z->outpos)return -1;
            for(int i=0;i<length;i++){ if(z->outpos>=z->outcap)return -2; z->out[z->outpos]=z->out[z->outpos-dist]; z->outpos++; }
        }
    }
}
static int pz_fixed(pngz*z){
    static unsigned char ll[288],dl[30]; static int built=0;
    static pzhuff lh,dh;
    if(!built){
        for(int i=0;i<144;i++)ll[i]=8; for(int i=144;i<256;i++)ll[i]=9;
        for(int i=256;i<280;i++)ll[i]=7; for(int i=280;i<288;i++)ll[i]=8;
        for(int i=0;i<30;i++)dl[i]=5;
        pz_build(&lh,ll,288); pz_build(&dh,dl,30); built=1;
    }
    return pz_inflate_block(z,&lh,&dh);
}
static int pz_dynamic(pngz*z){
    int hlit=pz_getbits(z,5); if(hlit<0)return -1; hlit+=257;
    int hdist=pz_getbits(z,5); if(hdist<0)return -1; hdist+=1;
    int hclen=pz_getbits(z,4); if(hclen<0)return -1; hclen+=4;
    static const unsigned char ord[19]={16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
    unsigned char cl[19]; for(int i=0;i<19;i++)cl[i]=0;
    for(int i=0;i<hclen;i++){ int v=pz_getbits(z,3); if(v<0)return -1; cl[ord[i]]=(unsigned char)v; }
    pzhuff clh; pz_build(&clh,cl,19);
    unsigned char lengths[288+32]; int n=0,total=hlit+hdist;
    while(n<total){
        int sym=pz_decode(z,&clh); if(sym<0)return -1;
        if(sym<16) lengths[n++]=(unsigned char)sym;
        else if(sym==16){ if(n==0)return -1; int r=pz_getbits(z,2); if(r<0)return -1; r+=3; unsigned char p=lengths[n-1]; while(r--&&n<total)lengths[n++]=p; }
        else if(sym==17){ int r=pz_getbits(z,3); if(r<0)return -1; r+=3; while(r--&&n<total)lengths[n++]=0; }
        else { int r=pz_getbits(z,7); if(r<0)return -1; r+=11; while(r--&&n<total)lengths[n++]=0; }
    }
    static pzhuff lh,dh;
    pz_build(&lh,lengths,hlit);
    pz_build(&dh,lengths+hlit,hdist);
    return pz_inflate_block(z,&lh,&dh);
}
/* inflate full zlib stream (skips 2-byte header) into z->out; returns bytes or <0 */
static int pz_inflate(const unsigned char*src,int slen,unsigned char*out,int outcap){
    pngz z; z.src=src; z.slen=slen; z.spos=2; z.bitbuf=0; z.bitcnt=0; z.out=out; z.outcap=outcap; z.outpos=0;
    for(;;){
        int final=pz_getbit(&z); if(final<0)return -1;
        int type=pz_getbits(&z,2); if(type<0)return -1;
        if(type==0){ /* stored */
            z.bitcnt=0; if(z.spos+4>z.slen)return -1;
            int len=z.src[z.spos]|(z.src[z.spos+1]<<8); z.spos+=4;
            for(int i=0;i<len;i++){ if(z.spos>=z.slen||z.outpos>=z.outcap)return -1; z.out[z.outpos++]=z.src[z.spos++]; }
        } else if(type==1){ if(pz_fixed(&z))return -1; }
        else if(type==2){ if(pz_dynamic(&z))return -1; }
        else return -1;
        if(final)break;
    }
    return z.outpos;
}
/* raw deflate (no 2-byte zlib header) — used by ZIP method 8 */
static int pz_inflate_raw(const unsigned char*src,int slen,unsigned char*out,int outcap){
    pngz z; z.src=src; z.slen=slen; z.spos=0; z.bitbuf=0; z.bitcnt=0; z.out=out; z.outcap=outcap; z.outpos=0;
    for(;;){
        int final=pz_getbit(&z); if(final<0)return -1;
        int type=pz_getbits(&z,2); if(type<0)return -1;
        if(type==0){ z.bitcnt=0; if(z.spos+4>z.slen)return -1;
            int len=z.src[z.spos]|(z.src[z.spos+1]<<8); z.spos+=4;
            for(int i=0;i<len;i++){ if(z.spos>=z.slen||z.outpos>=z.outcap)return -1; z.out[z.outpos++]=z.src[z.spos++]; }
        } else if(type==1){ if(pz_fixed(&z))return -1; }
        else if(type==2){ if(pz_dynamic(&z))return -1; }
        else return -1;
        if(final)break;
    }
    return z.outpos;
}

static int pz_paeth(int a,int b,int c){ int p=a+b-c,pa=p>a?p-a:a-p,pb=p>b?p-b:b-p,pc=p>c?p-c:c-p; if(pa<=pb&&pa<=pc)return a; if(pb<=pc)return b; return c; }

/* Decode PNG file bytes into RGBA. raw = scratch for filtered scanlines (>= h*(1+w*ch)).
   out = RGBA output (>= w*h*4). Returns 0 ok. */
static int png_decode(const unsigned char*png,int len,unsigned char*raw,int rawcap,unsigned char*out,int outcap,int*ow,int*oh){
    if(len<8||png[0]!=0x89||png[1]!=0x50)return -1;
    int p=8,w=0,h=0,ch=0,bitdepth=0,color=0;
    /* gather IDAT into 'out' temporarily? need separate compressed buffer; reuse 'raw' tail.
       We'll inflate IDAT directly: first concatenate IDAT into a compressed buffer = use 'out' as cz store. */
    unsigned char*cz=out; int czlen=0; /* borrow out as compressed scratch first */
    while(p+8<=len){
        int clen=(png[p]<<24)|(png[p+1]<<16)|(png[p+2]<<8)|png[p+3];
        const unsigned char*type=png+p+4;
        const unsigned char*data=png+p+8;
        if(p+12+clen>len)break;
        if(type[0]=='I'&&type[1]=='H'&&type[2]=='D'&&type[3]=='R'){
            w=(data[0]<<24)|(data[1]<<16)|(data[2]<<8)|data[3];
            h=(data[4]<<24)|(data[5]<<16)|(data[6]<<8)|data[7];
            bitdepth=data[8]; color=data[9];
            if(bitdepth!=8)return -2;
            if(color==2)ch=3; else if(color==6)ch=4; else return -3;
            if(data[12]!=0)return -4; /* interlace unsupported */
        } else if(type[0]=='I'&&type[1]=='D'&&type[2]=='A'&&type[3]=='T'){
            for(int i=0;i<clen;i++){ if(czlen>=outcap)return -5; cz[czlen++]=data[i]; }
        } else if(type[0]=='I'&&type[1]=='E'&&type[2]=='N'&&type[3]=='D') break;
        p+=12+clen;
    }
    if(w<=0||h<=0||ch==0)return -6;
    /* inflate cz -> raw */
    int need=h*(1+w*ch); if(need>rawcap)return -7;
    /* copy cz out of 'out' since inflate writes into raw (separate); cz currently in out */
    int rn=pz_inflate(cz,czlen,raw,rawcap);
    if(rn<need)return -8;
    /* unfilter raw (in place per scanline) and write RGBA to out */
    int stride=w*ch;
    for(int y=0;y<h;y++){
        unsigned char*row=raw+y*(1+stride);
        int ft=row[0]; unsigned char*cur=row+1;
        unsigned char*prev=(y>0)?(raw+(y-1)*(1+stride)+1):0;
        for(int x=0;x<stride;x++){
            int a=(x>=ch)?cur[x-ch]:0;
            int b=prev?prev[x]:0;
            int c=(prev&&x>=ch)?prev[x-ch]:0;
            int v=cur[x];
            if(ft==1)v+=a; else if(ft==2)v+=b; else if(ft==3)v+=(a+b)>>1; else if(ft==4)v+=pz_paeth(a,b,c);
            cur[x]=(unsigned char)v;
        }
        for(int x=0;x<w;x++){
            unsigned char r=cur[x*ch+0],g=cur[x*ch+1],bl=cur[x*ch+2];
            unsigned char al=(ch==4)?cur[x*ch+3]:255;
            int o=(y*w+x)*4; if(o+3>=outcap)return -9;
            out[o]=r; out[o+1]=g; out[o+2]=bl; out[o+3]=al;
        }
    }
    *ow=w; *oh=h; return 0;
}
#endif
