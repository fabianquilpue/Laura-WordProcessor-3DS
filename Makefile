
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path>")
endif

include $(DEVKITARM)/3ds_rules

# Configuración general
TARGET      := Laura
BUILD       := build
SOURCES     := source
INCLUDES    := include
DATA        := data
ROMFS       := romfs
PORTLIBS    := $(DEVKITPRO)/portlibs/3ds

ARCH        := -march=armv6k -mtune=mpcore -mfloat-abi=hard

CFLAGS      := -Wall -O2 -ffunction-sections \
               $(ARCH) -D__3DS__ \
               -I$(DEVKITPRO)/libctru/include \
               -I$(PORTLIBS)/include \
               $(foreach dir,$(INCLUDES),-I$(dir))

CXXFLAGS    := $(CFLAGS) -std=gnu++17 -fno-rtti -fno-exceptions
ASFLAGS     := $(ARCH)

LDFLAGS     := -specs=3dsx.specs $(ARCH) \
               -L$(DEVKITPRO)/libctru/lib \
               -L$(PORTLIBS)/lib

LIBS        := -lcitro2d -lcitro3d -lctru -lm

CFILES      := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.c))
CPPFILES    := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.cpp))
SFILES      := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.s))

OBJS        := $(addprefix $(BUILD)/,$(CFILES:.c=.o) \
               $(CPPFILES:.cpp=.o) \
               $(SFILES:.s=.o))

all: $(TARGET).smdh $(TARGET).3dsx

$(TARGET).smdh: assets/icon.png
	@echo "Generando metadatos de Laura..."
	smdhtool --create \
		"Laura" \
		"Procesador de texto minimalista para Nintendo 3DS" \
		"Laura Dev / Fabián Contreras" \
		assets/icon.png \
		$@

$(TARGET).3dsx: $(BUILD)/$(TARGET).elf $(TARGET).smdh
	@echo "Generando ejecutable Laura.3dsx..."
	3dsxtool $< $@ \
		--smdh=$(TARGET).smdh \
		--romfs=$(ROMFS)

$(BUILD)/$(TARGET).elf: $(OBJS)
	@echo "Enlazando Laura..."
	$(CXX) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo "Compilando C++ $<..."
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "Compilando C $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	@echo "Limpiando compilación..."
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).elf $(TARGET).smdh $(TARGET).bnr

.PHONY: all clean
