/* cowpack.c  Compress/decompress a cowfile 
   Author: Juergen Christ (October 2006)

   This program is free software; you can redistribute it and/or modify it 
   under the terms of the GNU General Public License as published by the Free 
   Software Foundation; either version 2, or (at your option) any later 
   version.

   This program is distributed in the hope that it will be useful, but WITHOUT 
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
   FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
   for more details.
   
   You should have received a copy of the GNU General Public License along 
   with this program; if not, write to the Free Software Foundation, Inc., 
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include "version.h"
#include "cowloop.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include "zlib.h"  /* See the 'z_includes_README' file */

static void prusage(char* name)
#ifdef __GNUC__
__attribute__((noreturn))
#endif
;
struct falfile;
static void pack(char* cn,char* sn,int zipped);
static void unpack(char* sn,char* cn,int zipped);
static void validatecowheader(struct cowhead* ch, char mode);
static int copyblocksave(struct falfile* c,struct falfile* s,long int shift,
			 void* cpbuf,size_t mu);
static int copyblockcow(struct falfile* s,struct falfile* c,long int shift,
			size_t mu);

struct falfile* falopen(char* name,int openmode,int zipped);
void falperror(const char* p,struct falfile* filp);
ssize_t falwrite(struct falfile* filp,void* buf,size_t len);
ssize_t falread(struct falfile* filp,void* buf,size_t len);
off_t falseek(struct falfile* filp,off_t offset,int whence);
int falflush(struct falfile* filp);
int falclose(struct falfile* filp);
void* falwrbuf(struct falfile* filp,size_t* size);
size_t falwrcount(struct falfile* filp);
size_t falwr(struct falfile* filp,size_t size);
int falwrflush(struct falfile* filp);
int falzipped(struct falfile* filp);

/*
  Parse options and start desired operation. Accepts following options:
  -p: Pack cowfile (source) to savefile (destination)
  -u: Unpack savefile (source) to cowfile (destination)
  -z: Write gzipped savefile (with pack), read gzipped savefile (with unpack).
*/
int
main(int argc,char** argv) {
    int zipped = 0,c,mode = -1;
    while( (c = getopt(argc,argv,"zpu")) != EOF ) {
	switch( c ) {
	    case 'z': zipped = 1; break;
	    case 'p': if( mode != -1 ) prusage(argv[0]); mode = 0; break;
	    case 'u': if( mode != -1 ) prusage(argv[0]); mode = 1; break;
	    default: prusage(argv[0]);
	}
    }
    if( mode == -1 ) prusage(argv[0]);
    if( mode )
	unpack(argv[optind],argv[optind+1],zipped);
    else
	pack(argv[optind],argv[optind+1],zipped);
    return 0;
}

/*
  Print usage information and exit. Therefore, this function is prototyped
  with GCC attribute noreturn.
  Parameters:
  n: Name of the program
*/
static void
prusage(char* n) {
    fprintf(stderr,"Usage: %s [-z] <-p | -u> <source> <dest>\n",n);
    fputs(" where: -z indicates GZipped input (with -u) or output (with -p)\n",
	  stderr);
    fputs("        -p indicates packing source (a cow file) to dest\n",stderr);
    fputs("        -u indicates unpacking source to dest (a cow file)\n",
	  stderr);
    fputs("NOTE: -p and -u are mutually exclusive\n",stderr);
    exit(1);
}

