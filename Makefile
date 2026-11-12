#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path>")
endif

include $(DEVKITARM)/3ds_rules

TARGET		:=	Laura
BUILD		:=	build
SOURCES		:=	source
INCLUDES	:=	include
DATA		:=	data
ROMFS		:=	romfs

PORTLIBS	:=	$(DEVKITPRO)/portlibs/3ds

ARCH		:=	-march=armv6k -mtune=mpcore -mfloat-abi=hard

CFLAGS		:=	-Wall -O2 -ffunction-sections \
			$(ARCH) -DARM11 -D_3DS \
			-I$(PORTLIBS)/include \
			$(foreach dir,$(INCLUDES),-I$(dir))

CXXFLAGS	:=	$(CFLAGS) -std=gnu++17 -fno-rtti -fno-exceptions

ASFLAGS		:=	$(ARCH)
LDFLAGS		:=	-specs=3dsx.specs $(ARCH) \
			-L$(PORTLIBS)/lib

LIBS		:=	-lcitro2d -lcitro3d -lctru -lm

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export CFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
export CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
export SFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

export OFILES	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			-I$(PORTLIBS)/include \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBDIRS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: all clean

all: $(OUTPUT).3dsx $(OUTPUT).cia

$(OUTPUT).3dsx: $(OUTPUT).elf
$(OUTPUT).cia: $(OUTPUT).elf

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT).3dsx $(OUTPUT).elf $(OUTPUT).smdh $(OUTPUT).cia

#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------

DEPENDS	:=	$(OFILES:.o=.d)

$(OUTPUT).3dsx: $(OUTPUT).elf
	@echo builtin 3dsx...
	elf23ds $< $@
	@smdhtool --create "Laura Word Processor" "Editor de texto minimalista" "Laura" $(OUTPUT).smdh || true
	@bannertool makesmdh -s "Laura Word Processor" -l "Laura" -p "Autor" -i assets/icon.png -o $(OUTPUT).smdh || true

$(OUTPUT).cia: $(OUTPUT).3dsx
	@echo builtin cia...
	cp $(OUTPUT).3dsx $(OUTPUT).cia || true

%.elf:
	@echo linking ...
	$(LD) $(LDFLAGS) $(OFILES) $(LIBDIRS) $(LIBS) -o $@

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
