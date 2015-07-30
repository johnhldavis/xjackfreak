/****************************************************************************
*
* This file (jack.c) is part of xjackfreak, an audio frequency analyser.
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

#include <jack/jack.h>

#define MAX_REC_BUF 1048576

unsigned long frags=0;
jack_port_t *iport[MAX_JACK_PORTS];
jack_port_t *oport[MAX_JACK_PORTS];
jack_nframes_t jack_buf_size=0;
jack_nframes_t jack_sample_rate=0;
jack_client_t *client;
int jack_num_ports=0;
int jack_frag_size=0;
int jack_frame_size=0;
double jack_frame_rate=0;

int do_update_mod_data=0;
int do_update_data_window=0;

float inbuf[MAX_REC_BUF],outbuf[MAX_REC_BUF];		// 1M of input/output record buffering: 1 channel only, for display purposes ...
int rec_frame=0,rec_off=0,draw_rec_frame=0;

// for each fft frame process, record the output WR index BEFORE it is incremented
// the display system will draw input from the input ring buffer at the same corresponding offset
// so input/fft/output should remain sync'd
#define FFT_BUF_MAX 16

float fft_buf[FFT_BUF_MAX][MAX_FRAG_SIZE];
int   fft_owr[FFT_BUF_MAX];
int   fft_wr=0;									// frame to write to ...

double mmdata[FFT_BUF_MAX][MAX_FFT_DATA_SIZE];				// modulus

float audio_inp_gain=1.0;
float audio_out_gain=1.0;
float audio_inp[2][MAX_SFRAG_SIZE];			// 2 channels of audio input
float audio_out[2][MAX_SFRAG_SIZE];			// 2 channels of audio output
int   audio_ird=0,audio_iwr=0;				// audio in  rd/wr indexes
int   audio_ord=0,audio_owr=0;				// audio out rd/wr indexes
int 	audio_disp_ch=0;
int	audio_data_window=0;						// start up with Hann
int   audio_data_merge=0;						// start up with add

int audio_roff=0;

int   max_mode=0;
float mmax_al1=0.0f;
float mmax_al2=0.0f;
float mmax_all_decay0=0.99990f;
float mmax_all_decay1=1.0f;
float mmax_all_decay2=1.0f;
float mmax_all_factor=512.0f;
int fft_ave_ratio=10,alt;
float max,mtmp;


float dwindow[MAX_FFT_DATA_SIZE];

void load_data_window(int type);

/*
buffers for drawing:
-fft_buf (pre-mod)

-mod_data

other buffers:
-front_buffer - 1/2 frame's worth of data for fft
-rec_buffer - record buffer of in/out

STEREO
------
copy input into fft input arrays: outr,outi
apply shaping

do fft
demux
apply mod_data[]
mux
do inv_fft

output new data

*/

