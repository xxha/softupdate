.PHONY: prebuild preclean

TARGET=libcommsvr.a

include config.mak
export T CLEAN_FLAG

SUBDIRS=mainsvr v400eeprom clientbase common

AR_FILES=$(foreach i,$(SUBDIRS),$(i)/$(i).a)

SUB_CHECK:=$(foreach i,$(SUBDIRS),echo [CHK] checking $(i)...; make -C $(i) || exit 1;)
SUB_CLEAN:=$(foreach i,$(SUBDIRS),echo [DEL] cleaning $(i)...; make -C $(i) clean;)

#check all files in sub directory.
prebuild:
	$(SUB_CHECK)

preclean: CLEAN_FLAG=YES
preclean:
	$(SUB_CLEAN)
	$(RM) $(TARGET)

$(TARGET): $(AR_FILES)
	$(RM) $@
	echo [AR ] linking $@ ...
	$(AR) -rc $@ $^	
	$(RANLIB) $@
	echo [LD ] Done.
