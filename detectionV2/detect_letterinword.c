// detect_letterinword.c
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>

// =======================================================
// FONCTIONS UTILITAIRES LOCALES (STATIC)
// =======================================================

static inline int clampi(int v,int lo,int hi){ return (v<lo)?lo:((v>hi)?hi:v); }

static inline int get_gray(GdkPixbuf *img, int x, int y){
    int W=gdk_pixbuf_get_width(img);
    int H=gdk_pixbuf_get_height(img);
    x=clampi(x,0,W-1); y=clampi(y,0,H-1);

    int nchan=gdk_pixbuf_get_n_channels(img);
    int rs=gdk_pixbuf_get_rowstride(img);
    guchar *p=gdk_pixbuf_get_pixels(img) + y*rs + x*nchan;

    int r=p[0], g=p[1], b=p[2];
    int gray=(299*r + 587*g + 114*b)/1000;
    return clampi(gray,0,255);
}

static void put_rgb(GdkPixbuf *pix, int x,int y, guint8 R,guint8 G,guint8 B)
{
    int W=gdk_pixbuf_get_width(pix), H=gdk_pixbuf_get_height(pix);
    if(x<0||y<0||x>=W||y>=H) return;
    int nchan=gdk_pixbuf_get_n_channels(pix);
    int rs=gdk_pixbuf_get_rowstride(pix);
    guchar *p=gdk_pixbuf_get_pixels(pix) + y*rs + x*nchan;
    p[0]=R; p[1]=G; p[2]=B;
}

static void draw_rect_thick(GdkPixbuf *pix, int x0,int y0,int x1,int y1,
                            guint8 R,guint8 G,guint8 B,int thick)
{
    int W=gdk_pixbuf_get_width(pix), H=gdk_pixbuf_get_height(pix);
    x0=clampi(x0,0,W-1); x1=clampi(x1,0,W-1);
    y0=clampi(y0,0,H-1); y1=clampi(y1,0,H-1);
    if(x0>x1) { int t=x0;x0=x1;x1=t; }
    if(y0>y1) { int t=y0;y0=y1;y1=t; }
    
    for(int t=0;t<thick;t++)
    {
        for(int x=x0; x<=x1; x++)
        { put_rgb(pix,x,y0+t,R,G,B); put_rgb(pix,x,y1-t,R,G,B); }
        for(int y=y0; y<=y1; y++)
        { put_rgb(pix,x0+t,y,R,G,B); put_rgb(pix,x1-t,y,R,G,B); }
    }
}

static double* row_ratio_band(GdkPixbuf *img, guint8 black_thr, int x0, int x1)
{
    int W=gdk_pixbuf_get_width(img), H=gdk_pixbuf_get_height(img);
    x0=clampi(x0,0,W-1); x1=clampi(x1,0,W-1);
    if(x0>x1) { int t=x0;x0=x1;x1=t; }
    
    double *row = (double*)malloc(sizeof(double)*H);
    if(!row) return NULL;
    
    for(int y=0;y<H;y++)
    {
        int black=0, tot=0;
        for(int x=x0;x<=x1;x++)
        {
            if(get_gray(img,x,y)<black_thr) { black++; }
            tot++;
        }
        row[y] = (tot>0) ? ((double)black/(double)tot) : 0.0;
    }
    return row;
}

static int ink_bbox(GdkPixbuf *img, int x0,int y0,int x1,int y1,
                    guint8 black_thr, int *rx0,int *ry0,int *rx1,int *ry1)
{
    int W=gdk_pixbuf_get_width(img), H=gdk_pixbuf_get_height(img);
    x0=clampi(x0,0,W-1); x1=clampi(x1,0,W-1);
    y0=clampi(y0,0,H-1); y1=clampi(y1,0,H-1);
    if(x0>x1) { int t=x0;x0=x1;x1=t; }
    if(y0>y1) { int t=y0;y0=y1;y1=t; }

    int found=0;
    int minx=W-1, maxx=0, miny=H-1, maxy=0;
    for(int y=y0;y<=y1;y++)
    {
        for(int x=x0;x<=x1;x++)
        {
            if(get_gray(img,x,y) < (int)black_thr)
            {
                if(x<minx)minx=x; if(x>maxx)maxx=x;
                if(y<miny)miny=y; if(y>maxy)maxy=y;
                found=1;
            }
        }
    }
    if(!found) return 0;
    *rx0=minx; *ry0=miny; *rx1=maxx; *ry1=maxy;
    return 1;
}

static int ensure_dir_local(const char *path)
{
    if(access(path, F_OK)==0) return 0;
    if(mkdir(path,0755)==0) return 0;
    if(errno==EEXIST) return 0;
    fprintf(stderr,"[detect_letterinword] mkdir('%s') a échoué: %s\n", path, strerror(errno));
    return -1;
}

// =======================================================
// FONCTION PRINCIPALE
// =======================================================

