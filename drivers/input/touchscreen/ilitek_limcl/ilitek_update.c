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

#include "ilitek_ts.h"
#include <linux/firmware.h>
#include <linux/vmalloc.h>

#ifdef ILITEK_UPDATE_FW
#include "ilitek_fw.h"
#endif	 

#define CAMARO_FIRMWARE_MAIN_BLOCK_SIZE (128)	//128K
typedef enum  { 
EMEM_ALL = 0, 
EMEM_MAIN, EMEM_INFO
} emem_type;
typedef enum { 
 CAMARO_SW_ID_XXXX = 0x0001, 
 CAMARO_SW_ID_YYYY =0x0002, 
 CAMARO_SW_ID_UNDEFINED = 0xFFFF
} camaro_sw_id_e;

static u16 sfr_addr3_byte0_1_value = 0x0000;
static u16 sfr_addr3_byte2_3_value = 0x0000;
uint8_t * target_fw_update_ptr;
unsigned int UpdateCRC(unsigned int crc, unsigned char newbyte)
{
    char i;		// loop counter
#define CRC_POLY 0x8408		// CRC16-CCITT FCS (X^16+X^12+X^5+1)
    crc = crc ^ newbyte;
    
    for (i = 0; i < 8; i++) {
        if (crc & 0x01) {
            crc = crc >> 1;
            crc ^= CRC_POLY;
        } else {
            crc = crc >> 1;
        }
    }
    
    return crc;
}

static void ilitek_camaro_access_eflash_init(void)
{
    // Disable cpu read flash
    ilitek_reg_set_lowbyte_value(0x1606, 0x20);
    ilitek_reg_set_lowbyte_value(0x1608, 0x20);
    // Clear PROGRAM erase password
    ilitek_reg_set_16bit_value(0x1618, 0xA55A);
}
static u32 ilitek_camaro_get_firmware_crc_by_hardware(emem_type type)
{
    u32 ret_val = 0;
    u32 retry_time = 0;
    u32 crc_end_addr = 0;
    u16 crc_down = 0;
    tp_log_debug(PRINT_FIRMWARE_UPGRATE," eEmemType = %d \n", type);
    ilitek_reset(HARDWARE_RESET_DELAY_TIME);
    /*enter ice mode */
    ilitek_enter_ice_mode();
    ilitek_camaro_access_eflash_init();
    
    if (type == EMEM_MAIN) {
        // Disable cpu read flash
        ilitek_reg_set_lowbyte_value(0x1608, 0x20);
        ilitek_reg_set_lowbyte_value(0x1606, 0x20);
        // Set read flag
        ilitek_reg_set_16bit_value(0x1610, 0x0001);
        // Mode reset main block
        ilitek_reg_set_16bit_value(0x1606, 0x0000);
        // CRC reset
        ilitek_reg_set_16bit_value(0x1620, 0x0002);
        ilitek_reg_set_16bit_value(0x1620, 0x0000);
        // Set CRC e-flash block start address => Main Block : 0x0000 ~ 0x7FFE
        ilitek_reg_set_16bit_value(0x1600, 0x0000);
        crc_end_addr =
            (CAMARO_FIRMWARE_MAIN_BLOCK_SIZE * 1024) / 4 - 2;
        ilitek_reg_set_16bit_value(0x1622, crc_end_addr);
        // Trigger CRC check
        ilitek_reg_set_16bit_value(0x1620, 0x0001);
        crc_down = ilitek_reg_get_16bit_value(0x1620);
        retry_time = 0;
        
        while ((crc_down >> 15) == 0) {
            mdelay(10);
            crc_down = ilitek_reg_get_16bit_value(0x1620);
            retry_time++;
            
            if (retry_time > 30) {
                tp_log_err
                ("Wait main block nCrcDown failed.\n");
                break;
            }
        }
        
        ret_val = ilitek_reg_get_16bit_value(0x1626);
        ret_val = (ret_val << 16) | ilitek_reg_get_16bit_value(0x1624);
    } else if (type == EMEM_INFO) {
        // Disable cpu read flash
        ilitek_reg_set_16bit_value(0x1608, 0x20);
        ilitek_reg_set_16bit_value(0x1606, 0x20);
        // Set read flag
        ilitek_reg_set_16bit_value(0x1610, 0x0001);
        // Mode reset info block
        ilitek_reg_set_16bit_value(0x1606, 0x0800);
        ilitek_reg_set_16bit_value(0x1604, 0x01);
        // CRC reset
        ilitek_reg_set_16bit_value(0x1620, 0x0002);
        ilitek_reg_set_16bit_value(0x1620, 0x0000);
        // Set CRC e-flash block start address => Info Block : 0x0020 ~ 0x01FE
        ilitek_reg_set_16bit_value(0x1600, 0x0020);
        ilitek_reg_set_16bit_value(0x1622, 0x01FE);
        // Trigger CRC check
        ilitek_reg_set_16bit_value(0x1620, 0x0001);
        crc_down = ilitek_reg_get_16bit_value(0x1620);
        retry_time = 0;
        
        while ((crc_down >> 15) == 0) {
            mdelay(10);
            crc_down = ilitek_reg_get_16bit_value(0x1620);
            retry_time++;
            
            if (retry_time > 30) {
                tp_log_err
                ("Wait info block nCrcDown failed.\n");
                break;
            }
        }
        
        ret_val = ilitek_reg_get_16bit_value(0x1626);
        ret_val = (ret_val << 16) | ilitek_reg_get_16bit_value(0x1624);
    }
    
    tp_log_debug(PRINT_FIRMWARE_UPGRATE,"Hardware CRC = 0x%x\n", ret_val);
    ilitek_exit_ice_mode();
    return ret_val;
}

