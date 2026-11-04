KERNEL := $(shell uname -r)
KERNEL_SRC := /lib/modules/$(KERNEL)/build


obj-m := placa_auxiliar.o
placa_auxiliar-y := src/main.o src/leds.o src/speaker.o src/buttons.o
ccflags-y += -I$(src)/include


.PHONY: all clean

all:
	mkdir -p build
	cp Makefile build/Makefile
	cp -R src include build/
	$(MAKE) -C "$(KERNEL_SRC)" M="$(CURDIR)/build" modules

clean:
	rm -rf -- build

