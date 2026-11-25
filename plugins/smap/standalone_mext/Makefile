CXX ?= $(shell command -v g++ 2>/dev/null || command -v clang++ 2>/dev/null)
CXXFLAGS ?= -std=c++17 -O3 -Wall -Wextra

SMAP_USER_DIR := ../src/user
MEXT_SRCS := \
	mextd.cpp \
	$(SMAP_USER_DIR)/predictor/swap_shadow_map.cpp \
	$(SMAP_USER_DIR)/predictor/tier1_table.cpp \
	$(SMAP_USER_DIR)/predictor/tiny_transformer.cpp \
	$(SMAP_USER_DIR)/predictor/mext_engine.cpp \
	$(SMAP_USER_DIR)/reclaim/proactive_reclaim.cpp

UNAME_S := $(shell uname -s)
KBUILD_DIR ?= /lib/modules/$(shell uname -r)/build

.PHONY: all user kmod check clean

all: user
ifeq ($(UNAME_S),Linux)
ifneq ($(wildcard $(KBUILD_DIR)/Makefile),)
	$(MAKE) -C kernel modules KERNELDIR=$(KBUILD_DIR)
endif
endif

user: mextd

mextd: $(MEXT_SRCS)
	$(CXX) $(CXXFLAGS) -I$(SMAP_USER_DIR) $(MEXT_SRCS) -lpthread -o $@

kmod:
	$(MAKE) -C kernel modules KERNELDIR=$(KBUILD_DIR)

check: mextd
	./mextd --self-test

clean:
	rm -f mextd
ifeq ($(UNAME_S),Linux)
ifneq ($(wildcard $(KBUILD_DIR)/Makefile),)
	$(MAKE) -C kernel clean KERNELDIR=$(KBUILD_DIR)
endif
endif
