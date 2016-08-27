# Makefile for 2.6 kernel modules - cowloop

KERNELRELEASE = $(shell uname -r)
KERNDIR = /lib/modules/$(KERNELRELEASE)/build
MODDIR  = /lib/modules/$(KERNELRELEASE)/kernel/drivers/block
THISDIR = $(shell pwd)
COWMAJOR= 241
DESTDIR =
MODULE  = cowloop.ko
UTILS   = cowdev cowwatch cowctl cowsync cowlist cowrepair cowmerge
UTIL_COWPACK = cowpack

obj-m  := cowloop.o

all:	$(MODULE) $(UTILS) $(UTIL_COWPACK)

module:	$(MODULE)

utils:	$(UTILS) $(UTIL_COWPACK)

cowloop.ko:	cowloop.c cowloop.h version.h
		make -C $(KERNDIR) M=$(THISDIR) -Wall modules
# The older (deprecated) version of this command was:
# make -C $(KERNDIR) SUBDIRS=$(THISDIR) -I. -Wall modules

cowdev:		cowdev.c version.h cowloop.h
		$(CC) $(CFLAGS) -I. -Wall -o cowdev cowdev.c

cowwatch:	cowwatch.c version.h cowloop.h
		$(CC) $(CFLAGS) -I. -Wall -o cowwatch cowwatch.c

cowctl:		cowctl.c version.h cowloop.h
		$(CC) $(CFLAGS) -I. -Wall -o cowctl cowctl.c

cowsync:	cowsync.c version.h cowloop.h
		$(CC) $(CFLAGS) -I. -Wall -o cowsync cowsync.c

cowlist:	cowlist.c version.h cowloop.h
		$(CC) $(CFLAGS) -I. -Wall -o cowlist cowlist.c

cowrepair:	cowrepair.c version.h cowloop.h
		$(CC) $(CFLAGS) -I. -Wall -o cowrepair cowrepair.c

cowmerge:	cowmerge.c version.h cowloop.h
		$(CC) $(CFLAGS) -I. -Wall -o cowmerge cowmerge.c

# This step is last since its resulting product is for comfort only.
# The make could fail because of missing libz.so \(gzip compression API\)
# I have added zlib.h zconf.h zutil.h to this distribution since they
# may not be present on yours. Feel free to use your /usr/include ones.
# See also the comments in the file 'z_includes_README'
cowpack:	cowpack.c version.h cowloop.h zlib.h zconf.h zutil.h
		@echo '**************************************************'
		@echo \* If the following step fails then you need to find a
		@echo \* version of the gzip-compression library libz.so
		@echo \* Otherwise you won\'t have the \'cowpack\' support utility,
		@echo \* but that is a non-essential service component anyhow.
		@echo \* This make-step is the last one - if it fails then
		@echo \* all the rest, apart from \'cowpack\', will be OK to use.
		@echo '**************************************************'
		-$(CC) $(CFLAGS) -I. -L/usr/lib -Wall -o cowpack cowpack.c -lz

#--------------------------------------------------------------------

install:	install-module install-dev install-man install-doc install-utils

install-module:	$(MODULE) 	
		@# Install the loadable kernel module
		@./gplaccept
		@# The $(MODDIR) directory should exist already, but if
		@# it doesn't, we create it so we can continue our job.
		if [ ! -d $(DESTDIR)$(MODDIR) ]; then \
			mkdir -p $(DESTDIR)$(MODDIR); fi
		cp cowloop.ko $(DESTDIR)$(MODDIR)
		@# Build a modules-dependancy database, so the
		@# modprobe-command will be able to find "cowloop":
		if [ -z "$(DESTDIR)" ]; then /sbin/depmod -a; fi

install-utils:	$(UTILS) 
		@# Install the utility-programs
		@./gplaccept
		if [ ! -d $(DESTDIR)/usr/sbin ]; then \
			mkdir -p $(DESTDIR)/usr/sbin; fi
		for UTIL in $(UTILS); do \
			cp $$UTIL $(DESTDIR)/usr/sbin; done

install-util-cowpack:	$(UTIL_COWPACK) 
		@# Install the cowpack utility-program
		@# Could fail if the -lz library is not present
		@# If it fails, don't worry. Cowpack is not essential
		@# Keep this step *last* in the Make procedure
		@./gplaccept
		if [ ! -d $(DESTDIR)/usr/sbin ]; then \
			mkdir -p $(DESTDIR)/usr/sbin; fi
		for UTIL in $(UTIL_COWPACK); do \
			-cp $$UTIL $(DESTDIR)/usr/sbin; done


install-dev:	
		@# Create the /dev files
		./makecows $(COWMAJOR) $(DESTDIR)  # create the special files

install-man:
		@# Install the manpages
		@./gplaccept
		if [ ! -d $(DESTDIR)/usr/share/man/man4 ]; then \
			mkdir -p $(DESTDIR)/usr/share/man/man4; fi
		cp ../man/man4/cowloop.4   $(DESTDIR)/usr/share/man/man4/
		if [ ! -d $(DESTDIR)/usr/share/man/man1 ]; then \
			mkdir -p $(DESTDIR)/usr/share/man/man1; fi
		for UTIL in $(UTILS); do \
			cp ../man/man1/$$UTIL.1 $(DESTDIR)/usr/share/man/man1/; done

install-doc:	
		@# Install the documentation (PostScript file)
		@./gplaccept
		if [ ! -d $(DESTDIR)/usr/share/doc/cowloop ]; then \
			mkdir -p $(DESTDIR)/usr/share/doc/cowloop; fi
		cp ../doc/cowloop.ps.bz2   $(DESTDIR)/usr/share/doc/cowloop/

#--------------------------------------------------------------------

uninstall:
		@# revert the 'make install' steps; ignore all errors
		-rm -f $(DESTDIR)$(MODDIR)/cowloop.ko
		-if [ -z "$(DESTDIR)" ]; then /sbin/depmod -a; fi
		-for UTIL in $(UTILS); do \
		    rm -f $(DESTDIR)/usr/sbin/$$UTIL; done
		-rm -f $(DESTDIR)/usr/sbin/$$UTIL_COWPACK; done
		-rm -f /dev/cowloop /dev/cow/*
		-rmdir /dev/cow 2>/dev/null
		-rm -f $(DESTDIR)/usr/share/man/man4/cowloop.4
		-for UTIL in $(UTILS) $(UTIL_COWPACK); do \
		    rm -f $(DESTDIR)/usr/share/man/man1/$$UTIL.1; done
		-rm -f $(DESTDIR)/usr/share/doc/cowloop/cowloop.ps.bz2
		-rmdir $(DESTDIR)/usr/share/doc/cowloop/ 2>/dev/null

#--------------------------------------------------------------------

clean:		
		@# revert the 'make' steps
		rm -f cowloop.ko cowloop.o cowloop.mod.[co]
		rm -f $(UTILS) $(UTIL_COWPACK)
		rm -f .cowloop.ko.cmd .cowloop.mod.o.cmd .cowloop.o.cmd
		rm -f .gpl_license_accepted
		rm -rf .tmp_versions
		rm -f  Module.markers Module.symvers modules.order
