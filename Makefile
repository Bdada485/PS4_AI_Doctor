OO_PS4_TOOLCHAIN ?= /lib/OpenOrbisSDK

TARGET = ps4aidoctor
LIBS   = -lc -lkernel -lSceUserService -lScePad

SRC    = src/main.cpp
OBJ    = $(SRC:.cpp=.o)

CXX = $(OO_PS4_TOOLCHAIN)/bin/linux/clang++
CC  = $(OO_PS4_TOOLCHAIN)/bin/linux/clang

CFLAGS   = -Wall -O2 -I$(OO_PS4_TOOLCHAIN)/include
CXXFLAGS = $(CFLAGS) -std=c++17 -fno-exceptions -fno-rtti
LDFLAGS  = -L$(OO_PS4_TOOLCHAIN)/lib

all: $(TARGET).elf
	$(OO_PS4_TOOLCHAIN)/bin/linux/create-fself -in=$(TARGET).elf --out=$(TARGET).oelf
	$(OO_PS4_TOOLCHAIN)/bin/linux/create-eboot -in=$(TARGET).oelf --out=eboot.bin --paid 0x3800000000000011
	@echo "OK -> eboot.bin"

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJ)
	$(CXX) $(OBJ) $(LDFLAGS) $(LIBS) -o $(TARGET).elf

clean:
	rm -f $(OBJ) $(TARGET).elf $(TARGET).oelf eboot.bin
