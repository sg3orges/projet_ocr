// detect_lettergrid.c — GTK3 (simple + autocorr + export PNG de chaque case)

#include <gtk/gtk.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

/* ============== utils pixels & dessin ============== */
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

/* ============== profils sur la zone ============== */
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

/* lissage boîte (fenêtre impaire) */
static void smooth_box(double *a, int n, int k){
    if(!a || n<=0 || k<=1) return;
    if(!(k&1)) k++;
    int r=k/2; double acc=0.0;
    for(int i=0;i<=r && i<n;i++) acc+=a[i];
    for(int i=0;i<n;i++){
        int L=(i-r<0)?0:(i-r);
        int R=(i+r>=n)?(n-1):(i+r);
        if(i==0){ acc=0.0; for(int j=L;j<=R;j++) acc+=a[j]; }
        else{
            int Lprev=(i-1-r<0)?0:(i-1-r);
            int Rprev=(i-1+r>=n)?(n-1):(i-1+r);
            acc += a[R]-a[Lprev];
        }
        a[i]=acc/(double)(R-L+1);
    }
}

/* ============== estimation du pas par autocorr ============== */
static int best_lag_autocorr(const double *p, int n, int lag_min, int lag_max){
    if(n<=0 || lag_min>=lag_max) return 0;
    double mean=0.0; for(int i=0;i<n;i++) mean+=p[i]; mean/= (n>0?n:1);
    double best=-1e300; int bestLag=lag_min;
    for(int k=lag_min; k<=lag_max; k++){
        double acc=0.0;
        for(int i=0; i+k<n; i++){
            double a = p[i]-mean;
            double b = p[i+k]-mean;
            acc += a*b;
        }
        if(acc > best){ best = acc; bestLag = k; }
    }
    return bestLag;
}
static void lag_bounds_from_extent(int extent, int *lag_min, int *lag_max){
    int mn = extent/80;  if(mn < 4) mn = 4;           // >= ~4 px
    int mx = extent/10;  if(mx < mn+4) mx = mn+4;     // marge
    if(mx > 200) mx = 200;                            // borne haute sûre
    *lag_min = mn; *lag_max = mx;
}

/* ============== mkdir si nécessaire ============== */
static int ensure_dir(const char *path){
    struct stat st;
    if(stat(path,&st)==0) return S_ISDIR(st.st_mode)?0:-1;
    return mkdir(path,0755);
}

/* ============== API principale ============== */
void detect_letters_in_grid(GdkPixbuf *img, GdkPixbuf *disp,
                            int gx0,int gx1,int gy0,int gy1,
                            guint8 black_thr,
                            guint8 R,guint8 G,guint8 B)
{
    /* 1) Profils sur la zone de grille */
    double *vr = col_black_ratio(img, black_thr, gx0,gx1, gy0,gy1);
    double *hr = row_black_ratio(img, black_thr, gx0,gx1, gy0,gy1);
    if(!vr || !hr){ free(vr); free(hr); return; }

    int nX = gx1-gx0+1, nY = gy1-gy0+1;

    /* 2) Lissage pour gommer les lettres */
    int wv = nX/100; if(wv<5) wv=5; if(!(wv&1)) wv++;
    int wh = nY/100; if(wh<5) wh=5; if(!(wh&1)) wh++;
    smooth_box(vr, nX, wv);
    smooth_box(hr, nY, wh);

    /* 3) Estimation du pas (taille de case) par autocorr */
    int lag_min_x, lag_max_x, lag_min_y, lag_max_y;
    lag_bounds_from_extent(nX, &lag_min_x, &lag_max_x);
    lag_bounds_from_extent(nY, &lag_min_y, &lag_max_y);

    int cell_w = best_lag_autocorr(vr, nX, lag_min_x, lag_max_x);
    int cell_h = best_lag_autocorr(hr, nY, lag_min_y, lag_max_y);

    free(vr); free(hr);

    /* garde-fous */
    if(cell_w < lag_min_x) cell_w = lag_min_x;
    if(cell_h < lag_min_y) cell_h = lag_min_y;
    if(cell_w > lag_max_x) cell_w = lag_max_x;
    if(cell_h > lag_max_y) cell_h = lag_max_y;

    /* 4) Quadrillage régulier et export */
    int gridW = gx1 - gx0 + 1;
    int gridH = gy1 - gy0 + 1;
    int cols = (cell_w>0) ? (int)llround((double)gridW / (double)cell_w) : 0;
    int rows = (cell_h>0) ? (int)llround((double)gridH / (double)cell_h) : 0;
    if(cols < 1) cols = 1;
    if(rows < 1) rows = 1;

    /* pas effectif: force le pavage exact de la zone */
    double stepX = (double)gridW / (double)cols;
    double stepY = (double)gridH / (double)rows;

    const int border_margin = 2;     // rester à l’intérieur du cadre rouge/vert
    const char *outdir = "cells";

    if(ensure_dir(outdir) != 0){
        fprintf(stderr, "[warn] impossible de créer le dossier '%s'\n", outdir);
    }

    int Wsrc = gdk_pixbuf_get_width(img);
    int Hsrc = gdk_pixbuf_get_height(img);

    for(int r=0; r<rows; r++){
        int y0 = (int)llround(gy0 + r*stepY)     + border_margin;
        int y1 = (int)llround(gy0 + (r+1)*stepY) - 1 - border_margin;

        for(int c=0; c<cols; c++){
            int x0 = (int)llround(gx0 + c*stepX)     + border_margin;
            int x1 = (int)llround(gx0 + (c+1)*stepX) - 1 - border_margin;

            /* clamp + filtrage de cas dégénérés */
            if(x0 < 0) x0 = 0; if(y0 < 0) y0 = 0;
            if(x1 >= Wsrc) x1 = Wsrc-1; if(y1 >= Hsrc) y1 = Hsrc-1;
            if(x1 - x0 + 1 < 6 || y1 - y0 + 1 < 6) continue;

            /* dessine le grand cadre bleu sur l'image d'affichage */
            draw_rect_thick(disp, x0,y0,x1,y1, R,G,B,2);

            /* export PNG de la case (vue recadrée, pas de souci d'alpha) */
            int w = x1 - x0 + 1, h = y1 - y0 + 1;
            GdkPixbuf *sub = gdk_pixbuf_new_subpixbuf(img, x0, y0, w, h);

            char path[512];
            snprintf(path, sizeof(path), "%s/cell_r%03d_c%03d.png", outdir, r, c);

            GError *err = NULL;
            if(!gdk_pixbuf_save(sub, path, "png", &err, NULL)){
                fprintf(stderr, "[warn] sauvegarde échouée: %s (%s)\n",
                        path, err ? err->message : "unknown");
                if(err) g_error_free(err);
            }
            g_object_unref(sub);
        }
    }

    /* debug optionnel
    fprintf(stderr,"[grid] cell_w=%d, cell_h=%d, cols=%d, rows=%d\n",
            cell_w, cell_h, cols, rows);
    */
}

