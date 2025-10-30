// detection2.c — GTK3 (trouve les 2 blocs + affiche + appelle les modules)
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>

/* ==== Prototypes des 2 modules ==== */
void detect_letters_in_words(GdkPixbuf *img, GdkPixbuf *disp,
                             int wx0,int wx1,int wy0,int wy1,
                             guint8 black_thresh,
                             guint8 R,guint8 G,guint8 B);

void detect_letters_in_grid(GdkPixbuf *img, GdkPixbuf *disp,
                            int gx0,int gx1,int gy0,int gy1,
                            guint8 black_thresh,
                            guint8 R,guint8 G,guint8 B);

/* ================= Utils image ================= */
static inline int clampi(int v, int lo, int hi){ return v<lo?lo:(v>hi?hi:v); }

static inline guint8 get_gray(GdkPixbuf *pix, int x, int y){
    int n  = gdk_pixbuf_get_n_channels(pix);
    int rs = gdk_pixbuf_get_rowstride(pix);
    guchar *p = gdk_pixbuf_get_pixels(pix) + y*rs + x*n;
    return (p[0] + p[1] + p[2]) / 3;
}

static void draw_rect(GdkPixbuf *pix, int x0,int y0,int x1,int y1,
                      guint8 R,guint8 G,guint8 B){
    int W=gdk_pixbuf_get_width(pix), H=gdk_pixbuf_get_height(pix);
    int n=gdk_pixbuf_get_n_channels(pix), rs=gdk_pixbuf_get_rowstride(pix);
    guchar *px=gdk_pixbuf_get_pixels(pix);
    x0=clampi(x0,0,W-1); x1=clampi(x1,0,W-1); y0=clampi(y0,0,H-1); y1=clampi(y1,0,H-1);
    if(x0>x1||y0>y1) return;
    for(int x=x0;x<=x1;++x){
        guchar *t=px+y0*rs+x*n, *b=px+y1*rs+x*n;
        t[0]=R; t[1]=G; t[2]=B; b[0]=R; b[1]=G; b[2]=B;
    }
    for(int y=y0;y<=y1;++y){
        guchar *l=px+y*rs+x0*n, *r=px+y*rs+x1*n;
        l[0]=R; l[1]=G; l[2]=B; r[0]=R; r[1]=G; r[2]=B;
    }
}
static void draw_rect_thick(GdkPixbuf *pix,int x0,int y0,int x1,int y1,
                            guint8 R,guint8 G,guint8 B,int t){
    for(int k=0;k<t;k++) draw_rect(pix, x0-k,y0-k,x1+k,y1+k, R,G,B);
}
static GdkPixbuf* clone_to_rgba32(GdkPixbuf *src){
    int W=gdk_pixbuf_get_width(src), H=gdk_pixbuf_get_height(src);
    GdkPixbuf *dst=gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, W, H);
    if(!dst) return NULL;
    gdk_pixbuf_copy_area(src,0,0,W,H,dst,0,0);
    return dst;
}

/* ====== profils 1D ====== */
typedef struct { int start, end; } Interval;