static void ilitek_camaro_write_eflash_start(u16 start_addr, u8 * first_data,
        emem_type type)
{
    tp_log_debug(PRINT_FIRMWARE_UPGRATE," eEmemType = %d \n", type);
    // Disable cpu read flash
    ilitek_reg_set_lowbyte_value(0x1608, 0x20);
    ilitek_reg_set_lowbyte_value(0x1606, 0x20);
    // Set e-flash mode to write mode
    ilitek_reg_set_16bit_value(0x1606, 0x0040);
    // Set data align
    ilitek_reg_set_lowbyte_value(0x1640, 0x01);
    
    if (type == EMEM_INFO) {
        ilitek_reg_set_lowbyte_value(0x1607, 0x08);
    }
    
    // Set double buffer
    ilitek_reg_set_lowbyte_value(0x1604, 0x01);
    // Set e-flash mode trigger(Trigger write mode)
    ilitek_reg_set_lowbyte_value(0x1606, 0x81);
    // Set init data
    ilitek_reg_set_lowbyte_value(0x1602, first_data[0]);
    ilitek_reg_set_lowbyte_value(0x1602, first_data[1]);
    ilitek_reg_set_lowbyte_value(0x1602, first_data[2]);
    ilitek_reg_set_lowbyte_value(0x1602, first_data[3]);
    // Set initial address(for latch SA, CA)
    ilitek_reg_set_16bit_value(0x1600, start_addr / 4);
    // Set initial address(for latch PA)
    ilitek_reg_set_16bit_value(0x1600, start_addr / 4);
}

static void ilitek_camaro_write_eflash_end(void)
{
    tp_log_debug(PRINT_FIRMWARE_UPGRATE,"write eflash end\n");
}
void ilitek_camaro_read_eflash_start(u16 start_addr, emem_type type)
{
    tp_log_debug(PRINT_FIRMWARE_UPGRATE," eEmemType = %d \n", type);
    // Disable cpu read flash
    ilitek_reg_set_lowbyte_value(0x1608, 0x20);
    ilitek_reg_set_lowbyte_value(0x1606, 0x20);
    ilitek_reg_set_lowbyte_value(0x1606, 0x02);
    ilitek_reg_set_lowbyte_value(0x1600, start_addr);
    
    if (type == EMEM_MAIN) {
        // Set main block
        ilitek_reg_set_lowbyte_value(0x1607, 0x00);
        // Set main double buffer
        ilitek_reg_set_lowbyte_value(0x1604, 0x01);
        // Set e-flash mode to read mode for main
        ilitek_reg_set_16bit_value(0x1606, 0x0001);
    } else if (type == EMEM_INFO) {
        // Set info block
        ilitek_reg_set_lowbyte_value(0x1607, 0x08);
        // Set info double buffer
        ilitek_reg_set_lowbyte_value(0x1604, 0x01);
        // Set e-flash mode to read mode for info
        ilitek_reg_set_16bit_value(0x1606, 0x0801);
    }
}

void ilitek_camaro_read_eflash_do_read(u16 read_addr, u8 * p_read_Data)
{
    u16 reg_data1 = 0, reg_data2 = 0;
    tp_log_debug(PRINT_FIRMWARE_UPGRATE,"*** ReadAddr = 0x%x ***\n", read_addr);
    // Set read address
    ilitek_reg_set_16bit_value(0x1600, read_addr);
    // Read 16+16 bits
    reg_data1 = ilitek_reg_get_16bit_value(0x160A);
    reg_data2 = ilitek_reg_get_16bit_value(0x160C);
    p_read_Data[0] = reg_data1 & 0xFF;
    p_read_Data[1] = (reg_data1 >> 8) & 0xFF;
    p_read_Data[2] = reg_data2 & 0xFF;
    p_read_Data[3] = (reg_data2 >> 8) & 0xFF;
}
void ilitek_camaro_write_eflash_do_write(u16 write_addr,
                                       u8 * p_write_Data)
{
    tp_log_debug(PRINT_FIRMWARE_UPGRATE,"*** WriteAddr = 0x%x  ***\n", write_addr / 4);
    // Write data
    ilitek_reg_set_lowbyte_value(0x1602, p_write_Data[0]);
    ilitek_reg_set_lowbyte_value(0x1602, p_write_Data[1]);
    ilitek_reg_set_lowbyte_value(0x1602, p_write_Data[2]);
    ilitek_reg_set_lowbyte_value(0x1602, p_write_Data[3]);
    // Set address
    ilitek_reg_set_16bit_value(0x1600, write_addr / 4);
}
void ilitek_camaro_read_eflash_end(void)
{
    // Set read done
    ilitek_reg_set_lowbyte_value(0x1606, 0x02);
    // Unset info flag
    ilitek_reg_set_lowbyte_value(0x1607, 0x00);
    // Clear address
    ilitek_reg_set_16bit_value(0x1600, 0x0000);
}
static u32 ilitek_camaro_retrieve_firmware_crc_from_eflash(emem_type type)
{
    u32 ret_val = 0;
    u16 read_addr = 0;
    u8 data[4] = {0 };

    tp_log_debug(PRINT_FIRMWARE_UPGRATE,"eEmemType = %d \n", type);
    ilitek_enter_ice_mode();
    // Stop MCU
    ilitek_reg_set_16bit_value(0x0FE6, 0x01);
    
    if (type == EMEM_MAIN) {	// Read main block CRC(128KB-4) from main block
        ilitek_camaro_read_eflash_start(0x7FFF, EMEM_MAIN);
        read_addr = 0x7FFF;
    } else if (type == EMEM_INFO) {	// Read info block CRC(2KB-CAMARO_EMEM_SIZE_BYTES_PER_ONE_PAGE-4) from info block
        ilitek_camaro_read_eflash_start(0x81FF, EMEM_INFO);
        read_addr = 0x81FF;
    }
    
    ilitek_camaro_read_eflash_do_read(read_addr, &data[0]);
    tp_log_debug(PRINT_FIRMWARE_UPGRATE,
        "Data = 0x%x, 0x%x, 0x%x, 0x%x\n", data[0], data[1], data[2], data[3]); // add for debug
    ilitek_camaro_read_eflash_end();
    ret_val = (data[3] << 24);
    ret_val |= (data[2] << 16);
    ret_val |= (data[1] << 8);
    ret_val |= data[0];
    tp_log_debug(PRINT_FIRMWARE_UPGRATE,"CRC = 0x%x\n", ret_val);
    ilitek_exit_ice_mode();
    return ret_val;
}

