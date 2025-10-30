// detect_letterinword.c — GTK3 (1 bbox par lettre dans la liste de mots)
#include <gtk/gtk.h>
#include <stdlib.h>

static inline guint8 get_gray(GdkPixbuf *pix, int x, int y){
    int n=gdk_pixbuf_get_n_channels(pix), rs=gdk_pixbuf_get_rowstride(pix);
    guchar *p=gdk_pixbuf_get_pixels(pix)+y*rs+x*n;
    return (p[0]+p[1]+p[2])/3;
}
static inline int clampi(int v,int lo,int hi){ return v<lo?lo:(v>hi?hi:v); }
static void draw_rect(GdkPixbuf *pix,int x0,int y0,int x1,int y1,guint8 R,guint8 G,guint8 B){
    int W=gdk_pixbuf_get_width(pix),H=gdk_pixbuf_get_height(pix);
    int n=gdk_pixbuf_get_n_channels(pix), rs=gdk_pixbuf_get_rowstride(pix);
    guchar *px=gdk_pixbuf_get_pixels(pix);
    x0=clampi(x0,0,W-1); x1=clampi(x1,0,W-1); y0=clampi(y0,0,H-1); y1=clampi(y1,0,H-1);
    if(x0>x1||y0>y1) return;
    for(int x=x0;x<=x1;x++){ guchar *t=px+y0*rs+x*n,*b=px+y1*rs+x*n; t[0]=R;t[1]=G;t[2]=B; b[0]=R;b[1]=G;b[2]=B; }
    for(int y=y0;y<=y1;y++){ guchar *l=px+y*rs+x0*n,*r=px+y*rs+x1*n; l[0]=R;l[1]=G;l[2]=B; r[0]=R;r[1]=G;r[2]=B; }
}
static void draw_rect_thick(GdkPixbuf *pix,int x0,int y0,int x1,int y1,guint8 R,guint8 G,guint8 B,int t){
    for(int k=0;k<t;k++) draw_rect(pix,x0-k,y0-k,x1+k,y1+k,R,G,B);
}

/* ratio encre par ligne dans [x0,x1] */
static double* row_ratio_band(GdkPixbuf *pix,guint8 thr,int x0,int x1){
    int W=gdk_pixbuf_get_width(pix),H=gdk_pixbuf_get_height(pix);
    x0=clampi(x0,0,W-1); x1=clampi(x1,0,W-1);
    double *r=malloc(sizeof(double)*H); if(!r) return NULL;
    for(int y=0;y<H;y++){ int black=0,tot=0; for(int x=x0;x<=x1;x++){ if(get_gray(pix,x, y)<thr) black++; tot++; } r[y]=(double)black/(double)tot; }
    return r;
}
static int ink_bbox(GdkPixbuf *pix,int x0,int y0,int x1,int y1,guint8 thr,int *rx0,int *ry0,int *rx1,int *ry1){
    int W=gdk_pixbuf_get_width(pix),H=gdk_pixbuf_get_height(pix);
    x0=clampi(x0,0,W-1); x1=clampi(x1,0,W-1); y0=clampi(y0,0,H-1); y1=clampi(y1,0,H-1);
    int found=0,minx=x1,maxx=x0,miny=y1,maxy=y0;
    for(int y=y0;y<=y1;y++) for(int x=x0;x<=x1;x++) if(get_gray(pix,x,y)<thr){
        if(!found){found=1;minx=maxx=x;miny=maxy=y;}
        else{ if(x<minx)minx=x; if(x>maxx)maxx=x; if(y<miny)miny=y; if(y>maxy)maxy=y; }
    }
    if(!found) return 0;
    *rx0=minx;*ry0=miny;*rx1=maxx;*ry1=maxy; return 1;
}

/* --- API --- */
void detect_letters_in_words(GdkPixbuf *img, GdkPixbuf *disp,
                             int wx0,int wx1,int wy0,int wy1,
                             guint8 black_thr,
                             guint8 R,guint8 G,guint8 B)
{
    /* 1) bandes horizontales (une par mot) */
    double *row=row_ratio_band(img, black_thr, wx0, wx1); if(!row) return;
    int in=0, ystart=0, last=-1e9;
    for(int y=wy0;y<=wy1;y++){
        if(row[y]>0.05){ if(!in){in=1; ystart=y;} last=y; }
        else if(in && (y-last)>10){
            int y0=ystart, y1=last; in=0;
            if(y1-y0+1<6) continue;

            /* 2) colonne d’encre sur la bande → segmentation par colonnes blanches */
            int cw = wx1-wx0+1;
            double *col = (double*)malloc(sizeof(double)*cw); if(!col) continue;
            for(int x=0;x<cw;x++){
                int black=0,tot=0; for(int yy=y0;yy<=y1;yy++){ if(get_gray(img, wx0+x, yy)<black_thr) black++; tot++; }
                col[x] = (double)black/(double)tot;
            }
            int cx_in=0, cx0=0, cx_last=-1e9;
            for(int x=0;x<cw;x++){
                double cr = col[x];
                if(cr>0.08){ if(!cx_in){cx_in=1; cx0=x;} cx_last=x; }
                else if(cx_in && (x-cx_last)>2){
                    int X0 = wx0+cx0, X1 = wx0+cx_last;
                    int rx0,ry0,rx1,ry1;
                    if((X1-X0)>=3 && ink_bbox(img, X0,y0, X1,y1, black_thr, &rx0,&ry0,&rx1,&ry1))
                        draw_rect_thick(disp, rx0,ry0,rx1,ry1, R,G,B,2);
                    cx_in=0;
                }
            }
            if(cx_in){
                int X0 = wx0+cx0, X1 = wx0+cx_last;
                int rx0,ry0,rx1,ry1;
                if((X1-X0)>=3 && ink_bbox(img, X0,y0, X1,y1, black_thr, &rx0,&ry0,&rx1,&ry1))
                    draw_rect_thick(disp, rx0,ry0,rx1,ry1, R,G,B,2);
            }
            free(col);
        }
    }
    free(row);
}