static double* compute_col_black_ratio(GdkPixbuf *pix, guint8 thr){
    int W=gdk_pixbuf_get_width(pix), H=gdk_pixbuf_get_height(pix);
    double *r=malloc(sizeof(double)*W); if(!r) return NULL;
    for(int x=0;x<W;x++){
        int black=0; for(int y=0;y<H;y++) if(get_gray(pix,x,y)<thr) black++;
        r[x]=(double)black/(double)H;
    }
    return r;
}
static double* compute_row_black_ratio(GdkPixbuf *pix, guint8 thr, int xmin,int xmax){
    int W=gdk_pixbuf_get_width(pix), H=gdk_pixbuf_get_height(pix);
    xmin=clampi(xmin,0,W-1); xmax=clampi(xmax,0,W-1);
    double *r=malloc(sizeof(double)*H); if(!r) return NULL;
    for(int y=0;y<H;y++){
        int black=0,tot=0; for(int x=xmin;x<=xmax;x++){ if(get_gray(pix,x,y)<thr) black++; tot++; }
        r[y]=(double)black/(double)tot;
    }
    return r;
}
static void smooth_box(double *a, int n, int k){
    if(!a || n<=0 || k<=1) return; int r=k/2; double acc=0.0;
    int L=0, R=(r<n? r:n-1); for(int j=L;j<=R;j++) acc+=a[j];
    for(int i=0;i<n;i++){
        L=(i-r<0)?0:(i-r); R=(i+r>=n)?(n-1):(i+r);
        if(i==0){ acc=0.0; for(int j=L;j<=R;j++) acc+=a[j]; }
        else{
            int Lprev=(i-1-r<0)?0:(i-1-r);
            int Rprev=(i-1+r>=n)?(n-1):(i-1+r);
            acc += a[R]-a[Lprev];
        }
        a[i]=acc/(double)(R-L+1);
    }
}
static Interval* find_vertical_components(const double *arr,int n,double thr,int gap,int *out_n){
    Interval *comps=malloc(sizeof(Interval)*n); if(!comps){*out_n=0;return NULL;}
    int cnt=0,in=0,start=0,last=-1000000;
    for(int x=0;x<n;x++){
        if(arr[x]>thr){ if(!in){in=1;start=x;} last=x; }
        else if(in && (x-last)>gap){ comps[cnt++] = (Interval){start,last}; in=0; }
    }
    if(in) comps[cnt++] = (Interval){start,last};
    *out_n=cnt; return comps;
}

static void find_vertical_bounds(GdkPixbuf *pix, guint8 thr,
                                 int xmin,int xmax,double row_thr,
                                 int *ymin,int *ymax){
    double *rr=compute_row_black_ratio(pix,thr,xmin,xmax);
    int H=gdk_pixbuf_get_height(pix); if(!rr){*ymin=0;*ymax=H-1;return;}
    int y0=-1,y1=-1;
    for(int y=0;y<H;y++){ if(rr[y]>row_thr){ if(y0<0) y0=y; y1=y; } }
    free(rr);
    if(y0<0){ y0=0; y1=H-1; }
    *ymin=y0; *ymax=y1;
}

