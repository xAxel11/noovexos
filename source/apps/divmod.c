/* minimal __divmoddi4 / __udivmoddi4 for -m32 freestanding */
typedef unsigned long long u64; typedef long long s64;
u64 __udivmoddi4(u64 n, u64 d, u64* rem){
    if(d==0){ if(rem)*rem=0; return 0; }
    u64 q=0,r=0;
    for(int i=63;i>=0;i--){ r=(r<<1)|((n>>i)&1ULL); if(r>=d){ r-=d; q|=(1ULL<<i); } }
    if(rem)*rem=r; return q;
}
s64 __divmoddi4(s64 n, s64 d, s64* rem){
    int sn=n<0, sd=d<0; u64 un=sn?(u64)(-n):(u64)n, ud=sd?(u64)(-d):(u64)d, ur;
    u64 uq=__udivmoddi4(un,ud,&ur);
    s64 q=(s64)uq; if(sn!=sd)q=-q;
    s64 r=(s64)ur; if(sn)r=-r;
    if(rem)*rem=r; return q;
}
s64 __divdi3(s64 n,s64 d){ return __divmoddi4(n,d,0); }
s64 __moddi3(s64 n,s64 d){ s64 r; __divmoddi4(n,d,&r); return r; }
u64 __udivdi3(u64 n,u64 d){ return __udivmoddi4(n,d,0); }
u64 __umoddi3(u64 n,u64 d){ u64 r; __udivmoddi4(n,d,&r); return r; }
