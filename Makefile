TOPDIR:=$(CURDIR)
export TOPDIR

$(shell find -name "*.c" | xargs chmod -x)
$(shell find -name "*.h" | xargs chmod -x)

include $(TOPDIR)/config.mk

TARGET_NAME:=skproxy
TARGET:=$(addprefix $(DIR),$(TARGET_NAME))
export TARGET

TARGET_DMACRO:=-DTARGET_NAME=\"$(TARGET_NAME)\" -DDLOG_PRINT
export TARGET_DMACRO

ALL_HEARDS:=$(shell ls *.h)
TARGET_SOURCES:=$(shell ls *.c)
TARGET_OBJS:=$(filter %.o,$(addprefix $(DIR),$(patsubst %.c,%.o,$(TARGET_SOURCES))))

.PHONY:all clean help

all:$(TARGET)

$(TARGET):target_comshow $(TARGET_OBJS)
	$(call echocmd,TAR,$@, \
	  $(TARGET_CC) $(TARGET_DMACRO) -O2 -o $@ $(TARGET_OBJS))
	@$(TARGET_STRIP) $@

$(DIR)%.o:%.c $(ALL_HEARDS)
	@if [ ! -d "$(dir $@)" ]; then mkdir -p $(dir $@); fi;
	$(call echocmd,CC, $@, \
	  $(TARGET_CC) $(TARGET_DMACRO) -O2 -o $@ -c $<)

target_comshow:
	@echo ===========================================================
	@echo **compile $(TARGET_NAME): $(TARGET_CC)
	@echo ===========================================================

clean:
	(find -name "*.[oa]" | xargs $(RM)) && $(RM) $(TARGET)

help:
	@echo "help:"
	@echo "  prefix=[compiler]	\"Cross compiler prefix.\""
	@echo "  V=[1|99]		\"Default value is 1, and 99 show more details.\""
	@echo "  dir=<path>		\"binaries path.\""
