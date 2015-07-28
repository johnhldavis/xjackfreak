/****************************************************************************
*
* This file (jfgraplib1.c) is part of xjackfreak, an audio frequency analyser.
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

//#define MAX_DOTS 65536
#define MAX_DOTS 131072

struct _clip
	{
	int x1;
	int x2;
	int y1;
	int y2;
	};

struct _dot
	{
	int x;
	int y;
	};

struct _dot dots[MAX_DOTS];
int dot_count=0;
int dots_full=0;
int jfdepth=0;
struct _clip clip;


void graph_init(int depth);
void set_clip(int x1,int y1,int x2,int y2);
void blank_screen();

void (*dot1)(int _x,int _y,int _r,int _g,int _b);
void (*dot2)(int _x,int _y,int _r,int _g,int _b);
void (*dot3)(int _x,int _y,int _r,int _g,int _b);

void _dot_null(int _x,int _y,int _r,int _g,int _b);

void _dot1_32(int _x,int _y,int _r,int _g,int _b);
void _dot2_32(int _x,int _y,int _r,int _g,int _b);
void _dot3_32(int _x,int _y,int _r,int _g,int _b);

void _dot1_16(int _x,int _y,int _r,int _g,int _b);
void _dot2_16(int _x,int _y,int _r,int _g,int _b);
void _dot3_16(int _x,int _y,int _r,int _g,int _b);

void line(int x1,int y1,int x2,int y2,int r,int g,int b);


void graph_init(int depth)
	{
	if ((depth!=32) && (depth!=16))
		{
		fprintf(stderr,"Cannot handle your strange depth! (%d)\n",depth);
		exit(15);
		}
	dot1=&(_dot_null);
	dot2=&(_dot_null);
	dot3=&(_dot_null);
	if (depth==32)
		{
		dot1=&(_dot1_32);
		dot2=&(_dot2_32);
		dot3=&(_dot3_32);
		jfdepth=depth;
		}
	else if (depth==16)
		{
		dot1=&(_dot1_16);
		dot2=&(_dot2_16);
		dot3=&(_dot3_16);
		jfdepth=depth;
		}
	}
	
void set_clip(int x1,int y1,int x2,int y2)
	{
	clip.x1=x1;
	clip.y1=y1;
	clip.x2=x2;
	clip.y2=y2;
	}

void blank_screen()
	{
	int i,j;

	if (dots_full)
		{
		for (i=0;i<image_height;i++)
			{
			for (j=0;j<image_width;j++) dot2(j,i,0,0,0);
			}
		}
	else
		{
		for (i=0;i<dot_count;i++) dot2(dots[i].x,dots[i].y,0,0,0);
		}
	dot_count=0;
	dots_full=0;
	}

void _dot_null(int _x,int _y,int _r,int _g,int _b)
	{
	return;
	}

void _dot1_32(int _x,int _y,int _r,int _g,int _b)
	{
	int idx,bb=0,gg=0,rr=0;

	if (_x<0) return; if (_x>image_width-1) return;
	if (_y<0) return; if (_y>image_height-1) return;
	if (_x< clip.x1) return; if (_x>=clip.x2) return;
	if (_y< clip.y1) return; if (_y>=clip.y2) return;
	idx=((_y*image_width+_x)*4);
	if (idx<0) return; if (idx>=MAX_BUF) return;
	bb=data0[idx];
	gg=data0[idx+1];
	rr=data0[idx+2];
	bb=(bb+_b)&0xff;
	gg=(gg+_g)&0xff;
	rr=(rr+_r)&0xff;
	data0[idx]  =(data0[idx]+(unsigned char)_b)&0xff;
	data0[idx+1]=(data0[idx+1]+(unsigned char)_g)&0xff;
	data0[idx+2]=(data0[idx+2]+(unsigned char)_r)&0xff;
	data0[idx+3]=0;
	if (dot_count<MAX_DOTS) { dots[dot_count].x=_x; dots[dot_count].y=_y; dot_count++; }
	else { dots_full=1; fprintf(stderr,"DOTS_FULL\n"); fflush(stdout); }
	}

void _dot1_16(int _x,int _y,int _r,int _g,int _b)
	{
	int idx,bb=0,gg=0,rr=0;

	if (_x<0) return; if (_x>image_width-1) return;
	if (_y<0) return; if (_y>image_height-1) return;
	if (_x< clip.x1) return; if (_x>=clip.x2) return;
	if (_y< clip.y1) return; if (_y>=clip.y2) return;
	idx=((_y*image_width+_x)*2);
	if (idx<0) return; if (idx>=MAX_BUF) return;
	bb=(data0[idx]&0x1f)<<3;
	gg=((data0[idx]&0x07)<<2) + (data0[idx+1]&0xe0);
	rr=(data0[idx+1]&0xf8);
	bb=(bb+_b)&0xff;
	gg=(gg+_g)&0xff;
	rr=(rr+_r)&0xff;
	bb=(_b&0xff)>>3;
	gg=(_g&0xff)>>2;
	rr=(_r&0xff)>>3;
	data0[idx+1] = (rr<<3) | (gg&0x38)>>3;
	data0[idx  ] = (gg&0x07)<<5 | bb;
	if (dot_count<MAX_DOTS) { dots[dot_count].x=_x; dots[dot_count].y=_y; dot_count++; }
	else { dots_full=1; fprintf(stderr,"DOTS_FULL!\n"); fflush(stdout); }
	}

void _dot2_32(int _x,int _y,int _r,int _g,int _b)
	{
	int idx;

	if (_x<0) return; if (_x>image_width-1) return;
	if (_y<0) return; if (_y>image_height-1) return;
	idx=((_y*image_width+_x)*4);	// 32 bpp
	if (idx<0) return; if (idx>=MAX_BUF) return;
	data0[idx]  =_b&0xff;
	data0[idx+1]=_g&0xff;
	data0[idx+2]=_r&0xff;
	data0[idx+3]=0;
	if ((_b>0) || (_g>0) || (_r>0))
		{
		if (dot_count<MAX_DOTS) { dots[dot_count].x=_x; dots[dot_count].y=_y; dot_count++; }
		else { dots_full=1; fprintf(stderr,"DOTS_FULL!\n"); fflush(stdout); }
		}
	}

void _dot2_16(int _x,int _y,int _r,int _g,int _b)
	{
	int idx,bb=0,gg=0,rr=0;

	if (_x<0) return; if (_x>image_width-1) return;
	if (_y<0) return; if (_y>image_height-1) return;
	idx=((_y*image_width+_x)*2);	// 16 bpp
	if (idx<0) return; if (idx>=MAX_BUF) return;
	bb=(_b&0xff)>>3;
	gg=(_g&0xff)>>2;
	rr=(_r&0xff)>>3;
	data0[idx+1] = (rr<<3) | (gg&0x38)>>3;
	data0[idx  ] = (gg&0x07)<<5 | bb;
	if ((_b>0) || (_g>0) || (_r>0))
		{
		if (dot_count<MAX_DOTS) { dots[dot_count].x=_x; dots[dot_count].y=_y; dot_count++; }
		else { dots_full=1; printf("."); fflush(stdout); }
		}
	}

void _dot3_32(int _x,int _y,int _r,int _g,int _b)
	{
	int idx;

	if (_x<0) return; if (_x>image_width-1) return;
	if (_y<0) return; if (_y>image_height-1) return;
	idx=((_y*image_width+_x)*4);	// 32 bpp
	if (idx<0) return; if (idx>=MAX_BUF) return;
	data0[idx]  =_b&0xff;
	data0[idx+1]=_g&0xff;
	data0[idx+2]=_r&0xff;
	data0[idx+3]=0;
	}

void _dot3_16(int _x,int _y,int _r,int _g,int _b)
	{
	int idx,rr=0,gg=0,bb=0;

	if (_x<0) return; if (_x>image_width-1) return;
	if (_y<0) return; if (_y>image_height-1) return;
	idx=((_y*image_width+_x)*2);	// 16 bpp
	if (idx<0) return; if (idx>=MAX_BUF) return;
	bb=(_b&0xff)>>3;
	gg=(_g&0xff)>>2;
	rr=(_r&0xff)>>3;
	data0[idx+1] = (rr<<3) | (gg&0x38)>>3;
	data0[idx  ] = (gg&0x07)<<5 | bb;
	}

void line(int x1,int y1,int x2,int y2,int r,int g,int b)
	{
	int dx,dy,p,s,st,n;

	dx=x1-x2;
	dy=y1-y2;
	if (dx==0)
		{
		if (dy==0) dot1(x1,y1,r,g,b);
		else if (dy<0)
			{
			for (p=y1;p<=y2;p++) dot1(x1,p,r,g,b);
			}
		else
			{
			for (p=y2;p<=y1;p++) dot1(x1,p,r,g,b);
			}
		return;
		}
	if (dy==0)
		{
		if (dx==0) dot1(x1,y1,r,g,b);
		else if (dx<0)
			{
			for (p=x1;p<=x2;p++) dot1(p,y1,r,g,b);
			}
		else
			{
			for (p=x2;p<=x1;p++) dot1(p,y1,r,g,b);
			}
		return;
		}
	if (dx<0)
		{
		s=1;
		dx=-dx;
		}
	else s=-1;
	if (dy<0)
		{
		st=1;
		dy=-dy;
		}
	else st=-1;
	if (abs(dx)>=abs(dy))
		{
		if (dx%2==1) n=1;
		else n=2;
		dot1(x1,y1,r,g,b);
		dot1(x2,y2,r,g,b);
		n=2;
		p=2*dy-dx;
		while (n<dx)
			{
			if (p>=0)
				{
				y1=y1+st;
				y2=y2-st;
				p=p+2*(dy-dx);
				}
			else p=p+2*dy;
			x1=x1+s;
			x2=x2-s;
			dot1(x1,y1,r,g,b);
			dot1(x2,y2,r,g,b);
			n=n+2;
			}
		if (dx%2==0)
			{
			if (p>=0) y1=y1+st;
			x1=x1+s;
			dot1(x1,y1,r,g,b);
			}
		}
	else
		{
		if (dy%2==1) n=1;
		else n=2;
		dot1(x1,y1,r,g,b);
		dot1(x2,y2,r,g,b);
		p=2*dx-dy;
		while (n<dy)
			{
			if (p>=0)
				{
				x1=x1+s;
				x2=x2-s;
				p=p+2*(dx-dy);
				}
			else p=p+2*dx;
			y1=y1+st;
			y2=y2-st;
			dot1(x1,y1,r,g,b);
			dot1(x2,y2,r,g,b);
			n=n+2;
			}
		if (dy%2==0)
			{
			if (p>=0) x1=x1+s;
			y1=y1+st;
			dot1(x1,y1,r,g,b);
			}
		}		
	}