static u16 ilitek_camaro_get_sw_id(emem_type type)
{
    u16 ret_val = 0;
    u16 read_addr = 0;
    u8 data[4] = {0};

    tp_log_debug(PRINT_FIRMWARE_UPGRATE,"eEmemType = %d \n", type);
    ilitek_enter_ice_mode();
    // Stop MCU
    ilitek_reg_set_lowbyte_value(0x0FE6, 0x01);
    
    if (type == EMEM_MAIN) {	// Read SW ID from main block
        ilitek_camaro_read_eflash_start(0x7FFD, EMEM_MAIN);
        read_addr = 0x7FFD;
    } else if (type == EMEM_INFO) {	// Read SW ID from info block
        ilitek_camaro_read_eflash_start(0x81FB, EMEM_INFO);
        read_addr = 0x81FB;
    }
    
    ilitek_camaro_read_eflash_do_read(read_addr, &data[0]);
    ilitek_camaro_read_eflash_end();
    /*
       Ex. SW ID in Main Block :
       Major low byte at address 0x7FFD
    
       SW ID in Info Block :
       Major low byte at address 0x81FB
     */
    ret_val = (data[1] << 8);
    ret_val |= data[0];
        tp_log_debug(PRINT_FIRMWARE_UPGRATE,"SW ID = 0x%x\n", ret_val);
    ilitek_exit_ice_mode();
    return ret_val;
}

int ilitek_check_upgrade_firmware_by_sw_id(void)
{
    u32 nCrcMainA, nCrcMainB;
    u16 nUpdateBinMajor = 0, nUpdateBinMinor = 0;
    u16 eSwId = 0xFFFF;
//check if need upgrade
    nCrcMainA = ilitek_camaro_get_firmware_crc_by_hardware(EMEM_MAIN);
    nCrcMainB = ilitek_camaro_retrieve_firmware_crc_from_eflash(EMEM_MAIN);
    tp_log_debug(PRINT_FIRMWARE_UPGRATE,
        "nCrcMainA=0x%x, nCrcMainB=0x%x\n", nCrcMainA, nCrcMainB);
    
    if (nCrcMainA == nCrcMainB) {
        eSwId = ilitek_camaro_get_sw_id(EMEM_MAIN);
        tp_log_debug(PRINT_FIRMWARE_UPGRATE,"eSwId=0x%x\n", eSwId);
        
        if (eSwId == ILITEK_UPDATE_SW_ID) {
#ifdef CONFIG_UPDATE_FIRMWARE_BY_TWO_DIMENSIONAL_ARRAY
            nUpdateBinMajor =
                (*(target_fw_update_ptr + 127 * 1024 + 1013) << 8 |
                 *(target_fw_update_ptr + 127 * 1024 + 1012));
            nUpdateBinMinor =
                (*(target_fw_update_ptr + 127 * 1024 + 1015) << 8 |
                 *(target_fw_update_ptr + 127 * 1024 + 1014));
#else // By one dimensional array
            nUpdateBinMajor =
                (*(target_fw_update_ptr + 0x1FFF5) << 8 |
                 *(target_fw_update_ptr + 0x1FFF4));
            nUpdateBinMinor =
                (*(target_fw_update_ptr + 0x1FFF7) << 8 |
                 *(target_fw_update_ptr + 0x1FFF6));
#endif	 
             tp_log_debug(PRINT_FIRMWARE_UPGRATE,
             "nUpdateBinMinor = 0x%x, customer_minor = 0x%x\n",
             (nUpdateBinMinor & 0xFF),(ilitek_data->customer_minor & 0xFF));
             
            if ((nUpdateBinMinor & 0xFF) >
                (ilitek_data->customer_minor & 0xFF)) {
                tp_log_info("Update new fw.\n");
                return 0;
            } else {
                tp_log_info
                ("The update bin version is older than or equal to the current firmware version on e-flash.\n");
            }
        } else
            tp_log_info("eSwId = 0x%x is not match SW ID.\n",
                       eSwId);
    } else {
        eSwId = ilitek_camaro_get_sw_id(EMEM_INFO);
        tp_log_err("eSwId=0x%x\n", eSwId);
        
        if (eSwId == ILITEK_UPDATE_SW_ID) {
            tp_log_info("Update new fw");
            return 0;
        }
        
        tp_log_info("eSwId = 0x%x is not match SW ID.\n", eSwId);
    }
    
    tp_log_info("Go to normal boot up process.\n");
    return (-1);
}

static u32 ilitek_camaro_calculate_target_fw_crc(u8 * pfwData, u32 offset,
        u32 size)
{
    u32 i;
    u32 data = 0, crc = 0;
    u32 crc_rule = 0x0C470C06;	// 0000 1100 0100 0111 0000 1100 0000 0110
    
    for (i = 0; i < size; i += 4) {
        data =
            (pfwData[offset + i]) | (pfwData[offset + i + 1] << 8) |
            (pfwData[offset + i + 2] << 16) | (pfwData[offset + i + 3]
                                               << 24);
        crc = (crc >> 1) ^ (crc << 1) ^ (crc & crc_rule) ^ data;
    }
    
    return crc;
}

static u32 ilitek_camaro_retrieve_target_fw_crc(u8 * pfwData, emem_type type)
{
    u32 ret_val = 0;
   tp_log_debug(PRINT_FIRMWARE_UPGRATE,"    eEmemType = %d    \n", type);
    
    if (type == EMEM_MAIN) {
        ret_val = *(pfwData + 127 * 1024 + 1023);
        ret_val = (ret_val << 8) | *(pfwData + 127 * 1024 + 1022);
        ret_val = (ret_val << 8) | *(pfwData + 127 * 1024 + 1021);
        ret_val = (ret_val << 8) | *(pfwData + 127 * 1024 + 1020);
    } else if (type == EMEM_INFO) {
        ret_val = *(pfwData + 129 * 1024 + 1023);
        ret_val = (ret_val << 8) | *(pfwData + 129 * 1024 + 1022);
        ret_val = (ret_val << 8) | *(pfwData + 129 * 1024 + 1021);
        ret_val = (ret_val << 8) | *(pfwData + 129 * 1024 + 1020);
    }
    
    return ret_val;
}

