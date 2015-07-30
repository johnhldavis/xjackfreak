/****************************************************************************
*
* This file (xjackfreak1.c) is part of xjackfreak, an audio frequency analyser.
*
* Copyright (C) 1996-2015  J.Davis dev@mirainet.org.uk   
*
* DTK is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* DTK is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/shm.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/StringDefs.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/extensions/XShm.h>


// max image size 512x276x4 = 0.5MB
#define MAX_BUF 565248

// audio limits
#define MAX_FRAG_SIZE 8192		// jack frame size
#define MAX_SFRAG_SIZE 32768	// super-frame size

#define MAX_JACK_PORTS 2

#define FFT_FREQ_DISP_COMP 10


#define BUT_BYPASS bon[ 0]		// bypass				orange
#define BUT_GRID   bon[ 1]		// grid on/off			green
#define BUT_MOD    bon[ 2]		// mod_data				green
#define BUT_PHASE  bon[ 3]		// phase					green
#define BUT_INPUT  bon[ 4]		// input					green
#define BUT_FFT1   bon[ 5]		// fft					green
#define BUT_FFT2   bon[ 6]		// post-fft				green
#define BUT_OUTPUT bon[ 7]		// output				green
#define BUT_X_LOG  bon[ 8]		// x-log					orange
#define BUT_Y_LOG  bon[ 9]		// y-log					orange
#define BUT_FFTAVE bon[10]		// fft_ave				orange
#define BUT_BLOCK  bon[11]		// block mode			orange
#define BUT_HFCOMP bon[12]		// hf comp				orange
#define BUT_LINK   bon[13]		// link					orange
#define BUT_SMOTH  bon[14]		// mod_data smooth
#define BUT_RESET  bon[15]		// mod_data reset		-
#define BUT_RECBUF bon[16]		// rec_buf				green
#define BUT_LEFT   bon[17]		// left					-
#define BUT_RIGHT  bon[18]		// right					-			
#define BUT_UP     bon[19]		// up						-
#define BUT_DOWN   bon[20]		// down					-

int fft_freq_disp_comp=FFT_FREQ_DISP_COMP;

int debug=0;

void draw_main(int all);
void draw_controls();
void draw_status();

void draw_controls_post();
void draw_status_post();

void draw_main_put();
void draw_controls_put();
void draw_status_put();
void draw_status_controls_put();
void draw_status_controls_main_put();

// various display options
double fps=25.0f;
// == 96 dB
double log_rangey=-53.333;
// == 120 dB
//double log_rangey=-42.5;
//double log_rangey=-10.5;
// buttons stuff
int  bon[128];			// mode: off/on
int  bpc[128];			// ppm choice
int  disp_off=20;
char welcome[64]     = { "XjackFreak V1 " };
char status_line[64] = { "XjackFreak V1 " };
int  wel_idx=0;
int  do_intro=1;
int  disp_max=0;
int  stereo_mode=1;
int  edit_param=0;
#define EDIT_PARAM_MAX 31

int old_but_x=0,old_but_y=0;

//	-data window type
//	-averaging ratio for FFT
//	-smoothing function for mod_data
//	-colours of various displays

int smooth_func=1;
int delay_bypass=1;
int wait_on_complete=0;   // flag to indicate we're waiting on an SHM completion event
int win_mapped=0;

// graphics mode: -<gmode>                X11    X12       X13        opengl
//                                        x11    x12       x13        gl
int wmode=3;           // wanted gmode
int gmode=0;           // 0:unset  1:X11  2:XImage  3:XIm+SHM  4:opengl

#define COL_GRD1 0	// grid colour: 			90,90,90
#define COL_GRD2 1	// grid colour: 			80,80,80
#define COL_MOD  2	// mod_data					0,50,200
#define COL_INP  3	// input						0,200,0
#define COL_FFT1 4	// fft1						175,175,0
#define COL_FFT2 5	// fft2						0,75,175
#define COL_OUT  6	// output					200,0,0
#define COL_RDAT 7	// recbuf: fft data:		0,200,0
#define COL_RWIN 8	// recbuf: fft window	0,0,200
#define COL_RINP 9	// recbuf: intput			0,200,0
#define COL_ROUT 10	// recbuf: output			200,0,0

struct _colour_
	{
	unsigned int r;
	unsigned int g;
	unsigned int b;
	unsigned int a;
	};

struct _colour_ cols[16];

#define EVENT_MASK	KeyPressMask | KeyReleaseMask | StructureNotifyMask | \
								ExposureMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask | \
								PointerMotionMask | EnterWindowMask | LeaveWindowMask | FocusChangeMask | \
								VisibilityChangeMask

#include "visualx11.c"
#include "fftlib1.c"
#include "plist.c"

XTextProperty text_prop;
Atom wm_delete_window;		// need to save this so we can trap a client message ...
XShmSegmentInfo shminfo;

unsigned char *data0;
char title[32];
char *tptr;
int  win_width=520,win_height=284;
int  image_width=512,image_height=276;
unsigned long white,black;

// framing
int  running=0,old_running=0;
long render_count=0,wait_count=0,drop_count=0;
long frame_rate=40000;
int  pause_framing=0;
long frame_count=0;

// fft bits
int fft_size=1024;
int fft_size2=512;
int fft_size4=256;
int fft_places=10;
int rct_size=1024;

int disp_offy=148;
//int disp_mul=128;
float disp_mul=100.0f;

// double-buffered arrays for display
int input_frame=0;
int input_data[2][2][MAX_FRAG_SIZE];		// 2 channels of 2 frames of input data
int output_data[2][MAX_FRAG_SIZE];

// mod_data[]
float mod_data[MAX_FFT_DATA_SIZE];
float real_mod_data[MAX_FFT_DATA_SIZE];
float new_mod_data[MAX_FFT_DATA_SIZE];
float max_mod=2.0;

#include "jack.c"
#include "signals.c"
#include "jfgraplib1.c"
#include "load_ppm.c"


void draw_grid()
	{
	int i;

	for (i=0;i<4 ;i++) line(0,i*64+disp_off,510,i*64+disp_off,cols[COL_GRD2].r,cols[COL_GRD2].g,cols[COL_GRD2].b);		// horiz
	for (i=0;i<8;i++)
		{
		line(i*64,disp_off+  1,i*64, 63+disp_off,cols[COL_GRD2].r,cols[COL_GRD2].g,cols[COL_GRD2].b);					// vert
		line(i*64,disp_off+ 65,i*64,127+disp_off,cols[COL_GRD2].r,cols[COL_GRD2].g,cols[COL_GRD2].b);					// vert
		line(i*64,disp_off+129,i*64,191+disp_off,cols[COL_GRD2].r,cols[COL_GRD2].g,cols[COL_GRD2].b);					// vert
		line(i*64,disp_off+193,i*64,254+disp_off,cols[COL_GRD2].r,cols[COL_GRD2].g,cols[COL_GRD2].b);					// vert
		}
	line(  0,255+disp_off,510,255+disp_off,cols[COL_GRD2].r,cols[COL_GRD2].g,cols[COL_GRD2].b);					// bot
	line(511,    disp_off,511,511+disp_off,cols[COL_GRD2].r,cols[COL_GRD2].g,cols[COL_GRD2].b);					// rhs
	}

int draw_rec_buf(int _frame,int _off)
	{
	int i,j,oldy,newy,idx,idx2,iframe=(fft_wr-1+FFT_BUF_MAX)%FFT_BUF_MAX;

//	printf("draw_rec_buf(%d,%d)\n",_frame,_off);
	blank_screen();
	if (BUT_GRID) line(0,disp_offy,511,disp_offy,cols[COL_GRD2].r,cols[COL_GRD2].g,cols[COL_GRD2].b);
	
//	draw fft_buf before it's processed HERE!!
	if (BUT_FFT1)
		{
		do_intro=0;
		sprintf(status_line,"draw FFT frame");

		oldy=disp_offy-(int)(fft_buf[iframe][0]*disp_mul);
		for (i=1;i<fft_size;i++)
			{
			newy=disp_offy-(int)(fft_buf[iframe][i]*disp_mul);
			if ((i%fft_size2==0) && (BUT_GRID)) line(i/2,21,i/2,275,cols[COL_GRD2].r,cols[COL_GRD2].g,cols[COL_GRD2].b);
			line((i-1)/2,oldy,i/2,newy,cols[COL_RDAT].r,cols[COL_RDAT].g,cols[COL_RDAT].b);
			oldy=newy;
			}
// display data window !
		oldy=disp_offy-(int)(dwindow[0]*disp_mul);
		for (i=1;i<fft_size;i++)
			{
			if (i>=fft_size2) newy=disp_offy-(int)(dwindow[fft_size-i-1]*disp_mul);
			else newy=disp_offy-(int)(dwindow[i]*disp_mul);
			if ((i%fft_size2==0) && (BUT_GRID)) line(i/2,21,i/2,275,cols[COL_GRD2].r,cols[COL_GRD2].g,cols[COL_GRD2].b);
			line((i-1)/2,oldy,i/2,newy,cols[COL_RWIN].r,cols[COL_RWIN].g,cols[COL_RWIN].b);
			oldy=newy;
			}
		return 0;
		}
//	draw rec input here
	do_intro=0;
	sprintf(status_line,"IN frame: %d:%04d",_frame,_off);
	idx=_frame*jack_frame_size+(_off/sizeof(jack_default_audio_sample_t));
	oldy=disp_offy-(int)(inbuf[idx]*disp_mul);
	for (i=1;i<512;i++)
		{
		idx=(_frame*jack_frame_size+_off/sizeof(jack_default_audio_sample_t)+i)%MAX_REC_BUF;
		newy=disp_offy-(int)(inbuf[idx]*disp_mul);
		if (BUT_GRID)
			{
			idx2=idx%fft_size2;
			if (idx%jack_frame_size==0)
				{
				line(i  ,21,i  ,275,cols[COL_GRD1].r,cols[COL_GRD1].g,cols[COL_GRD1].b);
				line(i+1,21,i+1,275,cols[COL_GRD1].r,cols[COL_GRD1].g,cols[COL_GRD1].b);
				}
			else if (idx2==0)
				{
				for (j=22;j<275;j=j+5) line(i  ,j,i  ,j+2,cols[COL_GRD2].r,cols[COL_GRD2].g,cols[COL_GRD2].b);
				}
			else if (idx2==fft_size4-audio_knit_size)
				{
				for (j=85;j<211;j=j+5) line(i  ,j,i  ,j+2,cols[COL_GRD2].r,cols[COL_GRD2].g,cols[COL_GRD2].b);
				}
			else if (idx2==fft_size4+audio_knit_size)
				{
				for (j=85;j<211;j=j+5) line(i  ,j,i  ,j+2,cols[COL_GRD2].r,cols[COL_GRD2].g,cols[COL_GRD2].b);
				}
			}
		line(i-1,oldy,i,newy,cols[COL_RINP].r,cols[COL_RINP].g,cols[COL_RINP].b);
		oldy=newy;
		}
// draw output here !!
	oldy=disp_offy-(int)(outbuf[_frame*jack_frame_size+_off/sizeof(jack_default_audio_sample_t)]*disp_mul);
	for (i=1;i<512;i++)
		{
		newy=disp_offy-(int)(outbuf[(_frame*jack_frame_size+_off/sizeof(jack_default_audio_sample_t)+i)%MAX_REC_BUF]*disp_mul);
		line(i-1,oldy,i,newy,cols[COL_ROUT].r,cols[COL_ROUT].g,cols[COL_ROUT].b);
		oldy=newy;
		}
	pentry_add(2,0);
	return 0;
	}

int draw_rec_buf_zoom(int _frame,int _off,int _x,int _y)
	{
	int i,oldy,newy,idx;
	float zoom=8.0f;

//	printf("draw_rec_buf_zoom(%d,%d,%d,%d)\n",_frame,_off,_x,_y);
	do_intro=0;
	sprintf(status_line,"zoom IN frm-x: %d-%d",_frame,_x);
	blank_screen();
	line(0,disp_offy,511,disp_offy,cols[COL_GRD2].r,cols[COL_GRD2].g,cols[COL_GRD2].b);
//	draw rec input here
	idx=_frame*jack_frame_size+(_off/sizeof(jack_default_audio_sample_t))+_x-jack_frame_size/(2*zoom);
	oldy=disp_offy-(int)(inbuf[idx]*disp_mul*zoom);
//	printf("idx=%d ",idx);
	for (i=1;i<jack_frame_size/(int)zoom;i++)
		{
		idx=(_frame*jack_frame_size+_off/sizeof(jack_default_audio_sample_t)+i+_x-jack_frame_size/(2*(int)zoom))%MAX_REC_BUF;
		newy=disp_offy-(int)(inbuf[idx]*disp_mul*zoom);
		if (idx%jack_frame_size==0) line(zoom*i,21,zoom*i,275,cols[COL_GRD2].r,cols[COL_GRD2].g,cols[COL_GRD2].b);
		line(zoom*(i-1),oldy,zoom*i,newy,cols[COL_RINP].r,cols[COL_RINP].g,cols[COL_RINP].b);
		oldy=newy;
		}
// draw output here !!
	oldy=disp_offy-(int)(outbuf[_frame*jack_frame_size+_off/sizeof(jack_default_audio_sample_t)+_x-jack_frame_size/(2*(int)zoom)]*disp_mul*zoom);
	for (i=1;i<jack_frame_size/(int)zoom;i++)
		{
		newy=disp_offy-(int)(outbuf[(_frame*jack_frame_size+_off/sizeof(jack_default_audio_sample_t)+i+_x-jack_frame_size/(2*(int)zoom))%MAX_REC_BUF]*disp_mul*zoom);
		line(zoom*(i-1),oldy,zoom*i,newy,cols[COL_ROUT].r,cols[COL_ROUT].g,cols[COL_ROUT].b);
		oldy=newy;
		}
	pentry_add(2,0);
	return 0;
	}

void do_display()
	{
	int i;
	int oldy,newy,oldx,newx;
	double tmpd,diff,lin_rangex,lin_offx,log_rangex,log_offx;
	int pos,oldpos;
	int iframe=(fft_wr-2+FFT_BUF_MAX)%FFT_BUF_MAX;
	int ioff=fft_owr[iframe],off;
	float mmax_tmp=0.0f;


	blank_screen();

	lin_rangex=(double)jack_sample_rate/(double)fft_size;
	log_offx=log10(lin_rangex);

	tmpd=(double)(fft_size/2-1)*(double)jack_sample_rate/(double)fft_size;
	log_rangex=log10(tmpd)-log_offx;
	log_rangex=510.0f/log_rangex;

	tmpd=2.0f*(double)jack_sample_rate/(double)fft_size;
	tmpd=log10(tmpd)-log_offx;
	tmpd=tmpd*log_rangex;
	lin_offx=tmpd;

	tmpd=(double)(fft_size/2-1)*(double)jack_sample_rate/(double)fft_size;
	log_rangex=log10(tmpd)-log_offx;
	log_rangex=(510.0f-lin_offx)/log_rangex;

//printf("DD:lin_offx=%f lin_rangex=%f  log_offx=%f log_rangex=%f\n",lin_offx,lin_rangex,log_offx,log_rangex);

//	draw phase here
	if (BUT_PHASE)
		{
		oldx=      256+(int)(audio_inp[0][ioff]*128.0f);
		oldy=disp_offy+(int)(audio_inp[1][ioff]*128.0f);
		for (i=1;i<fft_size2;i++)
			{
			off=(ioff+i)%MAX_SFRAG_SIZE;
			newx=      256+(int)(audio_inp[0][off]*128.0f);
			newy=disp_offy+(int)(audio_inp[1][off]*128.0f);
			line(oldx,oldy,newx,newy,cols[COL_INP].r,cols[COL_INP].g,cols[COL_INP].b);
			oldy=newy;
			oldx=newx;
			}
		newx=      256+(int)(audio_inp[0][ioff]*128.0f);
		newy=disp_offy+(int)(audio_inp[1][ioff]*128.0f);
		line(oldx,oldy,newx,newy,cols[COL_INP].r,cols[COL_INP].g,cols[COL_INP].b);
		}

//	draw input here
	if (BUT_INPUT)
		{
		off=(ioff-audio_roff+MAX_SFRAG_SIZE)%MAX_SFRAG_SIZE;
		oldy=disp_offy-(int)(audio_inp[audio_disp_ch][off]*disp_mul);
		diff=(double)512/(double)(fft_size2-1);
		oldpos=(int)4;
		for (i=1;i<fft_size2;i++)
			{
//			off=(ioff+i)%MAX_SFRAG_SIZE;
			off=(ioff+i-audio_roff+MAX_SFRAG_SIZE)%MAX_SFRAG_SIZE;
			newy=disp_offy-(int)(audio_inp[audio_disp_ch][off]*disp_mul);
			pos=(int)(((double)i*diff)+0.5f);
			line(oldpos,oldy,pos,newy,cols[COL_INP].r,cols[COL_INP].g,cols[COL_INP].b);
			oldy=newy;
			oldpos=pos;
			}
		}
// draw output here !!
	if (BUT_OUTPUT)
		{
		if (BUT_BYPASS)
			{
			off=(ioff-audio_roff+MAX_SFRAG_SIZE)%MAX_SFRAG_SIZE;
			oldy=disp_offy-(int)(audio_inp[audio_disp_ch][off]*disp_mul);
			diff=(double)512/(double)(fft_size2-1);
			oldpos=(int)4;
			for (i=1;i<fft_size2;i++)
				{
//				off=(ioff+i)%MAX_SFRAG_SIZE;
				off=(ioff+i-audio_roff+MAX_SFRAG_SIZE)%MAX_SFRAG_SIZE;
				newy=disp_offy-(int)(audio_inp[audio_disp_ch][off]*disp_mul);
				pos=(int)(((double)i*diff)+0.5f);
				line(oldpos,oldy,pos,newy,cols[COL_OUT].r,cols[COL_OUT].g,cols[COL_OUT].b);
				oldy=newy;
				oldpos=pos;
				}
			}
		else
			{
			oldy=disp_offy-(int)(audio_out[audio_disp_ch][ioff]*disp_mul);
			diff=(double)512/(double)(fft_size2-1);
			oldpos=(int)4;
			for (i=1;i<fft_size2;i++)
				{
				off=(ioff+i)%MAX_SFRAG_SIZE;
				newy=disp_offy-(int)(audio_out[audio_disp_ch][off]*disp_mul);
				pos=(int)(((double)i*diff)+0.5f);
				line(oldpos,oldy,pos,newy,cols[COL_OUT].r,cols[COL_OUT].g,cols[COL_OUT].b);
				oldy=newy;
				oldpos=pos;
				}
			}
		}

// now draw fft !!
// original but in block mode
	if ((BUT_FFT1) && (mmax_al1>0.0f))
		{
//		tmpd=0;
		tmpd=mmdata[iframe][0];
		tmpd=tmpd/mmax_al1;
		if (BUT_Y_LOG)
			{
			if (tmpd==0.0) tmpd=255.0f;
			else
				{
				tmpd=log_rangey*log10(tmpd);
				if (tmpd<0.0f) tmpd=0.0f;
				if (tmpd>255.0f) tmpd=255.0f;
				}
			}
		else
			{
			if (BUT_HFCOMP) tmpd=255.0f-tmpd*128.0f;
			else tmpd=255.0f-tmpd*256.0f;
			}
		oldy=tmpd+20.0f;
		diff=(double)512/(double)(fft_size2);
		oldpos=(int)diff;
		if (BUT_X_LOG)
			{
			if (BUT_BLOCK) line(0,oldy,oldpos,oldy,cols[COL_FFT1].r,cols[COL_FFT1].g,cols[COL_FFT1].b);
			else line(0,275,oldpos,oldy,cols[COL_FFT1].r,cols[COL_FFT1].g,cols[COL_FFT1].b);
			}
		else line(0,oldy,oldpos,oldy,cols[COL_FFT1].r,cols[COL_FFT1].g,cols[COL_FFT1].b);
		for (i=1;i<=fft_size2;i++)
			{
			tmpd=mmdata[iframe][i];
			tmpd=tmpd/mmax_al1;
			if (BUT_Y_LOG)
				{
				if (tmpd==0.0f) tmpd=255.0f;
				else
					{
					tmpd=log_rangey*log10(tmpd);
					if (tmpd<0.0f) tmpd=0.0f;
					if (tmpd>255.0f) tmpd=255.0f;
					}
				}
			else
				{
				if (BUT_HFCOMP) tmpd=255.0f-tmpd*(128.0f+(double)i*(double)fft_freq_disp_comp);
				else tmpd=255.0f-tmpd*256.0f;
				}
			newy=(int)(tmpd+20.0f);
			if (BUT_X_LOG)
				{
				tmpd=(double)i*lin_rangex;
				tmpd=log_rangex*(log10(tmpd)-log_offx);
				pos=(int)(lin_offx+tmpd);
				if (!BUT_BLOCK) pos=pos-(pos-oldpos)/3;
				}
			else
				{
				if (BUT_BLOCK) pos=(int)(((double)(i+1)*diff)+0.5f);
				else         pos=(int)((((double)i*diff)+diff/2.0)+0.5f);
				}
			if (BUT_BLOCK)
				{
				if (oldy!=newy) line(oldpos,oldy,oldpos,newy,cols[COL_FFT1].r,cols[COL_FFT1].g,cols[COL_FFT1].b);
//				if (oldpos!=pos) 
				line(oldpos,newy,pos,newy,cols[COL_FFT1].r,cols[COL_FFT1].g,cols[COL_FFT1].b);
				}
			else
				{
//				if (oldpos!=pos) 
				line(oldpos,oldy,pos,newy,cols[COL_FFT1].r,cols[COL_FFT1].g,cols[COL_FFT1].b);
				}
			oldy=newy;
			oldpos=pos;
			}
		}
// post fft
	if (BUT_LINK) mmax_tmp=mmax_al1;
	else mmax_tmp=mmax_al2;

	if ((BUT_FFT2) && (mmax_tmp>0))
		{
//		tmpd=0;
		tmpd=mmdata[iframe][0];
		tmpd=mod_data[0]*tmpd/mmax_tmp;
		if (BUT_Y_LOG)
			{
			if (tmpd==0.0f) tmpd=255.0f;
			else
				{
				tmpd=log_rangey*log10(tmpd);
				if (tmpd<0.0f) tmpd=0.0f;
				if (tmpd>255.0f) tmpd=255.0f;
				}
			}
		else
			{
			if (BUT_HFCOMP) tmpd=255.0f-tmpd*128.0f;
			else tmpd=255.0f-tmpd*256.0f;
			}
		oldy=tmpd+20.0f;
		diff=(double)512/(double)(fft_size2);
		oldpos=(int)diff;
		if (BUT_X_LOG)
			{
			if (BUT_BLOCK) line(0,oldy,oldpos,oldy,cols[COL_FFT2].r,cols[COL_FFT2].g,cols[COL_FFT2].b);
			else line(0,275,oldpos,oldy,cols[COL_FFT2].r,cols[COL_FFT2].g,cols[COL_FFT2].b);
			}
		else line(0,oldy,oldpos,oldy,cols[COL_FFT2].r,cols[COL_FFT2].g,cols[COL_FFT2].b);
		for (i=1;i<=fft_size2;i++)
			{
			tmpd=mmdata[iframe][i];
			tmpd=mod_data[i]*tmpd/mmax_tmp;
			if (BUT_Y_LOG)
				{
				if (tmpd==0.0f) tmpd=255.0f;
				else
					{
					tmpd=log_rangey*log10(tmpd);
					if (tmpd<0.0f) tmpd=0.0f;
					if (tmpd>255.0f) tmpd=255.0f;
					}
				}
			else
				{
				if (BUT_HFCOMP) tmpd=255.0f-tmpd*(128.0f+(double)i*(double)fft_freq_disp_comp);
				else tmpd=255.0f-tmpd*256.0f;
				}
			newy=(int)(tmpd+20.0f);
			if (BUT_X_LOG)
				{
				tmpd=(double)i*lin_rangex;
				tmpd=log_rangex*(log10(tmpd)-log_offx);
				pos=(int)(lin_offx+tmpd);
				if (!BUT_BLOCK) pos=pos-(pos-oldpos)/3;
				}
			else
				{
				if (BUT_BLOCK) pos=(int)(((double)(i+1)*diff)+0.5f);
				else pos=(int)((((double)i*diff)+diff/2.0)+0.5f);
				}
			if (BUT_BLOCK)
				{
				if (oldy!=newy) line(oldpos,oldy,oldpos,newy,cols[COL_FFT2].r,cols[COL_FFT2].g,cols[COL_FFT2].b);
//				if (oldpos!=pos) 
				line(oldpos,newy,pos,newy,cols[COL_FFT2].r,cols[COL_FFT2].g,cols[COL_FFT2].b);
				}
			else
				{
//				if (oldpos!=pos) 
				line(oldpos,oldy,pos,newy,cols[COL_FFT2].r,cols[COL_FFT2].g,cols[COL_FFT2].b);
				}
			oldy=newy;
			oldpos=pos;
			}
		}
	}

void button(int x,int y,int wid,int hgt,int on,int pic)
	{
	int i,j;
	int idx,xx,yy;
	float tmpf;

	if ((pic<0) || (pic>=ppm_count)) return;
	for (i=0;i<hgt;i++)
		{
		tmpf=i;
		tmpf=tmpf/hgt;
		tmpf=tmpf*ppm_hgt[pic];
		yy=(int)tmpf;
		for (j=0;j<wid;j++)
			{
			tmpf=j;
			tmpf=tmpf/wid;
			tmpf=tmpf*ppm_wid[pic]/2;
			xx=(int)tmpf;
			if (on) idx=((yy*ppm_wid[pic]+xx+ppm_wid[pic]/2)*4);
			else idx=((yy*ppm_wid[pic]+xx)*4);
			dot3(j+x,i+y,ppm[pic][idx],ppm[pic][idx+1],ppm[pic][idx+2]);
			}
		}
	}

void draw_status_post()
	{
	char t_str[64];

	snprintf(t_str,20,"%-19s",status_line);
	XDrawImageString(dpy,win,gc1,392,17,t_str,19);
	}

void draw_status()
	{
	int i,j;

// blank out status window
	for (i=385;i<515;i++) for (j=2;j<18;j++) dot3(i,j,0,0,0);
// draw border
	for (i=0;i<disp_off;i++)
		{
		for (j=382;j<384;j++) dot3(j,i,46,43,58);
		for (j=510;j<519;j++) dot3(j,i,46,43,58);
		}
	for (i=382;i<519;i++)
		{
		for (j=0;j<2;j++)         dot3(i,j,46,43,58);
		for (j=19;j<disp_off;j++) dot3(i,j,46,43,58);
		}
	}

void draw_controls_post()
	{
// button legends
	if (BUT_BYPASS) XDrawString(dpy,win,gc2, 11,17,"B",1); else XDrawString(dpy,win,gc3, 11,17,"B",1);
	if (BUT_GRID  ) XDrawString(dpy,win,gc2, 29,17,"G",1); else XDrawString(dpy,win,gc3, 29,17,"G",1);
	if (BUT_MOD   ) XDrawString(dpy,win,gc2, 47,17,"M",1); else XDrawString(dpy,win,gc3, 47,17,"M",1);
	if (BUT_PHASE ) XDrawString(dpy,win,gc2, 65,17,"L",1); else XDrawString(dpy,win,gc3, 65,17,"L",1);
	if (BUT_INPUT ) XDrawString(dpy,win,gc2, 83,17,"I",1); else XDrawString(dpy,win,gc3, 83,17,"I",1);
	if (BUT_FFT1  ) XDrawString(dpy,win,gc2,101,17,"F",1); else XDrawString(dpy,win,gc3,101,17,"F",1);
	if (BUT_FFT2  ) XDrawString(dpy,win,gc2,119,17,"P",1); else XDrawString(dpy,win,gc3,119,17,"P",1);
	if (BUT_OUTPUT) XDrawString(dpy,win,gc2,137,17,"O",1); else XDrawString(dpy,win,gc3,137,17,"O",1);
	if (BUT_X_LOG ) XDrawString(dpy,win,gc2,155,17,"X",1); else XDrawString(dpy,win,gc3,155,17,"X",1);
	if (BUT_Y_LOG ) XDrawString(dpy,win,gc2,173,17,"Y",1); else XDrawString(dpy,win,gc3,173,17,"Y",1);
	if (BUT_FFTAVE) XDrawString(dpy,win,gc2,191,17,"A",1); else XDrawString(dpy,win,gc3,191,17,"A",1);
	if (BUT_BLOCK ) XDrawString(dpy,win,gc2,209,17,"B",1); else XDrawString(dpy,win,gc3,209,17,"B",1);
	if (BUT_HFCOMP) XDrawString(dpy,win,gc2,227,17,"C",1); else XDrawString(dpy,win,gc3,227,17,"C",1);
	if (BUT_LINK  ) XDrawString(dpy,win,gc2,245,17,"L",1); else XDrawString(dpy,win,gc3,245,17,"L",1);
	XDrawString(dpy,win,gc1,263,17,"S",1);
	XDrawString(dpy,win,gc1,281,17,"R",1);
	}

void draw_controls()
	{
	int i,j;

	for (i=0;i<disp_off;i++)
		{
		for (j=0;j<382;j++) dot3(j,i,46,43,58);
		}
	for (i=0;i<21;i++)
		{
		if (bpc[i]==0) button(i*18+3,2,18,disp_off-2,bon[i],bpc[i]);
		else if ((bpc[i]==5) || (bpc[i]==4) || (bpc[i]==6) || (bpc[i]==7)) button(i*18+3,3,14,14,bon[i],bpc[i]);
		else button(i*18,0,18,disp_off,bon[i],bpc[i]);
		}
	}

float lindB_to_log(float inp)
	{
	float val;

	if (inp<0.0f) val=0.0f;
	else
		{
		val=-(inp-1.0)*24.0f;
		val=pow(10.0,-val/10.0);
		}
	return val;
	}

float log_to_lindB(float inp)
	{
	float val=fabs(inp);

	if (inp==0.0f) return inp;
	val=(10.0*log10(val)/24.0f)+1.0f;
	if (inp<0.0f) return -val;
	return val;
	}

void print_mod()
	{
	int oldy,newy,i;
	double offx;

	if (BUT_MOD==1)
		{
		offx=(double)512/(double)fft_size2;
		oldy=275-(int)(128.0f*log_to_lindB(mod_data[0]));
		line(0,oldy,(int)(offx),oldy,cols[COL_MOD].r,cols[COL_MOD].g,cols[COL_MOD].b);
		for (i=1;i<fft_size2;i++)
			{
			newy=275-(int)(128.0f*log_to_lindB(mod_data[i]));
			line((int)((double)i*offx),oldy,(int)((double)i*offx),newy,cols[COL_MOD].r,cols[COL_MOD].g,cols[COL_MOD].b);
			line((int)((double)i*offx),newy,(int)((double)(i+1)*offx),newy,cols[COL_MOD].r,cols[COL_MOD].g,cols[COL_MOD].b);
			oldy=newy;
			}
		}
	else if (BUT_MOD>1)
		{
// inp gain
		oldy=275-(int)(128.0f*log_to_lindB(audio_inp_gain));
		for (i=10;i<25;i++) line(i,275,i,oldy,cols[COL_MOD].r,cols[COL_MOD].g,cols[COL_MOD].b);
// out gain
		oldy=275-(int)(128.0f*log_to_lindB(audio_out_gain));
		for (i=492;i<507;i++) line(i,275,i,oldy,cols[COL_MOD].r,cols[COL_MOD].g,cols[COL_MOD].b);
		}
	}

// save settings to file ...
int save_settings(char *fname)
	{
	FILE *fp;
	char _fname[256];
	int i;
	float tmpf,freq;
	struct tm *tm_now;
	struct timeval tv; 

	if (rct_size!=fft_size)
		{
		snprintf(_fname,256,"%s.%d",fname,fft_size);
		fprintf(stderr,"xjackfreak: save_settings(%s): fft_size differs from currently loaded.\n",fname);
		fprintf(stderr,"xjackfreak: so doing a save to [%s] instead.\n",_fname);
		}
	else snprintf(_fname,256,"%s",fname);
	if ((fp=fopen(_fname,"w"))==NULL) return 1;
// date
	gettimeofday(&tv,NULL);
	tm_now=localtime((time_t*)&tv.tv_sec);
	fprintf(fp,"date=%04d/%02d/%02d\n",tm_now->tm_year+1900,tm_now->tm_mon+1,tm_now->tm_mday);
// time
	fprintf(fp,"time=%02d:%02d:%02d\n",tm_now->tm_hour,tm_now->tm_min,tm_now->tm_sec);
// fft size
	fprintf(fp,"fft_size=%d\n",fft_size);
// internal vars
	fprintf(fp,"display_ch=%d\n",audio_disp_ch);
	fprintf(fp,"log_range=%f (%3.1f dB)\n",log_rangey,5120.0f/log_rangey);
	fprintf(fp,"freq_comp=%d\n",fft_freq_disp_comp);
	switch (audio_data_window)
		{
		case 0: fprintf(fp,"dwin=0 (Hann)\n"); break;
		case 1: fprintf(fp,"dwin=1 (Bartlett)\n"); break;
		case 2: fprintf(fp,"dwin=2 (Hann #2)\n"); break;
		case 3: fprintf(fp,"dwin=3 (Welch)\n");
		case 4: fprintf(fp,"dwin=4 (square)\n");
		case 5: fprintf(fp,"dwin=5 (square-wide)\n");
		}
	switch (audio_data_merge)
		{
		case  0: fprintf(fp,"dmerge=0 (add)\n"); break;
		case  1: fprintf(fp,"dmerge=1 (xfade)\n"); break;
		case  2: fprintf(fp,"dmerge=2 (ave)\n"); break;
		case  3: fprintf(fp,"dmerge=3 (ave(a+x))\n"); break;
		case  4: fprintf(fp,"dmerge=4 (ave(a+v))\n"); break;
		case  5: fprintf(fp,"dmerge=5 (ave(x+v))\n"); break;
		case  6: fprintf(fp,"dmerge=6 (ave(a+x+v))\n"); break;
		case  7: fprintf(fp,"dmerge=7 (xfade2))\n"); break;
		}
	fprintf(fp,"ave_ratio=%d\n",fft_ave_ratio);
	fprintf(fp,"smooth_fn=%d\n",smooth_func);
	fprintf(fp,"delay_bypass=%d\n",delay_bypass);
	fprintf(fp,"max_mode=%d\n",max_mode);
	fprintf(fp,"max_decay0=%6.5f\n",mmax_all_decay0);
	fprintf(fp,"max_decay1=%6.5f\n",mmax_all_decay1);
	fprintf(fp,"max_decay2=%6.5f\n",mmax_all_decay2);
// inp gain
	tmpf=log_to_lindB(audio_inp_gain);
	fprintf(fp,"audio_inp_gain=%f (%3.1f dB)\n",audio_inp_gain,(tmpf*24.0f)-24.0f);
// out gain
	tmpf=log_to_lindB(audio_out_gain);
	fprintf(fp,"audio_out_gain=%f (%3.1f dB)\n",audio_out_gain,(tmpf*24.0f)-24.0f);

	fprintf(fp,"grid1_RGB=%3hhu,%3hhu,%3hhu (0x%02X%02X%02X)\n",cols[COL_GRD1].r,cols[COL_GRD1].g,cols[COL_GRD1].b,cols[COL_GRD1].r,cols[COL_GRD1].g,cols[COL_GRD1].b);
	fprintf(fp,"grid2_RGB=%3hhu,%3hhu,%3hhu (0x%02X%02X%02X)\n",cols[COL_GRD2].r,cols[COL_GRD2].g,cols[COL_GRD2].b,cols[COL_GRD2].r,cols[COL_GRD2].g,cols[COL_GRD2].b);
	fprintf(fp,"mod___RGB=%3hhu,%3hhu,%3hhu (0x%02X%02X%02X)\n",cols[COL_MOD ].r,cols[COL_MOD ].g,cols[COL_MOD ].b,cols[COL_MOD ].r,cols[COL_MOD ].g,cols[COL_MOD ].b);
	fprintf(fp,"input_RGB=%3hhu,%3hhu,%3hhu (0x%02X%02X%02X)\n",cols[COL_INP ].r,cols[COL_INP ].g,cols[COL_INP ].b,cols[COL_INP ].r,cols[COL_INP ].g,cols[COL_INP ].b);
	fprintf(fp,"FFT1__RGB=%3hhu,%3hhu,%3hhu (0x%02X%02X%02X)\n",cols[COL_FFT1].r,cols[COL_FFT1].g,cols[COL_FFT1].b,cols[COL_FFT1].r,cols[COL_FFT1].g,cols[COL_FFT1].b);
	fprintf(fp,"FFT2__RGB=%3hhu,%3hhu,%3hhu (0x%02X%02X%02X)\n",cols[COL_FFT2].r,cols[COL_FFT2].g,cols[COL_FFT2].b,cols[COL_FFT2].r,cols[COL_FFT2].g,cols[COL_FFT2].b);
	fprintf(fp,"outputRGB=%3hhu,%3hhu,%3hhu (0x%02X%02X%02X)\n",cols[COL_OUT ].r,cols[COL_OUT ].g,cols[COL_OUT ].b,cols[COL_OUT ].r,cols[COL_OUT ].g,cols[COL_OUT ].b);

// mod_data
	for (i=0;i<=fft_size2;i++)
		{
		tmpf=log_to_lindB(mod_data[i]);
		freq=(float)i*(float)jack_sample_rate/(float)fft_size;
		if (tmpf==-1.0f) fprintf(fp,"mod_data[%d]=%f (%7.2f Hz: -00 dB)\n",i,mod_data[i],freq);
		else             fprintf(fp,"mod_data[%d]=%f (%7.2f Hz: %3.1f dB)\n",i,mod_data[i],freq,(tmpf*24.0f)-24.0f);
		}
	fclose(fp);
	return 0;
	}

// load settings to file ...
int load_settings(char *fname)
	{
	FILE *fp;
	double tmpf;
	char *ptr,*ptr2,t_str[256];
	int num,idx,load=0;

	if ((fp=fopen(fname,"r"))==NULL) return 1;
	while (!feof(fp))
		{
		if (fgets(t_str,256,fp)==NULL) break;
		if ((ptr=strstr(t_str,"="))!=NULL)
			{
			*ptr=0;
			ptr++;
			if (strcmp(t_str,"fft_size")==0)
				{
				rct_size=atoi(ptr);
				if (debug) printf("READ: fft_size=%d\n",rct_size);
				if (rct_size==fft_size) load=1;
				else fprintf(stderr,"xjackfreak: load_settings(%s): fft_size differs (%d) from current (%d): will not load mod_data[]!\n",fname,rct_size,fft_size);
				}
			if (strcmp(t_str,"display_ch")==0)     { audio_disp_ch=atoi(ptr);      if (debug) printf("READ: display_ch=%d\n",audio_disp_ch); }
			if (strcmp(t_str,"log_range")==0)      { log_rangey=atof(ptr);         if (debug) printf("READ: log_range=%f\n",log_rangey); }
			if (strcmp(t_str,"freq_comp")==0)      { fft_freq_disp_comp=atoi(ptr); if (debug) printf("READ: freq_comp=%d\n",fft_freq_disp_comp); }
			if (strcmp(t_str,"dwin")==0)           { audio_data_window=atoi(ptr);  if (debug) printf("READ: dwin=%d\n",audio_data_window); }
			if (strcmp(t_str,"dmerge")==0)         { audio_data_merge=atoi(ptr);   if (debug) printf("READ: dmerge=%d\n",audio_data_merge); }
			if (strcmp(t_str,"ave_ratio")==0)      { fft_ave_ratio=atoi(ptr);      if (debug) printf("READ: ave_ratio=%d\n",fft_ave_ratio); }
			if (strcmp(t_str,"smooth_fn")==0)      { smooth_func=atoi(ptr);        if (debug) printf("READ: smooth_func=%d\n",smooth_func); }
			if (strcmp(t_str,"delay_bypass")==0)   { delay_bypass=atoi(ptr);       if (debug) printf("READ: delay_bypass=%d\n",delay_bypass); }
			if (strcmp(t_str,"max_mode")==0)       { max_mode=atoi(ptr);           if (debug) printf("READ: max_mode=%d\n",max_mode); }
			if (strcmp(t_str,"max_decay0")==0)     { mmax_all_decay0=atof(ptr);    if (debug) printf("READ: mmax_all_decay0=%f\n",mmax_all_decay0); }
			if (strcmp(t_str,"max_decay1")==0)     { mmax_all_decay1=atof(ptr);    if (debug) printf("READ: mmax_all_decay1=%f\n",mmax_all_decay1); }
			if (strcmp(t_str,"max_decay2")==0)     { mmax_all_decay2=atof(ptr);    if (debug) printf("READ: mmax_all_decay2=%f\n",mmax_all_decay2); }
			if (strcmp(t_str,"audio_inp_gain")==0) { audio_inp_gain=atof(ptr);     if (debug) printf("READ: audio_inp_gain=%f\n",audio_inp_gain); }
			if (strcmp(t_str,"audio_out_gain")==0) { audio_out_gain=atof(ptr);     if (debug) printf("READ: audio_out_gain=%f\n",audio_out_gain); }
			if (strcmp(t_str,"grid1_RGB")==0)      { idx=COL_GRD1; cols[idx].r=atoi(ptr); if ((ptr=strstr(ptr,","))!=NULL) { ptr++; cols[idx].g=atoi(ptr); if ((ptr=strstr(ptr,","))!=NULL) { ptr++; cols[idx].b=atoi(ptr); }}}
			if (strcmp(t_str,"grid2_RGB")==0)      { idx=COL_GRD2; cols[idx].r=atoi(ptr); if ((ptr=strstr(ptr,","))!=NULL) { ptr++; cols[idx].g=atoi(ptr); if ((ptr=strstr(ptr,","))!=NULL) { ptr++; cols[idx].b=atoi(ptr); }}}
			if (strcmp(t_str,"mod___RGB")==0)      { idx=COL_MOD;  cols[idx].r=atoi(ptr); if ((ptr=strstr(ptr,","))!=NULL) { ptr++; cols[idx].g=atoi(ptr); if ((ptr=strstr(ptr,","))!=NULL) { ptr++; cols[idx].b=atoi(ptr); }}}
			if (strcmp(t_str,"input_RGB")==0)      { idx=COL_INP;  cols[idx].r=atoi(ptr); if ((ptr=strstr(ptr,","))!=NULL) { ptr++; cols[idx].g=atoi(ptr); if ((ptr=strstr(ptr,","))!=NULL) { ptr++; cols[idx].b=atoi(ptr); }}}
			if (strcmp(t_str,"FFT1 _RGB")==0)      { idx=COL_FFT1; cols[idx].r=atoi(ptr); if ((ptr=strstr(ptr,","))!=NULL) { ptr++; cols[idx].g=atoi(ptr); if ((ptr=strstr(ptr,","))!=NULL) { ptr++; cols[idx].b=atoi(ptr); }}}
			if (strcmp(t_str,"FFT2 _RGB")==0)      { idx=COL_FFT2; cols[idx].r=atoi(ptr); if ((ptr=strstr(ptr,","))!=NULL) { ptr++; cols[idx].g=atoi(ptr); if ((ptr=strstr(ptr,","))!=NULL) { ptr++; cols[idx].b=atoi(ptr); }}}
			if (strcmp(t_str,"outputRGB")==0)      { idx=COL_OUT;  cols[idx].r=atoi(ptr); if ((ptr=strstr(ptr,","))!=NULL) { ptr++; cols[idx].g=atoi(ptr); if ((ptr=strstr(ptr,","))!=NULL) { ptr++; cols[idx].b=atoi(ptr); }}}
			if ((ptr2=strstr(t_str,"mod_data["))==t_str)
				{
				ptr2=ptr2+9;
				num=atoi(ptr2);
				tmpf=atof(ptr);
				if (debug) printf("READ: found mod_data[]: num=%d value=%f ",num,tmpf);
				if ((num>=0) && (num<MAX_FFT_DATA_SIZE))
					{
					if (load>0)
						{
						mod_data[num]=tmpf;
						real_mod_data[num]=log_to_lindB(tmpf);
						if (debug) printf("loaded mod_data[%d]=%f\n",num,mod_data[num]);
						}
					}
				else printf("\n");
				}
			}
		}
	fclose(fp);
	return 0;
	}

void smooth_mod_data2()
	{
	int i;
	float tmp_mod_data[MAX_FFT_DATA_SIZE];

	for (i=0;i<fft_size/2;i++) tmp_mod_data[i]=0;
	for (i=1;i<fft_size/2-1;i++)
		{
		tmp_mod_data[i]=((real_mod_data[i-1]/2.0f)+(real_mod_data[i])+(real_mod_data[i+1]/2.0f))/2.0f;
		}
// copy over to proper mod_data array
	for (i=1;i<fft_size/2-1;i++) real_mod_data[i]=tmp_mod_data[i];
	}

// a basic averaging ...
void smooth_mod_data1()
	{
	int i;
	float tmp_mod_data[MAX_FFT_DATA_SIZE];

	for (i=0;i<fft_size/2;i++) tmp_mod_data[i]=0;
	for (i=1;i<fft_size/2-1;i++)
		{
		tmp_mod_data[i]=(real_mod_data[i-1]+3.0f*(real_mod_data[i]-1.0f)+real_mod_data[i+1]-2.0f)/5.0f;
		}
// copy over to proper mod_data array
	for (i=1;i<fft_size/2-1;i++) real_mod_data[i]=tmp_mod_data[i]+1.0f;
	}

void smooth_mod_data0()
	{
	int i,j;
	float val,val2,pi=3.141592654;
	float tmp_mod_data[MAX_FFT_DATA_SIZE];

	for (i=0;i<fft_size/2;i++) tmp_mod_data[i]=0.0f;
// need to sum this around zero rather than 1 !!
	for (i=0;i<fft_size/2;i++)
		{
		val=real_mod_data[i]-1.0f;
//		printf("val=%f  ",val);
		for (j=1;j<10;j++)
			{
			val2=j;
			val2=val2/10.0f;
			val2=(sin(pi*val2+pi/2.0f)+1.0f)/10.0f;
			val2=val*val2/2.0f;
//			printf("j=%d %f ",j,val2);
			if (i+j<fft_size/2) tmp_mod_data[i+j]=tmp_mod_data[i+j]+val2;
			if (i-j>0)          tmp_mod_data[i-j]=tmp_mod_data[i-j]+val2;
			}
//		printf("\n");
		tmp_mod_data[i]=tmp_mod_data[i]+(val)/10;
		}
	val=0.0f;
	for (i=0;i<fft_size/2;i++)
		{
		if (tmp_mod_data[i]>val) val=tmp_mod_data[i];
		}
	if (val>0) val=max_mod/val;
	else val=1.0;
	for (i=0;i<fft_size/2;i++)
		{
		if (tmp_mod_data[i]<1.0-max_mod) tmp_mod_data[i]=1.0-max_mod;
		if (tmp_mod_data[i]>1.0) tmp_mod_data[i]=tmp_mod_data[i]*val;
		}
// copy over to proper mod_data array - don't forget to add unity into this as it's a mul op
	for (i=0;i<fft_size/2;i++) real_mod_data[i]=tmp_mod_data[i]+1.0f;
	}

void smooth_mod_data()
	{
	switch (smooth_func)
		{
		case 0: smooth_mod_data0(); break;
		case 1: smooth_mod_data1(); break;
		case 2: smooth_mod_data2(); break;
		}
	}
		
void load_mod_data()
	{
	int i,retry=0;

// convert to +-24dB range ...
	for (i=0;i<fft_size/2;i++) new_mod_data[i]=lindB_to_log(real_mod_data[i]);
	if (new_mod_data[0]>1.0) new_mod_data[0]=1.0f;
	do_update_mod_data=1;
	while (do_update_mod_data)
		{
		usleep(10000);
		retry++;
		if (retry>100)
			{
			fprintf(stderr,"xjackfreak: waiting on reload of mod_data[]. Retries exceeded! Is jack running ?\n");
			break;
			}
		}
	}

void draw_status_put()
	{
	if (gmode==3) XShmPutImage(dpy,win,gc1,im0,382,0,382,2,win_width-382-8,disp_off,True);
	else             XPutImage(dpy,win,gc1,im0,382,0,382,2,win_width-382-8,disp_off);
	}

void draw_controls_put()
	{
	if (gmode==3) XShmPutImage(dpy,win,gc1,im0,0,0,4,2,382,disp_off,True);
	else             XPutImage(dpy,win,gc1,im0,0,0,4,2,382,disp_off);
	}

void draw_main_put()
	{
	if (gmode==3) XShmPutImage(dpy,win,gc1,im0,0,disp_off,4,disp_off+2,image_width,image_height-disp_off,True);
	else             XPutImage(dpy,win,gc1,im0,0,disp_off,4,disp_off+2,image_width,image_height-disp_off);
	}

void draw_status_controls_put()
	{
	if (gmode==3) XShmPutImage(dpy,win,gc1,im0,0,0,4,2,win_width-8,disp_off,True);
	else             XPutImage(dpy,win,gc1,im0,0,0,4,2,win_width-8,disp_off);
	}

void draw_status_controls_main_put()
	{
	if (gmode==3) XShmPutImage(dpy,win,gc1,im0,0,0,4,2,image_width,image_height,True);
	else             XPutImage(dpy,win,gc1,im0,0,0,4,2,image_width,image_height);
	}

void draw_main(int all)
	{
	if (do_intro) sprintf(status_line,"%s",welcome);
	if (all) draw_controls();
	do_display();
	if (BUT_MOD!=0) print_mod();
	if (BUT_GRID) draw_grid();
	}

void load_skin(char *skin)
	{
	if (load_ppm(skin))
		{
		fprintf(stderr,"xjackfreak: Cannot load skin [%s]!\n",skin);
		exit(3);
		}
	}

double change_log_rangey(double range,int direction)
	{
	double tmpd;

	tmpd=5120.0f/range;
	     if (abs(tmpd)>=96.0) tmpd=tmpd+5.0*direction;
	else if (abs(tmpd)>=20.0) tmpd=tmpd+4.0*direction;
	else if (abs(tmpd)>=10.0) tmpd=tmpd+2.0*direction;
	else if (abs(tmpd)>=5.0 ) tmpd=tmpd+1.5*direction;
	else if (abs(tmpd)>=2.0 ) tmpd=tmpd+1.0*direction;
	else tmpd=tmpd+0.5*direction;
	return 5120.0f/tmpd;
	}

void print_edit_param(int ep)
	{
//	-data window type
//	-averaging ratio for FFT
//	-smoothing function for mod_data
//	-colours of various displays
	switch (ep)
		{
		case  0: sprintf(status_line,"display chan: %d",audio_disp_ch); break;
		case  1: sprintf(status_line,"log rang: %3.1f dB",5120.0f/log_rangey); break;
		case  2: sprintf(status_line,"freq comp: %d",fft_freq_disp_comp); break;
		case  3:
			switch (audio_data_window)
				{
				case 0: sprintf(status_line,"win0 Hann (cos)"); break;
				case 1: sprintf(status_line,"win1 Bartlett (tri)"); break;
				case 2: sprintf(status_line,"win2 Hann#2 (cos^2)"); break;
				case 3: sprintf(status_line,"win3 Welch (N^2 poly)"); break;
				case 4: sprintf(status_line,"win4 square"); break;
				case 5: sprintf(status_line,"win4 square:wide"); break;
				}
			break;
		case  4:
			switch (audio_data_merge)
				{
				case  0: sprintf(status_line,"merge 0 add"); break;
				case  1: sprintf(status_line,"merge 1 xfade"); break;
				case  2: sprintf(status_line,"merge 2 ave"); break;
				case  3: sprintf(status_line,"merge 3 ave(a+x)"); break;
				case  4: sprintf(status_line,"merge 4 ave(a+v)"); break;
				case  5: sprintf(status_line,"merge 5 ave(x+v)"); break;
				case  6: sprintf(status_line,"merge 6 ave(a+x+v)"); break;
				case  7: sprintf(status_line,"merge 7 xfade2"); break;
				}
			break;
		case  5: sprintf(status_line,"ave ratio: %d",fft_ave_ratio); break;
		case  6: sprintf(status_line,"smooth fn: %d",smooth_func); break;
		case  7: sprintf(status_line,"delay bypass: %d",delay_bypass); break;
		case  8: 
			switch (max_mode)
				{
				case 0: sprintf(status_line,"max decay0: %6.5f",mmax_all_decay0); break;
				case 1: sprintf(status_line,"max decay1: %6.5f",mmax_all_decay1); break;
				case 2: sprintf(status_line,"max decay2: %6.5f",mmax_all_decay2); break;
				}
			break;
		case  9: sprintf(status_line,"max_mode: %d",max_mode); break;
		case 10: sprintf(status_line,"grd1 RED: %3hhu %02X",cols[COL_GRD1].r,cols[COL_GRD1].r); break;
		case 11: sprintf(status_line,"grd1 GRN: %3hhu %02X",cols[COL_GRD1].g,cols[COL_GRD1].g); break;
		case 12: sprintf(status_line,"grd1 BLU: %3hhu %02X",cols[COL_GRD1].b,cols[COL_GRD1].b); break;
		case 13: sprintf(status_line,"grd2 RED: %3hhu %02X",cols[COL_GRD2].r,cols[COL_GRD2].r); break;
		case 14: sprintf(status_line,"grd2 GRN: %3hhu %02X",cols[COL_GRD2].g,cols[COL_GRD2].g); break;
		case 15: sprintf(status_line,"grd2 BLU: %3hhu %02X",cols[COL_GRD2].b,cols[COL_GRD2].b); break;
		case 16: sprintf(status_line,"mod  RED: %3hhu %02X",cols[COL_MOD ].r,cols[COL_MOD ].r); break;
		case 17: sprintf(status_line,"mod  GRN: %3hhu %02X",cols[COL_MOD ].g,cols[COL_MOD ].g); break;
		case 18: sprintf(status_line,"mod  BLU: %3hhu %02X",cols[COL_MOD ].b,cols[COL_MOD ].b); break;
		case 19: sprintf(status_line,"inp  RED: %3hhu %02X",cols[COL_INP ].r,cols[COL_INP ].r); break;
		case 20: sprintf(status_line,"inp  GRN: %3hhu %02X",cols[COL_INP ].g,cols[COL_INP ].g); break;
		case 21: sprintf(status_line,"inp  BLU: %3hhu %02X",cols[COL_INP ].b,cols[COL_INP ].b); break;
		case 22: sprintf(status_line,"FFT1 RED: %3hhu %02X",cols[COL_FFT1].r,cols[COL_FFT1].r); break;
		case 23: sprintf(status_line,"FFT1 GRN: %3hhu %02X",cols[COL_FFT1].g,cols[COL_FFT1].g); break;
		case 24: sprintf(status_line,"FFT1 BLU: %3hhu %02X",cols[COL_FFT1].b,cols[COL_FFT1].b); break;
		case 25: sprintf(status_line,"FFT2 RED: %3hhu %02X",cols[COL_FFT2].r,cols[COL_FFT2].r); break;
		case 26: sprintf(status_line,"FFT2 GRN: %3hhu %02X",cols[COL_FFT2].g,cols[COL_FFT2].g); break;
		case 27: sprintf(status_line,"FFT2 BLU: %3hhu %02X",cols[COL_FFT2].b,cols[COL_FFT2].b); break;
		case 28: sprintf(status_line,"out  RED: %3hhu %02X",cols[COL_OUT ].r,cols[COL_OUT ].r); break;
		case 29: sprintf(status_line,"out  GRN: %3hhu %02X",cols[COL_OUT ].g,cols[COL_OUT ].g); break;
		case 30: sprintf(status_line,"out  BLU: %3hhu %02X",cols[COL_OUT ].b,cols[COL_OUT ].b); break;
		}
	}

void cols_init()
	{
	memset(cols,0,sizeof(cols));
	cols[COL_GRD1].r=  0; cols[COL_GRD1].g= 40; cols[COL_GRD1].b= 90;
	cols[COL_GRD2].r= 80; cols[COL_GRD2].g= 80; cols[COL_GRD2].b= 80;
	cols[COL_MOD ].r=  0; cols[COL_MOD ].g= 50; cols[COL_MOD ].b=200;
	cols[COL_INP ].r=  0; cols[COL_INP ].g=200; cols[COL_INP ].b=  0;
	cols[COL_FFT1].r=175; cols[COL_FFT1].g=175; cols[COL_FFT1].b=  0;
	cols[COL_FFT2].r=  0; cols[COL_FFT2].g= 75; cols[COL_FFT2].b=175;
	cols[COL_OUT ].r=200; cols[COL_OUT ].g=  0; cols[COL_OUT ].b=  0;
	cols[COL_RDAT].r=  0; cols[COL_RDAT].g=200; cols[COL_RDAT].b=  0;
	cols[COL_RWIN].r=  0; cols[COL_RWIN].g=  0; cols[COL_RWIN].b=200;
	cols[COL_RINP].r=  0; cols[COL_RINP].g=200; cols[COL_RINP].b=  0;
	cols[COL_ROUT].r=200; cols[COL_ROUT].g=  0; cols[COL_ROUT].b=  0;
	}

int shm_connect_error(Display * dpy,XErrorEvent * event)
	{
	fprintf(stderr,"xjackfreak: shm_connect_error(): *** SHM FAIL! *** Try: starting with '-noshm'.\n");
	if (dpy) XCloseDisplay(dpy);
	exit(4);
	return 0;
	}

void usage(FILE *fp)
	{
	fprintf(fp,"Usage: xjackfreak [options]\n");
	fprintf(fp,"-fps <fps>   - set frame rate\n");
	fprintf(fp,"-debug       - debug mode\n");
	fprintf(fp,"-?/-h/--help - print help\n");
	fprintf(fp,"-n <num>     - size of fft (def=1024 samples) - must be power of 2 eg 128,256\n");
	fprintf(fp,"-jack        - dont connect to jack\n");
	fprintf(fp,"-rc          - specify an RC file to load/save\n");
	fprintf(fp,"-noshm       - dont use X11 SHM\n");
	fprintf(fp,"-mono        - mono mode\n");
	fprintf(fp,"\nXwindows key commands while running:\n");
	fprintf(fp,"q,Q,^C,ESC   - quit\n");
	fprintf(fp,"n            - print a frame while stopped\n");
	fprintf(fp,"+            - increase display frame rate\n");
	fprintf(fp,"-            - decrease display frame rate\n");
	fprintf(fp,"<space_bar>  - start/stop display framing\n");
	}

int trans_butx(int butx)
	{
	float tmpf;
	int i;

	tmpf=512.0f/(float)(fft_size2);
//	tmpf=(float)(butx-4)/tmpf;
	tmpf=(float)(butx-5)/tmpf;
	i=(int)tmpf;
	if (i<0) i=0;
	return i;
	}

float trans_buty(int buty)
	{
	float tmpf;

	if (buty>256+24) tmpf=-1.0;
	else
		{
		tmpf=256.0f-(float)(buty-24);
		tmpf=tmpf/256.0f;
		tmpf=tmpf*max_mod;
		}
	return tmpf;
	}

int set_input_gain(float lev)
	{
	if (abs(lev)<=2.0f)
		{
		if (lev<0.0f) sprintf(status_line,"inp gain: -00 dB");
		else sprintf(status_line,"inp gain: %3.1f dB",(lev*24.0f)-24.0f);
		audio_inp_gain=lindB_to_log(lev);
		return 0;
		}
	return 1;
	}

int set_output_gain(float lev)
	{
	if (abs(lev)<=2.0f)
		{
		if (lev<0.0f) sprintf(status_line,"out gain: -00 dB");
		else sprintf(status_line,"out gain: %3.1f dB",(lev*24.0f)-24.0f);
		audio_out_gain=lindB_to_log(lev);
		return 0;
		}
	return 1;
	}

int process_xevent(	XEvent *evt)
	{
	KeySym keysym;
	char c;
	int i,j,k,ret=0,but,but_x,but_y,but_ch,startx,endx,inc;
	double freq,tmpf,diff,starty,endy;
	double tmpd,lin_rangex,lin_offx,log_rangex,log_offx;

	lin_rangex=(double)jack_sample_rate/(double)fft_size;
	log_offx=log10(lin_rangex);

	tmpd=(double)(fft_size/2-1)*(double)jack_sample_rate/(double)fft_size;
	log_rangex=log10(tmpd)-log_offx;
	log_rangex=510.0f/log_rangex;

	tmpd=2.0f*(double)jack_sample_rate/(double)fft_size;
	tmpd=log10(tmpd)-log_offx;
	tmpd=tmpd*log_rangex;
	lin_offx=tmpd;

	tmpd=(double)(fft_size/2-1)*(double)jack_sample_rate/(double)fft_size;
	log_rangex=log10(tmpd)-log_offx;
	log_rangex=(510.0f-lin_offx)/log_rangex;

//printf("PE:lin_offx=%f lin_rangex=%f  log_offx=%f log_rangex=%f\n",lin_offx,lin_rangex,log_offx,log_rangex);

	if (debug) printf("process_xevent(*evt):\n");
	switch (evt->type)
		{
		case ReparentNotify:
			if (debug) printf("ReparentNotify:\n");
			break;
		case VisibilityNotify:
			if (debug) printf("VisibilityNotify:\n");
			break;
		case DestroyNotify:
			if (debug) printf("DestroyNotify:\n");
			win_mapped=0;
			break;
		case MapNotify:
			if (debug) printf("MapNotify:\n");
			win_mapped=1;
			pentry_add(1,0);
			pentry_add(2,0);
			pentry_add(4,0);
			break;
		case UnmapNotify:
			if (debug) printf("UnmapNotify:\n");
			win_mapped=0;
			break;
		case EnterNotify:
			if (debug) printf("EnterNotify:\n");
			break;
		case LeaveNotify:
			if (debug) printf("LeaveNotify:\n");
			break;
		case FocusIn:
			if (debug) printf("FocusIn:\n");
			break;
		case FocusOut:
			if (debug) printf("FocusOut:\n");
			break;
		case ResizeRequest:     
			if (debug) printf("ResizeRequest:\n");
			break;
		case KeyPress:
			if (debug) printf("KeyPress:\n");
			c=0;
			XLookupString (&evt->xkey, &c, 1, &keysym, 0);
			if (debug) printf("key: %c\n",c);
			switch (c)
				{
				case 'q':
				case 'Q':
				case 3:   // ^C
				case 27:  // ESC
					if (running) running=0;
					ret=1;
					break;
				case 'd':
					disp_max=(disp_max+1)%2;
					if (disp_max==0) printf("\r                               \r");
					break;
				case 'm':
					max_mode=(max_mode+1)%3;
					break;
				case 'n':
					if (!running)
						{
						if ((BUT_FFT1) && (BUT_RECBUF))
							{
							draw_rec_buf(0,0);
							}
						else
							{
							sprintf(status_line,"snapshot");
							BUT_RECBUF=0;
							}
						pentry_add(4,0);
						}
					break;
				case '+':
					fps=fps+1;
					frame_rate=1000000/fps;
					if (debug) printf("fps=%3.3f frame_rate=%ld\n",fps,frame_rate);
					do_intro=0;
					sprintf(status_line,"refresh: %3.1f fps",fps);
					pentry_add(2,0);
					break;
				case '-':
					fps=fps-1;
					if (fps<1) fps=1;
					frame_rate=1000000/fps;
					if (debug) printf("fps=%3.3f frame_rate=%ld\n",fps,frame_rate);
					do_intro=0;
					sprintf(status_line,"refresh: %3.1f fps",fps);
					pentry_add(2,0);
					break;
		    	case ' ':
		    		do_intro=0;
					running=(running+1)%2;
					if (debug) printf("running=%d\n",running);
					do_intro=0;
					if (running) sprintf(status_line,"running");
					else         sprintf(status_line,"stopped");
					pentry_add(2,0);
				}
			break;
		case ClientMessage:
			if (debug) printf("ClientMessage:\n");
			if (evt->xclient.data.l[0]==wm_delete_window)
				{
				if (debug) printf("Delete_window event caught!\n");
				if (running) running=0;
				ret=1;
				}
			break;
		case Expose:
			if (debug) printf("Expose event caught! cnt=%d\n",evt->xexpose.count);
			pentry_add(1,0);
			pentry_add(2,0);
			pentry_add(4,0);
			break;
		case ConfigureNotify:
			if (debug) printf("ConfigreNotify event caught|\n");
			pentry_add(1,0);
			pentry_add(2,0);
			pentry_add(4,0);
			break;
		case MotionNotify:
			if (debug) printf("MotionNotify:\n");
			but=evt->xbutton.button;
			but_x=evt->xbutton.x;
			but_y=evt->xbutton.y;
			but_ch=(but_x-4)/18;
			if ((but_x>=4) && (but_x<520) && (but_y>24) && (but_y<284) && (!BUT_RECBUF))
				{
				if (BUT_MOD==1)
					{
					i=trans_butx(but_x);
					if ((i>=0) && (i<=fft_size2))
						{
						freq=(double)i*(double)jack_sample_rate/(double)fft_size;
						tmpf=real_mod_data[i];
						if (tmpf==0.0f) sprintf(status_line,"%7.1f Hz -00 dB",freq);
						else sprintf(status_line,"%7.1f Hz %3.1f dB",freq,(tmpf*24.0f)-24.0f);
						draw_status_post();
						}
					}
				else if (BUT_MOD==2)
					{
					i=trans_butx(but_x);
					if ((but_x>=4) && (but_x<260))
						{
						tmpf=log_to_lindB(audio_inp_gain);
						if (tmpf<0.0f) sprintf(status_line,"inp gain: -00 dB");
						else sprintf(status_line,"inp gain: %3.1f dB",(tmpf*24.0f)-24.0f);
						draw_status_post();
						}
					else if ((but_x>=261) && (but_x<=516))
						{
						tmpf=log_to_lindB(audio_out_gain);
						if (tmpf<0.0f) sprintf(status_line,"out gain: -00 dB");
						else sprintf(status_line,"out gain: %3.1f dB",(tmpf*24.0f)-24.0f);
						draw_status_post();
						}
					}
				else if ((BUT_FFT1==1) || (BUT_FFT2==1))
					{
					i=trans_butx(but_x);
					if ((but_x>=4) && (but_x<=516))
						{
						if (BUT_X_LOG)
							{
							if (but_x-4<=512/fft_size2) i=0;
							else
								{
								tmpd=(double)(but_x-4)-lin_offx;
								tmpd=pow(10.0f,tmpd/log_rangex+log_offx);
								i=1+(int)(tmpd/lin_rangex);
								}
							}
						freq=(double)i*(double)jack_sample_rate/(double)fft_size;
						sprintf(status_line,"%7.2f Hz",freq);
						draw_status_post();
						}
					}
				}
			break;
		case ButtonPress:
			if (debug) printf("ButtonPress:\n");
			but=evt->xbutton.button;
			but_x=evt->xbutton.x;
			but_y=evt->xbutton.y;
			// wts.width / wts.height
			but_ch=(but_x-4)/18;
			old_but_x=but_x;
			old_but_y=but_y;
//					if (debug) fprintf(stderr,"ButtonPress  : But=%d X=%d Y=%d CH=%d\n",but,but_x,but_y,but_ch);
			break;
		case ButtonRelease:
			if (debug) printf("ButtonRelease:\n");
			but=evt->xbutton.button;
			but_x=evt->xbutton.x;
			but_y=evt->xbutton.y;
			but_ch=(but_x-4)/18;
			if (debug) printf("but=%d  but.x/y=%d/%d but_ch=%d\n",but,but_x,but_y,but_ch);
			if ((but_ch>=0) && (but_ch<21) && (but_y>0) && (but_y<21))
				{
				if (but_ch==14)	// SMOOTH
					{
					smooth_mod_data();
					load_mod_data();
					do_intro=0;
					sprintf(status_line,"smooth MOD");
					}
				else if (but_ch==15)	// RESET
					{
					for (i=0;i<=fft_size/2;i++) real_mod_data[i]=1.0f;
					load_mod_data();
					audio_inp_gain=1.0f;
					audio_out_gain=1.0f;
					do_intro=0;
					sprintf(status_line,"reset MOD");
					}
				else if (but_ch==16)	// RECBUF
					{
					if (BUT_RECBUF==0)
						{
						do_intro=0;
						draw_rec_frame=rec_frame;
						rec_off=jack_frame_size-fft_size;
						old_running=running;
						running=0;
					//  if (debug) printf("running=%d\n",running);
						rec_off=0;
						draw_rec_buf(draw_rec_frame,rec_off);
						BUT_RECBUF=1;
						}
					else
						{
						BUT_RECBUF=0;
						running=old_running;
						}
					pentry_add(4,1);
					}
				else if (but_ch==17)	// LEFT
					{
					do_intro=0;
					if (BUT_RECBUF)
						{
						rec_off=(rec_off-jack_frag_size/8);
						if (rec_off<0)
							{
							draw_rec_frame--;
							if (draw_rec_frame<0) draw_rec_frame=(MAX_REC_BUF/jack_frag_size)-1;
							rec_off=(rec_off+jack_frag_size)%jack_frag_size;
							}
						draw_rec_buf(draw_rec_frame,rec_off);
						}
					else
						{
						edit_param=(edit_param-1+EDIT_PARAM_MAX)%EDIT_PARAM_MAX;
						print_edit_param(edit_param);
						}
					}
				else if (but_ch==18)	// RIGHT
					{
					do_intro=0;
					if (BUT_RECBUF)
						{
						rec_off=(rec_off+jack_frag_size/8);
						if (rec_off>=jack_frag_size)
							{
							draw_rec_frame++;
							if (draw_rec_frame>=MAX_REC_BUF/jack_frag_size) draw_rec_frame=0;
							rec_off=0;
							}
						draw_rec_buf(draw_rec_frame,rec_off);
						}
					else
						{
						edit_param=(edit_param+1)%EDIT_PARAM_MAX;
						print_edit_param(edit_param);
						}
					}
				else if (but_ch==19)	// UP
					{
					do_intro=0;
					switch (edit_param)
						{
						case 0: audio_disp_ch=(audio_disp_ch+1)%2; break;
						case 1:
							log_rangey=change_log_rangey(log_rangey,-1);
							BUT_DOWN=1;
							if (log_rangey>-10.0f)
								{
								log_rangey=-10.0f;
								BUT_UP=0;
								}
							if (debug)
								{
								printf("log_rangey=%3.3f\n",log_rangey);
								fflush(stdout);
								}
							break;
						case 2: fft_freq_disp_comp++; break;
						case 3: audio_data_window=(audio_data_window+1)%6; do_update_data_window=1; break;
						case 4: audio_data_merge=(audio_data_merge+1)%8; break;
						case 5: fft_ave_ratio++; break;
						case 6: smooth_func=(smooth_func+1)%3; break;
						case 7: delay_bypass=(delay_bypass+1)%2; break;
						case 8: 
							switch (max_mode)
								{
								case 0: mmax_all_decay0=mmax_all_decay0+0.0001f; if (mmax_all_decay0>1.0f) mmax_all_decay0=1.0f; break;
								case 1: mmax_all_decay1=mmax_all_decay1+0.0001f; if (mmax_all_decay1>1.0f) mmax_all_decay1=1.0f; break;
								case 2: mmax_all_decay2=mmax_all_decay2+0.0001f; if (mmax_all_decay2>1.0f) mmax_all_decay2=1.0f; break;
								}
							break;
						case 9: max_mode=(max_mode+1)%3; break;
						default:
							k=edit_param-8;
							if (k%3==0) { cols[k/3].r=(cols[k/3].r+1)%256; printf("cols[%d].r=%3hhu %02X\n",k/3,cols[k/3].r,cols[k/3].r); }
							else if (k%3==1) { cols[k/3].g=(cols[k/3].g+1)%256; printf("cols[%d].g=%3hhu %02X\n",k/3,cols[k/3].g,cols[k/3].g); }
							else { cols[k/3].b=(cols[k/3].b+1)%256;  printf("cols[%d].b=%3hhu %02X\n",k/3,cols[k/3].b,cols[k/3].b); }
							break;
						}
					do_intro=0;
					print_edit_param(edit_param);
					}
				else if (but_ch==20)	// DOWN
					{
					do_intro=0;
					switch (edit_param)
						{
						case 0: audio_disp_ch=(audio_disp_ch+1)%2; break;
						case 1:
							log_rangey=change_log_rangey(log_rangey,1);
							BUT_UP=1;
							if (log_rangey<=-5010.0f)
								{
								log_rangey=-5010.0f;
								BUT_DOWN=0;
								}
							if (debug)
								{
								printf("log_rangey=%3.3f\n",log_rangey);
								fflush(stdout);
								}
							break;
						case 2: if (fft_freq_disp_comp>0) fft_freq_disp_comp--; break;
						case 3: audio_data_window=(audio_data_window+5)%6; do_update_data_window=1; break;
						case 4: audio_data_merge=(audio_data_merge+7)%8; break;
						case 5: if (fft_ave_ratio>1) fft_ave_ratio--; break;
						case 6: smooth_func=(smooth_func-1+3)%3; break;
						case 7: delay_bypass=(delay_bypass+1)%2; break;
						case 8: 
							switch (max_mode)
								{
								case 0: mmax_all_decay0=mmax_all_decay0-0.0001f; if (mmax_all_decay0<0.0f) mmax_all_decay0=0.0f; break;
								case 1: mmax_all_decay1=mmax_all_decay1-0.0001f; if (mmax_all_decay1<0.0f) mmax_all_decay1=0.0f; break;
								case 2: mmax_all_decay2=mmax_all_decay2-0.0001f; if (mmax_all_decay2<0.0f) mmax_all_decay2=0.0f; break;
								}
							break;
						case 9: max_mode=(max_mode+2)%3; break;
						default:
							k=edit_param-8;
							if (k%3==0) { cols[k/3].r=(cols[k/3].r+255)%256; printf("cols[%d].r=%3hhu %02X\n",k/3,cols[k/3].r,cols[k/3].r); }
							else if (k%3==1) { cols[k/3].g=(cols[k/3].g+255)%256; printf("cols[%d].g=%3hhu %02X\n",k/3,cols[k/3].g,cols[k/3].g); }
							else { cols[k/3].b=(cols[k/3].b+255)%256;  printf("cols[%d].b=%3hhu %02X\n",k/3,cols[k/3].b,cols[k/3].b); }
							break;
						}
					do_intro=0;
					print_edit_param(edit_param);
					}
				else
					{
					do_intro=0;
					if (but_ch==2) bon[but_ch]=(bon[but_ch]+1)%3;
					else bon[but_ch]=(bon[but_ch]+1)%2;
					switch (but_ch)
						{
						case 0:
							if (bon[but_ch]) sprintf(status_line,"bypass ON");
							else sprintf(status_line,"bypass OFF");
							break;
						case 1:
							if (bon[but_ch]) sprintf(status_line,"grid ON");
							else sprintf(status_line,"grid OFF");
							break;
						case 2:
							if (bon[but_ch]) sprintf(status_line,"display MOD ON");
							else sprintf(status_line,"display MOD OFF");
							break;
						case 3:
							if (bon[but_ch]) sprintf(status_line,"phase ON");
							else sprintf(status_line,"phase OFF");
							break;
						case 4:
							if (bon[but_ch]) sprintf(status_line,"input wave ON");
							else sprintf(status_line,"input wave OFF");
							break;
						case 5:
							if (bon[but_ch]) sprintf(status_line,"input FFT ON");
							else sprintf(status_line,"input FFT OFF");
							break;
						case 6:
							if (bon[but_ch]) sprintf(status_line,"output FFT ON");
							else sprintf(status_line,"output FFT OFF");
							break;
						case 7:
							if (bon[but_ch]) sprintf(status_line,"output wave ON");
							else sprintf(status_line,"output wave OFF");
							break;
						case 8:
							if (bon[but_ch]) sprintf(status_line,"X log");
							else sprintf(status_line,"X lin");
							break;
						case 9:
							if (bon[but_ch]) sprintf(status_line,"Y log");
							else sprintf(status_line,"Y lin");
							break;
						case 10:
							if (bon[but_ch]) sprintf(status_line,"ave FFT ON");
							else sprintf(status_line,"ave FFT OFF");
							break;
						case 11:
							if (bon[but_ch]) sprintf(status_line,"block mode");
							else sprintf(status_line,"line mode");
							break;
						case 12:
							if (bon[but_ch]) sprintf(status_line,"freq comp ON");
							else sprintf(status_line,"freq comp OFF");
							break;
						case 13:
							if (bon[but_ch]) sprintf(status_line,"link ON");
							else sprintf(status_line,"link OFF");
//							do_update_fft_filter=1;
							break;
						}
					}
				}
			else if ((!BUT_RECBUF) && (BUT_MOD==1) && (but_y>24) && (but_y<284))
				{
				if (but_x!=old_but_x)
					{
					if (old_but_y==but_y)
						{
						starty=trans_buty(but_y);
						startx=trans_butx(old_but_x);
						endx  =trans_butx(but_x);
						if (startx>endx) { i=endx; j=startx; }
						else { i=startx; j=endx; }
						while (i<=j)
							{
							if ((i>=0) && (i<fft_size2))
								{
								real_mod_data[i]=starty;
//								printf("A: real_mod_data[%d]=%f\n",i,starty);
								}
							i++;
							}
						load_mod_data();
						}
					else
						{
						startx=trans_butx(old_but_x);
						endx  =trans_butx(but_x);
						if (startx>endx) { i=endx;   j=startx; inc=-1; }
						else             { i=startx; j=endx;   inc=1; }
						starty=trans_buty(old_but_y);
						endy  =trans_buty(but_y);
						diff=(starty-endy);
						if (i-j!=0) diff=diff/(double)(i-j);
						if (debug) printf("i=%d j=%d inc=%d diff=%f\n",i,j,inc,diff);
						while (i<=j)
							{
							if ((i>=0) && (i<fft_size2))
								{
								if (inc<0) tmpf=  endy - diff * (i-endx);
								else       tmpf=starty + diff * (i-startx);
								real_mod_data[i]=tmpf;
//								printf("B: real_mod_data[%d]=%f\n",j,tmpf);
								}
							i++;
							}
						load_mod_data();
						}
					}
				else
					{
					i=trans_butx(but_x);
					if ((i>=0) && (i<fft_size2))
						{
						freq=(float)i*(float)jack_sample_rate/(float)fft_size;
						if (but==1)
							{
							tmpf=trans_buty(but_y);
							real_mod_data[i]=tmpf;
//							printf("C: real_mod_data[%d]=%f\n",i,tmpf);
							if (tmpf==0.0f) sprintf(status_line,"%7.2f Hz: -00 dB",freq);
							else sprintf(status_line,"%7.2f Hz: %3.1f dB",freq,(tmpf*24.0f)-24.0f);
							load_mod_data();
							}
						else if (but==4)	// up
							{
							tmpf=real_mod_data[i];
							if (tmpf==0.0f) tmpf=1.0f/128.0f;
							else tmpf=tmpf+0.01f;
							real_mod_data[i]=tmpf;
//							printf("D: read_mod_data[%d]=%f\n",i,tmpf);
							if (tmpf==0.0f) sprintf(status_line,"%7.2f Hz: -00 dB",freq);
							else sprintf(status_line,"%7.2f Hz: %3.1f dB",freq,(tmpf*24.0f)-24.0f);
							load_mod_data();
							}
						else if (but==5)	// down
							{
							tmpf=real_mod_data[i];
							if (tmpf==0.0f) tmpf=1.0f/128.0f;
							else tmpf=tmpf-0.01f;
							real_mod_data[i]=tmpf;
//							printf("E: read_mod_data[%d]=%f\n",i,tmpf);
							if (tmpf==0.0f) sprintf(status_line,"%7.2f Hz: -00 dB",freq);
							else sprintf(status_line,"%7.2f Hz: %3.1f dB",freq,(tmpf*24.0f)-24.0f);
							load_mod_data();
							}
						}
					}
				}
			else if ((!BUT_RECBUF) && (BUT_MOD==2)  && (but_x<255) && (but_y>24) &&(but_y<284))
				{
//				printf("here...%d>%d ?\n",but_x,old_but_x);
				if (but==1)
					{
					tmpf=trans_buty(but_y);
					set_input_gain(tmpf);
					}
				else if (but==4)	// up
					{
					tmpf=log_to_lindB(audio_inp_gain);
					if (tmpf<2.0f) tmpf=tmpf+0.01f;
					set_input_gain(tmpf);
					}
				else if (but==5)	// down
					{
					tmpf=log_to_lindB(audio_inp_gain);
					if (tmpf>0.0f) tmpf=tmpf-0.01f;
					set_input_gain(tmpf);
					}
				}
			else if ((!BUT_RECBUF) && (BUT_MOD==2)  && (but_x>=260) && (but_y>24) &&(but_y<284))
				{
				if (but==1)
					{
					tmpf=trans_buty(but_y);
					set_output_gain(tmpf);
					}
				else if (but==4)	// up
					{
					tmpf=log_to_lindB(audio_out_gain);
					if (tmpf<2.0f) tmpf=tmpf+0.01f;
					set_output_gain(tmpf);
					}
				else if (but==5)	// down
					{
					tmpf=log_to_lindB(audio_out_gain);
					if (tmpf>0.0f) tmpf=tmpf-0.01f;
					if (tmpf<0.0f) tmpf=0.0f;
					set_output_gain(tmpf);
					}
				}
			else if ((BUT_RECBUF) && (but_x>=0) && (but_x<512) && (but_y>21) &&(but_y<276)) draw_rec_buf_zoom(rec_frame,rec_off,but_x,but_y-20);
			else do_intro=1;
			if (debug) fprintf(stderr,"ButtonRelease: But=%d X=%d Y=%d CH=%d\n",but,but_x,but_y,but_ch);
			pentry_add(2,0);
			if (!running) pentry_add(4,0);
			break;
		default:
			if (debug) printf("default:\n");
		}
	return ret;
	}

Bool predicate(Display * display,XEvent * event,XPointer args)
	{
	return True;
	}

int main(int argc, char *argv[])
	{
	XWindowAttributes wts;
	Visual *vis1;
	XSizeHints *xsh;
	XEvent event;
	fd_set set;
	char fname[256],t_str[256];
	void *old_handler;
	int CompletionType=0,i,j,ret=0,bye=0,use_jack=1;
	long tmpl,ppid;

	memset(&shminfo,0,sizeof(shminfo));
	fname[0]=0;
	sprintf(fname,"xjackfreak.rc");
	while (argc>1)
		{
		if ((strcmp(argv[1],"-h")==0) || (strcmp(argv[1],"-?")==0) || (strcmp(argv[1],"-help")==0) || (strcmp(argv[1],"--help")==0))
			{
			usage(stdout);
			exit(0);
			}
		if ((argc>2) && (strcmp(argv[1],"-fps")==0))
			{
			sprintf(t_str,"%s",argv[2]);
			tmpl=atol(t_str);
			fps=tmpl;
			frame_rate=1000000/tmpl;
			if (debug) printf("frame_rate=%ld fps\n",frame_rate);
			argc-=2;
			argv+=2;
			}
		else if ((argc>2) && (strcmp(argv[1],"-n")==0))
			{
			sprintf(t_str,"%s",argv[2]);
			fft_size=atoi(t_str);
			if (fft_size<8) fft_size=8;
			if (fft_size>4096) fft_size=4096;
			fft_size2=fft_size/2;
			fft_size4=fft_size/4;
			if (debug) printf("fft_size=%d samples\n",fft_size);
			argc-=2;
			argv+=2;
			}
		else if (strcmp(argv[1],"-debug")==0)
			{
			debug=1;
			argc--;
			argv++;
			}
		else if (strcmp(argv[1],"-jack")==0)
			{
			use_jack=0;
			argc--;
			argv++;
			}
		else if (strcmp(argv[1],"-mono")==0)
			{
			stereo_mode=0;
			argc--;
			argv++;
			}
		else if ((argc>2) && (strcmp(argv[1],"-freq_comp")==0))
			{
			fft_freq_disp_comp=atoi(argv[2]);
			if (fft_freq_disp_comp<0) fft_freq_disp_comp=1;
			if (fft_freq_disp_comp>255) fft_freq_disp_comp=255;
			argc-=2;
			argv+=2;
			}
		else if ((argc>2) && (strcmp(argv[1],"-rc")==0))
			{
			sprintf(fname,"%s",argv[2]);
			if (debug) printf("rc_file=[%s]\n",fname);
			argc-=2;
			argv+=2;
			}
		else if (strcmp(argv[1],"-noshm")==0)
			{
			if (wmode==3) wmode=2;
			if (debug) printf("NO SHM!\n");
			argc--;
			argv++;
			}
		else
			{
			fprintf(stderr,"unknown arg: \"%s\"\n",argv[1]);
			exit(1);
			}
		}

	cols_init();

	fft_places=calc_places(fft_size);
	if (debug) printf("fft_size=%d (%d)\n",fft_size,fft_places);
	i=fft_places;
	j=1;
	while (i) { j=j<<1; i--; }
	if (j!=fft_size)
		{
		fprintf(stderr,"xjackfreak: N must be power of 2 eg 32,64,.. \n");
		        printf("xjackfreak: N must be power of 2 eg 32,64,.. \n");
		exit(2);
		}

	ppid=(long)getpid();

// zero buffers 'n' stuff ..
	memset(outr,0,sizeof(outr));
	memset(outi,0,sizeof(outi));
	memset(mmdata,0,sizeof(mmdata));
	memset(mod_data,0,sizeof(mod_data));
	memset(real_mod_data,0,sizeof(real_mod_data));
	memset(new_mod_data,0,sizeof(new_mod_data));
	memset(bon,0,sizeof(bon));
	memset(bpc,0,sizeof(bpc));

	for (i=0;i<MAX_FFT_DATA_SIZE;i++)
		{
		mod_data[i]=1.0;
		real_mod_data[i]=1.0;
		new_mod_data[i]=1.0;
		}

	load_settings(fname);

	data0=0;
// init FFT stuff
	W_init(fft_size);

	load_skin(__PREFIX__"/etc/xjackfreak/led-grn-01.ppm");
	load_skin(__PREFIX__"/etc/xjackfreak/button03c.ppm");
	load_skin(__PREFIX__"/etc/xjackfreak/button04c.ppm");
	load_skin(__PREFIX__"/etc/xjackfreak/button05d.ppm");
	load_skin(__PREFIX__"/etc/xjackfreak/tributU01.ppm");
	load_skin(__PREFIX__"/etc/xjackfreak/tributD01.ppm");
	load_skin(__PREFIX__"/etc/xjackfreak/tributL01.ppm");
	load_skin(__PREFIX__"/etc/xjackfreak/tributR01.ppm");
// connect to jack
	if (use_jack)
		{
		sprintf(t_str,"xjackfreak-%ld",ppid);
		if (connect_to_jack(t_str,stereo_mode)) exit(5);
		if (register_jack_ports(1+stereo_mode)) exit(6);
		if (activate_jack()) exit(7);
		}
	if (debug) printf("WN: win_width =%d win_height =%d\n",win_width,win_height);
	sprintf(title,"xjackfreak-%ld",ppid);
	tptr=&title[0];
	if (debug) printf("Open X11 scr...\n");
	dpy=XOpenDisplay(NULL);
	scr=DefaultScreenOfDisplay(dpy);
   CompletionType=XShmGetEventBase(dpy)+ShmCompletion;

	if (wmode==3)
		{
		if (XShmQueryExtension(dpy)==False)
			{
			printf("SHM unavailable.\n");
			wmode=2;
			}
		}

// Get attributes
	if (debug)
		{
		printf("DefaultDepthOfScreen: %d\n",DefaultDepthOfScreen(scr));
		printf("CellsOfScreen: %d\n",CellsOfScreen(scr));
		printf("XScreenNumberOfScreen: %d\n",XScreenNumberOfScreen(scr));
		printf("HeightOfScreen: %d\n",HeightOfScreen(scr));
		printf("HeightMMOfScreen: %d\n",HeightMMOfScreen(scr));
		printf("PlanesOfScreen: %d\n",PlanesOfScreen(scr));
		printf("WidthOfScreen: %d\n",WidthOfScreen(scr));
		printf("WidthMMOfScreen: %d\n",WidthMMOfScreen(scr));
		printf("WhiteOfScreen: 0x%08lx\n",WhitePixelOfScreen(scr));
		printf("BlackOfScreen: 0x%08lx\n",BlackPixelOfScreen(scr));
		}
	if (debug) printf("Creating X11 window...\n");
	win=XCreateSimpleWindow(dpy,RootWindowOfScreen(scr),0,0,win_width,win_height,1,BlackPixelOfScreen(scr),BlackPixelOfScreen(scr));

	xsh=XAllocSizeHints();
	xsh->flags=PMinSize|PMaxSize;
	xsh->min_width=win_width;
	xsh->min_height=win_height;
	xsh->max_width=win_width;
	xsh->max_height=win_height;
	XSetWMNormalHints(dpy,win,xsh);
	XFree(xsh);

	if (debug) printf("Getting window attriubtes...\n");
	XGetWindowAttributes(dpy,win,&wts);

	XStringListToTextProperty(&tptr,1,&text_prop);
	XSetWMName(dpy,win,&text_prop);

	if (debug) printf("Selecting event input mask...\n");
	XSelectInput(dpy,win,wts.your_event_mask|EVENT_MASK);

// look for a useful visual
	if (debug) printf("doing visual_init()...\n");
	vis1=visual_init();

// we should have the display depth here ...
	if ((display_bits!=16) && (display_bits!=24) && (display_bits!=32))
		{
		fprintf(stderr,"Sorry, can only handle 16 or 24/32 bpp!\n");
		exit(8);
		}

	if (debug) printf("Creating graphic context...\n");
	white=WhitePixelOfScreen(scr);
	black=BlackPixelOfScreen(scr);
/*
	green=0x4d9e00;
	yellow=0x7d7100;
	red=0x7d0c00;
*/
	xgcvalues1.foreground=white;
	xgcvalues1.background=black;
	gc1=XCreateGC(dpy,win,GCForeground|GCBackground,&xgcvalues1);

	xgcvalues2.foreground=black;
	xgcvalues2.background=black;
	gc2=XCreateGC(dpy,win,GCForeground|GCBackground,&xgcvalues2);

	xgcvalues3.foreground=0x0d010a;
