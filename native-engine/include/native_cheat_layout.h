#ifndef EOG_NATIVE_CHEAT_LAYOUT_H
#define EOG_NATIVE_CHEAT_LAYOUT_H
/* Original dossier coordinates. Keep the full list above the cursor's lower
 * boundary, using the same rows for drawing, pointing and bottom-screen input. */
/* The seed is wider than On/Off. Fit the measured six-digit value inside
 * each dossier column, keeping the original positions for other values. */
static inline int native_cheat_value_x(int second_column,int seed,int width){
    return seed?(second_column?364:200)-width:second_column?344:179;
}
static inline int native_cheat_split(int count){return count>24?(count+1)/2:12;}
static inline int native_cheat_pitch(int count){
    int split=native_cheat_split(count),rows=count<split?count:split;
    if(count-split>rows)rows=count-split;
    return rows>12?220/(rows-1):20;
}
static inline int native_cheat_row_y(int row,int count){return 53+row*native_cheat_pitch(count);}
static inline int native_cheat_hit(int count,int x,int y){
    int split=native_cheat_split(count),start=(count>split && x>=220)?split:0;
    int rows=start?count-split:(count<split?count:split),selected=start;
    for(int row=1;row<rows;row++)if(y>=native_cheat_row_y(row,count))selected=start+row;
    return selected;
}
#endif
