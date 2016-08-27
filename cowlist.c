/*
** This program lists the status of a cowfile.
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
** $Log: cowlist.c,v $
** Revision 1.7  2006/12/12 hjt
** Support for COWPACKED flag added
**
** Revision 1.6  2005/07/21 06:14:48  root
** Cosmetic changes source code.
**
** Revision 1.5  2005/03/17 14:36:36  root
** Fixed some license issues.
**
** Revision 1.4  2005/02/25 12:46:01  root
** Support large files.
**
** Revision 1.3  2005/02/18 11:50:20  gerlof
** Printf formats adapted.
**
** Revision 1.2  2004/08/09 08:05:47  gerlof
** Size of rdofile not to be divided by 1024.
**
** Revision 1.1  2004/05/27 07:14:11  gerlof
** Initial revision
**
*/
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS       64

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include "version.h"
#include "cowloop.h"

static void prusage(char *);

int
main(int argc, char *argv[])
{
	long int	fd;
	struct cowhead	cowhead;

	/*
	** check number of command-line parameters (file names)
	*/
	if (argc < 2) {
		prusage(argv[0]);
		exit(1);
	}

	/*
	** open the cowfile
	*/
	if ( (fd = open(argv[1], O_RDWR|O_LARGEFILE)) == -1) {
		perror(argv[1]);
		exit(1);
	}

	/*
	** read the cowheader and check its consistency
	*/
	if ( read(fd, &cowhead, sizeof cowhead) < sizeof cowhead) {
		perror("error during read of cowheader");
		exit(1);
	}

	if (cowhead.magic != COWMAGIC) {
		fprintf(stderr,
		        "%s is not a valid cowfile (wrong magic)\n", argv[1]);
		exit(1);
	}

	printf("\nInfo about cowfile %s:\n", argv[1]);

	printf("     state cowfile: %9s",
				cowhead.flags & COWDIRTY ? "dirty" : "clean");
	if (cowhead.flags & COWPACKED) printf(" packed");
	printf("\n");
	printf("    header-version: %9d\n",
				cowhead.version);
	printf("       data offset: %9lu\n", cowhead.doffset);
	printf("           mapunit: %9lu\n",
				cowhead.mapunit);
	printf("     bitmap-blocks: %9lu (of %lu bytes)\n",
				cowhead.mapsize/cowhead.mapunit,
				cowhead.mapunit);
	printf("  cowblocks in use: %9lu (of %lu bytes)\n",
				cowhead.cowused, cowhead.mapunit);
	printf("      size rdofile: %9lu (of %lu bytes)\n",
				cowhead.rdoblocks,
                                cowhead.mapunit);
	return 0;
}

static void
prusage(char *pname)
{
	fprintf(stderr, "Usage: %s cowfile\n", pname);
}