int output_mix_frame_stereo(jack_nframes_t nframes,void *arg)
	{
	void *in[2],*out[2];
	int i,j,sz,frag_size,off,ioff,dist;
	float *ptr1,*ptr2;
	float tmpLr,tmpRr,tmpRi,tmpLi,tmpf1,tmpf2;

	if (do_update_mod_data)
		{
		for (i=0;i<fft_size/2;i++) mod_data[i]=new_mod_data[i];
		do_update_mod_data=0;
		}
	if (do_update_data_window)
		{
		load_data_window(audio_data_window);
		do_update_data_window=0;
		}
	in[0] =(jack_default_audio_sample_t *)jack_port_get_buffer(iport[0],nframes);
	out[0]=(jack_default_audio_sample_t *)jack_port_get_buffer(oport[0],nframes);
	in[1] =(jack_default_audio_sample_t *)jack_port_get_buffer(iport[1],nframes);
	out[1]=(jack_default_audio_sample_t *)jack_port_get_buffer(oport[1],nframes);

	frag_size=sizeof(jack_default_audio_sample_t)*nframes;

// bring in new audio frame ...
	memcpy(&audio_inp[0][audio_iwr],in[0],frag_size);
	memcpy(&audio_inp[1][audio_iwr],in[1],frag_size);
	if (audio_inp_gain!=1.0f)
		{
		for (j=0;j<nframes;j++)
			{
			ioff=(audio_iwr+j)%MAX_SFRAG_SIZE;
			audio_inp[0][ioff]=audio_inp[0][ioff]*audio_inp_gain;
			audio_inp[1][ioff]=audio_inp[1][ioff]*audio_inp_gain;
			}
		}
	audio_iwr=(audio_iwr+nframes)%MAX_SFRAG_SIZE;

// save L into inbuf[] for display
	if (!BUT_RECBUF)
		{
//		memcpy(&inbuf[rec_frame*nframes],&audio_inp[0][audio_ird],frag_size);
		for (j=0;j<nframes;j++)
			{
//			ioff=(audio_ord+j)%MAX_SFRAG_SIZE;
			ioff=(audio_ord+j-audio_roff+MAX_SFRAG_SIZE)%MAX_SFRAG_SIZE;
			inbuf[rec_frame*nframes+j]=audio_inp[audio_disp_ch][ioff];
			}
		}

// copy to output buf first in half sized fft chunks via the fft buffer ....
	off=(audio_ird-fft_size4+MAX_SFRAG_SIZE)%MAX_SFRAG_SIZE;
	dist=(audio_iwr-audio_ird+MAX_SFRAG_SIZE)%MAX_SFRAG_SIZE;
	dist=dist-fft_size2;
	for (i=0;i<dist/fft_size2;i++)
//	for (i=0;i<nframes/fft_size2;i++)
		{
		if (off+fft_size>MAX_SFRAG_SIZE)	// going over end of buffer !!
			{
// calc size that we can copy
			sz=off+fft_size-MAX_SFRAG_SIZE;
			memcpy(&outr[0],&audio_inp[0][off],(fft_size-sz)*4);
			memcpy(&outi[0],&audio_inp[1][off],(fft_size-sz)*4);
			memcpy(&outr[fft_size-sz],&audio_inp[0][0],sz*4);
			memcpy(&outi[fft_size-sz],&audio_inp[1][0],sz*4);
			}
		else
			{
			memcpy(&outr[0],&audio_inp[0][off],fft_size*4);
			memcpy(&outi[0],&audio_inp[1][off],fft_size*4);
			}
		audio_ird=(audio_ird+fft_size2)%MAX_SFRAG_SIZE;
		off=(off+fft_size2)%MAX_SFRAG_SIZE;

// apply shaping from data window
		for (j=0;j<fft_size2;j++)
			{
			outr[j]=outr[j]*dwindow[j];
			outr[fft_size-j-1]=outr[fft_size-j-1]*dwindow[j];
			outi[j]=outi[j]*dwindow[j];
			outi[fft_size-j-1]=outi[fft_size-j-1]*dwindow[j];
			}

// save L into fft_buf[] for display - post-shaping ....
		if (audio_disp_ch) memcpy(fft_buf[fft_wr],outi,fft_size*4);
		else memcpy(fft_buf[fft_wr],outr,fft_size*4);

// do fft
		fast_fourier(fft_size,fft_places);

// demux
// seperate fft data into L and R sides using complex conjugates ...
// L-real is first  half of real array
// L-imag is first  half of imag array
// R-real is second half of real array (backwards)
// R-imag is second half of imag array (backwards)
		for (j=1;j<fft_size/2;j++)
			{
			tmpLr=(outr[j]+outr[fft_size-j])/2.0;
			tmpRi=(outr[j]-outr[fft_size-j])/2.0;
			tmpRr=(outi[j]+outi[fft_size-j])/2.0;
			tmpLi=(outi[j]-outi[fft_size-j])/2.0;
			outr[j]=tmpLr;
			outr[fft_size-j]=tmpRr;
			outi[j]=tmpLi;
			outi[fft_size-j]=tmpRi;
			}
		outr[fft_size]=outi[0];
		outi[0]=0.0f;

// decay mmax_all to slowly reset it
		switch (max_mode)
			{
			case 0: mmax_al1=mmax_al1*mmax_all_decay0; mmax_al2=mmax_al2*mmax_all_decay0; break;
			case 1: mmax_al1=mmax_al1*mmax_all_decay1; mmax_al2=mmax_al2*mmax_all_decay1; break;
			case 2: mmax_al1=mmax_al1*mmax_all_decay2; mmax_al2=mmax_al2*mmax_all_decay2; break;
			}
// load mdata with |Z| and find maximum: save into buffer for display
		max=0.0f;
		alt=(fft_wr-1+FFT_BUF_MAX)%FFT_BUF_MAX;
		for (j=0;j<fft_size2;j++)
			{
//			mtmp=mod_data(outr[kk],outi[kk]);
//			if (audio_disp_ch) mtmp=sqrt(outr[fft_size-j]*outr[fft_size-j]);
//			else mtmp=sqrt(outr[j]*outr[j]);

			if (audio_disp_ch==0) mtmp=sqrt(outr[j]*outr[j]+outi[j]*outi[j]);
			else mtmp=sqrt(outr[fft_size-j]*outr[fft_size-j]+outi[fft_size-j]*outi[fft_size-j]);

			if (BUT_FFTAVE) mmdata[fft_wr][j] = mtmp/fft_ave_ratio+(fft_ave_ratio-1)*mmdata[alt][j]/fft_ave_ratio;
			else mmdata[fft_wr][j] = mtmp;
			if (mtmp>max) max=mtmp;
			}
		switch (max_mode)
			{
			case 0: if (max>mmax_al1) mmax_al1=max; break;
			case 1: mmax_al1=((mmax_all_factor-1.0f)*mmax_al1+max)/mmax_all_factor; break;
			case 2: if (max>mmax_al1) mmax_al1=max; else mmax_al1=((mmax_all_factor-1.0f)*mmax_al1+max)/mmax_all_factor; break;
			}

		fft_owr[fft_wr]=audio_owr;
		fft_wr=(fft_wr+1)%FFT_BUF_MAX;

// apply mod_data[]
//		outr[0]=outr[0]*mod_data[0];
//		outi[0]=outi[0]*mod_data[0];
//		outr[fft_size]=outr[fft_size]*mod_data[0];
//		outi[fft_size]=outi[fft_size]*mod_data[0];
		for (j=1;j<fft_size2;j++)
			{
			outr[j]=outr[j]*mod_data[j];
			outi[j]=outi[j]*mod_data[j];
			outr[fft_size-j]=outr[fft_size-j]*mod_data[j];
			outi[fft_size-j]=outi[fft_size-j]*mod_data[j];
			}

// re-scan after mod-data in case we don't draw at same scale as input
		max=0.0f;
		for (j=1;j<fft_size2;j++)
			{
			if (audio_disp_ch==0) mtmp=sqrt(outr[j]*outr[j]+outi[j]*outi[j]);
			else mtmp=sqrt(outr[fft_size-j]*outr[fft_size-j]+outi[fft_size-j]*outi[fft_size-j]);
			if (mtmp>max) max=mtmp;
			}

		switch (max_mode)
			{
			case 0: if (max>mmax_al2) mmax_al2=max; break;
			case 1: mmax_al2=((mmax_all_factor-1.0f)*mmax_al2+max)/mmax_all_factor; break;
			case 2: if (max>mmax_al2) mmax_al2=max; else mmax_al2=((mmax_all_factor-1.0f)*mmax_al2+max)/mmax_all_factor; break;
			}

// repack data !
		for (j=1;j<fft_size/2;j++)
			{
			tmpLi=outi[j]+outr[fft_size-j];
			tmpRi=outr[fft_size-j]-outi[j];
			tmpRr=outr[j]-outi[fft_size-j];
			tmpLr=outi[fft_size-j]+outr[j];
			outr[j]=tmpLr;
			outr[fft_size-j]=tmpRr;
			outi[j]=tmpLi;
			outi[fft_size-j]=tmpRi;
			}
		outi[0]=outr[fft_size];
		outr[fft_size]=0.0f;

// do inv_fft
		inv_fast_fourier(fft_size,fft_places);

/*
data window:
 0 - Hann - cos
 1 - Bartlett - triangle
 2 - Hann#2 - cos ^ 2
 3 - Welch - 2nd degree poly
 4 - fft_size/2 square window
 5 - fft_size   square window

data merge:
 0 - add
 1 - xfade
 2 - ave
 3 - ave(a+x)
 4 - ave(a+v)
 5 - ave(x+v)
 6 - ave(a+x+v)
*/

// merge first half: write second half
		if (audio_data_merge==0)			// addition
			{
			for (j=0;j<fft_size2;j++)
				{
				ioff=(audio_owr+j-fft_size4+MAX_SFRAG_SIZE)%MAX_SFRAG_SIZE;
				audio_out[0][ioff]=audio_out[0][ioff]+outr[j];
				audio_out[1][ioff]=audio_out[1][ioff]+outi[j];
				}
			}
		else if (audio_data_merge==1)		// xfade
			{
			for (j=0;j<fft_size2;j++)
				{
				tmpf1=(double)j/(double)(fft_size2-1);
				tmpf2=1.0f-tmpf1;
				ioff=(audio_owr+j-fft_size4+MAX_SFRAG_SIZE)%MAX_SFRAG_SIZE;
				audio_out[0][ioff]=tmpf2*audio_out[0][ioff]+tmpf1*outr[j];
				audio_out[1][ioff]=tmpf2*audio_out[1][ioff]+tmpf1*outi[j];
				}
			}
		else if (audio_data_merge==2)		// average
			{
			for (j=0;j<fft_size2;j++)
				{
				ioff=(audio_owr+j-fft_size4+MAX_SFRAG_SIZE)%MAX_SFRAG_SIZE;
				audio_out[0][ioff]=(audio_out[0][ioff]+outr[j])/2.0f;
				audio_out[1][ioff]=(audio_out[1][ioff]+outi[j])/2.0f;
				}
			}
		else if (audio_data_merge==3)			// ave(a+x)
			{
			for (j=0;j<fft_size2;j++)
				{
				ioff=(audio_owr+j-fft_size4+MAX_SFRAG_SIZE)%MAX_SFRAG_SIZE;
				tmpf1=(double)j/(double)(fft_size2-1);
				tmpf2=2.0f-tmpf1;
				tmpf1=1.0f+tmpf1;
				audio_out[0][ioff]=(tmpf2*audio_out[0][ioff]+tmpf1*outr[j])/2.0f;
				audio_out[1][ioff]=(tmpf2*audio_out[1][ioff]+tmpf1*outi[j])/2.0f;
				}
			}
		else if (audio_data_merge==4)			// ave(a+v)
			{
			for (j=0;j<fft_size2;j++)
				{
				ioff=(audio_owr+j-fft_size4+MAX_SFRAG_SIZE)%MAX_SFRAG_SIZE;
				audio_out[0][ioff]=3.0f*(audio_out[0][ioff]+outr[j])/4.0f;
				audio_out[1][ioff]=3.0f*(audio_out[1][ioff]+outi[j])/4.0f;
				}
			}
		else if (audio_data_merge==5)			// ave(x+v)
			{
			for (j=0;j<fft_size2;j++)
				{
				ioff=(audio_owr+j-fft_size4+MAX_SFRAG_SIZE)%MAX_SFRAG_SIZE;
				tmpf1=(double)j/(double)(fft_size2-1);
				tmpf2=1.0f-tmpf1;
				tmpf1=tmpf1*2.0f+1.0f;
				tmpf2=tmpf2*2.0f+1.0f;
				audio_out[0][ioff]=(tmpf2*audio_out[0][ioff]+tmpf1*outr[j])/4.0f;
				audio_out[1][ioff]=(tmpf2*audio_out[1][ioff]+tmpf1*outi[j])/4.0f;
				}
			}
		else if (audio_data_merge==6)			// ave(a+x+v)
			{
			for (j=0;j<fft_size2;j++)
				{
				ioff=(audio_owr+j-fft_size4+MAX_SFRAG_SIZE)%MAX_SFRAG_SIZE;
				tmpf1=(double)j/(double)(fft_size2-1);
				tmpf2=1.0f-tmpf1;
				tmpf1=tmpf1*2.0f+3.0f;
				tmpf2=tmpf2*2.0f+3.0f;
				audio_out[0][ioff]=(tmpf2*audio_out[0][ioff]+tmpf1*outr[j])/6.0f;
				audio_out[1][ioff]=(tmpf2*audio_out[1][ioff]+tmpf1*outi[j])/6.0f;
				}
			}

		ioff=(audio_owr+fft_size4)%MAX_SFRAG_SIZE;
		memcpy(&audio_out[0][ioff],&outr[fft_size2],fft_size);
		memcpy(&audio_out[1][ioff],&outi[fft_size2],fft_size);
		ioff=(audio_owr+fft_size2)%MAX_SFRAG_SIZE;
		memcpy(&audio_out[0][ioff],&outr[fft_size2+fft_size4],fft_size);
		memcpy(&audio_out[1][ioff],&outi[fft_size2+fft_size4],fft_size);

		if (audio_out_gain!=1.0f)
			{
			for (j=0;j<fft_size2;j++)
				{
				ioff=(audio_owr+j-fft_size4+MAX_SFRAG_SIZE)%MAX_SFRAG_SIZE;
				audio_out[0][ioff]=audio_out[0][ioff]*audio_out_gain;
				audio_out[1][ioff]=audio_out[1][ioff]*audio_out_gain;
				}
			}
		audio_owr=(audio_owr+fft_size2)%MAX_SFRAG_SIZE;
		}

	if (BUT_BYPASS)
		{
		if (!delay_bypass)
			{
			memcpy(out[0],in[0],frag_size);
			memcpy(out[1],in[1],frag_size);
			}
		else
			{
			ptr1=out[0];
			ptr2=out[1];
			for (j=0;j<nframes;j++)
				{
//				ioff=(audio_ord+j)%MAX_SFRAG_SIZE;
				ioff=(audio_ord-audio_roff+j+MAX_SFRAG_SIZE)%MAX_SFRAG_SIZE;
				*ptr1=audio_inp[0][ioff];
				*ptr2=audio_inp[1][ioff];
				ptr1++;
				ptr2++;
				}
			}
		}
	else
		{
		ptr1=out[0];
		ptr2=out[1];
		for (j=0;j<nframes;j++)
			{
			ioff=(audio_ord+j)%MAX_SFRAG_SIZE;
			*ptr1=audio_out[0][ioff];
			*ptr2=audio_out[1][ioff];
			ptr1++;
			ptr2++;
			}
		if (!BUT_RECBUF)
			{
//			memcpy(&outbuf[rec_frame*nframes],&audio_out[0][audio_ord],frag_size);
			for (j=0;j<nframes;j++)
				{
				ioff=(audio_ord+j)%MAX_SFRAG_SIZE;
				outbuf[rec_frame*nframes+j]=audio_out[audio_disp_ch][ioff];
				}
			rec_frame=(rec_frame+1)%(MAX_REC_BUF/frag_size);
			}
		}
	audio_ord=(audio_ord+nframes)%MAX_SFRAG_SIZE;
	frags++;
	return 0;
	}