/* ===================== Pipeline ====================== */
static void run_detection_and_show(GtkWidget *win, const char *path){
    GError *err=NULL;
    GdkPixbuf *src=gdk_pixbuf_new_from_file(path,&err);
    if(!src){ g_printerr("Chargement image: %s\n", err->message); g_clear_error(&err); return; }
    GdkPixbuf *disp=clone_to_rgba32(src); if(!disp){ g_object_unref(src); return; }

    const guint8 BLACK_T=160;
    int W=gdk_pixbuf_get_width(src), H=gdk_pixbuf_get_height(src);

    /* 1) profil colonnes + lissage pour regrouper les zones massives */
    double *cr=compute_col_black_ratio(src,BLACK_T); if(!cr){ g_object_unref(src); g_object_unref(disp); return; }
    int smooth_win = W/40; if(smooth_win<9) smooth_win=9; if(smooth_win%2==0) smooth_win++;
    smooth_box(cr, W, smooth_win);

    /* 2) composants avec gap adaptatif + filtrage largeur mini */
    int ncomp=0, gap = W/25; if(gap<8) gap=8;
    Interval *comps=find_vertical_components(cr,W, 0.04, gap, &ncomp); free(cr);
    if(!comps || ncomp==0){
        GtkWidget *img=gtk_image_new_from_pixbuf(disp);
        gtk_container_add(GTK_CONTAINER(win), img);
        gtk_widget_show_all(win);
        if(comps) free(comps); g_object_unref(src); g_object_unref(disp); return;
    }
    int min_w = W/10;
    for(int i=0;i<ncomp;i++){ int w=comps[i].end-comps[i].start+1; if(w<min_w){ comps[i].start=comps[i].end=-1; } }

    /* 3) sélectionner les 2 plus larges, ordonnées gauche/droite */
    int iA=-1,iB=-1, wA=0,wB=0;
    for(int i=0;i<ncomp;i++){
        if(comps[i].start<0) continue;
        int w=comps[i].end-comps[i].start+1;
        if(w>wA){ wB=wA; iB=iA; wA=w; iA=i; }
        else if(w>wB){ wB=w; iB=i; }
    }
    if(iA<0) iA=0; if(iB<0) iB=(ncomp>=2?1:0);
    Interval left=comps[iA], right=comps[iB];
    if(left.start>right.start){ Interval t=left; left=right; right=t; }
    free(comps);

    /* 4) bornes verticales (avec marge pour ne rien couper côté mots) */
    int wx0=left.start,  wx1=left.end;
    int gx0=right.start, gx1=right.end;
    int wy0,wy1, gy0,gy1;
    find_vertical_bounds(src, BLACK_T, wx0,wx1, 0.04, &wy0,&wy1);
    find_vertical_bounds(src, BLACK_T, gx0,gx1, 0.04, &gy0,&gy1);

    /* marges */
    const int WORDS_MARGIN_Y=18, WORDS_MARGIN_X=6;
    wy0 = (wy0 - WORDS_MARGIN_Y < 0) ? 0 : wy0 - WORDS_MARGIN_Y;
    wy1 = (wy1 + WORDS_MARGIN_Y > H-1) ? H-1 : wy1 + WORDS_MARGIN_Y;
    wx0 = (wx0 - WORDS_MARGIN_X < 0) ? 0 : wx0 - WORDS_MARGIN_X;
    wx1 = (wx1 + WORDS_MARGIN_X > W-1) ? W-1 : wx1 + WORDS_MARGIN_X;

    /* affichage console */
    printf("ZONE MOTS  : x=[%d,%d], y=[%d,%d]\n", wx0,wx1, wy0,wy1);
    printf("ZONE GRILLE: x=[%d,%d], y=[%d,%d]\n", gx0,gx1, gy0,gy1);

    /* cadres de zones */
    draw_rect_thick(disp, wx0,wy0,wx1,wy1,  0,255,0, 2);   // mots (vert)
    draw_rect_thick(disp, gx0,gy0,gx1,gy1, 255,  0,0, 2);  // grille (rouge)

    /* 5) détection lettre par lettre */
    detect_letters_in_words(src, disp, wx0,wx1,wy0,wy1, BLACK_T, 0,128,255);
    detect_letters_in_grid (src, disp, gx0,gx1,gy0,gy1, BLACK_T, 0,128,255);

    /* 6) affichage */
    GtkWidget *img=gtk_image_new_from_pixbuf(disp);
    gtk_container_add(GTK_CONTAINER(win), img);
    gtk_widget_show_all(win);

    g_object_unref(src);
    g_object_unref(disp);
}

/* ============================== GTK ============================== */
static void on_open(GApplication *app, GFile **files, int n_files, const char *hint){
    GtkWidget *win=gtk_application_window_new(GTK_APPLICATION(app));
    gtk_window_set_title(GTK_WINDOW(win), "Détection GTK3");
    gtk_window_set_default_size(GTK_WINDOW(win), 1100, 800);
    char *path=g_file_get_path(files[0]);
    run_detection_and_show(win, path);
    g_free(path);
}
static void on_activate(GtkApplication *app){
    GtkWidget *win=gtk_application_window_new(GTK_APPLICATION(app));
    gtk_window_set_title(GTK_WINDOW(win), "Détection GTK3");
    gtk_window_set_default_size(GTK_WINDOW(win), 1100, 800);
    run_detection_and_show(win, "level_1_image_1_bw.bmp");
}
int main(int argc, char **argv){
    GtkApplication *app=gtk_application_new("com.projet.ocr", G_APPLICATION_HANDLES_OPEN);
    g_signal_connect(app,"open",G_CALLBACK(on_open),NULL);
    g_signal_connect(app,"activate",G_CALLBACK(on_activate),NULL);
    int status=g_application_run(G_APPLICATION(app),argc,argv);
    g_object_unref(app);
    return status;
}

