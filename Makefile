#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>/devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

#---------------------------------------------------------------------------------
# Configuración del Proyecto Laura
#---------------------------------------------------------------------------------
TARGET		:= Laura
BUILD		:= build
SOURCES		:= source
DATA		:= data
INCLUDES	:= include
GRAPHICS	:= gfx
ROMFS		:= romfs

APP_TITLE	:= Laura
APP_DESCRIPTION	:= Procesador de Texto Minimalista para Nintendo 3DS
APP_AUTHOR	:= Laura Dev

#---------------------------------------------------------------------------------
# Opciones de Compilación (C y C++17)
#---------------------------------------------------------------------------------
ARCH		:= -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS		:= -g -Wall -O2 -mword-relocations \
			   -fomit-frame-pointer -ffunction-sections \
			   $(ARCH)

CFLAGS		+= $(INCLUDE) -D__3DS__

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17

ASFLAGS		:= -g $(ARCH)
LDFLAGS		= -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

#---------------------------------------------------------------------------------
# Librerías enlazadas (orden estricto de dependencias: citro2d -> citro3d -> ctru -> math)
#---------------------------------------------------------------------------------
LIBS		:= -lcitro2d -lcitro3d -lctru -lm

#---------------------------------------------------------------------------------
# Directorios de librerías (libctru, portlibs)
#---------------------------------------------------------------------------------
LIBDIRS		:= $(CTRULIB) $(PORTLIBS)

#---------------------------------------------------------------------------------
# Rutas internas de compilación
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT	:= $(CURDIR)/$(TARGET)
export TOPDIR	:= $(CURDIR)

export VPATH	:= $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			   $(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:= $(CURDIR)/$(BUILD)

CFILES		:= $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:= $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:= $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:= $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

ifeq ($(strip $(CPPFILES)),)
	export LD	:= $(CC)
else
	export LD	:= $(CXX)
endif

export OFILES_SOURCES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES_BIN := $(addsuffix .o,$(BINFILES))
export OFILES := $(OFILES_BIN) $(OFILES_SOURCES)

export HFILES_BIN := $(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE	:= $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			   $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			   -I$(CURDIR)/$(BUILD)

export LIBPATHS	:= $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

ifeq ($(strip $(ICON)),)
	icons := $(wildcard *.png)
	ifneq (,$(findstring $(TARGET).png,$(icons)))
		export APP_ICON := $(CURDIR)/$(TARGET).png
	else
		ifneq (,$(findstring icon.png,$(icons)))
			export APP_ICON := $(CURDIR)/icon.png
		endif
	endif
else
	export APP_ICON := $(CURDIR)/$(ICON)
endif

ifeq ($(strip $(NO_SMDH)),)
	export _3DSXFLAGS += --smdh=$(CURDIR)/$(TARGET).smdh
endif

ifneq ($(ROMFS),)
	export _3DSXFLAGS += --romfs=$(CURDIR)/$(ROMFS)
endif

.PHONY: $(BUILD) clean all

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo Limpiando archivos de compilación...
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).smdh $(TARGET).elf

else

DEPENDS	:= $(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# Reglas principales de enlazado
#---------------------------------------------------------------------------------
$(OUTPUT).3dsx	:	$(OUTPUT).elf $(_3DSXDEPS)
$(OFILES_SOURCES) : $(HFILES_BIN)
$(OUTPUT).elf	:	$(OFILES)

%.bin.o	%_bin.h :	%.bin
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

endif
