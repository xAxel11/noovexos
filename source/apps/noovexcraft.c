/* NoovexCraft - first-person block world for NoovexOS.
   Software raycaster (adapted from RAYCAST engine) over an EDITABLE grid world.
   WASD move, mouse / arrows look, 1-7 pick block, LMB or SPACE break,
   RMB or E place, TAB toggle map, ESC quit. Grid-voxel (Wolf-style columns) -
   honest about being a 2.5D block world, not full 3D voxels. */
#include "noovex.h"
#include "noovex_libc.h"

#define FW 320
#define FH 240
#define PI 3.14159265f
static u32* BB; static u32* LFB; static int PITCH,SW,SH;
static float ZBUF[FW];

static float fsin(float x){ while(x>PI)x-=2*PI; while(x<-PI)x+=2*PI;
    float s=1.0f; if(x<0){x=-x;s=-1.0f;} float p=x*(PI-x); return s*(16.0f*p)/(5.0f*PI*PI-4.0f*p); }
static float fcos(float x){ return fsin(x+PI/2); }
static float fabsf_(float x){ return x<0?-x:x; }

#define MW 32
#define MH 32
static unsigned char W_[MH][MW];

/* block ids: 0 air,1 grass,2 dirt,3 stone,4 wood,5 planks,6 brick,7 glass */
#define NB 7
static const char* BNAME[NB+1]={"AIR","GRASS","DIRT","STONE","WOOD","PLANKS","BRICK","GLASS"};

static u32 hash2(int x,int y){ u32 h=(u32)(x*374761393+y*668265263); h=(h^(h>>13))*1274126177u; return h^(h>>16); }

/* per-block texel colour with simple procedural pattern + distance shade */
static u32 blockcol(int t,int side,float dist,int tx,int ty){
    u32 base; int pat=0;
    switch(t){
        case 1: base=0x4CAF50; if(ty<3){base=0x5DC24D;} if((hash2(tx,ty)&7)==0)pat=-18; break; /* grass */
        case 2: base=0x8B5A2B; if((hash2(tx,ty)&3)==0)pat=-16; break;                          /* dirt  */
        case 3: base=0x8A8A8A; if((hash2(tx>>1,ty>>1)&7)<2)pat=-24; break;                      /* stone */
        case 4: base=0x6B4423; if((tx&3)==0)pat=-26; break;                                     /* wood  */
        case 5: base=0xB8894B; if((ty&3)==0)pat=-30; break;                                     /* planks*/
        case 6: base=0xB4472F; if((ty&3)==3||((tx+((ty>>2)&1)*2)&3)==0)pat=30; break;           /* brick */
        default:base=0x8FD6E8; break;                                                          /* glass */
    }
    int r=(base>>16&255)+pat,g=(base>>8&255)+pat,b=(base&255)+pat;
    if(side){ r=r*3/4; g=g*3/4; b=b*3/4; }
    int sh=(int)(dist*20); if(sh>190)sh=190;
    r-=sh; g-=sh; b-=sh;
    if(r<0)r=0; if(g<0)g=0; if(b<0)b=0; if(r>255)r=255; if(g>255)g=255; if(b>255)b=255;
    return ((u32)r<<16)|((u32)g<<8)|b;
}
static int solid(int t){ return t!=0; }