int output_mix_frame_mono(jack_nframes_t nframes,void *arg)
	{
	void *in[1],*out[1];
	int i,j,sz,frag_size,off,ioff,dist;
	float *ptr1,tmpf1,tmpf2;

	if (do_update_mod_data)
		{
		for (i=0;i<fft_size/2;i++) mod_data[i]=new_mod_data[i];
		do_update_mod_data=0;
		}
	in[0] =(jack_default_audio_sample_t *)jack_port_get_buffer(iport[0],nframes);
	out[0]=(jack_default_audio_sample_t *)jack_port_get_buffer(oport[0],nframes);
	frag_size=sizeof(jack_default_audio_sample_t)*nframes;

// bring in new audio frame ...
	if (audio_inp_gain!=1.0f)
		{
		for (j=0;j<nframes;j++)
			{
			ioff=(audio_iwr+j)%MAX_SFRAG_SIZE;
			audio_inp[0][ioff]=audio_inp[0][ioff]*audio_inp_gain;
			}
		}
	else memcpy(&audio_inp[0][audio_iwr],in[0],frag_size);
	audio_iwr=(audio_iwr+nframes)%MAX_SFRAG_SIZE;

// save L into inbuf[] for display
	if (!BUT_RECBUF)
		{
		for (j=0;j<nframes;j++)
			{
//			ioff=(audio_ord+j)%MAX_SFRAG_SIZE;
			ioff=(audio_ord+j-audio_roff+MAX_SFRAG_SIZE)%MAX_SFRAG_SIZE;
			inbuf[rec_frame*nframes+j]=audio_inp[0][ioff];
			}
		}

	off=(audio_ird-fft_size4+MAX_SFRAG_SIZE)%MAX_SFRAG_SIZE;

	dist=(audio_iwr-audio_ird+MAX_SFRAG_SIZE)%MAX_SFRAG_SIZE;
	dist=dist-fft_size2;
	for (i=0;i<dist/fft_size2;i++)
		{
		if (off+fft_size>MAX_SFRAG_SIZE)	// going over end of buffer !!
			{
// calc size that we can copy
			sz=off+fft_size-MAX_SFRAG_SIZE;
			memcpy(&outr[0],&audio_inp[0][off],(fft_size-sz)*4);
			memset(&outi[0],0,(fft_size-sz)*4);
			memcpy(&outr[fft_size-sz],&audio_inp[0][0],sz*4);
			memset(&outi[fft_size-sz],0,sz*4);
			}
		else
			{
			memcpy(&outr[0],&audio_inp[0][off],fft_size*4);
			memset(&outi[0],0,fft_size*4);
			}
		audio_ird=(audio_ird+fft_size2)%MAX_SFRAG_SIZE;
		off=(off+fft_size2)%MAX_SFRAG_SIZE;

// apply shaping from data window
		for (j=0;j<fft_size2;j++)
			{
			outr[j]=outr[j]*dwindow[j];
			outr[fft_size-j-1]=outr[fft_size-j-1]*dwindow[j];
			}

// save L into fft_buf[] for display - post-shaping ....
		memcpy(fft_buf[fft_wr],outr,fft_size*4);

// do fft
		fast_fourier(fft_size,fft_places);

// decay mmax_all to slowly reset it
		switch (max_mode)
			{
			case 0: mmax_al1=mmax_al1*mmax_all_decay0; mmax_al2=mmax_al2*mmax_all_decay0; break;
			case 1: mmax_al1=mmax_al1*mmax_all_decay1; mmax_al2=mmax_al2*mmax_all_decay1; break;
			case 2: mmax_al1=mmax_al1*mmax_all_decay2; mmax_al2=mmax_al2*mmax_all_decay2; break;
			}

// load mdata with |Z| and find maximum
		max=0.0f;
		alt=(fft_wr-1+FFT_BUF_MAX)%FFT_BUF_MAX;
		for (j=0;j<fft_size2;j++)
			{
//			mtmp=mod_data(outr[j],outi[j]);
//			mtmp=sqrt(outr[j]*outr[j]);
			mtmp=sqrt(outr[j]*outr[j]+outi[j]*outi[j]);
			if (BUT_FFTAVE) mmdata[fft_wr][j] = mtmp/fft_ave_ratio+(fft_ave_ratio-1)*mmdata[alt][j]/fft_ave_ratio;
			else mmdata[fft_wr][j] = mtmp;
			if (mtmp>max) max=mtmp;
			}
		switch (max_mode)
			{
			case 0: if (max>mmax_al1) mmax_al1=max; break;
			case 1: mmax_al1=((mmax_all_factor-1.0f)*mmax_al1+max)/mmax_all_factor; break;
			case 2: if (max>mmax_al1) mmax_al1=max; else mmax_al1=((mmax_all_factor-1.0f)*mmax_al1+max)/mmax_all_factor; break;
			}

		fft_owr[fft_wr]=audio_owr;
		fft_wr=(fft_wr+1)%FFT_BUF_MAX;

// apply mod_data[]
//		outr[0]=outr[0]*mod_data[0];
//		outr[fft_size2]=outr[fft_size2]*mod_data[0];
		for (j=1;j<fft_size2;j++)
			{
			outr[j]=outr[j]*mod_data[j];
			outr[fft_size-j]=outr[fft_size-j]*mod_data[j];
			}
// re-scan in case out is not drawn at same scale as input
		max=0.0f;
		for (j=1;j<fft_size2;j++)
			{
//			mtmp=sqrt(outr[j]*outr[j]);
			mtmp=sqrt(outr[j]*outr[j]+outi[j]*outi[j]);
			if (mtmp>max) max=mtmp;
			}
		switch (max_mode)
			{
			case 0: if (max>mmax_al2) mmax_al2=max; break;
			case 1: mmax_al2=((mmax_all_factor-1.0f)*mmax_al2+max)/mmax_all_factor; break;
			case 2: if (max>mmax_al2) mmax_al2=max; else mmax_al2=((mmax_all_factor-1.0f)*mmax_al2+max)/mmax_all_factor; break;
			}

// do inv_fft
		inv_fast_fourier(fft_size,fft_places);

// merge first half: write second half
		if (audio_data_merge==0)			// addition
			{
			for (j=0;j<fft_size2;j++)
				{
				ioff=(audio_owr+j-fft_size4+MAX_SFRAG_SIZE)%MAX_SFRAG_SIZE;
				audio_out[0][ioff]=audio_out[0][ioff]+outr[j];
				}
			}
		else if (audio_data_merge==1)		// xfade
			{
			for (j=0;j<fft_size2;j++)
				{
				tmpf1=(double)j/(double)(fft_size2-1);
				tmpf2=1.0f-tmpf1;
				ioff=(audio_owr+j-fft_size4+MAX_SFRAG_SIZE)%MAX_SFRAG_SIZE;
				audio_out[0][ioff]=tmpf2*audio_out[0][ioff]+tmpf1*outr[j];
				}
			}
		else if (audio_data_merge==2)		// average
			{
			for (j=0;j<fft_size2;j++)
				{
				ioff=(audio_owr+j-fft_size4+MAX_SFRAG_SIZE)%MAX_SFRAG_SIZE;
				audio_out[0][ioff]=(audio_out[0][ioff]+outr[j])/2.0f;
				}
			}
		else if (audio_data_merge==3)			// ave(a+x)
			{
			for (j=0;j<fft_size2;j++)
				{
				ioff=(audio_owr+j-fft_size4+MAX_SFRAG_SIZE)%MAX_SFRAG_SIZE;
				tmpf1=(double)j/(double)(fft_size2-1);
				tmpf2=2.0f-tmpf1;
				tmpf1=1.0f+tmpf1;
				audio_out[0][ioff]=(tmpf2*audio_out[0][ioff]+tmpf1*outr[j])/2.0f;
				}
			}
		else if (audio_data_merge==4)			// ave(a+v)
			{
			for (j=0;j<fft_size2;j++)
				{
				ioff=(audio_owr+j-fft_size4+MAX_SFRAG_SIZE)%MAX_SFRAG_SIZE;
				audio_out[0][ioff]=3.0f*(audio_out[0][ioff]+outr[j])/4.0f;
				}
			}
		else if (audio_data_merge==5)			// ave(x+v)
			{
			for (j=0;j<fft_size2;j++)
				{
				ioff=(audio_owr+j-fft_size4+MAX_SFRAG_SIZE)%MAX_SFRAG_SIZE;
				tmpf1=(double)j/(double)(fft_size2-1);
				tmpf2=1.0f-tmpf1;
				tmpf1=tmpf1*2.0f+1.0f;
				tmpf2=tmpf2*2.0f+1.0f;
				audio_out[0][ioff]=(tmpf2*audio_out[0][ioff]+tmpf1*outr[j])/4.0f;
				}
			}
		else if (audio_data_merge==6)			// ave(a+x+v)
			{
			for (j=0;j<fft_size2;j++)
				{
				ioff=(audio_owr+j-fft_size4+MAX_SFRAG_SIZE)%MAX_SFRAG_SIZE;
				tmpf1=(double)j/(double)(fft_size2-1);
				tmpf2=1.0f-tmpf1;
				tmpf1=tmpf1*2.0f+3.0f;
				tmpf2=tmpf2*2.0f+3.0f;
				audio_out[0][ioff]=(tmpf2*audio_out[0][ioff]+tmpf1*outr[j])/6.0f;
				}
			}

		ioff=(audio_owr+fft_size4)%MAX_SFRAG_SIZE;
		memcpy(&audio_out[0][ioff],&outr[fft_size2],fft_size);
		ioff=(audio_owr+fft_size2)%MAX_SFRAG_SIZE;
		memcpy(&audio_out[0][ioff],&outr[fft_size2+fft_size4],fft_size);

		if (audio_out_gain!=1.0f)
			{
			for (j=0;j<fft_size2;j++)
				{
				ioff=(audio_owr+j-fft_size4+MAX_SFRAG_SIZE)%MAX_SFRAG_SIZE;
				audio_out[0][ioff]=audio_out[0][ioff]*audio_out_gain;
				}
			}
		audio_owr=(audio_owr+fft_size2)%MAX_SFRAG_SIZE;
		}

	if (BUT_BYPASS)
		{
		if (!delay_bypass) memcpy(out[0],in[0],frag_size);
		else
			{
			ptr1=out[0];
			for (j=0;j<nframes;j++)
				{
//				ioff=(audio_ord+j)%MAX_SFRAG_SIZE;
				ioff=(audio_ord-audio_roff+j+MAX_SFRAG_SIZE)%MAX_SFRAG_SIZE;
				*ptr1=audio_inp[0][ioff];
				ptr1++;
				}
			}
		}
	else
		{
//		memcpy(out[0],&audio_out[0][audio_ord],frag_size);
		ptr1=out[0];
		for (j=0;j<nframes;j++)
			{
			ioff=(audio_ord+j)%MAX_SFRAG_SIZE;
			*ptr1=audio_out[0][ioff];
			ptr1++;
			}
		if (!BUT_RECBUF)
			{
//			memcpy(&outbuf[rec_frame*nframes],&audio_out[0][audio_ord],frag_size);
			for (j=0;j<nframes;j++)
				{
				ioff=(audio_ord+j)%MAX_SFRAG_SIZE;
				outbuf[rec_frame*nframes+j]=audio_out[0][ioff];
				}
			rec_frame=(rec_frame+1)%(MAX_REC_BUF/frag_size);
			}
		}
	audio_ord=(audio_ord+nframes)%MAX_SFRAG_SIZE;
	frags++;
	return 0;
	}

