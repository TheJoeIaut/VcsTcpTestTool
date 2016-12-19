##
## @file Makefile
## @date 2016/12/10
##
## @author Juergen Schoener <ic16b049@technikum-wien.at>
## @author Juergen Spandl <ic16b029@technikum-wien.at>
##
## @date 2016/14
## @version $Revision: 1 $
##
##
## Last Modified: $Author: spj $
##
## ------------------------------------------------------------- variables --
##

CC=gcc52
CFLAGS=-DDEBUG -Wall -pedantic -Werror -Wextra -Wstrict-prototypes -fno-common -g -O3 -std=gnu11 
LDFLAGS=-lsimple_message_client_commandline_handling
CP=cp
CD=cd
MV=mv
GREP=grep
DOXYGEN=doxygen
CLIENT=simple_message_client
SERVER=simple_message_server


EXCLUDE_PATTERN=footrulewidth

##
## ----------------------------------------------------------------- rules --
##

%.o: %.c
	$(CC) $(CFLAGS) -c $<

##
## --------------------------------------------------------------- targets --
##

all: $(CLIENT) $(SERVER)

simple_message_client: $(CLIENT).o
	$(CC) $(CFLAGS) $(CLIENT).o -o $(CLIENT) $(LDFLAGS) 
	
simple_message_server: $(SERVER).o
	$(CC) $(CFLAGS) $(SERVER).o -o $(SERVER)	

clean:
	$(RM) *.o *~ $(CLIENT) $(SERVER)

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