static s32 ilitek_check_target_firmware_available(void)
{
    u32 crc_main = 0, nCrcMainBin = 0;
    u32 crc_info = 0, nCrcInfoBin = 0;
    u32 ret = 0;
    /* Calculate main block CRC & info block CRC by device driver itself */
    crc_main =
        ilitek_camaro_calculate_target_fw_crc(target_fw_update_ptr, 0,
                                            CAMARO_FIRMWARE_MAIN_BLOCK_SIZE
                                            * 1024 -
                                            CAMARO_EMEM_SIZE_BYTES_ONE_WORD);
    crc_info =
        ilitek_camaro_calculate_target_fw_crc(target_fw_update_ptr,
                                            CAMARO_FIRMWARE_MAIN_BLOCK_SIZE
                                            * 1024 +
                                            CAMARO_EMEM_SIZE_BYTES_PER_ONE_PAGE,
                                            CAMARO_FIRMWARE_INFO_BLOCK_SIZE
                                            * 1024 -
                                            CAMARO_EMEM_SIZE_BYTES_PER_ONE_PAGE
                                            -
                                            CAMARO_EMEM_SIZE_BYTES_ONE_WORD);
    /* Read main block CRC & info block CRC from firmware bin file */
    nCrcMainBin =
        ilitek_camaro_retrieve_target_fw_crc(target_fw_update_ptr, EMEM_MAIN);
    nCrcInfoBin =
        ilitek_camaro_retrieve_target_fw_crc(target_fw_update_ptr, EMEM_INFO);
        tp_log_debug(PRINT_FIRMWARE_UPGRATE,
        "crc_main=0x%x, crc_info=0x%x, nCrcMainBin=0x%x, nCrcInfoBin=0x%x\n",
     crc_main, crc_info, nCrcMainBin, nCrcInfoBin);
     
    if ((nCrcMainBin != crc_main) || (nCrcInfoBin != crc_info)) {
       tp_log_debug(PRINT_FIRMWARE_UPGRATE,
        "target firmware isn't available. cancel update firmware.\n");
        ret = -1;
    } else {
        tp_log_debug(PRINT_FIRMWARE_UPGRATE,"target firmware is available. update firmware.\n");
        ret = 0;
    }
    
    return ret;
}

