#ccflags-y += -Wall

ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/msg2xxx/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/

obj-$(CONFIG_TOUCHSCREEN_ILITEK_TDD_TS) += ilitek_drv_main.o \
	ilitek_drv_mp_test.o \
	ilitek_drv_qcom.o \
	ilitek_drv_update.o \
	mp_result.o \
	mp_misc.o \
	mp_parse.o \
	mp_test.o \
	mp_main.o \