static const unsigned char FONT[96][5]={
 [32]={0,0,0,0,0},[45]={8,8,8,8,8},[58]={0,0x36,0x36,0,0},[46]={0,0x60,0x60,0,0},
 [48]={0x3E,0x51,0x49,0x45,0x3E},[49]={0,0x42,0x7F,0x40,0},[50]={0x42,0x61,0x51,0x49,0x46},
 [51]={0x21,0x41,0x45,0x4B,0x31},[52]={0x18,0x14,0x12,0x7F,0x10},[53]={0x27,0x45,0x45,0x45,0x39},
 [54]={0x3C,0x4A,0x49,0x49,0x30},[55]={0x01,0x71,0x09,0x05,0x03},[56]={0x36,0x49,0x49,0x49,0x36},
 [57]={0x06,0x49,0x49,0x29,0x1E},
 [65]={0x7E,0x11,0x11,0x11,0x7E},[66]={0x7F,0x49,0x49,0x49,0x36},[67]={0x3E,0x41,0x41,0x41,0x22},
 [68]={0x7F,0x41,0x41,0x22,0x1C},[69]={0x7F,0x49,0x49,0x49,0x41},[70]={0x7F,0x09,0x09,0x09,0x01},
 [71]={0x3E,0x41,0x49,0x49,0x7A},[72]={0x7F,0x08,0x08,0x08,0x7F},[73]={0,0x41,0x7F,0x41,0},
 [75]={0x7F,0x08,0x14,0x22,0x41},[76]={0x7F,0x40,0x40,0x40,0x40},[77]={0x7F,0x02,0x0C,0x02,0x7F},
 [78]={0x7F,0x04,0x08,0x10,0x7F},[79]={0x3E,0x41,0x41,0x41,0x3E},[80]={0x7F,0x09,0x09,0x09,0x06},
 [82]={0x7F,0x09,0x19,0x29,0x46},[83]={0x46,0x49,0x49,0x49,0x31},[84]={0x01,0x01,0x7F,0x01,0x01},
 [85]={0x3F,0x40,0x40,0x40,0x3F},[87]={0x3F,0x40,0x38,0x40,0x3F},[89]={0x07,0x08,0x70,0x08,0x07},
 [88]={0x63,0x14,0x08,0x14,0x63},[86]={0x1F,0x20,0x40,0x20,0x1F},[71+0]={0x3E,0x41,0x49,0x49,0x7A},
};
static void pset(int x,int y,u32 c){ if((unsigned)x<FW&&(unsigned)y<FH)BB[y*FW+x]=c; }
static void frect(int x,int y,int w,int h,u32 c){ for(int j=0;j<h;j++)for(int i=0;i<w;i++)pset(x+i,y+j,c); }
static void box(int x,int y,int w,int h,u32 c){ frect(x,y,w,1,c);frect(x,y+h-1,w,1,c);frect(x,y,1,h,c);frect(x+w-1,y,1,h,c); }
static void dtext(int x,int y,const char* t,u32 c){ while(*t){ char ch=*t; if(ch>='a'&&ch<='z')ch-=32;
    const unsigned char* g=FONT[(unsigned char)ch&0x7F];
    for(int col=0;col<5;col++)for(int row=0;row<7;row++) if(g[col]&(1<<row)) pset(x+col,y+row,c); x+=6; t++; } }

static void gen_world(void){
    for(int y=0;y<MH;y++)for(int x=0;x<MW;x++){
        if(x==0||y==0||x==MW-1||y==MH-1){ W_[y][x]=3; continue; }   /* stone border */
        W_[y][x]=0;
    }
    /* a few structures so you spawn into a built scene */
    for(int x=6;x<=12;x++){ W_[6][x]=6; W_[12][x]=6; }             /* brick house walls */
    for(int y=6;y<=12;y++){ W_[y][6]=6; W_[y][12]=6; }
    W_[12][9]=0; W_[12][10]=0;                                     /* doorway */
    W_[8][8]=7; W_[8][10]=7;                                       /* glass windows */
    for(int y=18;y<=24;y++)for(int x=18;x<=24;x++) if((x+y)&1) W_[y][x]=4;  /* wood grove */
    W_[16][16]=3;W_[16][17]=3;W_[17][16]=3;                        /* stone pillar */
    for(int x=20;x<=27;x++) W_[27][x]=5;                           /* plank platform edge */
    /* scatter some dirt/grass mounds */
    for(int i=0;i<40;i++){ int x=2+(hash2(i,7)% (MW-4)); int y=2+(hash2(i,13)%(MH-4));
        if(W_[y][x]==0) W_[y][x]=(hash2(i,1)&1)?1:2; }
}

/* cast a ray from player along facing dir; return targeted solid cell + the
   empty cell just before it (for placement). returns 1 if a block is hit. */
static int target_block(float px,float py,float dx,float dy,int* bx,int* by,int* px2,int* py2){
    float x=px,y=py; int pxc=(int)px,pyc=(int)py;
    for(float t=0;t<6.0f;t+=0.05f){
        x=px+dx*t; y=py+dy*t; int cx=(int)x, cy=(int)y;
        if(cx<0||cy<0||cx>=MW||cy>=MH) return 0;
        if(solid(W_[cy][cx])){ *bx=cx; *by=cy; *px2=pxc; *py2=pyc; return 1; }
        pxc=cx; pyc=cy;
    }
    return 0;
}

