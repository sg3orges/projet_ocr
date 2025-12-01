// detect_letterinword.c
// Découpe des lettres dans la zone "mots" et enregistre chaque lettre en PNG
// Sortie: letterInWord/word_XXX/letter_YYY.png

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#define LETTER_TARGET_W 48
#define LETTER_TARGET_H 48
/* ===================== Utils locaux (static) ===================== */

static inline int clampi(int v,int lo,int hi){ return (v<lo)?lo:((v>hi)?hi:v); }

/* lecture niveau de gris (0..255) d’un pixel */
static inline int get_gray(GdkPixbuf *img, int x, int y){
    int W=gdk_pixbuf_get_width(img);
    int H=gdk_pixbuf_get_height(img);
    x=clampi(x,0,W-1); y=clampi(y,0,H-1);

    int nchan=gdk_pixbuf_get_n_channels(img);
    int rs=gdk_pixbuf_get_rowstride(img);
    guchar *p=gdk_pixbuf_get_pixels(img) + y*rs + x*nchan;

    // suppose RGB(A) 8 bits
    int r=p[0], g=p[1], b=p[2];
    // luma BT.601 approx
    int gray=(299*r + 587*g + 114*b)/1000;
    return clampi(gray,0,255);
}

/* trace rectangle (épais) sur un pixbuf */
static void put_rgb(GdkPixbuf *pix, int x,int y, guint8 R,guint8 G,guint8 B){
    int W=gdk_pixbuf_get_width(pix), H=gdk_pixbuf_get_height(pix);
    if(x<0||y<0||x>=W||y>=H) return;
    int nchan=gdk_pixbuf_get_n_channels(pix);
    int rs=gdk_pixbuf_get_rowstride(pix);
    guchar *p=gdk_pixbuf_get_pixels(pix) + y*rs + x*nchan;
    p[0]=R; p[1]=G; p[2]=B;
}

static void draw_rect_thick(GdkPixbuf *pix, int x0,int y0,int x1,int y1,
                            guint8 R,guint8 G,guint8 B,int thick){
    int W=gdk_pixbuf_get_width(pix), H=gdk_pixbuf_get_height(pix);
    x0=clampi(x0,0,W-1); x1=clampi(x1,0,W-1);
    y0=clampi(y0,0,H-1); y1=clampi(y1,0,H-1);
    if(x0>x1){ int t=x0;x0=x1;x1=t; }
    if(y0>y1){ int t=y0;y0=y1;y1=t; }
    for(int t=0;t<thick;t++){
        for(int x=x0; x<=x1; x++){ put_rgb(pix,x,y0+t,R,G,B); put_rgb(pix,x,y1-t,R,G,B); }
        for(int y=y0; y<=y1; y++){ put_rgb(pix,x0+t,y,R,G,B); put_rgb(pix,x1-t,y,R,G,B); }
    }
}


/* Sauvegarde une lettre (zone mots) avec une petite marge autour de la bbox */
static int save_letter_with_margin_word(GdkPixbuf *img, GdkPixbuf *disp,
                                        int rx0,int ry0,int rx1,int ry1,
                                        guint8 R,guint8 G,guint8 B,
                                        const char *word_dir,
                                        int letter_idx,
                                        int margin)
{
    int W = gdk_pixbuf_get_width(img);
    int H = gdk_pixbuf_get_height(img);

    // Étendre légèrement le rectangle
    int x0 = clampi(rx0 - margin, 0, W - 1);
    int y0 = clampi(ry0 - margin, 0, H - 1);
    int x1 = clampi(rx1 + margin, 0, W - 1);
    int y1 = clampi(ry1 + margin, 0, H - 1);

    int ww = x1 - x0 + 1;
    int hh = y1 - y0 + 1;
    if (ww <= 0 || hh <= 0)
        return letter_idx;

    // Dessine le rectangle élargi sur l'image d'affichage
    draw_rect_thick(disp, x0, y0, x1, y1, R, G, B, 2);

    int sx = x0;
    int sy = y0;
    ww = clampi(ww, 1, W - sx);
    hh = clampi(hh, 1, H - sy);

    GdkPixbuf *sub = gdk_pixbuf_new_subpixbuf(img, sx, sy, ww, hh);
    if (sub)
    {
        // Redimensionnement à taille fixe
        GdkPixbuf *scaled = gdk_pixbuf_scale_simple(
            sub,
            LETTER_TARGET_W,
            LETTER_TARGET_H,
            GDK_INTERP_BILINEAR
        );

        if (scaled)
        {
            char out_path[512];
            snprintf(out_path, sizeof(out_path),
                     "%s/letter_%03d.png", word_dir, letter_idx);
            if (!gdk_pixbuf_save(scaled, out_path, "png", NULL, NULL))
            {
                fprintf(stderr,
                        "[detect_letterinword] Échec sauvegarde %s\n",
                        out_path);
            }
            g_object_unref(scaled);
            letter_idx++;
        }

        g_object_unref(sub);
    }

    return letter_idx;
}