/*
  Create a save file from a cow file. If resulting save file is not zipped, 
  file alignment is preserved.
  Parameters:
  cn: Name of the cowfile (may include a path)
  sn: Name of the savefile (may include a path)
  zipped: Use gzip to compress savefile (0 = no)
*/
static void
pack(char* cn,char* sn,int zipped) {
    // Input and output file
    struct falfile *in,*out;
    struct cowhead cowhead;
    // Bitmap and copy buffer
    char *bitmap,*cpbuf;
    off_t offset;
    // Seeking helpers
    unsigned long shift,last;
    // counters
    unsigned long i;
    int j;
    // Input is cowfile (never gzipped!!!)
    if( !(in = falopen(cn,O_RDONLY | O_LARGEFILE,0)) ) {
	falperror("Opening cowfile",0);
	exit(1);
    }
    // Save file may be zipped
    if( !(out = falopen(sn,O_WRONLY | O_LARGEFILE | O_CREAT,zipped)) ) {
	falperror("Opening outputfile",0);
	falclose(in);
	exit(1);
    }
    if( falread(in,&cowhead,sizeof(cowhead)) < (ssize_t)sizeof(cowhead) ) {
	falperror("Reading cowheader",in);
	falclose(in);
	falclose(out);
	exit(1);
    }
    // Won't return if header is invalid
    validatecowheader(&cowhead, 'p');
    if( falwrite(out,&cowhead,sizeof(cowhead)) < (ssize_t)sizeof(cowhead) ) {
	falperror("Writing cowheader",in);
	falclose(in);
	falclose(out);
	exit(1);
    }
    offset = cowhead.mapunit;
    if( falseek(in,offset,SEEK_SET) == -1 ) {
	falperror("Seeking source bitmap",in);
	falclose(in);
	falclose(out);
	exit(1);
    }
    if( falseek(out,offset,SEEK_SET) == -1 ) {
	falperror("Seeking destination bitmap",out);
	falclose(in);
	falclose(out);
	exit(1);
    }
    if( !(bitmap = malloc(cowhead.mapsize)) ) {
	perror("Allocating bitmap");
	falclose(in);
	falclose(out);
	exit(1);
    }
    if( !(cpbuf = malloc(cowhead.mapunit)) ) {
	perror("Allocating copy buffer");
	free(bitmap);
	falclose(in);
	falclose(out);
	exit(1);
    }
    if( falread(in,bitmap,cowhead.mapsize) < (ssize_t)cowhead.mapsize ) {
	falperror("Reading bitmap",in);
	free(bitmap);
	free(cpbuf);
	falclose(in);
	falclose(out);
	exit(1);
    }
    if( falwrite(out,bitmap,cowhead.mapsize) < (ssize_t)cowhead.mapsize ) {
	falperror("Writing bitmap",out);
	free(bitmap);
	free(cpbuf);
	falclose(in);
	falclose(out);
	exit(1);
    }
    offset = cowhead.doffset;
    if( falseek(in,offset,SEEK_SET) == -1 ) {
	falperror("Seeking source data",in);
	free(bitmap);
	free(cpbuf);
	falclose(in);
	falclose(out);
	exit(1);
    }
    if( falseek(out,offset,SEEK_SET) == -1 ) {
	falperror("Seeking destination data",out);
	free(bitmap);
	free(cpbuf);
	falclose(in);
	falclose(out);
	exit(1);
    }
    // No of cowhead.mapunit blocks from cowhead.doffset.
    shift = -1;
    // Last block that can be written without seeking.
    last = 0;
    // Loop over every byte in the bitmap
    for( i = 0; i < cowhead.mapsize; ++i ) {
	// Loop over all blocks
	for( j = 0; j < 8; ++j ) {
	    ++shift;
	    if( (*(bitmap + i)) & (1 << j) ) {
		// Block is present in cowfile. Copy it to savefile.
		int cpres;
		if( (cpres = copyblocksave(in,out,shift-last,cpbuf,
					   cowhead.mapunit)) < 0 ) {
		    /* Ugly: copyblocksave returns -1 if reading or seeking 
		       fails and -2 if writing fails */
		    falperror("Copying data",(cpres == -1) ? in : out);
		    free(bitmap);
		    free(cpbuf);
		    falclose(in);
		    falclose(out);
		    exit(1); 
		}
		/* I've written block at shift, so I can write block at shift+1
		   without seeking. File position is already appropriate.
		*/
		last = shift + 1;
	    }
	}
    }
    // Cleanup before exit
    free(bitmap);
    free(cpbuf);
    falflush(in);
    falflush(out);
    falclose(in);
    falclose(out);
}

