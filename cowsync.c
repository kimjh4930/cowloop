/*
** This program asks the cowloop-driver to flush the cowhead and bitmaps
**
** Author: Gerlof Langeveld - AT Computing (May 2004)
** Current maintainer: Hendrik-Jan Thomassen - AT Computing (Feb. 2009)
** Email:  hjt@ATComputing.nl
** -----------------------------------------------------------------------
** Copyright (C) 2005,2009 AT Computing
**
** This program is free software; you can redistribute it and/or modify it
** under the terms of the GNU General Public License as published by the
** Free Software Foundation; either version 2, or (at your option) any
** later version.
**
** This program is distributed in the hope that it will be useful, but
** WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
** See the GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
** -----------------------------------------------------------------------
**
** Note that the following number is *not* the cowloop package version no.
** The cowloop version no. is in the file version.h
** $Log: cowsync.c,v $
** Revision 1.7  2005/07/21 06:14:55  root
** Cosmetic changes source code.
**
** Revision 1.6  2005/03/17 14:36:02  root
** Fixed some license issues.
**
** Revision 1.5  2005/02/18 11:51:06  root
** Ioctl-command and device name adapted to new conventions.
**
** Revision 1.4  2004/08/09 08:05:27  gerlof
** Typo corrected.
**
** Revision 1.3  2004/05/27 06:00:13  owner
** Added another sync before flushing the bitmap.
**
** Revision 1.2  2004/05/26 21:23:45  owner
** added sync system call.
**
** Revision 1.1  2004/05/26 13:24:41  owner
** Initial revision
**
*/
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include "version.h"
#include "cowloop.h"

int
main(int argc, char *argv[])
{
	int fd;

	/*
	** open cowloop
	*/
	if ( (fd = open(COWCONTROL, O_RDONLY)) == -1) {
		perror(COWCONTROL);
		exit(1);
	}

	/*
	** sync all stuff before flushing bitmap to cowfile
	*/
	sync();

	/*
	** issue ioctl to force flushing bitmap
	*/
	if ( ioctl(fd, COWSYNC, 0) < 0) {
		perror("ioctl" COWCONTROL);
		exit(1);
	}

	/*
	** sync flushed bitmap to cowfile
	*/
	sync();

	exit(0);
}
