// detect_lettergrid.c — version simple (GTK3)
// 1) Estime la taille d'une case (cell_w, cell_h) via profils lissés.
// 2) Tuilage régulier de la zone de grille et dessin d'un carré centré par case.

#include <gtk/gtk.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

/* ================== utils pixels ================== */
static inline guint8 get_gray(GdkPixbuf *pix,int x,int y){
    int n=gdk_pixbuf_get_n_channels(pix), rs=gdk_pixbuf_get_rowstride(pix);
    guchar *p=gdk_pixbuf_get_pixels(pix)+y*rs+x*n;
    return (p[0]+p[1]+p[2])/3;
}
static inline int clampi(int v,int lo,int hi){ return v<lo?lo:(v>hi?hi:v); }

static void draw_rect(GdkPixbuf *pix,int x0,int y0,int x1,int y1,
                      guint8 R,guint8 G,guint8 B){
    int W=gdk_pixbuf_get_width(pix),H=gdk_pixbuf_get_height(pix);
    int n=gdk_pixbuf_get_n_channels(pix), rs=gdk_pixbuf_get_rowstride(pix);
    guchar *px=gdk_pixbuf_get_pixels(pix);
    x0=clampi(x0,0,W-1); x1=clampi(x1,0,W-1);
    y0=clampi(y0,0,H-1); y1=clampi(y1,0,H-1);
    if(x0>x1||y0>y1) return;
    for(int x=x0;x<=x1;x++){
        guchar *t=px+y0*rs+x*n, *b=px+y1*rs+x*n;
        t[0]=R; t[1]=G; t[2]=B; b[0]=R; b[1]=G; b[2]=B;
    }
    for(int y=y0;y<=y1;y++){
        guchar *l=px+y*rs+x0*n, *r=px+y*rs+x1*n;
        l[0]=R; l[1]=G; l[2]=B; r[0]=R; r[1]=G; r[2]=B;
    }
}
static void draw_rect_thick(GdkPixbuf *pix,int x0,int y0,int x1,int y1,
                            guint8 R,guint8 G,guint8 B,int t){
    for(int k=0;k<t;k++) draw_rect(pix,x0-k,y0-k,x1+k,y1+k,R,G,B);
}

/* ================== profils sur la zone ================== */
static double* col_black_ratio(GdkPixbuf *pix, guint8 thr,
                               int gx0,int gx1,int gy0,int gy1){
    int W=gdk_pixbuf_get_width(pix), H=gdk_pixbuf_get_height(pix);
    gx0=clampi(gx0,0,W-1); gx1=clampi(gx1,0,W-1);
    gy0=clampi(gy0,0,H-1); gy1=clampi(gy1,0,H-1);
    int n=gx1-gx0+1; double *r=malloc(sizeof(double)*n); if(!r) return NULL;
    for(int i=0;i<n;i++){
        int x=gx0+i, black=0, tot=0;
        for(int y=gy0;y<=gy1;y++){ if(get_gray(pix,x,y)<thr) black++; tot++; }
        r[i]=(double)black/(double)tot;
    }
    return r;
}
static double* row_black_ratio(GdkPixbuf *pix, guint8 thr,
                               int gx0,int gx1,int gy0,int gy1){
    int W=gdk_pixbuf_get_width(pix), H=gdk_pixbuf_get_height(pix);
    gx0=clampi(gx0,0,W-1); gx1=clampi(gx1,0,W-1);
    gy0=clampi(gy0,0,H-1); gy1=clampi(gy1,0,H-1);
    int n=gy1-gy0+1; double *r=malloc(sizeof(double)*n); if(!r) return NULL;
    for(int j=0;j<n;j++){
        int y=gy0+j, black=0, tot=0;
        for(int x=gx0;x<=gx1;x++){ if(get_gray(pix,x,y)<thr) black++; tot++; }
        r[j]=(double)black/(double)tot;
    }
    return r;
}

/* lissage boîte (fenêtre impaire recommandée) */
static void smooth_box(double *a, int n, int k){
    if(!a || n<=0 || k<=1) return;
    if(!(k&1)) k++;  // force impaire
    int r=k/2; double acc=0.0;
    for(int i=0;i<=r;i++) acc+=a[i];
    for(int i=0;i<n;i++){
        int L = (i-r<0)?0:(i-r);
        int R = (i+r>=n)?(n-1):(i+r);
        if(i==0){ acc=0.0; for(int j=L;j<=R;j++) acc+=a[j]; }
        else{
            int Lprev = (i-1-r<0)?0:(i-1-r);
            int Rprev = (i-1+r>=n)?(n-1):(i-1+r);
            acc += a[R] - a[Lprev];
        }
        a[i] = acc / (double)(R-L+1);
    }
}

/* Trouve des "composants" au-dessus d'un seuil → centres de pics */
typedef struct { int start, end; } Interval;

static int find_components(const double *arr,int n,double thr,int gap_min,
                           Interval *out){
    int cnt=0,in=0,start=0,last=-1;
    for(int i=0;i<n;i++){
        if(arr[i]>thr){ if(!in){in=1; start=i;} last=i; }
        else if(in && (i-last)>gap_min){ out[cnt++] = (Interval){start,last}; in=0; }
    }
    if(in) out[cnt++] = (Interval){start,last};
    return cnt;
}

