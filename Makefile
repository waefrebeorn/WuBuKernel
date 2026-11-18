# WuBuKernel — WuBuOS' own C11 kernel core
# make predefines CC=cc; only take gcc when the caller did not choose one
ifeq ($(origin CC),default)
CC := gcc
endif
CFLAGS  ?= -O2 -std=c11 -Wall -Wextra -ffreestanding -fno-stack-protector
INC     := -Iinclude

FREESTANDING_INC := $(shell $(CC) -print-file-name=include)

CORE    := src/wubukcore.c src/wubudriver.c src/wubukernel_pmm.c
TEST    := src/wubukernel_test.c
PMMTEST := src/wubukernel_pmm_test.c

all: test

test: build/test_wubukernel build/test_wubukernel_pmm
	./build/test_wubukernel
	./build/test_wubukernel_pmm

build:
	mkdir -p build

build/test_wubukernel: $(CORE) $(TEST) include/wubukcore.h include/wubudriver.h | build
	$(CC) $(CFLAGS) $(INC) $(CORE) $(TEST) -o $@

build/test_wubukernel_pmm: src/wubukernel_pmm.c $(PMMTEST) include/wubukernel_pmm.h | build
	$(CC) $(CFLAGS) $(INC) src/wubukernel_pmm.c $(PMMTEST) -o $@

# Proof the kernel pieces need no libc header: compiler's own include dir only.
freestanding: | build
	$(CC) -std=c11 -ffreestanding -nostdinc -isystem $(FREESTANDING_INC) $(INC) \
	      -c src/wubukcore.c -o build/wubukcore.freestanding.o
	$(CC) -std=c11 -ffreestanding -nostdinc -isystem $(FREESTANDING_INC) $(INC) \
	      -c src/wubudriver.c -o build/wubudriver.freestanding.o
	$(CC) -std=c11 -ffreestanding -nostdinc -isystem $(FREESTANDING_INC) $(INC) \
	      -c src/wubukernel_pmm.c -o build/wubukernel_pmm.freestanding.o

clean:
	rm -rf build

.PHONY: all test freestanding clean