static void ilitek_camaro_get_sfraddr_3value(void)
{
    // Disable cpu read flash
    ilitek_reg_set_lowbyte_value(0x1608, 0x20);
    ilitek_reg_set_lowbyte_value(0x1606, 0x20);
    // Set e-flash mode to read mode
    ilitek_reg_set_lowbyte_value(0x1606, 0x01);
    ilitek_reg_set_lowbyte_value(0x1610, 0x01);
    ilitek_reg_set_lowbyte_value(0x1607, 0x20);
    // Set read address
    ilitek_reg_set_lowbyte_value(0x1600, 0x03);
    ilitek_reg_set_lowbyte_value(0x1601, 0x00);
    sfr_addr3_byte0_1_value = ilitek_reg_get_16bit_value(0x160A);
    sfr_addr3_byte2_3_value = ilitek_reg_get_16bit_value(0x160C);
   tp_log_debug(PRINT_FIRMWARE_UPGRATE,
    "sfr_addr3_byte0_1_value = 0x%4X, sfr_addr3_byte2_3_value = 0x%4X\n",
     sfr_addr3_byte0_1_value, sfr_addr3_byte2_3_value);
}
static void ilitek_camaro_unset_protect_bit(void)
{
    u8 b0, b1, b2, b3;
    ilitek_camaro_get_sfraddr_3value();
    b0 = sfr_addr3_byte0_1_value & 0xFF;
    b1 = (sfr_addr3_byte0_1_value & 0xFF00) >> 8;
    b2 = sfr_addr3_byte2_3_value & 0xFF;
    b3 = (sfr_addr3_byte2_3_value & 0xFF00) >> 8;
    tp_log_debug(PRINT_FIRMWARE_UPGRATE,
        "nB0 = 0x%2X, nB1 = 0x%2X, nB2 = 0x%2X, nB3 = 0x%2X\n",
               b0, b1, b2, b3);
    b2 = b2 & 0xBF;	// 10111111
    b3 = b3 & 0xFC;		// 11111100
    tp_log_debug(PRINT_FIRMWARE_UPGRATE,
        "nB0 = 0x%2X, nB1 = 0x%2X, nB2 = 0x%2X, nB3 = 0x%2X\n", 
                b0, b1, b2, b3);
    // Disable cpu read flash
    ilitek_reg_set_lowbyte_value(0x1608, 0x20);
    ilitek_reg_set_lowbyte_value(0x1606, 0x20);
    ilitek_reg_set_lowbyte_value(0x1610, 0x80);
    ilitek_reg_set_lowbyte_value(0x1607, 0x10);
    // Trigger SFR write
    ilitek_reg_set_lowbyte_value(0x1606, 0x01);
    // Set write data
    ilitek_reg_set_lowbyte_value(0x1602, b0);
    ilitek_reg_set_lowbyte_value(0x1602, b1);
    ilitek_reg_set_lowbyte_value(0x1602, b2);
    ilitek_reg_set_lowbyte_value(0x1602, b3);
    // Set write address
    ilitek_reg_set_lowbyte_value(0x1600, 0x03);
    ilitek_reg_set_lowbyte_value(0x1601, 0x00);
    // Set TM mode = 0
    ilitek_reg_set_lowbyte_value(0x1607, 0x00);
}
void ilitek_camaro_set_protect_bit(void)
{
    u8 b0, b1, b2, b3;
    b0 = sfr_addr3_byte0_1_value & 0xFF;
    b1 = (sfr_addr3_byte0_1_value & 0xFF00) >> 8;
    b2 = sfr_addr3_byte2_3_value & 0xFF;
    b3 = (sfr_addr3_byte2_3_value & 0xFF00) >> 8;
   tp_log_debug(PRINT_FIRMWARE_UPGRATE,
    "nB0 = 0x%2X, nB1 = 0x%2X, nB2 = 0x%2X, nB3 = 0x%2X\n",
               b0, b1, b2, b3);
    // Disable cpu read flash
    ilitek_reg_set_lowbyte_value(0x1608, 0x20);
    ilitek_reg_set_lowbyte_value(0x1606, 0x20);
    ilitek_reg_set_lowbyte_value(0x1610, 0x80);
    ilitek_reg_set_lowbyte_value(0x1607, 0x10);
    // Trigger SFR write
    ilitek_reg_set_lowbyte_value(0x1606, 0x01);
    // Set write data
    ilitek_reg_set_lowbyte_value(0x1602, b0);
    ilitek_reg_set_lowbyte_value(0x1602, b1);
    ilitek_reg_set_lowbyte_value(0x1602, b2);
    ilitek_reg_set_lowbyte_value(0x1602, b3);
    // Set write address
    ilitek_reg_set_lowbyte_value(0x1600, 0x03);
    ilitek_reg_set_lowbyte_value(0x1601, 0x00);
    ilitek_reg_set_lowbyte_value(0x1606, 0x02);
}
void ilitek_dbus_i2c_response_ack(void)
{
    ilitek_reg_mask_16bit_value(0x1E4F, BIT(15), BIT(15), ADDRESS_MODE_16BIT);
}
void ilitek_camaro_erase_emem(emem_type type)
{
    u32 info_addr = 0x20;
    u32 time_out = 0;
    u8 reg_data = 0;
    tp_log_debug(PRINT_FIRMWARE_UPGRATE,"     eEmemType = %d    \n", type);
    ilitek_reset(HARDWARE_RESET_DELAY_TIME);
    ilitek_enter_ice_mode();
    ilitek_dbus_i2c_response_ack();
    tp_log_debug(PRINT_FIRMWARE_UPGRATE,"Erase start\n");
    ilitek_camaro_access_eflash_init();
    // Stop mcu
    ilitek_reg_set_lowbyte_value(0x0FE6, 0x01);
    // Set PROGRAM erase password
    ilitek_reg_set_16bit_value(0x1618, 0x5AA5);
    ilitek_camaro_unset_protect_bit();
    
    if (type == EMEM_MAIN) {	// 128KB
        tp_log_debug(PRINT_FIRMWARE_UPGRATE,"Erase main block\n");
        // Set main block
        ilitek_reg_set_lowbyte_value(0x1607, 0x00);
        // Set e-flash mode to erase mode
        ilitek_reg_set_lowbyte_value(0x1606, 0xC0);
        // Set page erase main
        ilitek_reg_set_lowbyte_value(0x1607, 0x03);
        // e-flash mode trigger
        ilitek_reg_set_lowbyte_value(0x1606, 0xC1);
        time_out = 0;
        
        while (1) {	// Wait erase done
            reg_data = ilitek_reg_get_lowbyte_value(0x160E);
            reg_data = (reg_data & BIT(3));
            tp_log_debug(PRINT_FIRMWARE_UPGRATE,"Wait erase done flag nRegData = 0x%x\n",
                       reg_data);
                       
            if (reg_data == BIT(3)) {
                break;
            }
            
            mdelay(10);
            
            if ((time_out++) > 10) {
               tp_log_debug(PRINT_FIRMWARE_UPGRATE,
                "Erase main block failed. Timeout.\n");
                goto EraseEnd;
            }
        }
    } else if (type == EMEM_INFO) {	// 2KB
        tp_log_debug(PRINT_FIRMWARE_UPGRATE,"Erase info block\n");
        // Set info block
        ilitek_reg_set_lowbyte_value(0x1607, 0x08);
        // Set info double buffer
        ilitek_reg_set_lowbyte_value(0x1604, 0x01);
        // Set e-flash mode to erase mode
        ilitek_reg_set_lowbyte_value(0x1606, 0xC0);
        // Set page erase info
        ilitek_reg_set_lowbyte_value(0x1607, 0x09);
        
        for (info_addr = 0x20;
             info_addr <= CAMARO_EMEM_INFO_MAX_ADDR;
             info_addr += 0x20) {
            tp_log_debug(PRINT_FIRMWARE_UPGRATE,"nInfoAddr = 0x%x\n", info_addr);	// add for debug
            // Set address
            ilitek_reg_set_16bit_value(0x1600, info_addr);
            // e-flash mode trigger
            ilitek_reg_set_lowbyte_value(0x1606, 0xC1);
            time_out = 0;
            
            while (1) {	// Wait erase done
                reg_data = ilitek_reg_get_lowbyte_value(0x160E);
                reg_data = (reg_data & BIT(3));
                tp_log_debug(PRINT_FIRMWARE_UPGRATE,
                    "Wait erase done flag nRegData = 0x%x\n",
                 reg_data);
                 
                if (reg_data == BIT(3)) {
                    break;
                }
                
                mdelay(10);
                
                if ((time_out++) > 10) {
                    tp_log_debug(PRINT_FIRMWARE_UPGRATE,
                    "Erase info block failed. Timeout.\n");
                    // Set main block
                    ilitek_reg_set_lowbyte_value(0x1607, 0x00);
                    goto EraseEnd;
                }
            }
        }
        
        // Set main block
        ilitek_reg_set_lowbyte_value(0x1607, 0x00);
    }
    
EraseEnd:
    ilitek_camaro_set_protect_bit();
    ilitek_reg_set_lowbyte_value(0x1606, 0x00);
    ilitek_reg_set_lowbyte_value(0x1607, 0x00);
    // Clear PROGRAM erase password
    ilitek_reg_set_16bit_value(0x1618, 0xA55A);
    tp_log_debug(PRINT_FIRMWARE_UPGRATE,"Erase end\n");
    ilitek_exit_ice_mode();
}

