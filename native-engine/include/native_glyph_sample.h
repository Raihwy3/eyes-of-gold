#ifndef EOG_NATIVE_GLYPH_SAMPLE_H
#define EOG_NATIVE_GLYPH_SAMPLE_H
/* Bilinear coverage at destination pixel centres. Same-size glyphs reproduce
 * the source exactly; no threshold, gain, dilation or colour adjustment. */
static inline unsigned native_glyph_sample(const unsigned char *data,int stride,int width,int height,int x,int y,int out_width,int out_height) {
    if(width<=0 || height<=0 || out_width<=0 || out_height<=0)return 0;
    int sx=((2*x+1)*width*128)/out_width-128;
    int sy=((2*y+1)*height*128)/out_height-128;
    if(sx<0)sx=0;
    if(sy<0)sy=0;
    if(sx>(width-1)*256)sx=(width-1)*256;
    if(sy>(height-1)*256)sy=(height-1)*256;
    int x0=sx/256,y0=sy/256,x1=x0+1<width?x0+1:x0,y1=y0+1<height?y0+1:y0;
    unsigned fx=sx&255,fy=sy&255;
    unsigned top=data[y0*stride+x0]*(256-fx)+data[y0*stride+x1]*fx;
    unsigned bottom=data[y1*stride+x0]*(256-fx)+data[y1*stride+x1]*fx;
    return (top*(256-fy)+bottom*fy+32768)/65536;
}
#endif
