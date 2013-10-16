TARGET=softupdate

#you should define TARGET before include config.mk!
include ../config.mak

prebuild:
#no need prepare.

CFLAGS += -I../../../include -I. -I../common

COMMLIB:=../libcommsvr.a
LDFLAGS+=-L..
LDFLAGS+=-L../common/common.a
LDFLAGS+=-L../v400eeprom
LDFLAGS+=-L$(HOME)/v400-build/v400-buildroot/lib
#LDFLAGS+=-L../ftdieeprom

LDFLAGS+=-lftdi

$(TARGET): $(OBJ_FILES) $(COMMLIB) 
	@echo "[LD ] linking $@..."
	$(RM) $@
	$(CXX) $(LDFLAGS) -o $@ \
			-Wl,--start-group \
				$(OBJ_FILES) -lcommsvr -lpthread \
				-lSUSI-3.02 \
			-Wl,--end-group

$(COMMLIB):
	$(MAKE) -C .. all

preclean:
	$(MAKE) -C .. clean