static void ilitek_camaro_isp_burst_write_eflash_do_write(u8 * bufferData,
        u32 length)
{
    u32 i;
    u8 tx_data[3 + CAMARO_EMEM_SIZE_BYTES_PER_ONE_PAGE] = { 0 };
    tx_data[0] = 0x10;
    tx_data[1] = 0x16;
    tx_data[2] = 0x02;
    
    for (i = 0; i < length; i++) {
        tx_data[3 + i] = bufferData[i];
    }
    
    ilitek_i2c_write_and_read(tx_data, (3 + length), 5, tx_data, 0);
}

static void ilitek_camaro_isp_burst_write_eflash_start(u16 start_addr,
        u8 * first_data,u16 page_num,emem_type type)
{
    u8 tx_data[3] = {0};
    
    tp_log_debug(PRINT_FIRMWARE_UPGRATE,
    "***  nStartAddr = 0x%x, nPageNum = %d, eEmemType = %d ***\n",
     start_addr, page_num, type);
    /* Disable cpu read flash */
    ilitek_reg_set_lowbyte_value(0x1608, 0x20);
    ilitek_reg_set_lowbyte_value(0x1606, 0x20);
    // Set e-flash mode to page write mode
    ilitek_reg_set_16bit_value(0x1606, 0x0080);
    // Set data align
    ilitek_reg_set_lowbyte_value(0x1640, 0x01);
    
    if (type == EMEM_INFO) {
        ilitek_reg_set_lowbyte_value(0x1607, 0x08);
    }
    
    // Set double buffer
    ilitek_reg_set_lowbyte_value(0x1604, 0x01);
    // Set page write number
    ilitek_reg_set_16bit_value(0x161A, page_num);
    // Set e-flash mode trigger(Trigger write mode)
    ilitek_reg_set_lowbyte_value(0x1606, 0x81);
    // Set init data
    ilitek_reg_set_lowbyte_value(0x1602, first_data[0]);
    ilitek_reg_set_lowbyte_value(0x1602, first_data[1]);
    ilitek_reg_set_lowbyte_value(0x1602, first_data[2]);
    ilitek_reg_set_lowbyte_value(0x1602, first_data[3]);
    // Set initial address(for latch SA, CA)
    ilitek_reg_set_16bit_value(0x1600, (start_addr / 4));
    // Set initial address(for latch PA)
    ilitek_reg_set_16bit_value(0x1600, (start_addr / 4));
    // enable burst mode
    ilitek_reg_set_lowbyte_value(0x1608, 0x21);
    tx_data[0] = 0x10;
    tx_data[1] = 0x16;
    tx_data[2] = 0x02;
    ilitek_i2c_write_and_read(tx_data, 3, 5, tx_data, 0);
    tx_data[0] = 0x20;
    ilitek_i2c_write_and_read(tx_data, 1, 5, tx_data, 0);
}

