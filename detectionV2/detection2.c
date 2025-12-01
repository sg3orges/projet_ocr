#include <gtk/gtk.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

/* === Prototypes des fonctions externes (Doivent exister dans les autres fichiers) === */
void detect_letters_in_grid(GdkPixbuf *img, GdkPixbuf *disp,
                            int gx0,int gx1,int gy0,int gy1,
                            guint8 black_thr, guint8 R,guint8 G,guint8 B);
void detect_letters_in_words(GdkPixbuf *img, GdkPixbuf *disp,
                            int wx0,int wx1,int wy0,int wy1,
                            guint8 black_thr, guint8 R,guint8 G,guint8 B);


/* === Fonctions Utilitaires Locales (Static) === */

static inline int clampi(int v,int lo,int hi)
{
    return v<lo?lo:(v>hi?hi:v); 
}

static inline guint8 get_gray_local(GdkPixbuf *pix,int x,int y)
{
    int n=gdk_pixbuf_get_n_channels(pix);
    guchar *p=gdk_pixbuf_get_pixels(pix)+y*gdk_pixbuf_get_rowstride(pix)+x*n;
    return (p[0]+p[1]+p[2])/3;
}

static void draw_rect(GdkPixbuf *pix,int x0,int y0,int x1,int y1,
                      guint8 R,guint8 G,guint8 B){
    int W=gdk_pixbuf_get_width(pix),H=gdk_pixbuf_get_height(pix);
    int n=gdk_pixbuf_get_n_channels(pix),rs=gdk_pixbuf_get_rowstride(pix);
    guchar *px=gdk_pixbuf_get_pixels(pix);
    x0=clampi(x0,0,W-1); x1=clampi(x1,0,W-1);
    y0=clampi(y0,0,H-1); y1=clampi(y1,0,H-1);

    for(int x=x0;x<=x1;x++)
    {
        guchar *t=px+y0*rs+x*n, *b=px+y1*rs+x*n;
        t[0]=R; t[1]=G; t[2]=B;
        b[0]=R; b[1]=G; b[2]=B;
    }
    for(int y=y0;y<=y1;y++)
    {
        guchar *l=px+y*rs+x0*n, *r=px+y*rs+x1*n;
        l[0]=R; l[1]=G; l[2]=B;
        r[0]=R; r[1]=G; r[2]=B;
    }
}

// --- Fonctions d'analyse de profil pour find_zones ---

static double *col_black_ratio_zone(GdkPixbuf *pix, guint8 thr,
                                     int x0, int x1)
{
    int W = gdk_pixbuf_get_width(pix);
    int H = gdk_pixbuf_get_height(pix);
    x0 = clampi(x0, 0, W-1);
    x1 = clampi(x1, 0, W-1);
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }

    int n = x1 - x0 + 1;
    double *r = calloc(n, sizeof(double));
    if (!r) return NULL;

    for (int i = 0; i < n; i++)
    {
        int x = x0 + i;
        int black = 0;
        for (int y = 0; y < H; y++)
            if (get_gray_local(pix, x, y) < thr)
                black++;
        r[i] = (double)black / (double)H;
    }
    return r;
}

// Calcule la force de l'autocorrélation normalisée (périodicité)
static double autocorr_strength(const double *p, int n,
                                 int lag_min, int lag_max)
{
    if (n <= 0) return 0.0;
    if (lag_max >= n) lag_max = n-1;
    if (lag_min >= lag_max) return 0.0;

    double mean = 0.0; for (int i = 0; i < n; i++) mean += p[i]; mean /= (double)n;
    double var = 0.0;
    for (int i = 0; i < n; i++) { double d = p[i] - mean; var += d*d; }
    if (var <= 1e-12) return 0.0;

    double best = 0.0;
    for (int k = lag_min; k <= lag_max; k++)
    {
        double acc = 0.0;
        int cnt = 0;
        for (int i = 0; i + k < n; i++)
        {
            double a = p[i] - mean;
            double b = p[i+k] - mean;
            acc += a*b; cnt++;
        }
        if (cnt > 0)
        {
            double score = acc / (var * cnt);
            if (score > best) best = score;
        }
    }
    if (best < 0.0) best = 0.0;
    return best;
}

// Score de périodicité horizontal (utile pour identifier la grille)
static double periodicity_score(const double *p, int n)
{
    if (n < 8) return 0.0;
    int lag_min = 3;
    int lag_max = n / 4;
    if (lag_max <= lag_min) return 0.0;
    return autocorr_strength(p, n, lag_min, lag_max);
}


