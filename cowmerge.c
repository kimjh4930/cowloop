/*
** This program updates the read-only file with all modifications stored
** in the cow-file.
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
** $Log: cowmerge.c,v $
** Revision 1.4  2006/12/12  hjt
** Support for COWPACKED flag added
**
** Revision 1.3  2005/07/21 06:14:54  root
** Cosmetic changes source code.
**
** Revision 1.2  2005/07/20 07:38:00  root
** Terminology change: checksum renamed to fingerprint
**
** Revision 1.1  2005/05/09 13:11:54  root
** Initial revision
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

static void 		prusage(char *);
static unsigned long	calcsum(int, char *, int);

int
main(int argc, char *argv[])
{
	int		fdrdo, fdcow, bytenum, bitnum, modifications=0;
	char		*progname = argv[0],
			*rdofile  = argv[1],
			*cowfile  = argv[2];
	char		*bitmap, *cowbuf;
	off_t		offset;
	struct cowhead	cowhead;

	/*
	** check number of command-line parameters (file names)
	*/
	if (argc != 3) {
		prusage(progname);
		exit(1);
	}

	/*
	** open cowfile
	*/
	if ( (fdcow = open(cowfile, O_RDONLY|O_LARGEFILE)) == -1) {
		fprintf(stderr, "%s - can not open ", progname);
		perror(cowfile);
		exit(1);
	}

	/*
	** read the cowheader and check its consistency
	*/
	if ( read(fdcow, &cowhead, sizeof cowhead) < sizeof cowhead) {
		perror("read of cowheader");
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
			"cowfile %s is packed (cowpack -u needed)\n", cowfile);
		exit(1);
	}
	if (cowhead.flags &= COWDIRTY) {
		fprintf(stderr,
			"cowfile %s is dirty (repair needed)\n", cowfile);
		exit(1);
	}

	/*
	** allocate space for entire bitmap and read it into memory
	*/
	if ( (bitmap = malloc(cowhead.mapsize)) == NULL) {
		fprintf(stderr, "cannot allocate %lu bytes for bitmap\n",
		                cowhead.mapsize);
		exit(1);
	}

	offset = cowhead.mapunit;

	if ( lseek(fdcow, offset, SEEK_SET) == -1) {
		perror("lseek to bitmap in cowfile");
		exit(1);
	}

	if ( read(fdcow, bitmap, cowhead.mapsize) < cowhead.mapsize) {
		perror("read bitmap from cowfile");
		exit(1);
	}

	/*
	** allocate space to read modified data blocks from cowfile
	*/
	if ( (cowbuf = malloc(cowhead.mapunit)) == NULL) {
		fprintf(stderr, "cannot allocate %lu bytes for data0block\n",
		                cowhead.mapunit);
		exit(1);
	}

	/*
	** cowfile is ready to be read now
	**
	** open read-only file for read-write (yes, this sounds odd)
 	*/
	if ( (fdrdo = open(rdofile, O_RDWR|O_LARGEFILE)) == -1) {
		fprintf(stderr, "%s - can not open ", progname);
		perror(rdofile);
		exit(1);
	}

	/*
	** be sure that the fingerprint of the cowfile corresponds with
	** this rdofile
	*/
	if ( calcsum(fdrdo, cowbuf, cowhead.mapunit) != cowhead.rdofingerprint){
		fprintf(stderr,
			"%s - fingerprint of %s does not correspond with %s\n",
			progname, rdofile, cowfile);
		exit(1);
	}

	/*
	** be sure that the user wants the read-only file to be modified
	*/
	printf("Really want to modify %s with updates from %s (Y/N)? ",
							rdofile, cowfile);

	fflush(stdout);

	if ( getchar() != 'Y')
		exit(0);

	/*
	** search for modified blocks in the bitmap of the cowfile
	*/
	for (bytenum=0; bytenum < cowhead.mapsize; bytenum++) {
		if ( *(bitmap+bytenum) == 0)	/* eight unmodified blocks ? */
			continue;

		/*
		** at least one block is set to modified in this byte
		*/
		for (bitnum=0; bitnum < 8; bitnum++) {
			if ( *(bitmap+bytenum) & (1<<bitnum)) {	/* bit set ? */
				modifications++;

				/*
				** seek datablock in cowfile and read it
				*/
				offset = (off_t)cowhead.doffset +
				         ((bytenum*8+bitnum) * cowhead.mapunit);

				if (lseek(fdcow, offset, SEEK_SET) == -1) {
					perror("lseek cowfile");
					exit(1);
				}

				if ( read(fdcow, cowbuf, cowhead.mapunit)
				                < sizeof cowhead.mapunit ) {
					perror("read from cowfile failed");
					exit(1);
				}

				/*
				** seek datablock in rdofile and write it
				*/
				offset = (off_t)(bytenum*8+bitnum) *
				                 cowhead.mapunit;

				if (lseek(fdrdo, offset, SEEK_SET) == -1) {
					perror("lseek rdofile");
					exit(1);
				}

				if ( write(fdrdo, cowbuf, cowhead.mapunit)
				                 < sizeof cowhead.mapunit ) {
					perror("write to rdofile failed");
					exit(1);
				}
			}
		}
	}

	close(fdcow);
	close(fdrdo);

	printf("Number of blocks modified in %s: %d\n",
					rdofile, modifications);

	exit(0);
}

static void
prusage(char *pname)
{
	fprintf(stderr, "Usage: %s rdofile cowfile\n", pname);
	fprintf(stderr, "\trdofile\tread-only file to be updated\n");
	fprintf(stderr, "\tcowfile\tcowfile containing the updates\n");
}

static unsigned long
calcsum(int fd, char *iobuf, int iolen)
{
	unsigned long	fingerprint;
	int		i, nrval;

	/*
	** determine fingerprint for read-only file
	**      calculate fingerprint from first four datablocks
	**      which do not contain binary zeroes
	*/
	for (i=0, fingerprint=0, nrval=0; nrval < 4; i++) {
		int		j;
		unsigned char	cs;

		/*
		** read next block
		*/
		if ( read(fd, iobuf, iolen) < iolen)
			break;

		/*
		** calculate fingerprint by adding all byte-values
		*/
		for (j=0, cs=0; j < iolen; j++)
			cs += *(iobuf+j);

		if (cs == 0)    /* block probably contained zeroes */
			continue;

		/*
		** shift byte-value to proper place in final fingerprint
		*/
		fingerprint |= cs << (nrval*8);
		nrval++;
	}

	return fingerprint;
}
