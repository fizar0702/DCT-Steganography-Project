CC := cc
AR := ar

LIBJPEG_DIR := libjpeg/libjpeg-master
SRC_DIR     := src
BUILD_DIR   := build

TARGET := stego

CFLAGS  ?= -O2 -Wall -std=gnu89 -Wno-implicit-function-declaration -Wno-implicit-int
CFLAGS  += -I$(SRC_DIR) -I$(LIBJPEG_DIR) -MMD -MP
LDFLAGS ?=

LIBJPEG_SRCS := \
    jcapimin.c jcapistd.c jctrans.c   jcparam.c  jdatadst.c \
    jcinit.c   jcmaster.c jcmarker.c  jcmainct.c jcprepct.c \
    jccoefct.c jccolor.c  jcsample.c  jchuff.c   jcphuff.c  \
    jcdctmgr.c jfdctfst.c jfdctflt.c  jfdctint.c \
    jdapimin.c jdapistd.c jdtrans.c   jdatasrc.c jdmaster.c \
    jdinput.c  jdmarker.c jdhuff.c    jdphuff.c  jdmainct.c \
    jdcoefct.c jdpostct.c jddctmgr.c  jidctfst.c jidctflt.c \
    jidctint.c jidctred.c jdsample.c  jdcolor.c  jquant1.c  \
    jquant2.c  jdmerge.c  jcomapi.c   jutils.c   jerror.c   \
    jmemmgr.c  jmemnobs.c

STEGO_SRCS := jpeg_wrapper.c stego_embed.c stego_extract.c stego_analyze.c main.c

LIBJPEG_OBJS := $(addprefix $(BUILD_DIR)/libjpeg/,$(LIBJPEG_SRCS:.c=.o))
STEGO_OBJS   := $(addprefix $(BUILD_DIR)/src/,$(STEGO_SRCS:.c=.o))
ALL_DEPS     := $(LIBJPEG_OBJS:.o=.d) $(STEGO_OBJS:.o=.d)

LIBJPEG_A := $(BUILD_DIR)/libjpeg.a

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(STEGO_OBJS) $(LIBJPEG_A)
	$(CC) $(STEGO_OBJS) $(LIBJPEG_A) $(LDFLAGS) -lm -o $@

$(LIBJPEG_A): $(LIBJPEG_OBJS)
	$(AR) rcs $@ $(LIBJPEG_OBJS)

$(BUILD_DIR)/libjpeg/%.o: $(LIBJPEG_DIR)/%.c | $(BUILD_DIR)/libjpeg
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/src/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)/src
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/libjpeg $(BUILD_DIR)/src:
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

-include $(ALL_DEPS)