/* médiane des gaps entre centres */
static double median_gap_from_intervals(const Interval *iv, int niv){
    if(niv < 2) return 0.0;
    int m = niv - 1;
    double *d = malloc(sizeof(double)*m);
    for(int i=0;i<m;i++){
        int c0 = (iv[i].start + iv[i].end)/2;
        int c1 = (iv[i+1].start + iv[i+1].end)/2;
        d[i] = (double)(c1 - c0);
    }
    for(int i=0;i<m;i++) for(int j=i+1;j<m;j++)
        if(d[j]<d[i]){ double t=d[i]; d[i]=d[j]; d[j]=t; }
    double med = (m%2) ? d[m/2] : 0.5*(d[m/2-1]+d[m/2]);
    free(d);
    return med;
}

/* ================== API ================== */
void detect_letters_in_grid(GdkPixbuf *img, GdkPixbuf *disp,
                            int gx0,int gx1,int gy0,int gy1,
                            guint8 black_thr,
                            guint8 R,guint8 G,guint8 B)
{
    /* 1) Profils limités à la zone de la grille */
    double *vr = col_black_ratio(img, black_thr, gx0,gx1, gy0,gy1);
    double *hr = row_black_ratio(img, black_thr, gx0,gx1, gy0,gy1);
    if(!vr || !hr){ free(vr); free(hr); return; }

    int nX = gx1-gx0+1, nY = gy1-gy0+1;

    /* 2) Lissage (réduit l'influence des lettres) */
    int wv = nX/100; if(wv<3) wv=3; if(!(wv&1)) wv++;
    int wh = nY/100; if(wh<3) wh=3; if(!(wh&1)) wh++;
    smooth_box(vr, nX, wv);
    smooth_box(hr, nY, wh);

    /* 3) Seuil relatif et composants (pics ~ lignes de la grille) */
    double meanX=0, meanY=0;
    for(int i=0;i<nX;i++) meanX+=vr[i]; meanX/=nX;
    for(int j=0;j<nY;j++) meanY+=hr[j]; meanY/=nY;

    /* facteur 1.6: robuste pour isoler des traits foncés */
    double thrX = 1.6*meanX, thrY = 1.6*meanY;

    Interval *ivX = malloc(sizeof(Interval)*nX);
    Interval *ivY = malloc(sizeof(Interval)*nY);
    int nIvX = find_components(vr, nX, thrX, 6, ivX);
    int nIvY = find_components(hr, nY, thrY, 6, ivY);

    /* 4) Estimation de la taille des cases par médiane des écarts */
    double cw_d = median_gap_from_intervals(ivX, nIvX);
    double ch_d = median_gap_from_intervals(ivY, nIvY);
    free(vr); free(hr); free(ivX); free(ivY);

    int cell_w = (int)llround(cw_d);
    int cell_h = (int)llround(ch_d);

    /* garde-fous : fallback si pas assez de pics détectés */
    if(cell_w < 2) cell_w = (nX>0) ? nX/20 : 8;  // valeur douce
    if(cell_h < 2) cell_h = (nY>0) ? nY/20 : 8;

    if(cell_w < 2) cell_w = 8;
    if(cell_h < 2) cell_h = 8;

    /* 5) Tuilage régulier de la zone et dessin */
    int gridW = gx1 - gx0 + 1;
    int gridH = gy1 - gy0 + 1;

    int cols = (int)llround((double)gridW / (double)cell_w);
    int rows = (int)llround((double)gridH / (double)cell_h);
    if(cols < 1) cols = 1;
    if(rows < 1) rows = 1;

    /* pas effectif pour couvrir EXACTEMENT la zone */
    double stepX = (double)gridW / (double)cols;
    double stepY = (double)gridH / (double)rows;

    const int border_margin = 2;     // rester à l’intérieur du cadre rouge/vert
    const double inner_frac  = 0.55; // carré “lettre” = 55% du côté

    for(int r=0; r<rows; r++){
        int y0 = (int)llround(gy0 + r*stepY)     + border_margin;
        int y1 = (int)llround(gy0 + (r+1)*stepY) - 1 - border_margin;
        if(y1 - y0 < 3) continue;

        for(int c=0; c<cols; c++){
            int x0 = (int)llround(gx0 + c*stepX)     + border_margin;
            int x1 = (int)llround(gx0 + (c+1)*stepX) - 1 - border_margin;
            if(x1 - x0 < 3) continue;

            /* cadre de case (couleur R,G,B passée par l'appelant) */
            draw_rect_thick(disp, x0,y0,x1,y1, R,G,B,2);

            /* carré bleu centré (zone lettre) */
            int cw = x1 - x0 + 1, ch = y1 - y0 + 1;
            int side = (int)llround(inner_frac * (cw < ch ? cw : ch));
            int cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
            int ix0 = cx - side/2, iy0 = cy - side/2;
            int ix1 = ix0 + side - 1, iy1 = iy0 + side - 1;
            draw_rect_thick(disp, ix0,iy0,ix1,iy1, 0,128,255,2);
        }
    }

    /* (facultatif) log de debug
    fprintf(stderr,"[grid] cell_w=%d, cell_h=%d, cols=%d, rows=%d\n",
            cell_w, cell_h, cols, rows);
    */
}

