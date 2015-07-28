/****************************************************************************
*
* This file (visualx11.c) is part of xjackfreak, an audio frequency analyser.
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

//#include <X11/Xlib.h>
//#include <X11/Xutil.h>
//#include <X11/StringDefs.h>
//#include <X11/Xatom.h>
//#include <X11/extensions/XShm.h>

struct _fmt
	{
	int depth;
	int order;
	int red;
	int green;
	int blue;
	int format;
	};

Display *dpy;
Screen  *scr;
XVisualInfo vinfo;
Window win=-1;
GC gc1,gc2,gc3;
XGCValues xgcvalues1,xgcvalues2,xgcvalues3;
XImage *im0;

int display_bits=0;
int display_bytes=0;
int pixmap_bytes=0;
int x11_byteswap=0;
struct _fmt fmt[32];
int x11_dpy_fmtid=0;

#define CF_NONE           0
#define CF_RGB08          1
#define CF_GRAY           2
#define CF_RGB15_LE       3
#define CF_RGB16_LE       4
#define CF_RGB15_BE       5
#define CF_RGB16_BE       6
#define CF_BGR24          7	// LE
#define CF_BGR32          8	// LE
#define CF_RGB24          9	// BE
#define CF_RGB32         10	// BE

char *colour_formats[] = 
	{
	"CF_NONE",
	"CF_RGB08",
	"CF_GRAY",
	"CF_RGB15_LE",
	"CF_RGB16_LE",
	"CF_RGB15_BE",
	"CF_RGB16_BE",
	"CF_BGR24",
	"CF_BGR32",
	"CF_RGB24",
	"CF_RGB32"
	};


int x11_get_colour_format(int xclass,int depth,int bytes,int byteorder)
	{
	if ((xclass==PseudoColor) && (depth==8)) return CF_RGB08;
	if ((xclass==StaticGray) && (depth==8)) return CF_GRAY;
	if ((xclass==GrayScale) && (depth==8)) return CF_GRAY;
	if ((xclass==StaticColor) && (depth==8)) return CF_RGB08;
	if ((xclass==PseudoColor) || (xclass==StaticGray) || (xclass==GrayScale) || (xclass==StaticColor)) return 0;
	if (byteorder==MSBFirst)
		{
		switch (bytes)
			{
			case 2: if (depth==15) return CF_RGB15_BE; else return CF_RGB16_BE; break;
			case 3: return CF_RGB24; break;
			case 4: return CF_RGB32; break;
			}
		}
	else
		{
		switch (bytes)
			{
			case 2: if (depth==15) return CF_RGB15_LE; else return CF_RGB16_LE; break;
			case 3: return CF_BGR24; break;
			case 4: return CF_BGR32; break;
			}
		}
	return 0;
	}

Visual *visual_init()
	{
	Visual *visual;
	XVisualInfo *tvinfo;
	XPixmapFormatValues *pf;
	int i,n,format=0;

	visual=DefaultVisualOfScreen(scr);
	vinfo.visualid=XVisualIDFromVisual(visual);

	if (debug>1) printf("XGetVisualInfo():\n");
	tvinfo=XGetVisualInfo(dpy,VisualIDMask,&vinfo,&i);
	vinfo=tvinfo[0];
	XFree(tvinfo);

	if (debug>1) printf("display_bits/display_bytes\n");
	display_bits=vinfo.depth;
	display_bytes=(display_bits+7)/8;

	if (debug>1) printf("pf=XListPixmapFormats()\n");
	pf=XListPixmapFormats(dpy,&n);
	if (debug>1) printf("Found %d pixel formats!\n",n);
	for (i=0;i<n;i++)
		{
		if (pf[i].depth==display_bits) pixmap_bytes=pf[i].bits_per_pixel/8;
		}
	free(pf);
	pf=0;

//printf("debug=%d\n",debug);
	if (debug>1) printf("x11: colour depth: ""%d bits, %d bytes - pixmap: %d bytes\n",display_bits,display_bytes,pixmap_bytes);
	if (debug>1) { if ((vinfo.class==TrueColor) || (vinfo.class==DirectColor)) printf("x11: colour masks: ""red=0x%08lx green=0x%08lx blue=0x%08lx\n",vinfo.red_mask,vinfo.green_mask,vinfo.blue_mask); }
	if (debug>1) { if (BYTE_ORDER==LITTLE_ENDIAN) printf("x11: client byte order: little endian\n"); else printf("x11: client byte order: big endian\n"); }
	if (ImageByteOrder(dpy)==MSBFirst)
		{
		if (debug>1) printf("x11: server byte order: big endian\n");
		if (BYTE_ORDER!=BIG_ENDIAN) x11_byteswap=1;
		}
	else
		{
		if (debug>1) printf("x11: server byte order: little endian\n");
		if (BYTE_ORDER!=LITTLE_ENDIAN) x11_byteswap=1;
		}
	format=x11_get_colour_format(vinfo.class,vinfo.depth,pixmap_bytes,ImageByteOrder(dpy));
	if (format==0)
		{
		fprintf(stderr, "xjackfreak: sorry, I can't handle your strange display!\n");
		exit(16);
		return NULL;
		}
	x11_dpy_fmtid=format;
	if (debug>1) printf("colour_format = (%d),[%s]\n",format,colour_formats[format]);
	return visual;
	}
