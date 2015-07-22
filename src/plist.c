/****************************************************************************
*
* This file (plist.c) is part of xjackfreak, an audio frequency analyser.
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

int to_draw=0,done_draw=0;

int pentry_add(int dtype,int all)
	{
	if (all) to_draw=7;
	else if (dtype==2) to_draw=to_draw|3;
	else to_draw=to_draw|dtype;
	return 0;
	}

/*
when get woc,
	reset woc
	do done_draw_post
	reset done_draw
*/
void pentry_woc()
	{
	if (gmode!=3) return;
	if (done_draw==0) return;
	if (win_mapped>0)
		{
		if ((done_draw  )>4)
			{
			draw_status_post();
			draw_controls_post();
			}
		else
			{
			if ((done_draw&1)>0) draw_status_post();
			if ((done_draw&2)>0) draw_controls_post();
			}
		}
	done_draw=0;
	return;
	}

/*
if (no_woc)
	do_put
	save to_draw -> done_draw
	reset to_draw

000 0
001 1 status
010 2 control
011 3 status_control
100 4 main
101 5 all
110 6 all
111 7 all
*/
int pentry_process()
	{
	int all=0,tmp_draw=to_draw;
	char t_str[32];

	sprintf(t_str,"---------------.");
//printf("pentry_process():\n");
	if (win_mapped==0) return 0;
	if (wait_on_complete>0) return 0;
	if (to_draw==0) return 0;


	t_str[0]=(char)(tmp_draw+48);

	if (gmode==3) { wait_on_complete=1; t_str[1]='W'; }
	if ((tmp_draw&7)>0) all=1;

	if ((tmp_draw&1)>0) { draw_status();   t_str[2]='S'; }
	if ((tmp_draw&2)>0) { draw_controls(); t_str[3]='C'; }
	if ((tmp_draw&4)>0) { if (BUT_RECBUF==0) draw_main(all); t_str[4]='M'; }

	if (tmp_draw>=5) { draw_status_controls_main_put(); t_str[ 6]='a'; }
	if (tmp_draw==4) { draw_main_put();                 t_str[ 7]='m'; }
	if (tmp_draw==3) { draw_status_controls_put();      t_str[ 8]='T'; }
	if (tmp_draw==2) { draw_controls_put();             t_str[ 9]='c'; }
	if (tmp_draw==1) { draw_status_put();               t_str[10]='s'; }
	if (gmode!=3)
		{
		if ((tmp_draw&1)>0) { draw_status_post();   t_str[11]='s'; }
		if ((tmp_draw&2)>0) { draw_controls_post(); t_str[12]='c'; }
		}
if (debug) printf("%s\n",t_str);
	done_draw=tmp_draw;
	to_draw=0;
	return 0;
	}