static void ilitek_camaro_isp_burst_write_eflash_end(void)
{
    u8 tx_data[1] = { 0 };
    tx_data[0] = 0x21;
    ilitek_i2c_write_and_read(tx_data, 1, 5, tx_data, 0);
    tx_data[0] = 0x7E;
    ilitek_i2c_write_and_read(tx_data, 1, 5, tx_data, 0);
    // Clear burst mode
    ilitek_reg_set_lowbyte_value(0x1608, 0x20);
}
static void ilitek_camaro_program_emem(emem_type type)
{
    u32 i, j;
    u32 page_num = 0, length = 0, index = 0, word_num = 0;
    u32 retry_time = 0;
    u8 reg_data = 0;
    u8 sz_first_data[CAMARO_EMEM_SIZE_BYTES_ONE_WORD] = { 0 };
    u8 sz_buffer_data[CAMARO_EMEM_SIZE_BYTES_PER_ONE_PAGE] = { 0 };
    uint8_t * ptr;
    
    tp_log_debug(PRINT_FIRMWARE_UPGRATE,"     eEmemType = %d    \n", type);
    ilitek_reset(HARDWARE_RESET_DELAY_TIME);
    ilitek_enter_ice_mode();
    ilitek_dbus_i2c_response_ack();
    tp_log_debug(PRINT_FIRMWARE_UPGRATE,"Program start\n");
    ilitek_camaro_access_eflash_init();
    // Stop mcu
    ilitek_reg_set_lowbyte_value(0x0FE6, 0x01);
    // Set PROGRAM erase password
    ilitek_reg_set_16bit_value(0x1618, 0x5AA5);
    ilitek_camaro_unset_protect_bit();
    
    if (type == EMEM_MAIN) {	// Program main block
        tp_log_debug(PRINT_FIRMWARE_UPGRATE,"Program main block\n");
        /* UPDATE FIRMWARE WITH 128 BYTE EACH TIME */
        page_num = (CAMARO_FIRMWARE_MAIN_BLOCK_SIZE * 1024) / CAMARO_EMEM_SIZE_BYTES_PER_ONE_PAGE;	// 128*1024/128=1024
        index = 0;
        ptr = target_fw_update_ptr;
        
        for (i = 0; i < page_num; i++) {
            if (i == 0) {
                // Read first data 4 bytes
                length = CAMARO_EMEM_SIZE_BYTES_ONE_WORD;
                sz_first_data[0] = *(ptr + 0);
                sz_first_data[1] = *(ptr + 1);
                sz_first_data[2] = *(ptr + 2);
                sz_first_data[3] = *(ptr + 3);
                ilitek_camaro_isp_burst_write_eflash_start
                (index, &sz_first_data[0], page_num,
                 EMEM_MAIN);
                index += length;
                /* UPDATE FIRMWARE WITH 128 BYTE EACH TIME */
                length = CAMARO_EMEM_SIZE_BYTES_PER_ONE_PAGE - CAMARO_EMEM_SIZE_BYTES_ONE_WORD;	// 124 = 128 - 4
                
                for (j = 0; j < length; j++) {
                    sz_buffer_data[j] = *(ptr + 4 + j);
                }
            } else {
                /* UPDATE FIRMWARE WITH 128 BYTE EACH TIME */
                length = CAMARO_EMEM_SIZE_BYTES_PER_ONE_PAGE;	// 128
                ptr =target_fw_update_ptr + (i / 8) * 1024 +
                    CAMARO_EMEM_SIZE_BYTES_PER_ONE_PAGE * (i - (8 * (i /  8)));
                              
                for (j = 0; j < length; j++) {
                    /* UPDATE FIRMWARE WITH 128 BYTE EACH TIME */
                    sz_buffer_data[j] = *(ptr + j);
                }
            }
            
            ilitek_camaro_isp_burst_write_eflash_do_write
            (&sz_buffer_data[0], length);
            udelay(2000);	// delay about 2ms
            index += length;
        }
        
        ilitek_camaro_isp_burst_write_eflash_end();
        // Set write done
        ilitek_reg_set_16bit_value_on(0x1606, BIT(2));
        // Check RBB
        reg_data = ilitek_reg_get_lowbyte_value(0x160E);
        retry_time = 0;
        
        while ((reg_data & BIT(3)) != BIT(3)) {
            mdelay(10);
            reg_data = ilitek_reg_get_lowbyte_value(0x160E);
            
            if (retry_time++ > 100) {
                tp_log_err
                ("main block can't wait write to done.\n");
                goto ProgramEnd;
            }
        }
    } else if (type == EMEM_INFO) {	// Program info block
      tp_log_debug(PRINT_FIRMWARE_UPGRATE,"Program info block\n");
        page_num = (CAMARO_FIRMWARE_INFO_BLOCK_SIZE * 1024) / CAMARO_EMEM_SIZE_BYTES_PER_ONE_PAGE;	// 2*1024/128=16
        index = 0;
        index += CAMARO_EMEM_SIZE_BYTES_PER_ONE_PAGE;	//128
        
        // Skip firt page(page 0) & Update page 1~14 by isp burst write mode
        for (i = 1; i < (page_num - 1); i++) {	// Skip the first 128 byte and the last 128 byte of info block
            if (i == 1) {
                // Read first data 4 bytes
                length = CAMARO_EMEM_SIZE_BYTES_ONE_WORD;
                ptr =
                    target_fw_update_ptr +
                    CAMARO_FIRMWARE_MAIN_BLOCK_SIZE * 1024 +
                    CAMARO_EMEM_SIZE_BYTES_PER_ONE_PAGE;
                sz_first_data[0] = *(ptr + 0);
                sz_first_data[1] = *(ptr + 1);
                sz_first_data[2] = *(ptr + 2);
                sz_first_data[3] = *(ptr + 3);
                ilitek_camaro_isp_burst_write_eflash_start
                (index, &sz_first_data[0], page_num - 1,
                 EMEM_INFO);
                index += length;
                length = CAMARO_EMEM_SIZE_BYTES_PER_ONE_PAGE - CAMARO_EMEM_SIZE_BYTES_ONE_WORD;	// 124 = 128 - 4
                
                for (j = 0; j < length; j++) {
                    sz_buffer_data[j] = *(ptr + 4 + j);
                }
            } else {
                length = CAMARO_EMEM_SIZE_BYTES_PER_ONE_PAGE;	//128
                
                // UPDATE FIRMWARE WITH 128 BYTE EACH TIME
                if (i < 8)	// 1 < i < 8
                    ptr = target_fw_update_ptr +
                    CAMARO_FIRMWARE_MAIN_BLOCK_SIZE *1024 +
                    CAMARO_EMEM_SIZE_BYTES_PER_ONE_PAGE * i;
                else	// i >= 8
                    ptr =target_fw_update_ptr +
                        CAMARO_FIRMWARE_MAIN_BLOCK_SIZE * 1024 +
                        CAMARO_EMEM_SIZE_BYTES_PER_ONE_PAGE* (i - 8);
                        
                for (j = 0; j < length; j++) {
                    sz_buffer_data[j] = *(ptr + j);
                }
            }
            
            ilitek_camaro_isp_burst_write_eflash_do_write
            (&sz_buffer_data[0], length);
            udelay(2000);	// delay about 2ms
            index += length;
        }
        
        ilitek_camaro_isp_burst_write_eflash_end();
        // Set write done
        ilitek_reg_set_16bit_value_on(0x1606, BIT(2));
        // Check RBB
        reg_data = ilitek_reg_get_lowbyte_value(0x160E);
        retry_time = 0;
        
        while ((reg_data & BIT(3)) != BIT(3)) {
            mdelay(10);
            reg_data = ilitek_reg_get_lowbyte_value(0x160E);
            
            if (retry_time++ > 100) {
                tp_log_debug(PRINT_FIRMWARE_UPGRATE,
                "Info block page 1~14 can't wait write to done.\n");
                goto ProgramEnd;
            }
        }
        
        ilitek_reg_set_16bit_value_off(0x1EBE, BIT(15));
        // Update page 15 by write mode
        index = 15 * CAMARO_EMEM_SIZE_BYTES_PER_ONE_PAGE;
        word_num = CAMARO_EMEM_SIZE_BYTES_PER_ONE_PAGE / CAMARO_EMEM_SIZE_BYTES_ONE_WORD;	// 128/4=32
        length = CAMARO_EMEM_SIZE_BYTES_ONE_WORD;
        
        for (i = 0; i < word_num; i++) {
            if (i == 0) {
                ptr =
                    target_fw_update_ptr +
                    (CAMARO_FIRMWARE_MAIN_BLOCK_SIZE + 1) * 1024 +
                    (7 * CAMARO_EMEM_SIZE_BYTES_PER_ONE_PAGE);
                
                sz_first_data[0] = *(ptr + 0);
                sz_first_data[1] = *(ptr + 1);
                sz_first_data[2] = *(ptr + 2);
                sz_first_data[3] = *(ptr + 3);
                ilitek_camaro_write_eflash_start(index, &sz_first_data [0], EMEM_INFO);
                
            } else {
                for (j = 0; j < length; j++) {
                    sz_first_data[j] =*(ptr + (4 * i) + j);
                }
                
                ilitek_camaro_write_eflash_do_write(index,  &sz_first_data [0]);
            }
            
            udelay(2000);	// delay about 2ms
            index += length;
        }
        
        ilitek_camaro_write_eflash_end();
        // Set write done
        ilitek_reg_set_16bit_value_on(0x1606, BIT(2));
        // Check RBB
        reg_data = ilitek_reg_get_lowbyte_value(0x160E);
        retry_time = 0;
        
        while ((reg_data & BIT(3)) != BIT(3)) {
            mdelay(10);
            reg_data = ilitek_reg_get_lowbyte_value(0x160E);
            
            if (retry_time++ > 100) {
                tp_log_debug(PRINT_FIRMWARE_UPGRATE,
                "Info block page 15 can't wait write to done.\n");
                goto ProgramEnd;
            }
        }
    }
    
ProgramEnd:
    ilitek_camaro_set_protect_bit();
    // Clear PROGRAM erase password
    ilitek_reg_set_16bit_value(0x1618, 0xA55A);
    tp_log_debug(PRINT_FIRMWARE_UPGRATE,"Program end\n");
    ilitek_exit_ice_mode();
}