//	xgcvalues3.foreground=0xede1a0;
//	xgcvalues3.foreground=white;
	xgcvalues3.background=black;
	gc3=XCreateGC(dpy,win,GCForeground|GCBackground,&xgcvalues3);

// setting font etc
//	XLoadFont(dpy,"helv*");

	if (debug)
		{
		XGetGCValues(dpy,gc1,GCForeground,&xgcvalues1);
		printf("GC foreground: 0x%08lx\n",xgcvalues1.foreground);
		XGetGCValues(dpy,gc1,GCBackground,&xgcvalues1);
		printf("GC background: 0x%08lx\n",xgcvalues1.background);

		XGetGCValues(dpy,gc2,GCForeground,&xgcvalues2);
		printf("GC foreground: 0x%08lx\n",xgcvalues2.foreground);
		XGetGCValues(dpy,gc2,GCBackground,&xgcvalues2);
		printf("GC background: 0x%08lx\n",xgcvalues2.background);

		XGetGCValues(dpy,gc3,GCForeground,&xgcvalues2);
		printf("GC foreground: 0x%08lx\n",xgcvalues2.foreground);
		XGetGCValues(dpy,gc3,GCBackground,&xgcvalues2);
		printf("GC background: 0x%08lx\n",xgcvalues2.background);
		}

	if (wmode==3)
		{
		if (debug) printf("Trying SHM...\n");
		old_handler=XSetErrorHandler(shm_connect_error);
		if (!shminfo.shmaddr) im0=XShmCreateImage(dpy,vinfo.visual,vinfo.depth,ZPixmap,0,&shminfo,image_width,image_height);
		else im0=XShmCreateImage(dpy,vinfo.visual,vinfo.depth,ZPixmap,shminfo.shmaddr,&shminfo,image_width,image_height);
		if (debug) printf("Get shmid ..\n");
		if (shminfo.shmid==0)
			{
			shminfo.shmid=shmget(IPC_PRIVATE,im0->bytes_per_line*im0->height,IPC_CREAT|0777);
			shminfo.shmaddr=im0->data=shmat(shminfo.shmid, 0, 0);
			if (shminfo.shmaddr!=0)
				{
				if (debug) printf("Got shmid!\n");
				shminfo.readOnly=False;
				XShmAttach(dpy,&shminfo);
				XSync(dpy,False);
				data0=(unsigned char *)shminfo.shmaddr;
				gmode=3;
				}
			else
				{
				if (debug) fprintf(stderr,"shmat(): SHM attach: Got zero addr!\n");
				fprintf(stderr,"SHM setup appears to have failed! Trying fallback ...\n");
				if (debug) printf("XDestroyImage()\n");   
				XDestroyImage(im0);
				if (debug) printf("shmdt()\n");
				shmdt(shminfo.shmaddr);
				if (debug) printf("shmctl()\n");
				shmctl(shminfo.shmid,IPC_RMID,0);
				shminfo.shmid=0;
				shminfo.shmaddr=0;
				im0=0;
				wmode=2;
				}
			}
		XSetErrorHandler(old_handler);
		}
	if (wmode==2)
		{
		if (debug) printf("XCreateImage(...)\n");
		if (debug) printf("NO SHM: init_imdat():\n");
		data0=malloc(MAX_BUF);
		if (!data0)
			{
			fprintf(stderr,"Couldn't allocate image buffer!\n");
			if (dpy) XCloseDisplay(dpy);
			exit(9);
			}
		if (debug) printf("Creating image im0...(%d x %d)\n",image_width,image_height);
		im0=XCreateImage(dpy,vinfo.visual,vinfo.depth,ZPixmap,0,(char*)data0,image_width,image_height,32,0);
		gmode=2;
		}
	if ((gmode==2) || (gmode==3))
		{
		if (!im0)
			{
			fprintf(stderr,"im0 seems to NULL! *** NO IMAGE BUFFER *** Bailing out...\n");
			if (dpy) XCloseDisplay(dpy);
			exit(10);
			}
		else
			{
			if (debug) printf("XInitImage(%d)\n",(int)im0);
			ret=XInitImage(im0);
			if (debug) printf("XInitImage() rtns %d\n",ret);
			}
		}
	if (gmode==0)
		{
		fprintf(stderr,"[ERROR] graphics mode not set! Bailing out...\n");
		exit(11);
		}

	if (vinfo.depth==24) graph_init(32);
	else graph_init(vinfo.depth);
	set_clip(0,disp_off,image_width,image_height);

