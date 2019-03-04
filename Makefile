SHELL := /bin/bash
PATH := $(PWD)/../depot_tools:$(PATH)

all:
	echo 'Nothing to do' && exit 1

build:
	gn gen out/Default
	ninja -C out/Default
.PHONY: build

update:
	gclient sync

example:
	g++ -g \
		-o example example.cpp \
		-I. -I./third_party/mini_chromium/mini_chromium \
		-std=c++14 \
		-L./out/Default/obj/client -lclient \
		-L./out/Default/obj/util -lutil \
		-L./out/Default/obj/third_party/mini_chromium/mini_chromium/base -lbase \
		-framework Foundation -framework Security -framework CoreText \
		-framework CoreGraphics -framework IOKit -lbsm
.PHONY: example
