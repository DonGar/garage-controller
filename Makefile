
# Id of device to push.
CORE_ID ?= Garage

.PHONY: default compile push flash wifi clean

default: compile

firmware.bin: firmware/*
	particle compile \
		firmware/* \
		particle-strip/firmware/*.h \
		particle-strip/firmware/*.cpp \
		--saveTo firmware.bin

compile: firmware.bin

push: clean compile
	particle flash ${CORE_ID} firmware.bin

flash:
	sudo particle flash --usb firmware.bin

wifi:
	sudo particle setup wifi

clean:
	rm -f firmware.bin