// catch signals
	if (debug) printf("Setting signal mask...\n");
	sys_init();

	if (debug) printf("Mapping window...\n");
	XMapWindow(dpy,win);

	if (debug>1) printf("Setting WM protocol to trap WM_DELETE_WINDOW...\n");
	wm_delete_window=XInternAtom(dpy,"WM_DELETE_WINDOW",False);
	XSetWMProtocols(dpy,win,&wm_delete_window,1);

	if (debug) printf("Hello...starting xjackfreak...\n");

	BUT_INPUT=1;
	BUT_FFT1=1;
	BUT_X_LOG=1;
	BUT_Y_LOG=1;
	BUT_FFTAVE=1;
//		BUT_BLOCK=1;
	BUT_UP=1;
	BUT_DOWN=1;
	BUT_LEFT=1;
	BUT_RIGHT=1;
	BUT_GRID=1;
// 0 == red led
// 1 == grn button
// 2 == blue button
// 3 == clear/orange button
// 4 == orange tri but U
// 5 == orange tri but D
// 6 == orange tri but L
// 7 == orange tri but R
	bpc[ 0]=3;
	bpc[ 1]=1;
	bpc[ 2]=1;
	bpc[ 3]=1;
	bpc[ 4]=1;
	bpc[ 5]=1;
	bpc[ 6]=1;
	bpc[ 7]=1;

	bpc[ 8]=3;
	bpc[ 9]=3;
	bpc[10]=3;
	bpc[11]=3;
	bpc[12]=3;
	bpc[13]=3;

	bpc[14]=-1;
	bpc[15]=-1;
	bpc[16]=0;
	bpc[17]=6;
	bpc[18]=7;
	bpc[19]=4;
	bpc[20]=5;

	if (debug) printf("Event loop...\n");
	running=1;

	while ((!bye) && (!got_sigdie))
		{
		if (XCheckIfEvent(dpy,&event,predicate,(XPointer)NULL)==False)
			{
			if (running)
				{
				pentry_add(4,0);
				pentry_process();
				render_count++;
				frame_count++;
				if ((debug) || (disp_max)) printf("\rmmax_all=%5.2f %5.2f ",mmax_al1,mmax_al2);
				fflush(stdout);
				if (frame_rate>=10000) usleep(frame_rate-7000);
				}
			else
				{
				if (debug) printf("\rselect()");
				XFlush(dpy);
				FD_ZERO(&set);
				FD_SET(ConnectionNumber(dpy),&set);
				ret=select(ConnectionNumber(dpy)+1,&set,NULL,NULL,NULL);
				if (ret==-1)
					{
					if (debug) perror("... select");
					continue;
					}
				}
			}
		else
			{
			if (debug) printf("\revent");
			if (event.type==CompletionType)
				{
				if (wait_on_complete==0) fprintf(stderr," **** GOT SHM completion when wasn't expecting one ! ***\n");
				pentry_woc();
				wait_on_complete=0;
				}
			else bye=process_xevent(&event);
			}
		pentry_process();
		}
	if ((gmode==2) && (im0)) XDestroyImage(im0);
	if (shminfo.shmaddr)
		{
		if (debug) printf("shutdown(): cleaning up SHM ...\n");
		XShmDetach(dpy,&shminfo);
		// XDestroyImage (image);
		shmdt(shminfo.shmaddr);
		shmctl(shminfo.shmid,IPC_RMID,0);
		}
	XDestroyWindow(dpy,win);
	XCloseDisplay(dpy);
	if (debug) printf("Bye X Windows.\n");
	if (use_jack)
		{
		if (debug) printf("Disconnecting from jack: ");
		jack_client_close(client);
		if (debug) printf(" [done]\n");
		}
	if (debug)
		{
		fprintf(stderr," *** Shutdown *** \n");
		fflush(stderr);
		}
	if (debug) printf("\n");
	save_settings(fname);
	exit(0);
	}
