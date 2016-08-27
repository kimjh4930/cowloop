/*
** This program checks the cow-file (first argument) and repairs the bitmap.
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
** $Log: cowrepair.c,v $
** Revision 1.10  2006/12/12  hjt
** Support for COWPACKED flag added
**
** Revision 1.9  2005/07/21 06:14:56  root
** Cosmetic changes source code.
**
** Revision 1.8  2005/03/17 14:36:10  root
** Fixed some license issues.
**
** Revision 1.7  2005/03/07 14:41:24  root
** Minor bug fix.
**
** Revision 1.6  2005/02/25 12:46:18  root
** Support large files.
**
** Revision 1.5  2005/02/18 11:50:49  gerlof
** Printf formats adapted.
**
** Revision 1.4  2004/08/09 08:07:16  gerlof
** Handling verbose-option simplified.
**
** Revision 1.3  2004/05/27 07:15:33  gerlof
** Removed listing-possibility (separate program cowlist).
**
*/
#define	_LARGEFILE64_SOURCE
#define	_FILE_OFFSET_BITS	64

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include "version.h"
#include "cowloop.h"

#define CALCBYTE(x)     ((x)>>3)
#define CALCBIT(x)      ((x)&7)

static void prusage(char *);

int
main(int argc, char *argv[])
{
	long int	i, c, fd, bnum, modified, bytenr, bitnr;
	char		*cowfile, *buf, *nullbuf, *bitmap;
	struct cowhead	cowhead;
	char		forcedflag = 0, nomodflag = 0, verboseflag = 0;
	off_t		offset;

	/*
	** check number of command-line parameters (file names)
	*/
	if (argc < 2) {
		prusage(argv[0]);
		exit(1);
	}

	/*
	** handle flags
 	*/
	while ( (c=getopt(argc, argv, "fnv")) != EOF) {
		switch (c) {
		   case 'f':
			forcedflag++;
			break;

		   case 'n':
			nomodflag++;
			break;

		   case 'v':
			verboseflag++;
			break;

		   default:
			prusage(argv[0]);
			exit(1);
		}
	}

	if (optind != argc) {
		cowfile = argv[optind];
	} else {
		prusage(argv[0]);
		exit(1);
	}

	/*
	** open the cowfile
	*/
	if ( (fd = open(cowfile, O_RDWR|O_LARGEFILE)) == -1) {
		perror(cowfile);
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
		        "%s is not a valid cowfile (wrong magic)\n", cowfile);
		exit(1);
	}

	if (cowhead.version != COWVERSION) {
		fprintf(stderr,
		        "version of cowfile %s not supported\n", cowfile);
		exit(1);
	}

	if (cowhead.flags &= COWPACKED) {
		fprintf(stderr,
		       "cowfile %s cannot be repaired while packed\n", cowfile);
		exit(1);
	}

	if ( !(cowhead.flags &= COWDIRTY) && !forcedflag) {
		fprintf(stderr, "cowfile %s is not dirty\n", cowfile);
		exit(0);
	}

	/*
	** allocate space and read entire bitmap into memory
	*/
	if ( (bitmap = malloc(cowhead.mapsize)) == NULL) {
		fprintf(stderr, "cannot allocate %lu bytes for bitmap\n",
		                cowhead.mapsize);
		exit(1);
	}
		
	offset = cowhead.mapunit;

	if (lseek(fd, offset, SEEK_SET) == -1) {
		perror("lseek to bitmap");
		exit(1);
	}

	if ( read(fd, bitmap, cowhead.mapsize) < cowhead.mapsize) {
		perror("read bitmap");
		exit(1);
	}

	/*
	** fill a buffer with binary zeroes to compare against
	** the data-block which is read from the cowfile
	*/
	if ( (nullbuf = malloc(cowhead.mapunit)) == NULL) {
		fprintf(stderr, "cannot allocate %lu bytes for null bytes\n",
		                cowhead.mapunit);
		exit(1);
	}

	memset(nullbuf, 0, cowhead.mapunit);

	if ( (buf = malloc(cowhead.mapunit)) == NULL) {
		fprintf(stderr, "cannot allocate %lu bytes for datablock\n",
		                cowhead.mapunit);
		exit(1);
	}

	/*
	** seek to the start-position of the datablocks
	** and start reading all datablocks, correcting
	** the bits in the bitmap (if needed)
	*/
	offset = cowhead.doffset;

	if (lseek(fd, offset, SEEK_SET) == -1) {
		perror("lseek to start-offset data");
		exit(1);
	}

	for (bnum=0, modified=0; read(fd, buf, cowhead.mapunit) > 0; bnum++) {
		/*
		** beware that this comparison is much faster than
		** checking the contents of buf[] byte-by-byte
		*/
		if (memcmp(buf, nullbuf, cowhead.mapunit) == 0) {
			/*
			** all bytes contain zero:
			**   this might mean we have read a non-written
			**   block or a block which has been written
			**   explicitly with binary zeroes (rare case)
			*/
			continue;
		}

		/*
		** this datablock contains written data
		** -> correct bitmap if needed
		*/
		bytenr	= CALCBYTE(bnum);
		bitnr	= CALCBIT (bnum);

		if ( *(bitmap+bytenr)&(1<<bitnr) )
			continue;	/* bit is set already */

		modified++;

		if (verboseflag) {
			printf("data block %9lu (offset %11llu) not marked "
			       "in bitmap; corrected\n",
				bnum,
				(unsigned long long)bnum * cowhead.mapunit);
		}

		*(bitmap+bytenr) |= 1<<bitnr;
	}

	/*
	** rewrite the modified bitmap to the cowfile
	*/
	if (!nomodflag && modified) {
		offset = cowhead.mapunit;

		if (lseek(fd, offset, SEEK_SET) == -1) {
			perror("lseek to bitmap");
			exit(1);
		}

		if ( write(fd, bitmap, cowhead.mapsize) < cowhead.mapsize) {
			perror("rewrite bitmap failed");
			exit(1);
		}
	}

	/*
	** count the number of bits set in bitmap and
	** rewrite the cowhead
	*/
	for (i=0, cowhead.cowused=0; i < cowhead.mapsize; i++) {
		if ((*(bitmap+i)) & 0x01)        cowhead.cowused++;
		if ((*(bitmap+i)) & 0x02)        cowhead.cowused++;
		if ((*(bitmap+i)) & 0x04)        cowhead.cowused++;
		if ((*(bitmap+i)) & 0x08)        cowhead.cowused++;
		if ((*(bitmap+i)) & 0x10)        cowhead.cowused++;
		if ((*(bitmap+i)) & 0x20)        cowhead.cowused++;
		if ((*(bitmap+i)) & 0x40)        cowhead.cowused++;
		if ((*(bitmap+i)) & 0x80)        cowhead.cowused++;
	}

	cowhead.flags &= ~COWDIRTY;

	if (!nomodflag) {
		if (lseek(fd, (off_t)0, SEEK_SET) == -1) {
			perror("lseek to cowhead");
			exit(1);
		}

		if ( write(fd, &cowhead, sizeof cowhead) < sizeof cowhead) {
			perror("rewrite cowhead failed");
			exit(1);
		}
	}

	printf("Number of corrections in bitmap: %ld", modified);

	if (nomodflag)
		printf(" (not done due to flag -n)\n");
	else
		printf("\n");

	close(fd);

	exit(0);
}

static void
prusage(char *pname)
{
	fprintf(stderr, "Usage: %s [-f] [-n] [-v[v]] cowfile\n\n", pname);
	fprintf(stderr,
		"   -f   forced:   repair cowfile which is not dirty\n");
	fprintf(stderr,
		"   -n   nomodify: do not modify cowfile (fake mode)\n");
	fprintf(stderr,
		"   -v   verbose:  provide extra info\n");
}
