
TARGET=common.a

#you should define TARGET before include config.mk!
include ../config.mak

prebuild:
#no need prepare.

CFLAGS += -fPIC -I../../../include -I../include -I./
CFLAGS += -I$(HOME)/v400-build/v400-buildroot/include

$(TARGET):$(OBJ_FILES)
	$(LINK_INCREMENT)
	

preclean:
#nothing to do before cleaning.


