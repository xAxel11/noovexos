#ifndef NVX_ZIP_H
#define NVX_ZIP_H
/* Minimal ZIP reader for NoovexOS.
   Supports STORE (method 0) and DEFLATE (method 8, via pz_inflate from png.h).
   Reads the End-Of-Central-Directory + central directory to enumerate entries,
   then the local file header to locate the compressed data. No ZIP64, no encryption. */

typedef struct {
    char  name[64];
    unsigned method;       /* 0 = store, 8 = deflate */
    unsigned comp_size;
    unsigned uncomp_size;
    unsigned local_off;    /* offset of local file header in the archive */
} ZipEntry;

#define ZIP_MAXENT 64

static unsigned zip_u16(const unsigned char* p){ return p[0] | (p[1]<<8); }
static unsigned zip_u32(const unsigned char* p){ return p[0] | (p[1]<<8) | (p[2]<<16) | ((unsigned)p[3]<<24); }

/* Parse the archive at (data,len). Fills ents[] (max ZIP_MAXENT). Returns count, or -1 on error. */
static int zip_list(const unsigned char* data, int len, ZipEntry* ents){
    if(len<22) return -1;
    /* find End Of Central Directory record: signature 0x06054b50, scan backwards */
    int eocd=-1;
    for(int i=len-22;i>=0 && i>len-22-65536;i--){
        if(data[i]==0x50&&data[i+1]==0x4b&&data[i+2]==0x05&&data[i+3]==0x06){ eocd=i; break; }
    }
    if(eocd<0) return -1;
    int total = zip_u16(data+eocd+10);
    unsigned cdoff = zip_u32(data+eocd+16);
    if(total>ZIP_MAXENT) total=ZIP_MAXENT;
    unsigned p=cdoff; int n=0;
    for(int e=0;e<total;e++){
        if(p+46>(unsigned)len) break;
        if(!(data[p]==0x50&&data[p+1]==0x4b&&data[p+2]==0x01&&data[p+3]==0x02)) break; /* central dir header */
        unsigned method = zip_u16(data+p+10);
        unsigned csize  = zip_u32(data+p+20);
        unsigned usize  = zip_u32(data+p+24);
        unsigned nlen   = zip_u16(data+p+28);
        unsigned elen   = zip_u16(data+p+30);
        unsigned clen   = zip_u16(data+p+32);
        unsigned loff   = zip_u32(data+p+42);
        ZipEntry* z=&ents[n];
        unsigned k=0; for(;k<nlen && k<63 && p+46+k<(unsigned)len;k++) z->name[k]=data[p+46+k];
        z->name[k]=0;
        z->method=method; z->comp_size=csize; z->uncomp_size=usize; z->local_off=loff;
        /* skip directory entries (names ending in /) */
        if(k>0 && z->name[k-1]!='/') n++;
        p += 46 + nlen + elen + clen;
    }
    return n;
}

/* Extract entry e from the archive into out (capacity outcap), using scratch (inflate workspace).
   Returns the number of bytes written, or -1 on error. */
static int zip_extract(const unsigned char* data, int len, const ZipEntry* e,
                       unsigned char* out, int outcap){
    unsigned p=e->local_off;
    if(p+30>(unsigned)len) return -1;
    if(!(data[p]==0x50&&data[p+1]==0x4b&&data[p+2]==0x03&&data[p+3]==0x04)) return -1; /* local header */
    unsigned nlen = zip_u16(data+p+26);
    unsigned elen = zip_u16(data+p+28);
    unsigned dstart = p + 30 + nlen + elen;
    if(dstart + e->comp_size > (unsigned)len) return -1;
    const unsigned char* src = data + dstart;
    if(e->method==0){ /* stored */
        int n = e->comp_size; if(n>outcap) n=outcap;
        for(int i=0;i<n;i++) out[i]=src[i];
        return n;
    } else if(e->method==8){ /* deflate (raw, no zlib header) */
        int n = pz_inflate_raw(src, e->comp_size, out, outcap);
        return n;
    }
    return -1;
}
#endif
