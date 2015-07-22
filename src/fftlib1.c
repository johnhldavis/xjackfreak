/****************************************************************************
*
* This file (fftlib1.c) is part of xjackfreak, an audio frequency analyser.
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

#include <stdlib.h>
#include <math.h>

//#define MAX_FFT_DATA_SIZE 8192
//#define MAX_FFT_W_SIZE 4096
#define MAX_FFT_DATA_SIZE 8193
#define MAX_FFT_W_SIZE 4097

float outr[MAX_FFT_DATA_SIZE];
float outi[MAX_FFT_DATA_SIZE];						// data arrays - FFT performed 
int br[MAX_FFT_DATA_SIZE][2];
int br_count=0;

double wr[MAX_FFT_W_SIZE],wi[MAX_FFT_W_SIZE];		// W array - one quarter of a circle
int fft_debug=0;
double pi;
long fft_count=0;


int calc_places(int x)
	{
	int k,count;

	k = 1;
	count = 0;
	while  (k<x-1)
		{
		k = k<<1;
		count++;
		}
	return count;
	}

int W_init(int x)
	{
	int i,jj,kk,tempk,brk,places;
	double theta;
	int dflag[MAX_FFT_DATA_SIZE];

	for (i=0;i<MAX_FFT_DATA_SIZE;i++)
		{
		outr[i]=0;
		outi[i]=0;
		}
if (fft_debug) printf("W_init(%d)\n",x);
	pi=acos(0)*2;
if (fft_debug) printf("PI=%2.12f\n",pi);
	for (i=0;i<x/4;i++)
		{
		theta = (2*pi);            //  use i-1 as array index starts from 1 ! (not anymore!)
		theta = (theta*i/x);            //  use i-1 as array index starts from 1 ! (not anymore!)
if (fft_debug) printf("theta=%1.10E ",theta);
		wr[i] = cos(theta);
		wi[i] = sin(theta);
if (fft_debug) printf("i=%d wrt=%1.10E wit=%1.10E\n",i,wr[i],wi[i]);
		}
// init array of sample moves for first part of fft
	places=calc_places(x);
	for (kk=0;kk<x;kk++) dflag[kk]=0;
	for (kk=0;kk<x;kk++)
		{
		brk=0;
		tempk=kk;
		for (jj=0;jj<places-1;jj++)             // places 
			{
			if (tempk%2==1) brk++;
			brk=brk<<1;
			tempk=tempk>>1;
			}
		if (tempk%2==1) brk++;
		if (fft_debug) printf("kk=%d  brk=%d  tempk=%d\n",kk,brk,tempk);
		if (dflag[kk]==0)
			{
			if (brk==kk) dflag[kk]=1;
			else
				{
//				swap_data(kk,brk);
				br[br_count][0]=kk;
				br[br_count][1]=brk;
				br_count++;
				dflag[kk]=1;
				dflag[brk]=1;
				}
			}
		}
	return 0;
	}

int _check_size(int x)
	{
	if (x%2==1) return 1;
	return 0;
	}

/* ++++++++ THE ACTUAL FAST Fourier algorithm arggghhhh ! yippee! +++++++++ */
double fast_fourier(int ts,int places)
	{
	int step,kk,jj,ll,s,t,i1,i2,ws,winc,s1,s2;
	double wrt,wit,temp1;
	double rtemp,itemp;

/* first bit reverse data in place */
	for (kk=0;kk<br_count;kk++)
		{
//		swap_data(kk,brk);
		s=br[kk][0];
		s1=br[kk][1];
//		if (fft_debug) printf("swap_data(%d,%d)\n",kk,brk);
		rtemp = outr[s];
		itemp = outi[s];
		outr[s] = outr[s1];
		outi[s] = outi[s1];
		outr[s1] = rtemp;
		outi[s1] = itemp;
		}
	for (kk=0;kk<ts/2;kk++)                      // sum/diff(n,2)
		{
		s=kk<<1;
		s2=s+1;
//		sumdiff_data(s,s+1);
		if (fft_debug) printf("sumdiff_data(%d,%d)\n",s,s+1);
		rtemp = outr[s];
		itemp = outi[s];
		outr[s] = outr[s] + outr[s2];
		outi[s] = outi[s] + outi[s2];
		outr[s2] = rtemp - outr[s2];
		outi[s2] = itemp - outi[s2];
		}
	if (ts==2) goto pass;
//pcnt=print_data(pcnt);

	for (kk=0;kk<ts/4;kk++)                    //  MUL(n,4)
		{
		s=3+(kk<<2);                            // calc index
//		i_data(s);                    // data * i
		if (fft_debug) printf("i_data(%d)\n",s);
		rtemp = outr[s];
		outr[s] = -outi[s];
		outi[s] = rtemp;
		}
//pcnt=print_data(pcnt);
	for (kk=0;kk<ts/4;kk++)                    //  SD(n,4)
		{
		s=kk<<2;
		s2=s+2;
//		sumdiff_data(s,s+2);
		if (fft_debug) printf("sumdiff_data(%d,%d)\n",s,s2);
		rtemp = outr[s];
		itemp = outi[s];
		outr[s] = outr[s] + outr[s2];
		outi[s] = outi[s] + outi[s2];
		outr[s2] = rtemp - outr[s2];
		outi[s2] = itemp - outi[s2];

//		sumdiff_data(s+1,s+3);
		s++;
		s2++;
		if (fft_debug) printf("sumdiff_data(%d,%d)\n",s,s2);
		rtemp = outr[s];
		itemp = outi[s];
		outr[s] = outr[s] + outr[s2];
		outi[s] = outi[s] + outi[s2];
		outr[s2] = rtemp - outr[s2];
		outi[s2] = itemp - outi[s2];
		}
	if (ts==4) goto pass;

	step=8;
	while (step<=ts)
		{
		if (fft_debug) printf("ts=%d   step=%d\n",ts,step);
		/* mul(size,step)  */
		for (kk=0;kk<ts/step;kk++)              // mul by i 
			{
			s=(step>>2)*(3+(kk<<2));
//			i_data(s);
			if (fft_debug) printf("i_data(%d)\n",s);
			rtemp = outr[s];
			outr[s] = -outi[s];
			outi[s] = rtemp;
			}
		i1=(step/2)+(step/4)-1;          // 1st index - 1 less than i index 
		i2=i1+2;                                  // 2nd index - 1 more then i index 
		ws=(i1-step/2)*(ts/step);         // W**n index 
		winc = ts/step;                         // W increment 
if (fft_debug) printf("i1=%d i2=%d ws=%d winc=%d\n",i1,i2,ws,winc);
		for (kk=1;kk<=step/4-1;kk++)                // outer loop calculating 
			{                                       // base indexes etc 
			wrt=wr[ws];                            // W**n temps 
			wit=wi[ws];
if (fft_debug) printf("wrt=%1.10E wit=%1.10E\n",wrt,wit);
			for (ll=0;ll<ts/step;ll++)
				{
				t=ll*step;
				s1=i1+t;
				s2=i2+t;
if (fft_debug) printf("s1=%d s2=%d\n",s1,s2);
				temp1=outr[s1];
				outr[s1]=outr[s1]*wrt-outi[s1]*wit;
				outi[s1]=outi[s1]*wrt+temp1*wit;

				temp1 = outr[s2];
				outr[s2]=-outr[s2]*wrt-outi[s2]*wit;
				outi[s2]=-outi[s2]*wrt+temp1*wit;
				}
			i1--;
			i2++;
			ws=ws-winc;
			}
		for (jj=0;jj<ts/step;jj++)             // SD (size,step)  
			{
			t=step>>1;
			s=jj*step;
			for (kk=0;kk<t;kk++)
				{
//				sumdiff_data(s+kk,s+kk+t);
				s1=s+kk;
				s2=s+kk+t;
		if (fft_debug) printf("sumdiff_data(%d,%d)\n",s1,s2);
				rtemp = outr[s1];
				itemp = outi[s1];
				outr[s1] = outr[s1] + outr[s2];
				outi[s1] = outi[s1] + outi[s2];
				outr[s2] = rtemp - outr[s2];
				outi[s2] = itemp - outi[s2];
//pcnt=print_data(pcnt);
				}
			}
 		step=step*2;
      }
	pass:
	fft_count++;
	return 0;
	}