void jack_shutdown(void *arg)
	{
	if (debug) fprintf(stderr,"jack_shutdown called!\n");
	exit(12);
	}

int connect_to_jack(char *_str,int stereo)
	{
	if (debug) fprintf(stderr,"connect_to_jack(%s)",_str);
	if ((client=jack_client_open(_str,0,NULL)) == 0)
		{
		if (debug) fprintf (stderr, " [FAIL]\n");
		return 1;
		}
	else 	if (debug) fprintf(stderr," [OK]\n");
	jack_buf_size=jack_get_buffer_size(client);
	if (debug) fprintf(stderr,"jack buf_size=%lu\n",(unsigned long)jack_buf_size);
	jack_sample_rate=jack_get_sample_rate(client);
	if (debug) fprintf(stderr,"jack rate=%lu\n",(unsigned long)jack_sample_rate);
	if ((!jack_buf_size) || (!jack_sample_rate))
		{
		fprintf(stderr,"bad buf_size or sample rate!\n");
		exit(13);
		}
	if (stereo) jack_set_process_callback(client,output_mix_frame_stereo,0);
	else jack_set_process_callback(client,output_mix_frame_mono,0);
	jack_on_shutdown(client,jack_shutdown,0);

// zero out audio buffers
	memset(audio_inp,0,sizeof(audio_inp));
	memset(audio_out,0,sizeof(audio_out));
	memset(mmdata,0,sizeof(mmdata));

	audio_ird=(0-fft_size/2+MAX_SFRAG_SIZE)%MAX_SFRAG_SIZE;
	if (debug) printf("AUDIO LAG  IN:  audio_ird=%d = %d samples\n",audio_ird,fft_size/2);

	audio_ord=(0-fft_size/4+MAX_SFRAG_SIZE)%MAX_SFRAG_SIZE;
	if (debug) printf("AUDIO LAG OUT:  audio_ord=%d = %d samples\n",audio_ord,fft_size/4);

	audio_roff=fft_size/2;
	if (debug) printf("AUDIO LAG REC: audio_roff=%d samples\n",audio_roff);

	load_data_window(audio_data_window);
	return 0;
	}