static s32 ilitek_camaro_update_firmware(emem_type type)
{
    u32 crc_main = 0, crc_main_hardware = 0, crc_main_eflash = 0;
    u32 crc_info = 0, crc_info_hardware = 0, crc_info_eflash = 0;
    u32 ret = 0;
    /*check target fw is available */
    ret = ilitek_check_target_firmware_available();
    
    if (ret < 0)
        return ret;
        
    if (type == EMEM_MAIN || type == EMEM_ALL) {
        ilitek_camaro_erase_emem(EMEM_MAIN);
    }
    
    if (type == EMEM_INFO || type == EMEM_ALL) {
        ilitek_camaro_erase_emem(EMEM_INFO);
    }
    
    tp_log_debug(PRINT_FIRMWARE_UPGRATE,"erase OK\n");
    
    /* Program */
    if (type == EMEM_MAIN || type == EMEM_ALL) {
        ilitek_camaro_program_emem(EMEM_MAIN);
    }
    
    if (type == EMEM_INFO || type == EMEM_ALL) {
        ilitek_camaro_program_emem(EMEM_INFO);
    }
    
    tp_log_debug(PRINT_FIRMWARE_UPGRATE,"program OK\n");
    
    /* Read main block CRC & info block CRC from TP */
    if (type == EMEM_MAIN || type == EMEM_ALL) {
        crc_main =
            ilitek_camaro_calculate_target_fw_crc(target_fw_update_ptr, 0,
                                                CAMARO_FIRMWARE_MAIN_BLOCK_SIZE
                                                * 1024 -
                                                CAMARO_EMEM_SIZE_BYTES_ONE_WORD);
        crc_main_hardware =
            ilitek_camaro_get_firmware_crc_by_hardware(EMEM_MAIN);
        crc_main_eflash =
            ilitek_camaro_retrieve_firmware_crc_from_eflash(EMEM_MAIN);
    }
    
    if (type == EMEM_INFO || type == EMEM_ALL) {
        crc_info =
            ilitek_camaro_calculate_target_fw_crc(target_fw_update_ptr,
                                                CAMARO_FIRMWARE_MAIN_BLOCK_SIZE
                                                * 1024 +
                                                CAMARO_EMEM_SIZE_BYTES_PER_ONE_PAGE,
                                                CAMARO_FIRMWARE_INFO_BLOCK_SIZE
                                                * 1024 -
                                                CAMARO_EMEM_SIZE_BYTES_PER_ONE_PAGE
                                                -
                                                CAMARO_EMEM_SIZE_BYTES_ONE_WORD);
        crc_info_hardware =
            ilitek_camaro_get_firmware_crc_by_hardware(EMEM_INFO);
        crc_info_eflash =
            ilitek_camaro_retrieve_firmware_crc_from_eflash(EMEM_INFO);
    }
    
       tp_log_debug(PRINT_FIRMWARE_UPGRATE,"crc_main=0x%x, crc_info=0x%x, crc_main_hardware=0x%x, crc_info_hardware=0x%x, crc_main_eflash=0x%x, crc_info_eflash=0x%x\n",
     crc_main, crc_info, crc_main_hardware, crc_info_hardware,
     crc_main_eflash, crc_info_eflash);
    ilitek_reset(HARDWARE_RESET_DELAY_TIME);
    mdelay(300);
    
    if (type == EMEM_MAIN || type == EMEM_ALL) {
        if ((crc_main_hardware != crc_main)
            || (crc_main_eflash != crc_main)) {
            tp_log_err("Update FAILED\n");
            return -1;
        }
    }
    
    if (type == EMEM_INFO || type == EMEM_ALL) {
        if ((crc_info_hardware != crc_info)
            || (crc_info_eflash != crc_info)) {
            tp_log_err("Uupdate FAILED\n");
            return -1;
        }
    }
    
    tp_log_info("Update SUCCESS\n");
    return ret;
}

int ilitek_upgrade_firmware(uint8_t force_update_flag)
{
    int ret = 0;
    /*********check if need to upgrade*********/
#ifdef ILITEK_UPDATE_FW
    
    if (force_update_flag == 0) {
        target_fw_update_ptr = &msg28xx_xxxx_update_bin[0][0];
        ret = ilitek_check_upgrade_firmware_by_sw_id();
        
        if (ret < 0)
            return ret;
    }
    
#endif	 
    /************** fw upgrade**************/
    ret = ilitek_camaro_update_firmware(EMEM_MAIN);
    return ret;
}


