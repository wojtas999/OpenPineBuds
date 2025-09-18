cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

obj-y := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)src/*.c))
obj-y := $(obj-y:.c=.o)

ccflags-y += \
    -Iapps/micstream_ble/include \
    -Iservices/audioflinger \
    -Iservices/ble_app/app_datapath \
    -Iplatform/hal

ifdef MICSTREAM_NODE_ID
ccflags-y += -DMICSTREAM_NODE_ID=$(MICSTREAM_NODE_ID)
endif
