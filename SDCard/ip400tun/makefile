################################################################################
# IP400 TUN Bridge Makefile
# Creates TUN interface for IP networking over IP400 mesh
################################################################################

RM := rm -rf
CC = gcc

# Source directories
IP400SPI_SRC = ../ip400spi/src
IP400SPI_INC = ../ip400spi/include

# Source files from this project
LOCAL_SRCS = \
./src/main.c \
./src/tun.c \
./src/ip400bridge.c \
./src/spitask.c \
./src/callsign.c

# Source files shared from ip400spi
SHARED_SRCS = \
$(IP400SPI_SRC)/spi.c \
$(IP400SPI_SRC)/logger.c \
$(IP400SPI_SRC)/dataq.c \
$(IP400SPI_SRC)/timer.c \
$(IP400SPI_SRC)/errno.c

# All sources
C_SRCS = $(LOCAL_SRCS) $(SHARED_SRCS)

# Object files
LOCAL_OBJS = \
./src/main.o \
./src/tun.o \
./src/ip400bridge.o \
./src/spitask.o \
./src/callsign.o

SHARED_OBJS = \
./src/spi.o \
./src/logger.o \
./src/dataq.o \
./src/timer.o \
./src/errno.o

OBJS = $(LOCAL_OBJS) $(SHARED_OBJS)

# Include paths
INCLUDES = -I"./include" -I"$(IP400SPI_INC)"

# Compiler flags
CFLAGS = $(INCLUDES) -O2 -g -Wall -c -fmessage-length=0 -pthread

# Linker flags
LDFLAGS = -lpthread -lm -lrt

# Target executable
TARGET = Ip400Tun

# All Target
all: $(TARGET)

# Tool invocations
$(TARGET): $(OBJS) makefile
	@echo 'Building target: $@'
	@echo 'Invoking: GCC Linker'
	$(CC) -o "$(TARGET)" $(OBJS) $(LDFLAGS)
	@echo 'Finished building target: $@'
	@echo ' '

# Compile local source files
src/%.o: src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC Compiler'
	$(CC) $(CFLAGS) -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

# Compile shared source files from ip400spi
src/spi.o: $(IP400SPI_SRC)/spi.c
	@echo 'Building shared file: $<'
	$(CC) $(CFLAGS) -o "$@" "$<"

src/logger.o: $(IP400SPI_SRC)/logger.c
	@echo 'Building shared file: $<'
	$(CC) $(CFLAGS) -o "$@" "$<"

src/dataq.o: $(IP400SPI_SRC)/dataq.c
	@echo 'Building shared file: $<'
	$(CC) $(CFLAGS) -o "$@" "$<"

src/timer.o: $(IP400SPI_SRC)/timer.c
	@echo 'Building shared file: $<'
	$(CC) $(CFLAGS) -o "$@" "$<"

src/errno.o: $(IP400SPI_SRC)/errno.c
	@echo 'Building shared file: $<'
	$(CC) $(CFLAGS) -o "$@" "$<"

# Clean
clean:
	-$(RM) $(TARGET)
	-$(RM) src/*.o
	-@echo ' '

.PHONY: all clean
