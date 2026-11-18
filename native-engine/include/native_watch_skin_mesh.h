#ifndef EOG_NATIVE_WATCH_SKIN_MESH_H
#define EOG_NATIVE_WATCH_SKIN_MESH_H
/* Local retail lengths/pivots; custom profiles share a subdued neutral finish.
 * Only the seconds hand is coloured. No extra dial marks or painted inlays. */
struct NativeWatchHand {short x[8],z[8];unsigned count,colour;};
static inline struct NativeWatchHand native_watch_hand(int skin,int hand,int inset) {
    struct NativeWatchHand p={{0},{0},0,0};
    if(skin<1 || skin>4 || hand<0 || hand>2 || inset)return p;
    if(hand==2){
        p.colour=skin==4?0x9c7b32ff:skin==1?0x783c18ff:skin==2?0x725e35ff:0x782521ff;
        p.count=3;
        p.x[0]=-3;p.z[0]=6;p.x[1]=0;p.z[1]=-121;p.x[2]=3;p.z[2]=6;
        return p;
    }
    int length=hand==0?60:120;
    p.colour=0x777777ff;
    if(skin==2){
        /* Dress: a slender, symmetric dauphine profile. */
        int width=hand==0?4:3;
        p.count=4;
        p.x[0]=0;p.z[0]=10;
        p.x[1]=-width;p.z[1]=-length/3;
        p.x[2]=0;p.z[2]=-length;
        p.x[3]=width;p.z[3]=p.z[1];
    }else{
        /* Field: straight baton with a short point. Tactical: narrow-rooted
         * sword with restrained shoulders and a longer point. */
        int width=(skin==1 || skin==4)?(hand==0?3:2):(hand==0?4:3);
        int root=(skin==1 || skin==4)?width:1;
        p.count=5;
        p.x[0]=-root;p.z[0]=10;
        p.x[1]=-width;p.z[1]=-length+((skin==1 || skin==4)?6:18);
        p.x[2]=0;p.z[2]=-length;
        p.x[3]=width;p.z[3]=p.z[1];
        p.x[4]=root;p.z[4]=10;
    }
    return p;
}
#endif
