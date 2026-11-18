#ifndef EOG_NATIVE_PAD_H
#define EOG_NATIVE_PAD_H
/* HID bit positions from libctru, kept independent of its ABI for host tests. */
enum { EOG_A=1u<<0,EOG_B=1u<<1,EOG_SELECT=1u<<2,EOG_START=1u<<3,
       EOG_DRIGHT=1u<<4,EOG_DLEFT=1u<<5,EOG_DUP=1u<<6,EOG_DDOWN=1u<<7,
       EOG_R=1u<<8,EOG_L=1u<<9,EOG_X=1u<<10,EOG_Y=1u<<11,EOG_ZL=1u<<14,EOG_ZR=1u<<15 };
enum { EOG_RELOAD=1,EOG_CROUCH=2,EOG_INTERACT=4 };
enum {
    EOG_BIND_NONE=0,EOG_BIND_A=1,EOG_BIND_B=2,EOG_BIND_X=3,EOG_BIND_Y=4,
    EOG_BIND_L=5,EOG_BIND_R=6,EOG_BIND_ZL=7,EOG_BIND_ZR=8,
    EOG_BIND_DUP=9,EOG_BIND_DDOWN=10,EOG_BIND_DLEFT=11,EOG_BIND_DRIGHT=12,
    EOG_BIND_SELECT=13,EOG_BIND_COUNT=14
};
enum {
    EOG_STICKS_NORMAL=0,EOG_STICKS_SOUTHPAW=1,
    EOG_STICKS_MOVE_ONLY=2,EOG_STICKS_LOOK_ONLY=3
};
enum { EOG_DIGITAL_OFF=0,EOG_DIGITAL_DPAD=1,EOG_DIGITAL_FACE=2 };
struct NativeCustomBinds {
    int sticks,fire,aim,action,weapon,reload,crouch,digital_aim,digital_move;
};
struct NativePad { unsigned buttons,actions;int x,y,move_x,move_y; };
static inline int native_pad_axis(int value) {
    if(value>-12 && value<12)return 0;
    value=value*80/156;
    return value>80?80:value< -80?-80:value;
}
static inline int native_pad_clamp80(int v) {
    return v>80?80:v< -80?-80:v;
}
static inline unsigned native_pad_bind_mask(int bind) {
    static const unsigned masks[EOG_BIND_COUNT]={
        0u,EOG_A,EOG_B,EOG_X,EOG_Y,EOG_L,EOG_R,EOG_ZL,EOG_ZR,
        EOG_DUP,EOG_DDOWN,EOG_DLEFT,EOG_DRIGHT,EOG_SELECT
    };
    return bind>=0 && bind<EOG_BIND_COUNT?masks[bind]:0u;
}
static inline struct NativeCustomBinds native_pad_custom_defaults(void) {
    struct NativeCustomBinds b={
        EOG_STICKS_NORMAL,EOG_BIND_R,EOG_BIND_L,EOG_BIND_A,
        EOG_BIND_B,EOG_BIND_X,EOG_BIND_Y,EOG_DIGITAL_OFF,EOG_DIGITAL_OFF
    };
    return b;
}
static inline struct NativePad native_pad_map_custom(unsigned held,int cx,int cy,int rx,int ry,int style,int gameplay,const struct NativeCustomBinds *binds) {
    struct NativePad p={0,0,0,0,0,0};
    p.move_x=native_pad_axis(cx);p.move_y=native_pad_axis(cy);
    p.x=p.move_x;p.y=p.move_y;
    if(held&EOG_START)p.buttons|=0x1000;
    if(style==10) {
        if(!gameplay) {
            if(held&EOG_A)p.buttons|=0x8000;
            if(held&EOG_B)p.buttons|=0x4000;
            if(held&EOG_DUP)p.buttons|=0x800;
            if(held&EOG_DDOWN)p.buttons|=0x400;
            if(held&EOG_DLEFT)p.buttons|=0x200;
            if(held&EOG_DRIGHT)p.buttons|=0x100;
            return p;
        }
        struct NativeCustomBinds b=binds?*binds:native_pad_custom_defaults();
        if(b.sticks==EOG_STICKS_SOUTHPAW) {
            p.move_x=native_pad_axis(rx);p.move_y=native_pad_axis(ry);
            p.x=native_pad_axis(cx);p.y=native_pad_axis(cy);
        } else if(b.sticks==EOG_STICKS_MOVE_ONLY) {
            p.x=0;p.y=0;
        } else if(b.sticks==EOG_STICKS_LOOK_ONLY) {
            p.x=p.move_x;p.y=p.move_y;p.move_x=0;p.move_y=0;
        } else {
            p.x=native_pad_axis(rx);p.y=native_pad_axis(ry);
        }
        if(b.digital_move==EOG_DIGITAL_DPAD) {
            p.move_x=native_pad_clamp80(p.move_x+((held&EOG_DRIGHT)?80:0)-((held&EOG_DLEFT)?80:0));
            p.move_y=native_pad_clamp80(p.move_y+((held&EOG_DUP)?80:0)-((held&EOG_DDOWN)?80:0));
        } else if(b.digital_move==EOG_DIGITAL_FACE) {
            p.move_x=native_pad_clamp80(p.move_x+((held&EOG_A)?80:0)-((held&EOG_Y)?80:0));
            p.move_y=native_pad_clamp80(p.move_y+((held&EOG_X)?80:0)-((held&EOG_B)?80:0));
        }
        if(b.digital_aim==EOG_DIGITAL_DPAD) {
            p.x=native_pad_clamp80(p.x+((held&EOG_DRIGHT)?80:0)-((held&EOG_DLEFT)?80:0));
            p.y=native_pad_clamp80(p.y+((held&EOG_DUP)?80:0)-((held&EOG_DDOWN)?80:0));
        } else if(b.digital_aim==EOG_DIGITAL_FACE) {
            p.x=native_pad_clamp80(p.x+((held&EOG_A)?80:0)-((held&EOG_Y)?80:0));
            p.y=native_pad_clamp80(p.y+((held&EOG_X)?80:0)-((held&EOG_B)?80:0));
        }
        if(held&native_pad_bind_mask(b.fire))p.buttons|=0x2000;
        if(held&native_pad_bind_mask(b.aim))p.buttons|=0x10;
        if(held&native_pad_bind_mask(b.action)){p.buttons|=0x4000;p.actions|=EOG_INTERACT;}
        if(held&native_pad_bind_mask(b.weapon))p.buttons|=0x8000;
        if(held&native_pad_bind_mask(b.reload))p.actions|=EOG_RELOAD;
        if(held&native_pad_bind_mask(b.crouch))p.actions|=EOG_CROUCH;
        return p;
    }
    if(held&(EOG_R|EOG_ZR|EOG_ZL))p.buttons|=0x2000;
    if(held&EOG_L)p.buttons|=0x10;
    if(style>=8 && style<=11 && style!=10) {
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
        } else if(style==9) {
            p.x=((held&EOG_A)?80:0)-((held&EOG_Y)?80:0);
            p.y=((held&EOG_X)?80:0)-((held&EOG_B)?80:0);
            if(held&EOG_DRIGHT){p.buttons|=0x4000;p.actions|=EOG_INTERACT;}
            if(held&EOG_DDOWN)p.buttons|=0x8000;
            if(held&EOG_DUP)p.actions|=EOG_RELOAD;
            if(held&EOG_DLEFT)p.actions|=EOG_CROUCH;
        } else { /* style==11: 1.8 Natalya — Circle Pad aim, X/B/Y/A move, D-Pad actions */
            p.move_x=((held&EOG_A)?80:0)-((held&EOG_Y)?80:0);
            p.move_y=((held&EOG_X)?80:0)-((held&EOG_B)?80:0);
            p.x=native_pad_axis(cx);p.y=native_pad_axis(cy);
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
static inline void native_pad_apply_swap(struct NativePad *p,int style,int gameplay,int swap_sticks) {
    if(!p || !swap_sticks || !gameplay || style<8)return;
    int tx=p->move_x,ty=p->move_y;
    p->move_x=p->x;p->move_y=p->y;
    p->x=tx;p->y=ty;
}
static inline struct NativePad native_pad_map(unsigned held,int cx,int cy,int rx,int ry,int style,int gameplay) {
    return native_pad_map_custom(held,cx,cy,rx,ry,style,gameplay,0);
}
#endif
