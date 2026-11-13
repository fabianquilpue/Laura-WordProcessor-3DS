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

APP_TITLE	:=	Laura
APP_DESCRIPTION	:=	Procesador de Texto Minimalista para Nintendo 3DS
APP_AUTHOR	:=	Laura Dev

PORTLIBS	:=	$(DEVKITPRO)/portlibs/3ds
LIBCTRU		:=	$(DEVKITPRO)/libctru

ARCH		:=	-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS		:=	-Wall -O2 -mword-relocations \
			-fomit-frame-pointer -ffunction-sections \
			$(ARCH) -D__3DS__ \
			-I$(LIBCTRU)/include \
			-I$(PORTLIBS)/include \
			$(foreach dir,$(INCLUDES),-I$(dir))

CXXFLAGS	:=	$(CFLAGS) -std=gnu++17 -fno-rtti -fno-exceptions

ASFLAGS		:=	-g $(ARCH)
LDFLAGS		:=	-specs=3dsx.specs -g $(ARCH) \
			-L$(LIBCTRU)/lib \
			-L$(PORTLIBS)/lib

LIBS		:=	-lcitro2d -lcitro3d -lctru -lm

CFILES		:=	$(foreach dir,$(SOURCES),$(wildcard $(dir)/*.c))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(wildcard $(dir)/*.cpp))
SFILES		:=	$(foreach dir,$(SOURCES),$(wildcard $(dir)/*.s))

OBJS		:=	$(addprefix $(BUILD)/,$(CFILES:.c=.o) $(CPPFILES:.cpp=.o) $(SFILES:.s=.o))

# Detectar automáticamente si existe icon.png o Laura.png en la raíz
ICON_FILE	:=	$(firstword $(wildcard $(TARGET).png icon.png))

# Incluir romfs si la carpeta existe y no está vacía
ROMFS_FLAG	:=	$(if $(wildcard $(ROMFS)/*),--romfs=$(ROMFS),)

.PHONY: all clean

all: $(TARGET).3dsx

# 1. Crear primero el archivo SMDH (con ícono si existe, o con ícono por defecto si no)
$(TARGET).smdh:
	@echo "Creando $(TARGET).smdh..."
ifneq ($(strip $(ICON_FILE)),)
	@smdhtool --create "$(APP_TITLE)" "$(APP_DESCRIPTION)" "$(APP_AUTHOR)" "$(ICON_FILE)" $@ || \
	 smdhtool --create "$(APP_TITLE)" "$(APP_DESCRIPTION)" "$(APP_AUTHOR)" $@
else
	@smdhtool --create "$(APP_TITLE)" "$(APP_DESCRIPTION)" "$(APP_AUTHOR)" $@
endif

# 2. Enlazar el ejecutable ELF
$(BUILD)/$(TARGET).elf: $(OBJS)
	@mkdir -p $(dir $@)
	@echo "Enlazando $(TARGET).elf..."
	@$(CXX) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

# 3. Empaquetar el .3dsx usando el .elf y el .smdh ya creados
$(TARGET).3dsx: $(BUILD)/$(TARGET).elf $(TARGET).smdh
	@echo "Generando $(TARGET).3dsx..."
	@3dsxtool $< $@ --smdh=$(TARGET).smdh $(ROMFS_FLAG)

# Compilación de archivos objeto
$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo "Compilando C++ $<..."
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "Compilando C $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	@echo "Ensamblando $<..."
	@$(CC) $(ASFLAGS) -c $< -o $@

clean:
	@echo "Limpiando archivos de compilación..."
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).smdh $(TARGET).elf $(TARGET).cia