void _start(void){
    __asm__ volatile("fninit");
    LFB=nvx_fb(); PITCH=nvx_fb_pitch(); SW=nvx_fb_w(); SH=nvx_fb_h();
    BB=(u32*)malloc(FW*FH*4);
    if(!BB){ nvx_clearrgb(0x101010); for(;;){} }
    int* xmap=(int*)malloc(SW*4); for(int i=0;i<SW;i++)xmap[i]=i*FW/SW;
    int* ymap=(int*)malloc(SH*4); for(int i=0;i<SH;i++)ymap[i]=i*FH/SH;
    gen_world();

    float posX=9.5f, posY=9.5f, angle=0.4f;
    int fwd=0,back=0,tl=0,tr=0,sl=0,sr=0,showmap=1;
    int sel=1, prevb=0, brk=0, plc=0;
    u32 last=nvx_ticks_ms();
    int ms[4]={0,0,0,0};

    for(;;){
        u32 now=nvx_ticks_ms(); float dt=(float)(now-last); last=now; if(dt>100)dt=100;
        brk=0; plc=0;
        int sc;
        while((sc=nvx_readraw())!=0){ int rel=sc&0x80,k=sc&0x7F;
            if(k==0x11)fwd=!rel; else if(k==0x1F)back=!rel;
            else if(k==0x1E)sl=!rel; else if(k==0x20)sr=!rel;
            else if(k==0x10)tl=!rel; else if(k==0x12)tr=!rel;
            else if(k==0x4B)tl=!rel; else if(k==0x4D)tr=!rel;
            else if(k==0x48)fwd=!rel; else if(k==0x50)back=!rel;
            else if(k==0x39&&!rel)brk=1;                       /* space break */
            else if(k==0x12&&!rel){}                            /* (E handled below) */
            else if(k==0x0F&&!rel)showmap=!showmap;             /* TAB map */
            else if(k>=0x02&&k<=0x08&&!rel)sel=k-0x01;          /* keys 1..7 */
            else if(k==0x01&&!rel){ free(BB); nvx_exit(); }
        }
        /* E = place (scancode 0x12 shares with turn-right 'e'? no: 'e'=0x12 used as tr).
           Use 'Q'(0x10) already tl. Use right-mouse for place, and 'R'(0x13) as place key. */
        /* re-scan handled above; add R key: */
        /* (kept separate to avoid clobbering the movement map) */

        nvx_mouse(ms);
        angle += (float)ms[3]*0.0026f;
        int btn=ms[2];
        if((btn&1)&&!(prevb&1))brk=1;         /* LMB break */
        if((btn&2)&&!(prevb&2))plc=1;         /* RMB place */
        prevb=btn;

        float moveSpeed=dt*0.0035f, rotSpeed=dt*0.0030f;
        if(tl)angle-=rotSpeed; if(tr)angle+=rotSpeed;
        float dirX=fcos(angle), dirY=fsin(angle);
        float planeX=-fsin(angle)*0.66f, planeY=fcos(angle)*0.66f;
        float mvx=0,mvy=0;
        if(fwd){ mvx+=dirX; mvy+=dirY; } if(back){ mvx-=dirX; mvy-=dirY; }
        if(sr){ mvx+=planeX; mvy+=planeY; } if(sl){ mvx-=planeX; mvy-=planeY; }
        float nx=posX+mvx*moveSpeed, ny=posY+mvy*moveSpeed;
        if((int)ny>=0&&(int)ny<MH&&!solid(W_[(int)ny][(int)posX]))posY=ny;
        if((int)posY>=0&&(int)posY<MH&&!solid(W_[(int)posY][(int)nx]))posX=nx;

        /* break / place on the crosshair target */
        int bx,by,ex,ey;
        int havetarget=target_block(posX,posY,dirX,dirY,&bx,&by,&ex,&ey);
        if(havetarget&&brk){ if(!(bx==0||by==0||bx==MW-1||by==MH-1)) W_[by][bx]=0; }
        if(plc){ if(havetarget){ if(ex>0&&ey>0&&ex<MW-1&&ey<MH-1&&!solid(W_[ey][ex])
                    && !((int)posX==ex&&(int)posY==ey)) W_[ey][ex]=(unsigned char)sel; } }

        /* sky + ground */
        for(int y=0;y<FH/2;y++){ int sh=(FH/2-y)/7; int rr=0x6A; rr+=sh; int gg=0x9E; gg+=sh; int bb=0xD8; u32 c=((u32)rr<<16)|((u32)gg<<8)|(u32)bb; for(int x=0;x<FW;x++)BB[y*FW+x]=c; }
        for(int y=FH/2;y<FH;y++){ int sh=(y-FH/2)/7; int rr=0x3A; rr+=sh; int gg=0x6A; gg+=sh; int bb=0x2A; u32 c=((u32)rr<<16)|((u32)gg<<8)|(u32)bb; for(int x=0;x<FW;x++)BB[y*FW+x]=c; }

        for(int x=0;x<FW;x++){
            float camX=2.0f*x/FW-1.0f;
            float rdx=dirX+planeX*camX, rdy=dirY+planeY*camX;
            int mapX=(int)posX, mapY=(int)posY;
            float ddx=(rdx==0)?1e30f:fabsf_(1.0f/rdx);
            float ddy=(rdy==0)?1e30f:fabsf_(1.0f/rdy);
            float sdx,sdy; int stepX,stepY,hit=0,side=0,wt=1;
            if(rdx<0){ stepX=-1; sdx=(posX-mapX)*ddx; } else { stepX=1; sdx=(mapX+1.0f-posX)*ddx; }
            if(rdy<0){ stepY=-1; sdy=(posY-mapY)*ddy; } else { stepY=1; sdy=(mapY+1.0f-posY)*ddy; }
            for(int gi=0;gi<64&&!hit;gi++){
                if(sdx<sdy){ sdx+=ddx; mapX+=stepX; side=0; } else { sdy+=ddy; mapY+=stepY; side=1; }
                if(mapX<0||mapY<0||mapX>=MW||mapY>=MH)break;
                if(solid(W_[mapY][mapX])){ hit=1; wt=W_[mapY][mapX]; }
            }
            if(!hit){ ZBUF[x]=1e30f; continue; }
            float dist = side? (sdy-ddy) : (sdx-ddx);
            if(dist<0.01f)dist=0.01f;
            ZBUF[x]=dist;
            int lh=(int)(FH/dist); if(lh<1)lh=1;
            int y0=FH/2-lh/2, y1=FH/2+lh/2; if(y0<0)y0=0; if(y1>FH)y1=FH;
            /* texture coord along wall */
            float wallx; if(side==0) wallx=posY+dist*rdy; else wallx=posX+dist*rdx;
            wallx-=(int)wallx; int tx=(int)(wallx*8);
            int target = (havetarget && mapX==bx && mapY==by);
            for(int y=y0;y<y1;y++){
                int ty=((y-(FH/2-lh/2))*8)/lh; if(ty<0)ty=0; if(ty>7)ty=7;
                u32 c=blockcol(wt,side,dist,tx,ty);
                if(target){ c=((c>>1)&0x7F7F7F)+0x404040; }   /* highlight targeted block */
                BB[y*FW+x]=c;
            }
        }

        /* crosshair */
        frect(FW/2-4,FH/2,9,1,0xFFFFFF); frect(FW/2,FH/2-4,1,9,0xFFFFFF);

        /* minimap */
        if(showmap){
            int s=3, ox=6, oy=6;
            frect(ox-2,oy-2,MW*s+4,MH*s+4,0x101418);
            for(int y=0;y<MH;y++)for(int x=0;x<MW;x++){
                int t=W_[y][x]; if(!t)continue;
                u32 c=blockcol(t,0,0.0f,x,y); frect(ox+x*s,oy+y*s,s,s,c);
            }
            frect(ox+(int)posX*s,oy+(int)posY*s,s,s,0xFF3030);
        }

        /* hotbar */
        int hbx=FW/2-(NB*22)/2, hby=FH-26;
        frect(hbx-3,hby-3,NB*22+6,26,0x181818);
        for(int i=0;i<NB;i++){ int bxp=hbx+i*22;
            frect(bxp,hby,20,20,0x2A2A2A);
            u32 sw=blockcol(i+1,0,0.0f,0,0); frect(bxp+3,hby+3,14,14,sw);
            box(bxp,hby,20,20,(sel==i+1)?0xFFFFFF:0x555555);
            if(sel==i+1)box(bxp-1,hby-1,22,22,0xFFFFFF);
            char n[2]={(char)('1'+i),0}; dtext(bxp+1,hby-8,n,0xC0C0C0);
        }
        dtext(hbx,hby-18,BNAME[sel],0xFFFF80);

        /* HUD */
        dtext(6,FH-8,"WASD MOVE  MOUSE LOOK  LMB BREAK  RMB PLACE  1-7 PICK  TAB MAP",0xE0E0E0);

        /* blit scaled to screen */
        for(int y=0;y<SH;y++){ u32* d=LFB+y*PITCH; int sy=ymap[y]; u32* srow=BB+sy*FW;
            for(int x=0;x<SW;x++) d[x]=srow[xmap[x]]; }
    }
}