/*
  Create a cow file from a save file.
  Parameters:
  sn: Name of the savefile (may include a path)
  cn: Name of the cowfile (may include a path)
  zipped: Use gzip to decompress savefile (0 = no)
*/
static void
unpack(char* sn,char* cn,int zipped) {
    // Input and output files
    struct falfile *in,*out;
    struct cowhead cowhead;
    // Bitmap and copy buffer.
    char *bitmap;
    off_t offset;
    // Block shifts
    unsigned long shift,last;
    // Counters
    unsigned long i;
    int j;
    // Input file is savefile. May be zipped.
    if( !(in = falopen(sn,O_RDONLY | O_LARGEFILE,zipped)) ) {
	falperror("Opening inputfile",0);
	exit(1);   
    }
    // Output is cowfile (not gzipped!!!)
    if( !(out = falopen(cn,O_WRONLY | O_LARGEFILE | O_CREAT,0)) ) {
	falperror("Opening cowfile",0);
	falclose(in);
	exit(1);
    }
    if( falread(in,&cowhead,sizeof(cowhead)) < (ssize_t)sizeof(cowhead) ) {
	falperror("Reading cowheader",in);
	falclose(in);
	falclose(out);
	exit(1);
    }
    // Won't return if cowheader is invalid
    validatecowheader(&cowhead, 'u');
    if( falwrite(out,&cowhead,sizeof(cowhead)) < (ssize_t)sizeof(cowhead) ) {
	falperror("Writing cowheader",in);
	falclose(in);
	falclose(out);
	exit(1);
    }
    offset = cowhead.mapunit;
    if( falseek(in,offset,SEEK_SET) == -1 ) {
	falperror("Seeking source bitmap",in);
	falclose(in);
	falclose(out);
	exit(1);
    }
    if( falseek(out,offset,SEEK_SET) == -1 ) {
	falperror("Seeking destination bitmap",out);
	falclose(in);
	falclose(out);
	exit(1);
    }
    if( !(bitmap = malloc(cowhead.mapsize)) ) {
	perror("Allocating bitmap");
	falclose(in);
	falclose(out);
	exit(1);
    }
    if( falread(in,bitmap,cowhead.mapsize) < (ssize_t)cowhead.mapsize ) {
	falperror("Reading bitmap",in);
	free(bitmap);
	falclose(in);
	falclose(out);
	exit(1);
    }
    if( falwrite(out,bitmap,cowhead.mapsize) < (ssize_t)cowhead.mapsize ) {
	falperror("Writing bitmap",out);
	free(bitmap);
	falclose(in);
	falclose(out);
	exit(1);
    }
    offset = cowhead.doffset;
    if( falseek(in,offset,SEEK_SET) == -1 ) {
	falperror("Seeking source data",in);
	free(bitmap);
	falclose(in);
	falclose(out);
	exit(1);
    }
    if( falseek(out,offset,SEEK_SET) == -1 ) {
	falperror("Seeking destination data",out);
	free(bitmap);
	falclose(in);
	falclose(out);
	exit(1);
    }
    // No of cowhead.mapunit blocks from cowhead.doffset.
    shift = -1;
    // Last block that can be written without seeking.
    last = 0;
    for( i = 0; i < cowhead.mapsize; ++i ) {
	for( j = 0; j < 8; ++j ) {
	    ++shift;
	    if( (*(bitmap + i)) & (1 << j) ) {
		int cpres;
		if( (cpres = copyblockcow(in,out,shift-last,cowhead.mapunit)) <
		    0 ) {
		    /* Ugly: copyblockcow returns -1 if reading from in fails
		       and -2 if seeking or writing to out fails. */
		    falperror("Copying data",(cpres == -1) ? in : out);
		    free(bitmap);
		    falclose(in);
		    falclose(out);
		    exit(1); 
		}
		/* I've written block shift. Now, I can write block shift+1 
		   without seeking the output file. */
		last = shift + 1;
	    }
	}
    }
    // Cleanup before return.
    free(bitmap);
    falflush(in);
    falflush(out);
    falclose(in);
    falclose(out);
}

