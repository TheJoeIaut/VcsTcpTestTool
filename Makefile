##
## @file Makefile
## Verteilte Systeme
## TCP/IP Uebung
##
## @author Juergen Schoener <ic16b049@technikum-wien.at>
## @author Juergen Spandl <ic16b029@technikum-wien.at>
## @date 2016/12/10
##
## @version $Revision: 1 $
##
## @todo: change gcc version on annuminas
##
## Last Modified: $Author: spj $
##
## ------------------------------------------------------------- variables --
##

CC=gcc52
#CC=gcc
CFLAGS=-DDEBUG -Wall -pedantic -Wextra -Wstrict-prototypes -fno-common -g -O3 -std=gnu11 -o simple_message_client.o
LDFLAGS = -lsimple_message_client_commandline_handling
CP=cp
CD=cd
MV=mv
GREP=grep
DOXYGEN=doxygen

OBJECTS=simple_message_client.o

EXCLUDE_PATTERN=footrulewidth

##
## ----------------------------------------------------------------- rules --
##

%.o: %.c
	$(CC) $(CFLAGS) -c $<

##
## --------------------------------------------------------------- targets --
##

all: simple_message_client

simple_message_client: $(OBJECTS)
	$(CC) $(CFLGS) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) *.o *~ simple_message_client

distclean: clean
	$(RM) -r doc

doc: html pdf

html:
	$(DOXYGEN) doxygen.dcf

pdf: html
	$(CD) doc/pdf && \
	$(MV) refman.tex refman_save.tex && \
	$(GREP) -v $(EXCLUDE_PATTERN) refman_save.tex > refman.tex && \
	$(RM) refman_save.tex && \
	make && \
	$(MV) refman.pdf refman.save && \
		(RM) *.pdf *.html *.tex *.aux *.sty *.log *.eps *.out *.ind *.idx \
		*.ilg *.toc *.tps Makefile && \
	$(MV) refman.save refman.pdf

##
## ---------------------------------------------------------- dependencies --
##

##
## =================================================================== eof ==
##