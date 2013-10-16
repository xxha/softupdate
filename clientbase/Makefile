
TARGET=clientbase.a

#you should define TARGET before include config.mk!
include ../config.mak

prebuild:
#no need prepare.

CFLAGS += -fPIC -I../../../include -I../common


$(TARGET):$(OBJ_FILES)
	$(LINK_INCREMENT)
	

preclean:
#nothing to do before cleaning.


