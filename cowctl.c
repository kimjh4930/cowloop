/*
** This program can be used to:
**	- reopen a cowfile read-only for an existing cowdevice
**	- close a cowfile for an existing cowdevice
**
** This functionality is mainly used for LiveCD's based on cowloop
** to be able to umount the filesystem holding the cowfile in a proper
** way during shutdown.
**
** Author: Gerlof Langeveld - AT Computing (August 2005)
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
** $Log: cowctl.c,v $
** Revision 1.2  2005/08/08 11:27:10  root
** Cosmetic changes.
**
** Revision 1.1  2005/08/08 11:23:00  root
** Initial revision
**
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

static void	cowctl        (char *, int);
static void	prusage       (char *);
static dev_t	new_decode_dev(dev_t);

int
main(int argc, char *argv[])
{
	/*
	** verify arguments
	*/
	if (argc < 2 || argv[1][0] != '-' || strlen(argv[1]) != 2) {
		prusage(argv[0]);
		exit(1);
	}

	/*
	** react on flag (always first argument)
	*/
	switch (argv[1][1]) {
	   case 'c':			/* close cowfile  */
		if (argc != 3) {
			prusage(argv[0]);
			exit(1);
		}
		cowctl(argv[2], COWCLOSE);
		break;

	   case 'r':			/* reopen cowfile read-only */
		if (argc != 3) {
			prusage(argv[0]);
			exit(1);
		}
		cowctl(argv[2], COWRDOPEN);
		break;

	   default:			/* wrong flag     */
		prusage(argv[0]);
		exit(1);
	}

	exit(0);
}

static void
cowctl(char *devpath, int cmd)
{
	int		fd;
	struct stat	statinfo;
	unsigned long	device;

	/*
	** open cowloop
	*/
	if ( (fd = open(COWCONTROL, O_RDONLY)) == -1) {
		perror(COWCONTROL);
		exit(2);
	}

	/*
	** determine major-minor number of device
	*/
	if (stat(devpath, &statinfo)) {
		perror("stat preferred device");
		exit(2);
	}

	if ( ! S_ISBLK(statinfo.st_mode) ) {
		fprintf(stderr, "%s: not a block device\n", devpath);
		exit(2);
	}

	device	= new_decode_dev(statinfo.st_rdev);

	/*
	** issue ioctl 
	*/
	if ( ioctl(fd, cmd, &device) < 0) {
		perror("close/reopen cowfile");
		exit(2);
	}
}

static void
prusage(char *prog)
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr,
		"\t%s -c cowdevice\tclose cowfile related to device\n", prog);
	fprintf(stderr,
		"\t%s -r cowdevice\treopen cowfile read-only\n", prog);
}

static dev_t
new_decode_dev(dev_t rdev)
{
        unsigned major = (rdev & 0xfff00) >> 8;
        unsigned minor = (rdev & 0xff) | ((rdev >> 12) & 0xfff00);

        return	 major << 20 | minor;
}