int register_jack_ports(int num_ports)
	{
	int i;
	char t_str[32];

	for (i=0;i<num_ports;i++)
		{
		sprintf(t_str,"in%d",i+1);
		if ((iport[i] = jack_port_register(client,t_str,JACK_DEFAULT_AUDIO_TYPE,JackPortIsInput,0))==NULL) return 1;
		sprintf(t_str,"out%d",i+1);
		if ((oport[i] = jack_port_register(client,t_str,JACK_DEFAULT_AUDIO_TYPE,JackPortIsOutput,0))==NULL) return 1;
		}
	jack_num_ports=num_ports;
	return 0;
	}

int activate_jack()
	{
	if (debug) fprintf(stderr,"activate_jack()");
// recalulate the frag size
	jack_frag_size=(int)jack_buf_size*sizeof(jack_default_audio_sample_t);
	jack_frame_size=(int)jack_buf_size;
	if (jack_frame_size!=0) jack_frame_rate=jack_sample_rate/jack_frame_size;
	if (jack_frame_size>MAX_FRAG_SIZE)
		{
		fprintf(stderr,"Blimey, your frame_size of %d is too large! Please recompile.\n",jack_frag_size);
		fprintf(stderr,"Disconnecting from jack");
		jack_client_close(client);
		fprintf(stderr," [done]\n");
		debug=1;
		fprintf(stderr,"*** Shutdown ***\nSee ya next time.\n");
		printf("\n*** Shutdown ***\nSee ya next time.\n");
		exit(14);
		}

// re-adjust LAG to allow FFT frames bigger than jack frame size ...
	if (jack_frame_size<fft_size)
		{
		printf("Doing FFT with frame (%d) > jack_frame (%d): re-adjusting LAG\n",fft_size,jack_frame_size);
		audio_ord=(0-fft_size-fft_size2+MAX_SFRAG_SIZE)%MAX_SFRAG_SIZE;
		if (debug) printf("AUDIO LAG OUT: audio_ord=%d = %d samples\n",audio_ord,fft_size+fft_size2);
		audio_roff=fft_size/2;
		if (debug) printf("AUDIO LAG REC: audio_roff=%d samples\n",audio_roff);
		}

	if (jack_activate(client))
		{
		if (debug) fprintf (stderr, " [FAIL]\n");
		return 1;
		}

	if (debug) fprintf(stderr," [OK]\n");
	return 0;
	}

