/*
** This program can be used to:
**	- list the existing cowdevices
**	- activate a new cowdevice
**	- deactivate an existing cowdevice
**
** Author: Gerlof Langeveld - AT Computing (March 2005)
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
** $Log: cowdev.c,v $
** Revision 1.5  2009/02/09 12:33:00  hjt
** Data type fixup
**
** Revision 1.4  2005/07/21 06:14:31  root
** Cosmetic changes source code.
**
** Revision 1.3  2005/03/17 14:35:36  root
** Fixed some license issues.
**
** Revision 1.2  2005/03/07 14:41:42  root
** Modified messages.
**
** Revision 1.1  2005/02/23 07:34:26  gerlof
** Initial revision
**
** Revision 1.1  2005/02/18 11:51:47  root
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

#define	LISTLAYOUT	"%-12s  %-32s %-32s\n"

char		*rdostring = "read-only file";
char		*cowstring = "copy-on-write file";

static void	pairlist(void);
static void	pairadd (char *, char *, char *);
static void	pairdel (char *);

static void	prusage (char *);
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
	   case 'l':			/* list cowdevices  */
		if (argc != 2) {
			prusage(argv[0]);
			exit(1);
		}
		pairlist();
		break;

	   case 'a':			/* activate cowdevice    */
		if (argc < 4 || argc > 5) {
			prusage(argv[0]);
			exit(1);
		}
		pairadd(argv[2], argv[3], argv[4]);
		break;

	   case 'd':			/* deactivate cowdevice */
		if (argc != 3) {
			prusage(argv[0]);
			exit(1);
		}
		pairdel(argv[2]);
		break;

	   default:			/* wrong flag     */
		prusage(argv[0]);
		exit(1);
	}

	exit(0);
}

/*
** list existing cowdevices
*/
static void
pairlist(void)
{
	int	i, nl, nr;
	char	procname[128], devname[128], line[256],
		key[PATH_MAX], value[PATH_MAX],
		rdopath[PATH_MAX], cowpath[PATH_MAX];
	FILE	*fp;

	/*
	** try to open all possible procfile names
	*/
	for (i=0, nl=0; i < MAXCOWS; i++) {
		snprintf(procname, sizeof procname, COWPROCFILE, i);

		if ( (fp = fopen(procname, "r")) == NULL)
			continue;

		/*
		** open succeeded
		** produce header-line before first device
		*/
		if (nl++ == 0) {
			printf(LISTLAYOUT, "cowdevice", rdostring, cowstring);
			printf("----------------------------------------"
			       "----------------------------------------\n");
		}

		/*
		** search filenames for read-only file and cowfile
		*/
		while ( fgets(line, sizeof line, fp) ) {
			if ( (nr = sscanf(line, "%[^:]:%s", key, value)) == 2) {
				if ( strstr(key, rdostring) )
					strcpy(rdopath, value);

				if ( strstr(key, cowstring) )
					strcpy(cowpath, value);
			}
		}

		fclose(fp);

		/*
		** produce output line for this device
		*/
		snprintf(devname, sizeof devname, COWDEVICE, (long int)i);

		printf(LISTLAYOUT, devname, rdopath, cowpath);
	}

	if (nl)
		printf("\n");
}

/*
** activate a cowdevice
*/
static void
pairadd (char *rdopath, char *cowpath, char *prefdev)
{
	int		fd;
	struct cowpair	cowpair;
	struct stat	statinfo;

	/*
	** open cowloop
	*/
	if ( (fd = open(COWCONTROL, O_RDONLY)) == -1) {
		perror(COWCONTROL);
		exit(2);
	}

	/*
	** fill structure info for ioctl COWMKPAIR
	*/
	cowpair.rdofile		= (unsigned char *)rdopath;
	cowpair.cowfile		= (unsigned char *)cowpath;

	cowpair.rdoflen		= strlen(rdopath);
	cowpair.cowflen		= strlen(cowpath);

	/*
	** check if optional preferred device is specified
	*/
	if (prefdev) {
		/*
		** specific cowdevice wanted
		** obtain its major-minor number
		*/
		if (stat(prefdev, &statinfo)) {
			perror("stat preferred cowdevice");
			exit(2);
		}

		if ( ! S_ISBLK(statinfo.st_mode) ) {
			fprintf(stderr, "%s: not a block device\n", prefdev);
			exit(2);
		}

		cowpair.device	= new_decode_dev(statinfo.st_rdev);
	} else {
		/*
		** arbitrary cowdevice wanted
		*/
		cowpair.device	= ANYDEV;
	}

	/*
	** issue ioctl to activate new cowdevice
	*/
	if ( ioctl(fd, COWMKPAIR, &cowpair) < 0) {
		perror("activate new cowdevice");
		exit(2);
	}

	/*
	** show cowdevice obtained
	*/
	printf(COWDEVICE, cowpair.device & 0xfffff);
	printf("\n");
}

static void
pairdel (char *devpath)
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
	** issue ioctl to deactivate cowdevice
	*/
	if ( ioctl(fd, COWRMPAIR, &device) < 0) {
		perror("deactivate existing cowdevice");
		exit(2);
	}
}

static void
prusage(char *prog)
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr,
		"\t%s -l                          "
	        "\tlist active cowdevices\n", prog);
	fprintf(stderr,
		"\t%s -a rdofile cowfile [devfile]"
		"\tactivate new cowdevice\n", prog);
	fprintf(stderr,
		"\t%s -d devfile                  "
		"\tdeactivate existing cowdevice\n", prog);
}

static dev_t
new_decode_dev(dev_t rdev)
{
        unsigned major = (rdev & 0xfff00) >> 8;
        unsigned minor = (rdev & 0xff) | ((rdev >> 12) & 0xfff00);

        return	 (major << 20) | minor;
}
