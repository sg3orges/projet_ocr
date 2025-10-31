#include <gtk/gtk.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

/* === Prototypes des fonctions externes === */
void detect_letters_in_grid(GdkPixbuf *img, GdkPixbuf *disp,
                            int gx0,int gx1,int gy0,int gy1,
                            guint8 black_thr,
                            guint8 R,guint8 G,guint8 B);
void detect_letters_in_words(GdkPixbuf *img, GdkPixbuf *disp,
                             int wx0,int wx1,int wy0,int wy1,
                             guint8 black_thr,
                             guint8 R,guint8 G,guint8 B);


static inline int clampi(int v,int lo,int hi)
{
       	return v<lo?lo:(v>hi?hi:v); 
}
static inline guint8 gray(GdkPixbuf *pix,int x,int y)
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

// Automatic detection of grid and word areas
typedef struct 
{
       	int start,end; double avg; 
} Block;

static void find_zones(GdkPixbuf *pix,
                       int *gx0,int *gx1,int *gy0,int *gy1,
                       int *wx0,int *wx1,int *wy0,int *wy1)
{
    const guint8 thr=180;
    int W=gdk_pixbuf_get_width(pix);
    int H=gdk_pixbuf_get_height(pix);
    double *dens=calloc(W,sizeof(double));
    if(!dens) 
    {
	    return;
    }

    // Average vertical density of black
    for(int x=0;x<W;x++)
    {
        int black=0;
        for(int y=0;y<H;y++)
            if(gray(pix,x,y)<thr)
	    {
		    black++;
	    }
        dens[x]=(double)black/H;
    }

    // Detection of dark bands (text areas)
    Block *tmp=malloc(W*sizeof(Block));
    int nb=0;
    bool inside=false;
    int start=0;
    for(int x=0;x<W;x++)
    {
        if(dens[x]>0.05 && !inside)
	{
            inside=true; start=x;
        }
	else if(dens[x]<=0.05 && inside)
	{
            inside=false;
            int end=x;
            double avg=0;
            for(int i=start;i<end;i++) avg+=dens[i];
            avg/=(end-start+1);
            tmp[nb++]=(Block){start,end,avg};
        }
    }

    // Merge adjacent bands (< 50 px difference)
    Block *blocks=malloc(nb*sizeof(Block));
    int fused=0;
    for(int i=0;i<nb;i++)
    {
        if(fused==0)
	{
	       	blocks[fused++]=tmp[i]; continue;
       	}
        int gap=tmp[i].start-blocks[fused-1].end;
        if(gap<50)
	{
	       	// fusion
            blocks[fused-1].end=tmp[i].end;
            blocks[fused-1].avg=(blocks[fused-1].avg+tmp[i].avg)/2.0;
        }
	else
       	{
	       	blocks[fused++]=tmp[i];
	}
    }
    free(tmp);

    if(fused<2)
    {
        // fallback simple
        *gx0=W/3; *gx1=W-1; *gy0=0; *gy1=H-1;
        *wx0=0;   *wx1=W/3; *wy0=0; *wy1=H-1;
        free(dens); free(blocks);
        return;
    }

    // Sort by width in descending order
    for(int i=0;i<fused-1;i++)
    {
        for(int j=i+1;j<fused;j++)
	{
            int wi=blocks[i].end-blocks[i].start;
            int wj=blocks[j].end-blocks[j].start;
            if(wj>wi)
	    {
                Block tmpb=blocks[i]; blocks[i]=blocks[j]; blocks[j]=tmpb;
            }
        }
    }

    Block b1=blocks[0];
    Block b2=blocks[1];
    if(b2.start<b1.start)
    {
        Block tmpb=b1; b1=b2; b2=tmpb;
    }

    int top=0,bottom=H-1;

    // Grid/word selection based on width and density
    int w1=b1.end-b1.start, w2=b2.end-b2.start;
    bool left_is_words=false;
    if(w1<w2 || b1.avg<b2.avg*0.8) 
    {
	    left_is_words=true;
    }

    if(left_is_words)
    {
        *wx0=b1.start;
       	*wx1=b1.end; 
	*wy0=top; 
	*wy1=bottom;
        *gx0=b2.start;
       	*gx1=b2.end;
       	*gy0=top;
       	*gy1=bottom;
    }
    else
    {
        *gx0=b1.start; 
	*gx1=b1.end;
       	*gy0=top;
       	*gy1=bottom;
        *wx0=b2.start;
       	*wx1=b2.end;
       	*wy0=top;
       	*wy1=bottom;
    }

    free(dens);
    free(blocks);
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
