CXX ?= $(shell command -v g++ 2>/dev/null || command -v clang++ 2>/dev/null)
CXXFLAGS ?= -std=c++17 -O3 -Wall -Wextra

SMAP_USER_DIR := ../src/user
COMMON_MEXT_SRCS := \
	$(SMAP_USER_DIR)/predictor/swap_shadow_map.cpp \
	$(SMAP_USER_DIR)/predictor/tier1_table.cpp \
	$(SMAP_USER_DIR)/predictor/tiny_transformer.cpp \
	$(SMAP_USER_DIR)/predictor/mext_engine.cpp

MEXTD_SRCS := \
	mextd.cpp \
	$(COMMON_MEXT_SRCS) \
	$(SMAP_USER_DIR)/reclaim/proactive_reclaim.cpp

MEXT_TRAIN_SRCS := \
	mext_train.cpp \
	$(COMMON_MEXT_SRCS)

UNAME_S := $(shell uname -s)
KBUILD_DIR ?= /lib/modules/$(shell uname -r)/build

.PHONY: all user kmod check clean

all: user
ifeq ($(UNAME_S),Linux)
ifneq ($(wildcard $(KBUILD_DIR)/Makefile),)
	$(MAKE) -C kernel modules KERNELDIR=$(KBUILD_DIR)
endif
endif

user: mextd mext_train

mextd: $(MEXTD_SRCS)
	$(CXX) $(CXXFLAGS) -I$(SMAP_USER_DIR) $(MEXTD_SRCS) -lpthread -o $@

mext_train: $(MEXT_TRAIN_SRCS)
	$(CXX) $(CXXFLAGS) -I$(SMAP_USER_DIR) $(MEXT_TRAIN_SRCS) -lpthread -o $@

kmod:
	$(MAKE) -C kernel modules KERNELDIR=$(KBUILD_DIR)

check: mextd mext_train
	./mextd --self-test
	./mext_train --self-test

clean:
	rm -f mextd mext_train
ifeq ($(UNAME_S),Linux)
ifneq ($(wildcard $(KBUILD_DIR)/Makefile),)
	$(MAKE) -C kernel clean KERNELDIR=$(KBUILD_DIR)
endif
endif
