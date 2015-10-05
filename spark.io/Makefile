
# Id of device to push.
CORE_ID ?= Garage

default: compile

compile:
	particle compile .  --saveTo firmware.bin

push:
	particle flash ${CORE_ID} .

flash:
	sudo particle flash --usb firmware.bin

wifi:
	sudo particle setup wifi

clean:
	rm firmware.bin