/*
  Check magic, version and state of a cowheader. If either magic or version 
  differ from builtin values or cowhead is dirty, this function exits.
  Parameters:
  cowhead: Cowheader to check.
  mode: 'p' for packing, 'u' for unpacking
*/
static void
validatecowheader(struct cowhead* cowhead, char mode) {
    if (cowhead->magic != COWMAGIC) {
	fputs("Not a valid cowfile (wrong magic)\n", stderr);
	exit(1);
    }
    
    if (cowhead->version != COWVERSION) {
	fputs("Version of cowfile not supported\n", stderr);
	exit(1);
    }
    
    if ( cowhead->flags & COWDIRTY ) {
	fputs("Cowfile is dirty\n", stderr);
	exit(1);
    }
    if (mode == 'p') cowhead->flags |=  COWPACKED;
    else             cowhead->flags &= ~COWPACKED;
}

/*
  Copy a block of mu bytes to the savefile. If the output file is not zipped, a
  buffer for efficient file system IO is used.
  Parameters:
  c: Open cow file
  s: Open save file
  shift: Number of mu blocks to seek
  cpbuf: copy buffer to use for zip files (needs to be at least mu bytes long)
  mu: Block size to copy (in bytes)
  Returns: -1 if operation on c fails,
  -2 if operation on s fails,
  0 on success
*/
static int
copyblocksave(struct falfile* c,struct falfile* s,long int shift,
			 void* cpbuf,size_t mu) {
    if( shift ) {
	if( falseek(c,(off_t)shift * mu,SEEK_CUR) == -1 ) 
	    return -1;
    }
    // Write through for zipped files.
    if( falzipped(s) ) {
	if( falread(c,cpbuf,mu) < (ssize_t)mu )
	    return -1;
	if( falwrite(s,cpbuf,mu) < (ssize_t)mu )
	    return -2;
    } else {
	size_t bs,used;
	// Get buffer and buffer size
	cpbuf = falwrbuf(s,&bs);
	used = falwrcount(s);
	// check buffer space
	if( bs - used < mu ) {
	    if( falwrflush(s) == -1 )
		return -2;
	    used = 0;
	}
	if( falread(c,cpbuf+used,mu) < (ssize_t)mu )
	    return -1;
	falwr(s,mu);
    }
    return 0;
}

/*
  Copy a block of mu bytes to the cowfile. A buffer for efficient file system
  IO is used to prevent write-modify-update of a block.
  Parameters:
  s: Open save file
  c: Open cow file
  shift: Number of mu blocks to seek
  cpbuf: copy buffer to use for zip files (needs to be at least mu bytes long)
  mu: Block size to copy (in bytes)
  Returns: -1 if operation on s fails,
  -2 if operation on c fails,
  0 on success
*/
static int
copyblockcow(struct falfile* s,struct falfile* c,long int shift,
			size_t mu) {
    void* cpbuf;
    size_t bs,used;
    // Get buffer for cowfile.
    cpbuf = falwrbuf(c,&bs);
    // If we have to shift, the buffer has to be flushed
    if( shift ) {
	if( falwrflush(c) == -1 ) {
	    return -2;
	}
	if( falseek(c,(off_t)shift * mu,SEEK_CUR) == -1 )
	    return -2;
    }
    used = falwrcount(c);
    // check buffer space
    if( bs - used < mu ) {
	if( falwrflush(c) == -1 )
	    return -2;
	used = 0;
    }
    if( falread(s,cpbuf + used,mu) < (ssize_t)mu )
	return -1;
    falwr(c,mu);
    return 0;
}
/*-------------------------------------------------------------------*/
/* fal.h - File abstraction layer */


