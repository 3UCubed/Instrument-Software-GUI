#
# Header Makefile for the Fast Light Tool Kit (FLTK).
#
# Copyright 1998-2021 by Bill Spitzak and others.
#
# This library is free software. Distribution and use rights are outlined in
# the file "COPYING" which should have been included with this file.  If this
# file is missing or damaged, see the license at:
#
#      https://www.fltk.org/COPYING.php
#
# Please see the following page on how to report bugs and issues:
#
#      https://www.fltk.org/bugs.php
#

include ../makeinclude

all:

clean:

depend:

install:
	echo "Installing include files in $(DESTDIR)$(includedir)..."
	$(RMDIR) $(DESTDIR)$(includedir)/FL
	$(INSTALL_DIR) $(DESTDIR)$(includedir)/FL
	for file in *.[hH]; do \
		$(INSTALL_DATA) $$file $(DESTDIR)$(includedir)/FL; \
	done
#	cd $(DESTDIR)$(includedir)/FL;\
#	for file in *.H; do\
#		$(RM) "`basename $$file H`h";\
#		$(LN) $$file "`basename $$file H`h";\
#	done
#	$(RM) $(DESTDIR)$(includedir)/FL/fl_file_chooser.H
#	$(LN) Fl_File_Chooser.H $(DESTDIR)$(includedir)/FL/fl_file_chooser.H
#	$(RM) $(DESTDIR)$(includedir)/FL/fl_file_chooser.h
#	$(LN) Fl_File_Chooser.H $(DESTDIR)$(includedir)/FL/fl_file_chooser.h
#	$(RM) $(DESTDIR)$(includedir)/Fl
#	$(LN) FL $(DESTDIR)$(includedir)/Fl

uninstall:
	echo "Uninstalling include files..."
	$(RMDIR) $(DESTDIR)$(includedir)/FL
#	$(RM) $(DESTDIR)$(includedir)/Fl
