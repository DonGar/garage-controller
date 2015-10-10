
# Id of device to push.
CORE_ID ?= Garage

.PHONY: default compile push flash wifi clean update_libs

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

update_libs:
	git checkout particle-strip_branch
	git pull
	git checkout master
	git merge --squash -s subtree --no-commit particle-strip_branch

clean:
	rm -f firmware.bin
