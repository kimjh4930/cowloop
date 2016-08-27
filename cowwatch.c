/*
** This program can be used to obtain information about the available
** space in the filesystem holding the cowfile or wait until the available 
** space for the cowfile drops below a specified threshold.
**
** Author: Gerlof Langeveld - AT Computing (July 2005)
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
** $Log: cowwatch.c,v $
** Revision 1.3  2005/07/21 06:32:41  root
** Cosmetic changes.
**
** Revision 1.2  2005/07/21 06:14:57  root
** Cosmetic changes source code.
**
** Revision 1.1  2005/07/20 13:08:10  root
** Initial revision
*/
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include "version.h"
#include "cowloop.h"

static void	prusage (char *);
static dev_t	new_decode_dev(dev_t);

int
main(int argc, char *argv[])
{
	int		fd;
	struct cowwatch	cowwatch;
	struct stat	statinfo;

	/*
	** verify arguments
	*/
	if (argc < 2 || argc > 3) {
		prusage(argv[0]);
		exit(1);
	}

	/*
	** open cowloop
	*/
	if ( (fd = open(COWCONTROL, O_RDONLY)) == -1) {
		perror(COWCONTROL);
		exit(2);
	}

	/*
	** fill structure info for ioctl COWWATCH
	**
	** first obtain major-minor number of cowdevice
	*/
	if ( stat(argv[1], &statinfo)) {
		perror("stat cowdevice");
		exit(2);
	}

	if ( ! S_ISBLK(statinfo.st_mode) ) {
		fprintf(stderr, "%s: not a block device\n", argv[1]);
		exit(2);
	}

	cowwatch.flags 	= 0;
	cowwatch.device	= new_decode_dev(statinfo.st_rdev);

	if (argc > 2) {
		char	*endptr;

		cowwatch.threshold = strtol(argv[2], &endptr, 0);

		if (*endptr) {
			fprintf(stderr,
			        "%s: not a valid numerical value\n", argv[2]);
			exit(3);
		} else {
			cowwatch.flags |= WATCHWAIT;
		}
	}

	/*
	** issue ioctl to obtain space info
	*/
	if ( ioctl(fd, COWWATCH, &cowwatch) < 0) {
		perror("watch space information");
		exit(5);
	}

	/*
	** show info obtained
	*/
	printf("Total: %9lu Kb\nAvail: %9lu Kb\n",
		cowwatch.totalkb, cowwatch.availkb);

	exit(0);
}

static void
prusage(char *prog)
{
	fprintf(stderr, "Usage: %s cowdevice [threshold]\n", prog);
	fprintf(stderr, "\tWhen a threshold-value is specified, cowwatch\n");
	fprintf(stderr, "\tblocks till the available space for the cowfile\n");
	fprintf(stderr, "\tdrops below the threshold-value (in Kb).\n");
}

static dev_t
new_decode_dev(dev_t rdev)
{
        unsigned major = (rdev & 0xfff00) >> 8;
        unsigned minor = (rdev & 0xff) | ((rdev >> 12) & 0xfff00);

        return major << 20 | minor;
}