// pre-calculate windowing data: only 1/2 of window as we mirror the other side
// type: 0 - Hann - cos
// type: 1 - Bartlett - triangle
// type: 2 - Hann#2 - cos ^ 2
// type: 3 - Welch - 2nd degree poly
// type: 4 - fft_size/2 square window
// type: 5 - fft_size   square window
void load_data_window(int type)
	{
	int j;

	switch (type)
		{
		case 0:	// Hann
			for (j=0;j<fft_size2;j++)
				{
				dwindow[j]=0.5f*(1.0f-cos(2.0f*pi*(double)j/(fft_size-1)));
//				printf("dwindow[%4d]=%f\n",j,dwindow[j]);
				}
			break;
		case 1:	// Bartlett
			for (j=0;j<fft_size2;j++)
				{
				dwindow[j]=(float)(((double)2.0f*(double)j)/(double)(fft_size-1));
//				printf("dwindow[%4d]=%f\n",j,dwindow[j]);
				}
			break;
		case 2:	// Hann^2
			for (j=0;j<fft_size2;j++)
				{
				dwindow[j]=cos(2.0f*pi*(double)j/(fft_size-1));
				if (dwindow[j]>0.0f) dwindow[j]=0.5f*(1.0f-dwindow[j]*dwindow[j]);
				else dwindow[j]=0.5f*(1.0f+dwindow[j]*dwindow[j]);
//				printf("dwindow[%4d]=%f\n",j,dwindow[j]);
				}
			break;
		case 3:	// Welch
			for (j=0;j<fft_size2;j++)
				{
				dwindow[j]=2.0f*(((double)j-(double)(fft_size-1)/2.0f)/(double)(fft_size-1));
				dwindow[j]=1.0f-dwindow[j]*dwindow[j];
//				printf("dwindow[%4d]=%f\n",j,dwindow[j]);
				}
			break;
		case 4:	// fft_size/2 square window
			for (j=0;j<fft_size2;j++)
				{
				if (j<fft_size4) dwindow[j]=0.0f;
				else dwindow[j]=1.0f;
//				printf("dwindow[%4d]=%f\n",j,dwindow[j]);
				}
			break;
		case 5:	// fft_size   square window - full width of fft
			for (j=0;j<fft_size2;j++)
				{
				dwindow[j]=1.0f;
//				printf("dwindow[%4d]=%f\n",j,dwindow[j]);
				}
			break;
		}
	}