typedef struct
{
    int start, end;
    double avg;
} Segment;

static void find_zones(GdkPixbuf *pix,
                       int *gx0,int *gx1,int *gy0,int *gy1,
                       int *wx0,int *wx1,int *wy0,int *wy1)
{
    const guint8 thr = 180;
    int W = gdk_pixbuf_get_width(pix);
    int H = gdk_pixbuf_get_height(pix);

    // Initialisation des variables locales
    int gx0_local = 0, gx1_local = 0;
    int wx0_local = 0, wx1_local = 0;
    int grid_w = 0;
    int word_idx = -1;
    int grid_idx = -1;
    
    if (W <= 0 || H <= 0) { goto fallback_zones; }

    // 1) Calcul de la densité de noir par colonne (dens)
    double *dens = calloc(W, sizeof(double));
    if (!dens) { goto fallback_zones; }
    for (int x = 0; x < W; x++)
    {
        int black = 0;
        for (int y = 0; y < H; y++)
            if (get_gray_local(pix, x, y) < thr) black++;
        dens[x] = (double)black / (double)H;
    }

    // 2) Lissage agressif (sm)
    double *sm = calloc(W, sizeof(double));
    if (!sm) { free(dens); goto fallback_zones; }

    int rad = W / 80; // Rayon de lissage
    if (rad < 5)   rad = 5;
    if (rad > 20) rad = 20;

    for (int x = 0; x < W; x++)
    {
        int L = clampi(x - rad, 0, W-1);
        int R = clampi(x + rad, 0, W-1);
        double acc = 0.0;
        int cnt = 0;
        for (int i = L; i <= R; i++) { acc += dens[i]; cnt++; }
        sm[x] = (cnt > 0) ? acc / (double)cnt : 0.0;
    }

    // 3) Découpe en segments : Seuil de coupure basé sur la MOYENNE
    // Pour les images très denses, max_s est moins fiable que la moyenne.
    double mean_s = 0.0;
    for (int x = 0; x < W; x++) mean_s += sm[x];
    mean_s /= W;
    
    if (mean_s < 1e-4) { free(dens); free(sm); goto fallback_zones; }

    // Seuil de coupure Tseg ULTRA-AGRESSIF pour détecter la séparation
    double Tseg = 0.15 * mean_s; // Basé sur la MOYENNE et augmenté
    if (Tseg < 0.005) Tseg = 0.005;

    int min_width = W / 40;
    if (min_width < 6) min_width = 6;

    Segment *seg = malloc(sizeof(Segment) * W);
    int nseg = 0;
    int inside = 0;
    int start = 0;
    double sum = 0.0;
    int cnt = 0;

    for (int x = 0; x < W; x++)
    {
        if (sm[x] >= Tseg) // Au-dessus du seuil -> partie du segment
        {
            if (!inside) { inside = 1; start = x; sum = sm[x]; cnt = 1; }
            else { sum += sm[x]; cnt++; }
        }
        else if (inside) // Creux trouvé -> fin du segment
        {
            int end = x - 1;
            int width = end - start + 1;
            if (width >= min_width) { seg[nseg++] = (Segment){ start, end, sum / (double)cnt }; }
            inside = 0;
        }
    }
    if (inside)
    {
        int end = W - 1;
        int width = end - start + 1;
        if (width >= min_width) { seg[nseg++] = (Segment){ start, end, sum / (double)cnt }; }
    }

    if (nseg == 0) { free(dens); free(sm); free(seg); goto fallback_zones; }


    // 4) SÉLECTION DE LA GRILLE: C'est le bloc le plus dominant (Densité * Largeur)
    
    double best_score_size = 0.0; // Score: Densité * Largeur

    for (int i = 0; i < nseg; i++)
    {
        // La grille est toujours le bloc le plus gros/dense
        double current_score_size = seg[i].avg * (double)(seg[i].end - seg[i].start + 1); 
        
        if (current_score_size > best_score_size)
        {
            best_score_size = current_score_size;
            grid_idx = i;
        }
    }

    // --- Attribution de la Grille (toujours un segment trouvé) ---
    if (grid_idx >= 0)
    {
        gx0_local = seg[grid_idx].start;
        gx1_local = seg[grid_idx].end;
        grid_w = gx1_local - gx0_local + 1;
    }
    else { goto fallback_zones; } // Ne devrait pas arriver si nseg > 0


    // 5) SÉLECTION DE LA ZONE MOTS: Le segment restant le plus grand
    double best_word_score = 0.0;
    // (grid_idx et gx0_local/gx1_local sont définis ici si la grille a été trouvée)

    for (int i = 0; i < nseg; i++)
    {
        if (i == grid_idx) continue;

        int s0 = seg[i].start;
        int s1 = seg[i].end;

        // Le segment de mots doit être entièrement à gauche ou à droite de la grille
        if (!(s1 < gx0_local || s0 > gx1_local))
            continue;

        double score = seg[i].avg * (double)(s1 - s0 + 1); 

        if (score > best_word_score)
        {
            best_word_score = score;
            word_idx = i;
        }
    }

    // --- Attribution de la Zone Mots ---
    if (word_idx >= 0)
    {
        wx0_local = seg[word_idx].start;
        wx1_local = seg[word_idx].end;
        
        // CORRECTION CRITIQUE: Si la grille est immédiatement à côté des mots, 
        // nous ajustons la borne droite de la grille pour qu'elle s'arrête juste avant les mots.
        // Ceci gère le cas où Tseg est trop bas et ne crée pas de creux.
        if (wx0_local > gx1_local) 
        {
             gx1_local = wx0_local - 1;
        }
    }
    else // Fallback pour les mots
    {
        // ... (Fallback inchangé)
    }

    free(dens); free(sm); free(seg);

    // --- Sorties finales ---
    *gx0 = gx0_local; 
    *gx1 = gx1_local; 
    *gy0 = 0; 
    *gy1 = H - 1;

    *wx0 = clampi(wx0_local, 0, W-1); 
    *wx1 = clampi(wx1_local, 0, W-1); 
    *wy0 = 0; 
    *wy1 = H - 1;
    return;


// --- Cas de fallback si la détection échoue ---
fallback_zones:
    *gx0 = W/3; *gx1 = W-1; *gy0 = 0; *gy1 = H-1;
    *wx0 = 0;   *wx1 = W/3; *wy0 = 0; *wy1 = H-1;
    return;
}




