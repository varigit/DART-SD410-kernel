/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Jijie Wang <jijie_wang@ilitek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA.
 *
 */

#ifndef __ILITEK_TS_H__
#define __ILITEK_TS_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <asm/delay.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/version.h>
#include <linux/rtc.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/pm_runtime.h>
#include <linux/inet.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#include <linux/wakelock.h>
#endif


/*Chip Id*/
#define CHIP_TYPE_CAMARO                    			(0xbf)

/*I2C*/
#define SLAVE_I2C_ID_DBBUS                  			(0xC4>>1) /*    0x62    */
#define SLAVE_I2C_ID_DWI2C                  			(0x4C>>1) /*    0x26    */

#define VDD_VOLTAGE_MAX    			    				3300000
#define VDD_VOLTAGE_MIN			    					2800000
#define I2C_VOLTAGE_MAX			    					1800000
#define I2C_VOLTAGE_MIN			    					1800000

#define HARDWARE_RESET_DELAY_TIME   					150

#define ILITEK_TS_NAME				    				"ilitek_ts"

#define GET_DTS_GPIO_NUM
#ifndef GET_DTS_GPIO_NUM
/*must set*/
#define ILITEK_IRQ_GPIO									9
#define ILITEK_RESET_GPIO			        			10
#endif

/*driver information*/
#define DERVER_VERSION_V0								1
#define DERVER_VERSION_V1		            			(ILITEK_UPDATE_SW_ID<<8 | AREA)
#define DERVER_VERSION_V2 		            			2
#define DERVER_VERSION_V3		            			0

#define AREA                                			0                                       

#define ILITEK_TOOL

/*#define ILITEK_ESD_PROTECTION*/

#define ILITEK_TOUCH_PROTOCOL_B

/*#define ILITEK_REPORT_PRESSURE*/


#define ILITEK_CLICK_WAKEUP

/*#define ILITEK_UPDATE_FW*/

#define CONFIG_UPDATE_FIRMWARE_BY_TWO_DIMENSIONAL_ARRAY

/*suport point*/
#define MUTUAL_MAX_TOUCH_NUM							(10)

#define ILITEK_MANUAL_SET_RESOLUTION
#define ILITEK_ROTATE_FLAG								0
#define ILITEK_REVERT_X									0
#define ILITEK_REVERT_Y									0
#define TOUCH_SCREEN_X_MAX								(240)  /*LCD_WIDTH*/
#define TOUCH_SCREEN_Y_MAX								(320)  /*LCD_HEIGHT*/
#define TPD_WIDTH										(2048)
#define TPD_HEIGHT										(2048)

#define ILITEK_ENABLE_REGULATOR_POWER_ON

#define ILITEK_UPDATE_SW_ID								0x00


/*fw upgrate information*/

#define CAMARO_FIRMWARE_MAIN_BLOCK_SIZE					(128) /*128K*/
#define CAMARO_FIRMWARE_INFO_BLOCK_SIZE					(2) /*2 	//K*/
#define CAMARO_FIRMWARE_WHOLE_SIZE						(CAMARO_FIRMWARE_MAIN_BLOCK_SIZE+CAMARO_FIRMWARE_INFO_BLOCK_SIZE) /*130K*/
#define CAMARO_EMEM_SIZE_BYTES_PER_ONE_PAGE				(128)
#define CAMARO_EMEM_SIZE_BYTES_ONE_WORD					(4)

#define CAMARO_EMEM_MAIN_MAX_ADDR						(0x3FFF) /*0~0x3FFF = 0x4000 = 16384 = 65536/4*/
#define CAMARO_EMEM_INFO_MAX_ADDR						(0x1FF) /*0~0x1FF = 0x200 = 512 = 2048/4*/
#define CAMARO_MAX_I2C_TRANSACTION_LENGTH_LIMIT			(250) /*(128) // Please change the value depends on the I2C transaction limitation for the platform that you are using.*/

#define ILITEK_DEMO_MODE								0x5A
#define ILITEK_DEBUG_MODE								0xA7
#define ILITEK_PACKET_TYPE_TOOTH_PATTERN				0x20
#define CAMARO_FIRMWARE_MODE_UNKNOWN_MODE				0xFF
#define CAMARO_FIRMWARE_MODE_DEMO_MODE					0x00
#define CAMARO_FIRMWARE_MODE_DEBUG_MODE					0x01


/*  Debug messages */
#ifdef BIT
#undef BIT
#endif
#define BIT(x)	(1 << (x))

enum {
       PRINT_NONE = 0, 
	PRINT_INFO = BIT(0),
	PRINT_FINGER_REPORT = BIT(1),
	PRINT_KEY_REPORT = BIT(2),
	PRINT_FIRMWARE_UPGRATE = BIT(3),
	PRINT_MPTEST = BIT(4),
	PRINT_I2C_IRQ = BIT(5),
	PRINT_ESD_CHECK = BIT(6),
	PRINT_ALL = ~0,
};


/* Distributed to all core functions */
extern uint32_t print_debug_level;

#define ILITEK_ERR_LOG_LEVEL							(1)
#define ILITEK_INFO_LOG_LEVEL							(3)

