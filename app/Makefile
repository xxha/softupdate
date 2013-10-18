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
#LDFLAGS+=-L../ftdieeprom


$(TARGET): $(OBJ_FILES) $(COMMLIB) 
	@echo "[LD ] linking $@..."
	$(RM) $@
	$(CXX) $(LDFLAGS) -o $@ \
			-Wl,--start-group \
				$(OBJ_FILES) -lcommsvr -lpthread \
				-lSUSI-3.02 -lftdi\
			-Wl,--end-group

$(COMMLIB):
	$(MAKE) -C .. all

preclean:
	$(MAKE) -C .. clean