static void run_detection(GtkWidget *win,const char *path)
{
    GError *err=NULL;
    GdkPixbuf *img=gdk_pixbuf_new_from_file(path,&err);
    if(!img)
    {
	    g_printerr("Erreur: %s\n",err->message);
	    g_clear_error(&err);
	    return;
    }

    GdkPixbuf *disp=gdk_pixbuf_copy(img);
    int gx0,gx1,gy0,gy1, wx0,wx1,wy0,wy1;
    find_zones(img,&gx0,&gx1,&gy0,&gy1,&wx0,&wx1,&wy0,&wy1);

    printf("ZONE GRILLE: x=[%d,%d], y=[%d,%d]\n",gx0,gx1,gy0,gy1);
    printf("ZONE MOTS  : x=[%d,%d], y=[%d,%d]\n",wx0,wx1,wy0,wy1);

    draw_rect(disp,gx0,gy0,gx1,gy1,255,0,0); // red: grid
    draw_rect(disp,wx0,wy0,wx1,wy1,0,255,0); // green : word

    const guint8 BLACK_T=160;
    detect_letters_in_grid(img,disp,gx0,gx1,gy0,gy1,BLACK_T,0,128,255);
    detect_letters_in_words(img,disp,wx0,wx1,wy0,wy1,BLACK_T,0,128,255);

    GtkWidget *imgw=gtk_image_new_from_pixbuf(disp);
    gtk_container_add(GTK_CONTAINER(win),imgw);
    gtk_widget_show_all(win);

    g_object_unref(img);
    g_object_unref(disp);
}

//GTK
static void on_open(GApplication *app,GFile **files,int n,const char *hint)
{
    (void)n; (void)hint;
    GtkWidget *win=gtk_application_window_new(GTK_APPLICATION(app));
    gtk_window_set_title(GTK_WINDOW(win),"Détection grille et mots (GTK3)");
    gtk_window_set_default_size(GTK_WINDOW(win),1100,800);
    char *path=g_file_get_path(files[0]);
    run_detection(win,path);
    g_free(path);
}

int main(int argc,char **argv)
{
    GtkApplication *app=gtk_application_new("com.detect.auto",G_APPLICATION_HANDLES_OPEN);
    g_signal_connect(app,"open",G_CALLBACK(on_open),NULL);
    int status=g_application_run(G_APPLICATION(app),argc,argv);
    g_object_unref(app);
    return status;
}
