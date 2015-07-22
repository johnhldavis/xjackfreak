/****************************************************************************
*
* This file (signals.c) is part of xjackfreak, an audio frequency analyser.
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

int got_sigusr1=0,got_sigusr2,got_sigdie=0,got_sigpipe=0,got_sighup=0,got_sigalarm=0;

void sigalarm(int sig)
	{
	got_sigalarm=1;
	}

void sigusr1(int sig)
	{
	got_sigusr1=1;
	running=1;
	}

void sigusr2(int sig)
	{
	got_sigusr2=1;
	running=0;
	}

void sigpipe(int sig)
	{
	got_sigpipe=1;
	}

void sighup(int sig)
	{
	got_sighup=1;
	}

void sigdie(int sig)
	{
	printf("sigdie: CAUGHT SIGDIE!\n");
	fflush(stdout);
	got_sigdie=1;
	running=0;
	}

void sys_init(void)
	{
	(void)signal(SIGALRM,sigalarm);
	(void)signal(SIGUSR1,sigusr1);
	(void)signal(SIGUSR2,sigusr2);
	(void)signal(SIGHUP ,sighup);
	(void)signal(SIGKILL,sigdie);
	(void)signal(SIGTERM,sigdie);
	(void)signal(SIGINT ,sigdie);
	(void)signal(SIGQUIT,sigdie);
	(void)signal(SIGPIPE,sigpipe);
	}