/* profil "taux d’encre" par ligne, limité à [x0..x1] (renvoie tableau de H) */
static double* row_ratio_band(GdkPixbuf *img, guint8 black_thr, int x0, int x1){
    int W=gdk_pixbuf_get_width(img), H=gdk_pixbuf_get_height(img);
    x0=clampi(x0,0,W-1); x1=clampi(x1,0,W-1);
    if(x0>x1){ int t=x0;x0=x1;x1=t; }
    double *row = (double*)malloc(sizeof(double)*H);
    if(!row) return NULL;
    for(int y=0;y<H;y++){
        int black=0, tot=0;
        for(int x=x0;x<=x1;x++){
            if(get_gray(img,x,y)<black_thr) black++;
            tot++;
        }
        row[y] = (tot>0) ? ((double)black/(double)tot) : 0.0;
    }
    return row;
}

/* bbox des pixels < black_thr dans [x0..x1]x[y0..y1] ; 1 si trouvé */
static int ink_bbox(GdkPixbuf *img, int x0,int y0,int x1,int y1,
                    guint8 black_thr, int *rx0,int *ry0,int *rx1,int *ry1){
    int W=gdk_pixbuf_get_width(img), H=gdk_pixbuf_get_height(img);
    x0=clampi(x0,0,W-1); x1=clampi(x1,0,W-1);
    y0=clampi(y0,0,H-1); y1=clampi(y1,0,H-1);
    if(x0>x1){ int t=x0;x0=x1;x1=t; }
    if(y0>y1){ int t=y0;y0=y1;y1=t; }

    int found=0;
    int minx=W-1, maxx=0, miny=H-1, maxy=0;
    for(int y=y0;y<=y1;y++){
        for(int x=x0;x<=x1;x++){
            if(get_gray(img,x,y) < (int)black_thr){
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

/* dossiers */
static int ensure_dir_local(const char *path){
    if(access(path, F_OK)==0) return 0;
    if(mkdir(path,0755)==0) return 0;
    if(errno==EEXIST) return 0;
    fprintf(stderr,"[detect_letterinword] mkdir('%s') a échoué: %s\n", path, strerror(errno));
    return -1;
}

/* ===================== API ===================== */
void detect_letters_in_words(GdkPixbuf *img, GdkPixbuf *disp,
                             int wx0,int wx1,int wy0,int wy1,
                             guint8 black_thr,
                             guint8 R,guint8 G,guint8 B)
{
    const int Wsrc=gdk_pixbuf_get_width(img);

    // --- LOGIQUE D'EXTENSION ET DE DÉTECTION DE BORDURE DE GRILLE ---
    // Nous étendons la zone de recherche de 200px, MAIS nous la limitons
    // s'il y a trop d'encre verticale (signe de la grille).
    
    int initial_wx1 = wx1;
    int max_x = clampi(wx1 + 200, 0, Wsrc - 1);
    const double GRID_DENSITY_THRESHOLD = 0.5; // 50% d'encre = limite de la grille

    // 1. Chercher le bord dense de la grille entre wx1 et max_x
    for (int x = initial_wx1 + 1; x <= max_x; x++) {
        int black_count = 0;
        int total_count = 0;
        for (int y = wy0; y <= wy1; y++) {
            if (get_gray(img, x, y) < black_thr) {
                black_count++;
            }
            total_count++;
        }
        
        // Si la colonne est trop dense, c'est la limite de la grille
        if (total_count > 0 && ((double)black_count / (double)total_count) > GRID_DENSITY_THRESHOLD) {
            max_x = x - 1; // Définir le nouveau bord droit JUSTE AVANT la colonne de grille
            break;
        }
    }

    // 2. Appliquer la nouvelle borne droite (soit la grille trouvée, soit +200)
    wx1 = max_x;
    // -------------------------------------------------------------------------

    /* dossier racine */
    if(ensure_dir_local("letterInWord")!=0){
        fprintf(stderr,"[detect_letterinword] Impossible de créer 'letterInWord' (on continue sans sauvegarde).\n");
    }

    /* 1) bandes horizontales (une par ligne de mots) */
    // Note: row_ratio_band utilise maintenant le wx1 ajusté
    double *row=row_ratio_band(img, black_thr, wx0, wx1);
    if(!row) return;

    const int Hsrc=gdk_pixbuf_get_height(img);

    int in=0, ystart=0, last=-1000000000;
    int word_idx=0;

    // --- SEUILS HORIZONTAUX (Dernière version stable demandée) ---
    const double T_INK_LETTER_MAINTAIN = 0.08; // 8% de densité pour être considéré comme une lettre
    const int T_LETTER_MAX_GAP = 1;            // 1 colonne vide tolérée avant coupure (très strict)
    // ----------------------------------------------------------------------------------


    for(int y=wy0; y<=wy1; y++){
        if(row[y]>0.05){
            if(!in){ in=1; ystart=y; }
            last=y;
        }else if(in && (y-last)>10){
            /* fin de bande */
            int y0=ystart, y1=last; in=0;
            y0=clampi(y0,0,Hsrc-1); y1=clampi(y1,0,Hsrc-1);
            if(y1-y0+1<6) continue;

            char word_dir[256];
            snprintf(word_dir,sizeof(word_dir),"letterInWord/word_%03d",word_idx);
            if(ensure_dir_local("letterInWord")==0) ensure_dir_local(word_dir);

            int cw = clampi(wx1,0,Wsrc-1) - clampi(wx0,0,Wsrc-1) + 1;
            if(cw<=0){ word_idx++; continue; }

            double *col=(double*)malloc(sizeof(double)*cw);
            if(!col){ word_idx++; continue; }

            for(int x=0;x<cw;x++){
                int black=0, tot=0;
                for(int yy=y0; yy<=y1; yy++){
                    int gx=clampi(wx0+x,0,Wsrc-1);
                    int gy=clampi(yy,0,Hsrc-1);
                    if(get_gray(img,gx,gy) < (int)black_thr) black++;
                    tot++;
                }
                col[x]=(tot>0)?((double)black/(double)tot):0.0;
            }

            int cx_in=0, cx0=0, cx_last=-1000000000;
            int letter_idx=0;

            for(int x=0;x<cw;x++){
                double cr=col[x];
                if(cr>0.08){ if(!cx_in){cx_in=1; cx0=x;} cx_last=x; }
                else if(cx_in && (x-cx_last)>T_LETTER_MAX_GAP){ // Utilisation du nouveau seuil T_LETTER_MAX_GAP=1
                    int X0=clampi(wx0+cx0,0,Wsrc-1);
                    int X1=clampi(wx0+cx_last,0,Wsrc-1);
                    int rx0,ry0,rx1,ry1;
                    if((X1-X0)>=3 && ink_bbox(img,X0,y0,X1,y1,black_thr,&rx0,&ry0,&rx1,&ry1)){
    int ww = rx1 - rx0 + 1;
    int hh = ry1 - ry0 + 1;
    if (ww >= 3 && hh >= 3) {
        int margin = 3; // marge autour de la lettre (à ajuster si besoin)
        letter_idx = save_letter_with_margin_word(img, disp,
                                                  rx0, ry0, rx1, ry1,
                                                  R, G, B,
                                                  word_dir,
                                                  letter_idx,
                                                  margin);
    }
}

                    cx_in=0;
                }
            }
            if(cx_in){
                int X0=clampi(wx0+cx0,0,Wsrc-1);
                int X1=clampi(wx0+cx_last,0,Wsrc-1);
                int rx0,ry0,rx1,ry1;
                if((X1-X0)>=3 && ink_bbox(img,X0,y0,X1,y1,black_thr,&rx0,&ry0,&rx1,&ry1)){
    int ww = rx1 - rx0 + 1;
    int hh = ry1 - ry0 + 1;
    if (ww >= 3 && hh >= 3) {
        int margin = 3;
        letter_idx = save_letter_with_margin_word(img, disp,
                                                  rx0, ry0, rx1, ry1,
                                                  R, G, B,
                                                  word_dir,
                                                  letter_idx,
                                                  margin);
    }
}


            }

            free(col);
            word_idx++;
        }
    }

    /* Bande qui se termine au bas de l'image */
    if(in){
        int y0=ystart, y1=last;
        y0=clampi(y0,0,Hsrc-1); y1=clampi(y1,0,Hsrc-1);
        if(y1-y0+1>=6){
            char word_dir[256];
            snprintf(word_dir,sizeof(word_dir),"letterInWord/word_%03d",word_idx);
            if(ensure_dir_local("letterInWord")==0) ensure_dir_local(word_dir);

            int cw = clampi(wx1,0,Wsrc-1) - clampi(wx0,0,Wsrc-1) + 1;
            if(cw>0){
                double *col=(double*)malloc(sizeof(double)*cw);
                if(col){
                    for(int x=0;x<cw;x++){
                        int black=0, tot=0;
                        for(int yy=y0; yy<=y1; yy++){
                            int gx=clampi(wx0+x,0,Wsrc-1);
                            int gy=clampi(yy,0,Hsrc-1);
                            if(get_gray(img,gx,gy) < (int)black_thr) black++;
                            tot++;
                        }
                        col[x]=(tot>0)?((double)black/(double)tot):0.0;
                    }
                    int cx_in=0, cx0=0, cx_last=-1000000000;
                    int letter_idx=0;
                    for(int x=0;x<cw;x++){
                        double cr=col[x];
                        if(cr > 0.08){ if(!cx_in){cx_in=1; cx0=x;} cx_last=x; }
                        else if(cx_in && (x-cx_last)>T_LETTER_MAX_GAP){ // Utilisation du nouveau seuil T_LETTER_MAX_GAP=1
                            int X0=clampi(wx0+cx0,0,Wsrc-1);
                            int X1=clampi(wx0+cx_last,0,Wsrc-1);
                            int rx0,ry0,rx1,ry1;
                            if((X1-X0)>=3 && ink_bbox(img,X0,y0,X1,y1,black_thr,&rx0,&ry0,&rx1,&ry1)){
                                draw_rect_thick(disp,rx0,ry0,rx1,ry1,R,G,B,2);
                                int ww=clampi(rx1,0,Wsrc-1)-clampi(rx0,0,Wsrc-1)+1;
                                int hh=clampi(ry1,0,Hsrc-1)-clampi(ry0,0,Hsrc-1)+1;
                                if(ww>=3 && hh>=3){
                                    int sx=clampi(rx0,0,Wsrc-1), sy=clampi(ry0,0,Hsrc-1);
                                    ww=clampi(ww,1,Wsrc-sx); hh=clampi(hh,1,Hsrc-sy);
                                    GdkPixbuf *sub=gdk_pixbuf_new_subpixbuf(img,sx,sy,ww,hh);
                                    if(sub){
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
                    if(cx_in){
                        int X0=clampi(wx0+cx0,0,Wsrc-1);
                        int X1=clampi(wx0+cx_last,0,Wsrc-1);
                        int rx0,ry0,rx1,ry1;
                        if((X1-X0)>=3 && ink_bbox(img,X0,y0,X1,y1,black_thr,&rx0,&ry0,&rx1,&ry1)){
                            draw_rect_thick(disp,rx0,ry0,rx1,ry1,R,G,B,2);
                            int ww=clampi(rx1,0,Wsrc-1)-clampi(rx0,0,Wsrc-1)+1;
                            int hh=clampi(ry1,0,Hsrc-1)-clampi(ry0,0,Hsrc-1)+1;
                            if(ww>=3 && hh>=3){
                                int sx=clampi(rx0,0,Wsrc-1), sy=clampi(ry0,0,Hsrc-1);
                                ww=clampi(ww,1,Wsrc-sx); hh=clampi(hh,1,Hsrc-sy);
                                GdkPixbuf *sub=gdk_pixbuf_new_subpixbuf(img,sx,sy,ww,hh);
                                if(sub){
                                    char out_path[512];
                                    snprintf(out_path,sizeof(out_path),"%s/letter_%03d.png",word_dir,letter_idx);
                                    gdk_pixbuf_save(sub,out_path,"png",NULL,NULL);
                                    g_object_unref(sub);
                                }
                            }
                        }
                    }
                    free(col);
                }
            }
        }
    }

    free(row);
}