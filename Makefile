#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
export DEVKITPRO	:=	/opt/devkitpro
endif

export DEVKITARM	:=	$(DEVKITPRO)/devkitARM
export DEVKITPPC	:=	$(DEVKITPRO)/devkitPPC

#---------------------------------------------------------------------------------
# Target configuration
#---------------------------------------------------------------------------------
TARGET		:=	Laura
BUILD		:=	build
SOURCES		:=	source
DATA		:=	data
INCLUDES	:=	include
ROMFS		:=	romfs

#---------------------------------------------------------------------------------
# Libraries and flags
#---------------------------------------------------------------------------------
ARCH		:=	-march=armv6k -mtune=mpcore -mfloat-abi=hard -mfpu=vfp

CFLAGS		:=	-g -Wall -O2 -mword-relocations \
				-ffunction-sections \
				$(ARCH) \
				-D_3DS

CXXFLAGS	:=	$(CFLAGS) -std=gnu++17 -fno-rtti -fno-exceptions

ASFLAGS		:=	-g $(ARCH)
LDFLAGS		:=	-specs=3dsx.specs -g $(ARCH) -Wl,--start-group -lcitro2d -lcitro3d -lctru -lm -Wl,--end-group

#---------------------------------------------------------------------------------
# Tools
#---------------------------------------------------------------------------------
PREFIX		:=	arm-none-eabi-
CC			:=	$(PREFIX)gcc
CXX			:=	$(PREFIX)g++
AS			:=	$(PREFIX)as
AR			:=	$(PREFIX)ar
LD			:=	$(PREFIX)g++
OBJCOPY		:=	$(PREFIX)objcopy
STRIP		:=	$(PREFIX)strip
ROMFSGEN	:=	romfs-gen
3DSX		:=	3dsx
SMDH		:=	smdh

#---------------------------------------------------------------------------------
# Rules
#---------------------------------------------------------------------------------
ifeq ($(strip $(BUILD)),$(notdir $(CURDIR)))
export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir))

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

OFILES		:=	$(addsuffix .o,$(basename $(CFILES))) \
				$(addsuffix .o,$(basename $(CPPFILES))) \
				$(addsuffix .o,$(basename $(SFILES)))

export DEPSDIR	:=	$(CURDIR)

CDEPS		:=	$(addsuffix .d,$(basename $(CFILES)))
CPPDEPS		:=	$(addsuffix .d,$(basename $(CPPFILES)))

ifneq ($(wildcard $(DEVKITPRO)/libctru/switch_rules),)
include $(DEVKITPRO)/libctru/switch_rules
else
$(error "Please install devkitPro and libctru (switch_rules not found at $(DEVKITPRO)/libctru/switch_rules)")
endif

output: $(TARGET).3dsx

$(TARGET).3dsx: $(TARGET).elf

else

export TARGET	:=	$(shell basename $(CURDIR))
export BUILD	:=	$(CURDIR)/build

all:
	@[ -d $(BUILD) ] || mkdir -p $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).elf $(TARGET).smdh

endif

#---------------------------------------------------------------------------------