/*
  Last error from errno or zlib?
*/
static int errnoerr;

/*
  Basic structure representing files in the file abstraction layer.
  Contains:
  nfilp: File descriptor representing the on disk file.
  zfilp: File descriptor representing a zipped file.
  pbuf: Private buffer for efficient file system IO.
  pbufsize: Size of the buffer.
  pbufuse: Buffer usage.
  zipped: Is the file a zipped file?
*/
struct falfile {
    int nfilp;
    gzFile zfilp;
    char* pbuf;
    size_t pbufsize;
    size_t pbufuse;
    int zipped;
};

/*
  Open a named file in the corresponding mode.
  If a new file is created, its mode will be 0300.
  Parameters:
  name: Name of the file to open.
  openmode: Mode to open the file in (see open(2))
  zipped: Create a zipped file.
  Returns: 0 on failure.
*/
struct falfile* falopen(char* name,int openmode,int zipped) {
    struct falfile* res;
    const mode_t m = S_IRUSR | S_IWUSR;
    res = malloc(sizeof(struct falfile));
    if( !res ) {
	errnoerr = 1;
	return 0;
    }
    if( zipped ) {
	char* nopm;
	// Transform Linux openmodes to zlib openmodes.
	if( openmode & O_ACCMODE )
	    nopm = "wb9";
	else
	    nopm = "rb9";
	res->zipped = 1;
	if( (res->nfilp = open(name,openmode,m)) == -1 ) {
	    int errnosave = errno;
	    errnoerr = 1;
	    free(res);
	    errno = errnosave;
	    return 0;
	}
	res->zfilp = gzdopen(res->nfilp,nopm);
	if( !res->zfilp ) {
	    errnoerr = 0;
	    close(res->nfilp);
	    free(res);
	    return 0;
	}
	// No buffer for zipped files.
	res->pbuf = 0;
	res->pbufsize = res->pbufuse = 0;
    } else {
	struct stat sb;
	res->zipped = 0;
	if( (res->nfilp = open(name,openmode,m)) == -1 ) {
	    int errnosave = errno;
	    errnoerr = 1;
	    free(res);
	    errno = errnosave;
	    return 0;
	}
	// Get file system information.
	if( fstat(res->nfilp,&sb) == -1 ) {
	    int errnosave = errno; 
	    errnoerr = 1;
	    close(res->nfilp);
	    free(res);
	    errno = errnosave;
	    return 0;
	}
	res->pbufsize = sb.st_blksize;
	res->pbufuse = 0;
	// Don't malloc. We have mmap!
	if( (res->pbuf = mmap(0,sb.st_blksize,
			      PROT_READ | PROT_WRITE,
			      MAP_PRIVATE | MAP_ANONYMOUS,-1,0)) == 
	    MAP_FAILED ) {
	    int errnosave = errno;
	    errnoerr = 1;
	    close(res->nfilp);
	    free(res);
	    errno = errnosave;
	    return 0;
	}
    }
    return res;
}

/*
  Print an error message for the last error occured on filp.
  Parameters:
  p: perror(2) like prefix string.
  filp: File the last error occured on.
*/
void falperror(const char* p,struct falfile* filp) {
    // Last error in errno.
    if( errnoerr )
	perror(p);
    else {
	int errnum;
	const char* gzerr;
	// Get last error message from filp.
	if( filp )
	    gzerr = gzerror(filp->zfilp,&errnum);
	else {
	    // Got NULL-pointer. If errno is set, use it
	    if( errno )
		errnum = Z_ERRNO;
	    else 
		// Otherwise, we are out of memory (see zlib docu)
		gzerr = "Insufficent memory";
	}	
	if( errnum == Z_ERRNO )
	    perror(p);
	else
	    fprintf(stderr,"%s : %s\n",p,gzerr);
    }
}