#define debug_level(level, fmt, arg...) do {\
		if (level == ILITEK_ERR_LOG_LEVEL) {\
			pr_err(" %s ERR  line = %d %s : "fmt, "ILITEK", __LINE__, __func__, ##arg);\
		}\
		else if (level == ILITEK_INFO_LOG_LEVEL) {\
			pr_info(" %s INFO line = %d %s : "fmt, "ILITEK", __LINE__, __func__, ##arg);\
		}\
} while (0)

#define tp_log_err(fmt, arg...) debug_level(ILITEK_ERR_LOG_LEVEL, fmt, ##arg)
#define tp_log_info(fmt, arg...) debug_level(ILITEK_INFO_LOG_LEVEL, fmt, ##arg)
#define tp_log_debug(level, fmt, arg...) do {\
		if (level & print_debug_level)	\
		    pr_info("ILITEK DEBUG %d: (%s, %d): " fmt,level,__func__, __LINE__,##arg);	\
	} while (0)

typedef enum {
    ADDRESS_MODE_8BIT = 0,
    ADDRESS_MODE_16BIT = 1
} address_mode_e;


struct ilitek_key_info {
    int id;
    int x;
    int y;
    int status;
    int flag;
};

struct ilitek_ts_data {
    struct i2c_client *client;
    struct input_dev *input_dev;
    struct regulator *vdd;
    struct regulator *vdd_i2c;
    struct regulator *vcc_io;
    int irq_gpio;
    int reset_gpio;
    bool system_suspend;
    int upgrade_FW_info_addr;
    unsigned char *p_platform_fw_ver;
    unsigned char *p_customer_fw_ver;
    unsigned char customer_minor;
    unsigned char customer_major;
    int fw_version_flag;
    int tp_max_x;
    int tp_max_y;
    int tp_min_x;
    int tp_min_y;
    int screen_max_x;
    int screen_max_y;
    int screen_min_x;
    int screen_min_y;
    int max_tp;
    int max_btn;
    uint16_t debug_mode_len;
    int mc_x_channel_num;
    int mc_y_channel_num;
    int sc_x_channel_num;
    int sc_y_channel_num;
    int keycount;
    int key_xlen;
    int key_ylen;
    struct ilitek_key_info keyinfo[10];
    bool irq_status;
    bool irq_trigger;
    struct task_struct * irq_thread;
    spinlock_t irq_lock;
    bool is_touched;
#ifdef ILITEK_CLICK_WAKEUP
    int touch_before_suspend;
#endif
    bool touch_key_hold_press;
    int touch_flag[10];
    
#if defined(CONFIG_FB)
    struct notifier_block fb_notif;
#elif defined(CONFIG_HAS_EARLYSUSPEND)
    struct early_suspend early_suspend;
#endif
#ifdef ILITEK_UPDATE_FW
    bool force_update;
    struct task_struct * update_thread;
#endif
    bool firmware_updating;
    bool operation_protection;
    
#ifdef ILITEK_ESD_PROTECTION
    struct workqueue_struct *esd_wq;
    struct delayed_work esd_work;
    bool esd_check;
    unsigned long esd_delay;
#endif
    struct kobject * ilitek_func_kobj;
    struct mutex ilitek_mutex;
    
};

#define ILITEK_I2C_RETRY_COUNT					3

extern int frimware_mode;
extern char ilitek_driver_information[];
extern struct ilitek_ts_data * ilitek_data;
extern void ilitek_resume(void);
extern void ilitek_suspend(void);

extern int ilitek_main_probe(struct ilitek_ts_data * ilitek_data);
extern int ilitek_main_remove(struct ilitek_ts_data * ilitek_data);
extern void ilitek_reset(int delay);
extern int ilitek_poll_int(void);
extern int ilitek_i2c_write(uint8_t * cmd, int length);
extern int ilitek_i2c_read(uint8_t *data, int length);
extern int ilitek_i2c_write_and_read(uint8_t *cmd,
                                     int write_len, int delay, uint8_t *data, int read_len);

extern void ilitek_irq_enable(void);
extern void ilitek_irq_disable(void);
extern int ilitek_read_tp_info(void);

extern void ilitek_reg_set_16bit_value_on(u16 addr, u16 bit);
extern void ilitek_reg_set_16bit_value_off(u16 addr, u16 bit) ;
extern u8 ilitek_reg_get_lowbyte_value(u16 addr);
extern void ilitek_reg_set_lowbyte_value(uint16_t addr, uint8_t data);
extern u16 ilitek_reg_get_16bit_value(uint16_t addr);
extern u16 ilitek_reg_set_16bit_value(uint16_t addr, uint16_t data);
extern u16 ilitek_reg_get_16bit_value_by_address_mode(u16 addr, address_mode_e address_mode);
extern void ilitek_reg_set_16bit_value_by_address_mode(u16 addr, u16 nData, address_mode_e address_mode);
extern void ilitek_reg_mask_16bit_value(u16 addr, u16 mask, u16 data, address_mode_e address_mode);
extern void ilitek_enter_ice_mode(void);
extern void ilitek_exit_ice_mode(void);

extern void ilitek_dbus_i2c_response_ack(void);


extern int ilitek_upgrade_firmware(uint8_t force_update_flag);

extern uint8_t *target_fw_update_ptr;

#ifdef ILITEK_TOOL
extern int ilitek_create_tool_node(void);
extern int ilitek_remove_tool_node(void);
#endif


#endif
