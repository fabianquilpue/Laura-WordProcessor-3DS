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

CFILES		:=	$(foreach dir,$(SOURCES),$(wildcard $(dir)/*.c))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(wildcard $(dir)/*.cpp))
SFILES		:=	$(foreach dir,$(SOURCES),$(wildcard $(dir)/*.s))

OBJS		:=	$(addprefix $(BUILD)/,$(CFILES:.c=.o) $(CPPFILES:.cpp=.o) $(SFILES:.s=.o))

all: $(TARGET).3dsx $(TARGET).cia

$(TARGET).3dsx: $(BUILD)/$(TARGET).elf
	@echo builtin 3dsx...
	elf23ds $< $@
	@smdhtool --create "Laura Word Processor" "Editor de texto minimalista" "Laura" $(TARGET).smdh || true
	@bannertool makesmdh -s "Laura Word Processor" -l "Laura" -p "Autor" -i assets/icon.png -o $(TARGET).smdh || true

$(TARGET).cia: $(TARGET).3dsx
	@echo builtin cia...
	cp $(TARGET).3dsx $(TARGET).cia || true

$(BUILD)/$(TARGET).elf: $(OBJS)
	@echo linking ...
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
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).elf $(TARGET).smdh $(TARGET).cia $(TARGET).bnr

.PHONY: all clean