double inv_fast_fourier(int ts,int places)
	{
	int step,kk,jj,ll,s,t,i1,i2,ws,winc,s1,s2;
	double max=0.0,wrt,wit,temp1,rtemp,itemp;
//	int dflag[MAX_FFT_DATA_SIZE];

	step=ts;
	while (step>=8)
		{
//		if (fft_debug) printf("ts=%d   step=%d\n",ts,step);
		for (jj=0;jj<ts/step;jj++)             // SD (size,step)  
			{
			t=step>>1;
			s=jj*step;
			for (kk=0;kk<t;kk++)
				{
//				inv_sumdiff_data(s+kk,s+kk+t);
				s1=s+kk;
				s2=s1+t;
				rtemp = outr[s1];
				itemp = outi[s1];
				outr[s1] = (outr[s1] + outr[s2])/2;
				outi[s1] = (outi[s1] + outi[s2])/2;
				outr[s2] = (rtemp - outr[s2])/2;
				outi[s2] = (itemp - outi[s2])/2;
				}
			}
		i1=(step/2)+(step/4)-1;          // 1st index - 1 less than i index 
		i2=i1+2;                                  // 2nd index - 1 more then i index 
		ws=(i1-step/2)*(ts/step);         // W**n index 
		winc = ts/step;                         // W increment 
		for (kk=1;kk<=step/4-1;kk++)                // outer loop calculating 
			{                                       // base indexes etc 
			wrt=wr[ws];                            // W**n temps 
			wit=wi[ws];
			for (ll=0;ll<ts/step;ll++)
				{
				t=ll*step;
				s1=i1+t;
				s2=i2+t;
				temp1=outr[s1];
				outr[s1]=outr[s1]*wrt+outi[s1]*wit;
				outi[s1]=outi[s1]*wrt-temp1*wit;

				temp1 = outr[s2];
				outr[s2]=-outr[s2]*wrt+outi[s2]*wit;
				outi[s2]=-outi[s2]*wrt-temp1*wit;
				}
			i1--;
			i2++;
			ws=ws-winc;
			}
		/* mul(size,step)  */
		for (kk=0;kk<ts/step;kk++)              // mul by i 
			{
			s=(step>>2)*(3+(kk<<2));
//			inv_i_data(s);
//			if (fft_debug) printf("i_data(%d)\n",s);
			rtemp = outr[s];
			outr[s] = outi[s];
			outi[s] = -rtemp;
			}
 		step=step/2;
      }
	for (kk=0;kk<ts/4;kk++)                    //  SD(n,4)
		{
		s=kk<<2;
		s2=s+2;
//		inv_sumdiff_data(s,s+2);
//		inv_sumdiff_data(s+1,s+3);
		rtemp = outr[s];
		itemp = outi[s];
		outr[s] = (outr[s] + outr[s2])/2;
		outi[s] = (outi[s] + outi[s2])/2;
		outr[s2] = (rtemp - outr[s2])/2;
		outi[s2] = (itemp - outi[s2])/2;

//		sumdiff_data(s+1,s+3);
		s++;
		s2++;
//		if (fft_debug) printf("sumdiff_data(%d,%d)\n",s,s2);
		rtemp = outr[s];
		itemp = outi[s];
		outr[s] = (outr[s] + outr[s2])/2;
		outi[s] = (outi[s] + outi[s2])/2;
		outr[s2] = (rtemp - outr[s2])/2;
		outi[s2] = (itemp - outi[s2])/2;
		}
	for (kk=0;kk<ts/4;kk++)                    //  MUL(n,4)
		{
		s=3+(kk<<2);                            // calc index
//		inv_i_data(s);                    // data * i
		if (fft_debug) printf("i_data(%d)\n",s);
		rtemp = outr[s];
		outr[s] = outi[s];
		outi[s] = -rtemp;
		}
	for (kk=0;kk<ts/2;kk++)                      // sum/diff(n,2)
		{
		s=kk<<1;
		s2=s+1;
//		inv_sumdiff_data(s,s+1);
		rtemp = outr[s];
		itemp = outi[s];
		outr[s] = (outr[s] + outr[s2])/2;
		outi[s] = (outi[s] + outi[s2])/2;
		outr[s2] = (rtemp - outr[s2])/2;
		outi[s2] = (itemp - outi[s2])/2;
		}
/* first bit reverse data in place */
	for (kk=0;kk<br_count;kk++)
		{
//		swap_data(kk,brk);
		s=br[kk][0];
		s1=br[kk][1];
//		if (fft_debug) printf("swap_data(%d,%d)\n",kk,brk);
		rtemp = outr[s];
		itemp = outi[s];
		outr[s] = outr[s1];
		outi[s] = outi[s1];
		outr[s1] = rtemp;
		outi[s1] = itemp;
		}
	return max;
	}