void detect_letters_in_words(GdkPixbuf *img, GdkPixbuf *disp,
                             int wx0,int wx1,int wy0,int wy1,
                             guint8 black_thr,
                             guint8 R,guint8 G,guint8 B)
{
    const int Wsrc=gdk_pixbuf_get_width(img);
    const int Hsrc=gdk_pixbuf_get_height(img);

    // Extensions généreuses pour capturer les mots longs
    wx0 = clampi(wx0 - 5, 0, Wsrc-1); 
    wx1 = clampi(wx1 + 200, 0, Wsrc-1); 
    wy0 = clampi(wy0 - 2, 0, Hsrc-1);
    wy1 = clampi(wy1 + 2, 0, Hsrc-1);

    if(ensure_dir_local("letterInWord")!=0){ 
        fprintf(stderr, "[detect_letterinword] Warning: Cannot create 'letterInWord'.\n");
    }

    double *row=row_ratio_band(img, black_thr, wx0, wx1);
    if(!row) { return; }

    const int W=gdk_pixbuf_get_width(img);
    const int H=gdk_pixbuf_get_height(img);

    int in=0, ystart=0, last=-1000000000;
    int word_idx=0;

    // --- SEUILS D'ENCRE ET DE COUPE POUR LES LETTRES ---
    // Ces seuils sont un compromis entre les polices grasses (0.05) et fines (0.01)
    const double T_INK_MAINTAIN = 0.05; // Seuil minimum pour maintenir le segment (Level 1)
    const int T_MAX_GAP = 2;            // Coupure après 3 colonnes vides consécutives (Level 2)
    // ----------------------------------------------------

    for(int y=wy0; y<=wy1; y++)
    {
        // 1. Détection des lignes de mots
        if(row[y]>0.01) // Seuil vertical très bas pour capturer les lignes fines
        {
            if(!in) { in=1; ystart=y; }
            last=y;
        }
        else if(in && (y-last)>8) // Coupure après 8 pixels de blanc
        {
            int y0=ystart, y1=last; in=0;
            y0=clampi(y0,0,H-1); y1=clampi(y1,0,H-1);
            if(y1-y0+1<6) { continue; }
            
            char word_dir[256];
            snprintf(word_dir,sizeof(word_dir),"letterInWord/word_%03d",word_idx);
            if(ensure_dir_local("letterInWord")==0)
            {
                ensure_dir_local(word_dir);
            }

            int cw = clampi(wx1,0,W-1) - clampi(wx0,0,W-1) + 1;
            if(cw<=0) { word_idx++; continue; }

            // Déclaration et Calcul du PROFIL DE COLONNE
            double *col=(double*)calloc(cw, sizeof(double)); 
            if(!col) { word_idx++; continue; }
            
            for(int x=0;x<cw;x++)
            {
                int black=0, tot=0;
                for(int yy=y0; yy<=y1; yy++)
                {
                    int gx=clampi(wx0+x,0,W-1);
                    int gy=clampi(yy,0,H-1);
                    if(get_gray(img,gx,gy) < (int)black_thr) { black++; } 
                    tot++;
                }
                col[x]=(tot>0)?((double)black/(double)tot):0.0;
            }

            int cx_in=0, cx0=0, cx_last=-1000000000;
            int letter_idx=0;
            
            // 2. Logique de Découpage Horizontal (Lettres)
            
            for(int x=0;x<cw;x++)
            {
                double cr=col[x]; 
                
                // Maintien/Démarrage de la lettre : Utiliser un seuil modéré pour la Level 1
                if(cr > T_INK_MAINTAIN) 
                {
                    if(!cx_in) { cx_in=1; cx0=x; }
                    cx_last=x;
                }
                // Coupure : Si on était dans un segment ET que l'espace est suffisant
                else if(cx_in && (x - cx_last) > T_MAX_GAP) 
                {
                    int X0=clampi(wx0+cx0,0,W-1);
                    int X1=clampi(wx0+cx_last,0,W-1);
                    int rx0,ry0,rx1,ry1;
                    
                    if((X1-X0)>=3 && ink_bbox(img,X0,y0,X1,y1,black_thr,&rx0,&ry0,&rx1,&ry1))
                    {
                        draw_rect_thick(disp,rx0,ry0,rx1,ry1,R,G,B,2);
                        
                        int ww=rx1-rx0+1;
                        int hh=ry1-ry0+1;
                        if(ww>=3 && hh>=3) {
                            int sx=rx0, sy=ry0;
                            GdkPixbuf *sub=gdk_pixbuf_new_subpixbuf(img,sx,sy,ww,hh);
                            if(sub) {
                                char out_path[512];
                                snprintf(out_path,sizeof(out_path),"%s/letter_%03d.png",word_dir,letter_idx);
                                gdk_pixbuf_save(sub,out_path,"png",NULL,NULL);
                                g_object_unref(sub);
                            }
                            letter_idx++;
                        }
                    }
                    cx_in=0;
                }
            }
            
            // Gestion de la dernière lettre (si le mot se termine en fin de ligne)
            if(cx_in) {
                int X0=clampi(wx0+cx0,0,W-1);
                int X1=clampi(wx0+cx_last,0,W-1);
                int rx0,ry0,rx1,ry1;
                if((X1-X0)>=3 && ink_bbox(img,X0,y0,X1,y1,black_thr,&rx0,&ry0,&rx1,&ry1))
                {
                    // ... (draw_rect_thick, sauvegarde)
                }
            }

            free(col);
            word_idx++;
        }
    }
    
    // --- Gestion de la dernière bande qui touche le bas de l'image (doit être complétée) ---
    // Cette section est omise pour la concision, mais si elle existe dans votre code, 
    // elle doit utiliser la même logique de découpage horizontal (T_INK_MAINTAIN & T_MAX_GAP).
    
    free(row);
}