/*
  Write data to a file.
  Parameters:
  filp: File to write to.
  buf: Data to write.
  len: Length of data.
  Returns:
  Number of bytes written.
*/
ssize_t falwrite(struct falfile* filp,void* buf,size_t len) {
    if( filp->zipped ) {
	errnoerr = 0;
	return (ssize_t)gzwrite(filp->zfilp,buf,(unsigned int)len);
    } else {
	errnoerr = 1;
	return write(filp->nfilp,buf,len);
    }
}

/*
  Read data from a file.
  Parameters:
  filp: File to read from.
  buf: Buffer to read into.
  len: Bytes to read.
  Returns:
  Number of bytes read.
*/
ssize_t falread(struct falfile* filp,void* buf,size_t len) {
    if( filp->zipped ) {
	int res;
	errnoerr = 0;
	res = gzread(filp->zfilp,buf,(unsigned int)len);
	return (ssize_t)res;
    } else {
	errnoerr = 1;
	return read(filp->nfilp,buf,len);
    }
}

/*
  Seek in a file.
  Parameters:
  filp: File to seek in.
  offset: Offset to seek.
  whence: Seek start description (see lseek(3p))
  Returns: -1 on failure.
*/
off_t falseek(struct falfile* filp,off_t offset,int whence) {
    if( filp->zipped ) {
	errnoerr = 0;
	// Fake here since we do not use zipped seeks (too slow)
	return offset;
    } else {
	errnoerr = 1;
	return lseek(filp->nfilp,offset,whence);
    }
}

/*
  Flush data to disk.
  Parameters:
  filp: File to flush.
  Returns: -1 on error.
*/
int falflush(struct falfile* filp) {
    if( filp->zipped ) {
	errnoerr = 0;
	return (gzflush(filp->zfilp,Z_FINISH) == Z_OK) ? 0 : -1;
    } else {
	errnoerr = 1;
	return fdatasync(filp->nfilp);
    }
}

/*
  Close a file and flush it's private buffer.
  Parameters:
  filp: File to close.
  Returns: -1 on error.
*/
int falclose(struct falfile* filp) {
    if( filp->zipped ) {
	int res;
	errnoerr = 0;
	res = (gzclose(filp->zfilp) == 0) ? 0 : -1;
	free(filp);
	return res;
    } else {
	errnoerr = 1;
	if( filp->pbufuse )
	    if( falwrflush(filp) == -1 )
		return -1;
	if( munmap(filp->pbuf,filp->pbufsize) == -1 )
	    return -1;
	if( close(filp->nfilp) == -1 )
	    return -1;
	free(filp);
	return 0;
    }
}

/*
  Get the buffer provided for efficient file system IO.
  Parameters:
  filp: File to get buffer for.
  size: Reference parameter to retrieve the buffer size.
  Returns: Private Buffer.
*/
void* falwrbuf(struct falfile* filp,size_t* size) {
    if( size )
	*size = filp->pbufsize;
    return filp->pbuf;
}

/*
  Get the current buffer usage.
*/
size_t falwrcount(struct falfile* filp) {
    return filp->pbufuse;
}

/*
  Reflect a write into the buffer.
  Parameters:
  filp: File whose buffer has been modified.
  size: Number of bytes appended.
*/
size_t falwr(struct falfile* filp,size_t size) {
    return filp->pbufuse += size;
}

/*
  Write the buffer to the file.
*/
int falwrflush(struct falfile* filp) {
    int res = falwrite(filp,filp->pbuf,filp->pbufuse);
    filp->pbufuse = 0;
    return res;
}

/*
  Does the file represent a zip file?
*/
int falzipped(struct falfile* filp) {
    return filp->zipped;
}
