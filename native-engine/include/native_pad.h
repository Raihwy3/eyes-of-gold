#ifndef EOG_NATIVE_PAD_H
#define EOG_NATIVE_PAD_H
/* HID bit positions from libctru, kept independent of its ABI for host tests. */
enum { EOG_A=1u<<0,EOG_B=1u<<1,EOG_SELECT=1u<<2,EOG_START=1u<<3,
       EOG_DRIGHT=1u<<4,EOG_DLEFT=1u<<5,EOG_DUP=1u<<6,EOG_DDOWN=1u<<7,
       EOG_R=1u<<8,EOG_L=1u<<9,EOG_X=1u<<10,EOG_Y=1u<<11,EOG_ZL=1u<<14,EOG_ZR=1u<<15 };
enum { EOG_RELOAD=1,EOG_CROUCH=2,EOG_INTERACT=4 };
struct NativePad { unsigned buttons,actions;int x,y,move_x,move_y; };
static inline int native_pad_axis(int value) {
    if(value>-12 && value<12)return 0;
    value=value*80/156;
    return value>80?80:value< -80?-80:value;
}
static inline struct NativePad native_pad_map(unsigned held,int cx,int cy,int rx,int ry,int style,int gameplay) {
    struct NativePad p={0,0,0,0,0,0};
    p.move_x=native_pad_axis(cx);p.move_y=native_pad_axis(cy);
    p.x=p.move_x;p.y=p.move_y;
    if(held&EOG_START)p.buttons|=0x1000;
    if(held&(EOG_R|EOG_ZR|EOG_ZL))p.buttons|=0x2000;
    if(held&EOG_L)p.buttons|=0x10;
    if(style>=8 && style<=9) {
        if(!gameplay) {
            if(held&EOG_A)p.buttons|=0x8000; /* accept */
            if(held&EOG_B)p.buttons|=0x4000; /* back */
            if(held&EOG_DUP)p.buttons|=0x800;
            if(held&EOG_DDOWN)p.buttons|=0x400;
            if(held&EOG_DLEFT)p.buttons|=0x200;
            if(held&EOG_DRIGHT)p.buttons|=0x100;
        } else if(style==8) {
            p.x=native_pad_axis(rx);p.y=native_pad_axis(ry);
            if(held&EOG_A){p.buttons|=0x4000;p.actions|=EOG_INTERACT;}
            if(held&EOG_B)p.buttons|=0x8000; /* original weapon cycle */
            if(held&EOG_X)p.actions|=EOG_RELOAD;
            if(held&EOG_Y)p.actions|=EOG_CROUCH;
        } else {
            p.x=((held&EOG_A)?80:0)-((held&EOG_Y)?80:0);
            p.y=((held&EOG_X)?80:0)-((held&EOG_B)?80:0);
            if(held&EOG_DRIGHT){p.buttons|=0x4000;p.actions|=EOG_INTERACT;}
            if(held&EOG_DDOWN)p.buttons|=0x8000;
            if(held&EOG_DUP)p.actions|=EOG_RELOAD;
            if(held&EOG_DLEFT)p.actions|=EOG_CROUCH;
        }
    } else {
        if(held&EOG_A)p.buttons|=0x4000;
        if(held&EOG_B)p.buttons|=0x8000;
        if(held&EOG_DUP)p.buttons|=0x800;
        if(held&EOG_DDOWN)p.buttons|=0x400;
        if(held&EOG_DLEFT)p.buttons|=0x200;
        if(held&EOG_DRIGHT)p.buttons|=0x100;
        if((held&EOG_X)||ry>40)p.buttons|=8;
        if((held&EOG_Y)||ry< -40)p.buttons|=4;
        if(rx< -40)p.buttons|=2;
        if(rx>40)p.buttons|=1;
    }
    return p;
}
#endif
