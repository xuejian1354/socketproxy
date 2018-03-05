ifneq ($(WHOLE_BUILD),1)
ifneq ("$(origin prefix)", "command line")
  prefix :=
# prefix :=mips-unknown-linux-uclibc-
endif

ifneq ("$(origin dir)", "command line")
  dir :=bin
endif

ifneq ("$(dir)", "")
  DIR :=$(dir)/
else
  DIR :=
endif

ifneq ("$(origin V)", "command line")
  V := 99
endif

export DIR V

TARGET_CC := $(prefix)gcc
TARGET_CXX := $(prefix)g++
TARGET_LD := $(prefix)ld
TARGET_AR := $(prefix)ar
TARGET_STRIP := $(prefix)strip
export TARGET_CC TARGET_CXX TARGET_LD TARGET_AR TARGET_STRIP

all:

WHOLE_BUILD :=1
export WHOLE_BUILD
endif

ifeq ($(V),1)
  define echocmd
    @echo '    [$1]' '   $2' && $3
  endef
else
  ifeq ($(V),99)
    define echocmd
      $3
    endef
  else
    $(error invalid value V=$(V), please set correct value of 1 or 99)
  endif
endif

