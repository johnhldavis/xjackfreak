/****************************************************************************
*
* This file (load_ppm.c) is part of xjackfreak, an audio frequency analyser.
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

#define MAX_PPM_DATA 65536
#define MAX_PPMS 8

int ppm_count=0;
int ppm_wid[MAX_PPMS],ppm_hgt[MAX_PPMS];
unsigned char ppm[MAX_PPMS][MAX_PPM_DATA];

int load_ppm(char *_fname)
	{
	FILE *fp;
	char t_str[3500];
	int wid=0,hgt=0,i,j;
	int bcnt=0;

	if (ppm_count>=MAX_PPMS) return 1;
	if ((fp=fopen(_fname,"r"))!=NULL)
		{
		fgets(t_str,255,fp);
		if (t_str[0]=='#') fgets(t_str,255,fp);
		if (strcmp(t_str,"P6\n")!=0)
			{
			fprintf(stderr,"%s: NOT P6!\n",_fname);
			fclose(fp);
			return 1;
			}
		fgets(t_str,255,fp);
		if (t_str[0]=='#') fgets(t_str,255,fp);
		if (sscanf(t_str,"%d %d",&wid,&hgt)!=2)
			{
			fprintf(stderr,"%s: NO width or height!\n",_fname);
			fclose(fp);
			return 1;
			}
		if ((wid>1024) || (hgt>1024))
			{
			fprintf(stderr,"%s: width or height>1024!\n",_fname);
			fclose(fp);
			return 1;
			}
		ppm_wid[ppm_count]=wid;
		ppm_hgt[ppm_count]=hgt;
		fgets(t_str,255,fp);
		if (t_str[0]=='#') fgets(t_str,255,fp);
		if (sscanf(t_str,"%d",&i)!=1)
			{
			fprintf(stderr,"%s: No maxcolour!\n",_fname);
			fclose(fp);
			return 1;
			}
		for (i=0;i<hgt;i++)
			{
//			if (fgets(t_str,3490,fp)==NULL)
			if (fread(t_str,1,wid*3,fp)!=wid*3)
				{
				fprintf(stderr,"%s: only read %d of %d rows!\n",_fname,i,hgt);
				fclose(fp);
				return 1;
				}
			for (j=0;j<wid;j++)
				{
				ppm[ppm_count][bcnt++]=t_str[j*3];
				ppm[ppm_count][bcnt++]=t_str[j*3+1];
				ppm[ppm_count][bcnt++]=t_str[j*3+2];
				ppm[ppm_count][bcnt++]=0;
				}
			}
		fclose(fp);
		ppm_count++;
		return 0;
		}
	return 1;
	}
