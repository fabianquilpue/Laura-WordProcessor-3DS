ifeq ($(strip $(DEVKITARM)),)
$(error "Por favor, configure DEVKITARM en su entorno.")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

TARGET		:=	Laura
BUILD		:=	build
SOURCES		:=	source
INCLUDES	:=	include
DATA		:=	data
ROMFS		:=	romfs

ARCH		:=	-march=armv6k -mtune=mpcore -mfloat-abi=hard -mfpu=vfp

CFLAGS		:=	-g -Wall -Wextra -O3 -mword-relocations \
				$(ARCH) -ffunction-sections \
				$(foreach dir,$(INCLUDES),-I$(dir)) \
				-D__3DS__

CXXFLAGS	:=	$(CFLAGS) -std=gnu++17 -fno-rtti -fno-exceptions

ASFLAGS		:=	-g $(ARCH)
LDFLAGS		:=	-specs=3dsx.specs $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS		:=	-lcitro2d -lcitro3d -lctru -lm

BANNERTOOL	:=	bannertool
SMDHTOOL	:=	smdhtool
MKBCFNT		:=	mkbcfnt

ICON_PNG	:=	assets/icon.png
BANNER_PNG	:=	assets/banner.png
SMDH		:=	$(TARGET).smdh
BANNER		:=	$(TARGET).bnr
CIA			:=	$(TARGET).cia
3DSX		:=	$(TARGET).3dsx

CFILES		:=	$(foreach dir,$(SOURCES),$(wildcard $(dir)/*.c))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(wildcard $(dir)/*.cpp))
SFILES		:=	$(foreach dir,$(SOURCES),$(wildcard $(dir)/*.s))

OBJS		:=	$(addprefix $(BUILD)/,$(CFILES:.c=.o) $(CPPFILES:.cpp=.o) $(SFILES:.s=.o))

all: $(3DSX) $(CIA)

$(3DSX): $(BUILD)/$(TARGET).elf $(SMDH)
	@echo "Generando $@..."
	@elf23dsx $< $@ --smdh=$(SMDH)

$(CIA): $(BUILD)/$(TARGET).elf $(SMDH) $(BANNER)
	@echo "Generando $@..."
	@makerom -f cia -o $@ -exefslogo -target t -elf $< -smdh $(SMDH) -banner $(BANNER)

$(BUILD)/$(TARGET).elf: $(OBJS)
	@echo "Enlazando $@"
	@$(CXX) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

$(SMDH): $(ICON_PNG)
	@echo "Creando archivo SMDH..."
	@$(SMDHTOOL) --create "Laura - Word Processor" "Procesador de textos para 3DS" "Laura" $(ICON_PNG) $@

$(BANNER): $(BANNER_PNG)
	@echo "Creando banner 3D..."
	@$(BANNERTOOL) makebanner -i $(BANNER_PNG) -o $(BANNER)

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo "Compilando C++ $<..."
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "Compilando C $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	@echo "Limpiando archivos..."
	@rm -fr $(BUILD) $(3DSX) $(CIA) $(SMDH) $(BANNER)

.PHONY: all clean
