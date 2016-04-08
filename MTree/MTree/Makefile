######################################################################
#                                                                    #
# Copyright (c) 1997,1998, 1999                                      #
# Multimedia DB Group and DEIS - CSITE-CNR,                          #
# University of Bologna, Bologna, ITALY.                             #
#                                                                    #
# All Rights Reserved.                                               #
#                                                                    #
# Permission to use, copy, and distribute this software and its      #
# documentation for NON-COMMERCIAL purposes and without fee is       #
# hereby granted provided  that this copyright notice appears in     #
# all copies.                                                        #
#                                                                    #
# THE AUTHORS MAKE NO REPRESENTATIONS OR WARRANTIES ABOUT THE        #
# SUITABILITY OF THE SOFTWARE, EITHER EXPRESS OR IMPLIED, INCLUDING  #
# BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY,      #
# FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT. THE AUTHOR  #
# SHALL NOT BE LIABLE FOR ANY DAMAGES SUFFERED BY LICENSEE AS A      #
# RESULT OF USING, MODIFYING OR DISTRIBUTING THIS SOFTWARE OR ITS    #
# DERIVATIVES.                                                       #
#                                                                    #
######################################################################

#         Makefile for M-tree test program

PROGRAM = MTree

CPP = g++

CFLAGS = -DUNIX -DPRINTING_OBJECTS

INCS = -I$(HOME)/GiST/ -I/usr/include/g++-include/

LINKER = g++

SRCS = MTfile.cpp MTentry.cpp MTnode.cpp MTpredicate.cpp MTobject.cpp MT.cpp MTcursor.cpp Main.cpp BulkLoad.cpp

OBJS = $(SRCS:.cpp=.o)

LIBS = $(HOME)/GiST/libGiST.a

DEST = .

$(PROGRAM): .depend $(OBJS) $(LIBS)
    $(LINKER) -o $(PROGRAM) $(LDFLAGS) $(OBJS) $(LIBS) 

gen:
    $(CPP) -o generate $(CFLAGS) $(INCS) $(LDFLAGS) generate.cpp

depend:
    makedepend $(DEFINES) $(INCS) $(SRCS)

.depend:
    $(MAKE) depend
    touch .depend

%.o: %.cpp
    $(CPP) -c $(CFLAGS) $(INCS) $*.cpp

clean:
    rm -f $(OBJS) $(PROGRAM) .depend

install: $(PROGRAM)
    echo Installing $(PROGRAM) in $(DEST)
    install -s $(PROGRAM) $(DEST)
