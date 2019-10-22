/*
 * Copyright (C) 2006-2017 ILITEK TECHNOLOGY CORP.
 *
 * Description: ILITEK I2C touchscreen driver for linux platform.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Dicky Chiang
 * Maintain: Luca Hsu, Tigers Huang
 */

#include "mp_common.h"


int run_short = 0;

static int new_clear_switch_status(void)
{
    int regData = 0;
    int timeout = 280;
    int t = 0;
    mp_info(PRINT_MPTEST, "*** %s() ***\n", __func__);
    
    do {
        ilitek_reg_set_16bit_value(0x1402, 0xFFFF);
        regData = ilitek_reg_get_16bit_value_by_address_mode(0x1401, ADDRESS_MODE_16BIT);
        mdelay(20);
        t++;
        
        if(t > timeout)
            return -1;
            
        mp_info(PRINT_MPTEST, "TIMER = %d, regData = 0x%04x\n", t, regData);
    } while(regData != 0xFFFF);
    
    return 0;
}

static int new_polling_data_ready(void)
{
    int ret = 0;
    int timer = 500;
    u16 RegData = 0;
    mp_info(PRINT_MPTEST, "*** %s() ***\n", __func__);
    
    while(RegData != 0x7744) {
        RegData = ilitek_reg_get_16bit_value_by_address_mode(0x1401, ADDRESS_MODE_16BIT);
//        mp_info(PRINT_MPTEST, "TIMER = %d, RegData = 0x%04x\n", timer, RegData);
        mdelay(20);
        timer--;
        
        if(timer < 0)
            break;
    }
    
    if(timer <= 0)
        ret = -1;
        
    return ret;
}

void ilitek_reg_get_x_bit_write_4byte_value(u16 addr, u8 * p_rx_data, u16 length, u16 max_i2c_length_limit)
{
    u16 read_addr = addr;
    u16 read_size = 0;
    u16 left = length;
    u16 offset = 0;
    u8  data[4] = {0};
    data[0] = 0x10;
    mutex_lock(&ilitek_data->ilitek_mutex);
    
    while(left > 0) {
        if(left >= max_i2c_length_limit) {
            read_size = max_i2c_length_limit;
            tp_log_debug(PRINT_NONE,"*** RegGetXBitValue# Length >= I2cMax   read_addr=%x, read_size=%d ***\n", read_addr, read_size);
            data[2] = (read_addr >> 8) & 0xFF;
            data[3] = read_addr & 0xFF;
            ilitek_i2c_write_and_read(data, 4, 5, &p_rx_data[offset], read_size);
            read_addr = read_addr + read_size;    //set next read address
            left = left - read_size;
            offset = offset + read_size;
            tp_log_debug(PRINT_NONE,"*** RegGetXBitValue# Length >= I2cMax   left=%d, offset=%d ***\n", left, offset);
        } else {
            read_size = left;
            tp_log_debug(PRINT_NONE,"*** RegGetXBitValue# Length < I2cMax   read_addr=%x, read_size=%d ***\n", read_addr, read_size);
            data[2] = (read_addr >> 8) & 0xFF;
            data[3] = read_addr & 0xFF;
            ilitek_i2c_write_and_read(data, 4, 5, &p_rx_data[offset], read_size);
            left = 0;
            offset = offset + read_size;
            tp_log_debug(PRINT_NONE,"*** RegGetXBitValue# Length < I2cMax   left=%d, offset=%d ***\n", left, offset);
        }
    }
    
    mutex_unlock(&ilitek_data->ilitek_mutex);
}

static int new_get_raw_data(s16 *pRawData, u16 count)
{
    int i, j, offset;
    u8 *pShotOriData = NULL;
    s32 *pShotDataAll = NULL;
    u16 fout_base_addr = 0x0, RegData = 0;
    u16 touch_info_length = 35;
    u16 data_length = count;
    mp_info(PRINT_MPTEST,  "*** %s() data_length = %d ***\n", __func__, data_length);
    
    if(run_short)
        touch_info_length = 0;
        
    /* one byte original data */
    pShotOriData = kcalloc(data_length * 2 * 2 + touch_info_length, sizeof(u8), GFP_KERNEL);
    
    if(ERR_ALLOC_MEM(pShotOriData)) {
        mp_err(PRINT_MPTEST,"Failed to allocate pShotOriData mem\n");
        return -1;
    }
    
    /* two bytes combined by above */
    pShotDataAll = kcalloc(data_length * 2, sizeof(s32), GFP_KERNEL);
    
    if(ERR_ALLOC_MEM(pShotDataAll)) {
        mp_err(PRINT_MPTEST,"Failed to allocate pShotDataAll mem\n");
        return -1;
    }
    
    /* Read dump time coeff */
    if(run_short)
        mp_test_data->short_dump1 = ilitek_reg_get_16bit_value_by_address_mode(0x1018, ADDRESS_MODE_16BIT);
        
    /* Read DQ base */
    RegData = ilitek_reg_get_16bit_value_by_address_mode(p_mp_func->fout_data_addr, ADDRESS_MODE_16BIT);
    fout_base_addr = (int)(RegData << 2);
    mp_info(PRINT_MPTEST, "fout_base_addr = 0x%x , data_length*2 = %d\n", fout_base_addr, data_length * 2);
    
    if(fout_base_addr <= 0) {
        mp_err(PRINT_MPTEST,"Failed to get fout_base_addr\n");
        return -1;
    }
    
    stop_mcu();
    //RegMask16BitValue(0x1066, 0x01, 0x01,ADDRESS_MODE_16BIT);
    //ilitek_reg_set_lowbyte_value(0x0F50, 0x00); //i2c access DQMEM
    
    /*
    * TODO: don't know why this addr gotten from IC is dfferent with PC tool.
    * so I just set a fixed address that is the same as the one.
    */
    
    /* Read data segmentally */
    for(offset = 0; offset < data_length * 2 + touch_info_length; offset += CAMARO_MAX_I2C_TRANSACTION_LENGTH_LIMIT) {
        ilitek_camaro_db_bus_read_dq_mem_start();
        
        if(offset == 0) {
            ilitek_reg_get_x_bit_write_4byte_value(fout_base_addr + offset, pShotOriData + offset, CAMARO_MAX_I2C_TRANSACTION_LENGTH_LIMIT, CAMARO_MAX_I2C_TRANSACTION_LENGTH_LIMIT);
        } else if(offset + CAMARO_MAX_I2C_TRANSACTION_LENGTH_LIMIT < data_length * 2 + touch_info_length) {
            ilitek_reg_get_x_bit_write_4byte_value(fout_base_addr + offset, pShotOriData + offset, CAMARO_MAX_I2C_TRANSACTION_LENGTH_LIMIT, CAMARO_MAX_I2C_TRANSACTION_LENGTH_LIMIT);
        } else {
            ilitek_reg_get_x_bit_write_4byte_value(fout_base_addr + offset, pShotOriData + offset, data_length * 2 + touch_info_length - offset, CAMARO_MAX_I2C_TRANSACTION_LENGTH_LIMIT);
        }
        
        ilitek_camaro_db_bus_read_dq_mem_end();
    }
    
      if (print_debug_level & PRINT_MPTEST){
        
            for(i = 0; i < data_length * 2 + touch_info_length; i++) {
                printk(" %02x ", pShotOriData[i]);
                
                if(i != 0 && (i % 16 == 0))
                    printk("\n");
            }
            
           printk(" \n\n ");
      }
    
    if(run_short) {
        for (j = 0; j < data_length; j++) {
            pShotDataAll[j] = (pShotOriData[2 * j] | pShotOriData[2 * j + 1 ] << 8);
        }
    } else {
        for (j = 0; j < data_length; j++) {
            pShotDataAll[j] = (pShotOriData[2 * j + touch_info_length + 1] | pShotOriData[2 * j + touch_info_length ] << 8);
        }
    }
    
      if (print_debug_level & PRINT_MPTEST){
    
            for(i = 0; i < data_length; i++) {
                 printk(" %04x ", pShotDataAll[i]);
                
                if(i != 0 && (i % 16 == 0))
                     printk("\n");
            }
        
            printk(" \n\n ");
      }
    
    for (i = 0; i < data_length; i++) {
        pRawData[i] = pShotDataAll[i];
    }
    
    mp_kfree(pShotOriData);
    mp_kfree(pShotDataAll);
    start_mcu();
    return 0;
}

static int new_get_deltac(int *delt)
{
    int i;
    s16 *pRawData = NULL, count = 0;
    mp_info(PRINT_MPTEST, "*** %s ***\n", __func__);
    pRawData = kcalloc(MAX_CHANNEL_SEN * MAX_CHANNEL_DRV * 2, sizeof(s16), GFP_KERNEL);
    
    //pRawData = kcalloc(2048, sizeof(s16), GFP_KERNEL);
    if(ERR_ALLOC_MEM(pRawData)) {
        mp_err(PRINT_MPTEST,"Failed to allocate pRawData mem \n");
        return -1;
    }
    
    if(run_short) {
        count = MAX_SUBFRAMES_30XX * MAX_AFENUMs_30XX;
        
        if(new_get_raw_data(pRawData, count) < 0) {
            mp_err(PRINT_MPTEST,"*** Get DeltaC failed! ***\n");
            return -1;
        }
    } else {
        count = mp_test_data->sensor_info.num_sen * mp_test_data->sensor_info.num_drv + mp_test_data->sensor_info.num_sen + mp_test_data->sensor_info.num_drv;
        
        if(new_get_raw_data(pRawData, count) < 0) {
            mp_err(PRINT_MPTEST,"*** Get DeltaC failed! ***\n");
            return -1;
        }
    }
    
    for (i = 0; i < count; i++)
        delt[i] = pRawData[i];
        
    if (print_debug_level & PRINT_MPTEST){
            
        if(run_short) {
            for(i = 0; i < count; i++) {
                 printk(" %05d ", delt[i]);
                
                if(i != 0 && (i % 10) == 0)
                     printk(" \n ");
            }
            
            printk(" \n ");
        } else {
            for(i = 0; i < count; i++) {
                 printk(" %05d ", delt[i]);
                
                if(i != 0 && (i % mp_test_data->sensor_info.num_drv) == 0)
                     printk(" \n ");
            }
            
             printk(" \n ");
        }
        
    }
    mp_kfree(pRawData);
    return 0;
}

static int new_send_test_cmd(u16 fmode, int mode)
{
    int ret = 0;
    u8 cmd[8] = {0};
    u8 freq, freq1, csub, cfb, chargeP;
    u8 post_idle, sample_hi, sample_lo;
    u16 chargeT, dumpT;
    mp_info(PRINT_MPTEST, "*** Mode = %d ***\n", mode);
    freq = mp_test_data->open_fixed_carrier;
    freq1 = mp_test_data->open_fixed_carrier1;
    csub = mp_test_data->open_test_csub;
    cfb = mp_test_data->open_test_cfb;
    chargeP = (mp_test_data->open_test_chargepump ? 0x02 : 0x00);
    chargeT = mp_test_data->open_charge;
    dumpT = mp_test_data->open_dump;
    post_idle = mp_test_data->post_idle;
    sample_lo = mp_test_data->self_sample_hi;
    sample_hi = mp_test_data->self_sample_lo;
    cmd[0] = 0xF1; /* Header */
    cmd[1] = mode;
    
    switch(cmd[1]) {
    case TYPE_SHORT:
        cmd[2] = 0;
        cmd[3] = 0;
        cmd[4] = 0;
        cmd[5] = 0;
        cmd[6] = 0;
        cmd[7] = 0;
        break;
        
    case TYPE_SELF:
        if (chargeT == 0 || dumpT == 0) {
            chargeT = 0x18;
            dumpT = 0x16;
        }
        
        cmd[2] = chargeT;
        cmd[3] = dumpT;
        cmd[4] = post_idle;
        cmd[5] = sample_hi;
        cmd[6] = sample_lo;
        cmd[7] = 0x0;
        break;
        
    case TYPE_OPEN:
        if(fmode == MUTUAL_SINE) {
            cmd[2] = (0x01 | chargeP);
            /* Open test by each frequency */
            cmd[3] = freq;
            cmd[4] = freq1;
            cmd[5] = 0x00;
        } else {
            cmd[2] = (0x00 | chargeP);
            
            if (chargeT == 0 || dumpT == 0) {
                chargeT = 0x18;
                dumpT = 0x16;
            }
            
            /* Switch cap mode */
            cmd[3] = chargeT;
            cmd[4] = dumpT;
            cmd[5] = post_idle; //postidle
        }
        
        //cmd[5] = csub;
        // if(cfb == 0)
        //     cmd[6] = 2; //30P
        // else if(cfb == 1)
        //     cmd[6] = 7; //25P
        // else if(cfb == 2)
        //     cmd[6] = 2; //30P
        // else if(cfb == 3)
        //     cmd[6] = 6; //35P
        // else if(cfb == 4)
        //     cmd[6] = 5; //45P
        // else if(cfb == 5)
        //     cmd[6] = 0; //50P
        cmd[6] = 0x0;   //Csub, default : 0
        cmd[7] = 0x0;
        break;
        
    default:
        mp_err(PRINT_MPTEST,"Mode is invalid\n");
        ret = -1;
        break;
    }
    
    if (print_debug_level & PRINT_MPTEST)
    {
        int i;
        
        for(i = 0; i < ARRAY_SIZE(cmd); i++)
             printk( "*** Test CMD[%d] = %d  ***\n", i, cmd[i]);
    }

    /* Writting commands via SMBus */
    ilitek_data->client->addr = SLAVE_I2C_ID_DWI2C;
    ilitek_i2c_write_and_read(cmd, ARRAY_SIZE(cmd), 5, cmd, 0);
    ilitek_data->client->addr = SLAVE_I2C_ID_DBBUS;
    mdelay(5);
    
    if(new_polling_data_ready() < 0) {
        mp_err(PRINT_MPTEST,"New Flow polling data timout !!\n");
        return -1;
    }
    
    /* Clear MP mode */
    ilitek_reg_set_16bit_value(0x1402, 0xFFFF);
    return ret;
}

static int new_flow_start_test(void)
{
    int ret = 0, deltac_size = 0;
    u16 fmode = MUTUAL_MODE;
    int *deltac_data = NULL;
    
    switch (mp_test_data->open_mode) {
    case 0:
        fmode = MUTUAL_MODE;
        break;
        
    case 1:
    case 2:
        fmode = MUTUAL_SINE;
        break;
    }
    
    if(run_short) {
        ret = new_send_test_cmd(fmode, TYPE_SHORT);
        
        if(ret < 0) {
            mp_err(PRINT_MPTEST,"Send cmd busy\n");
            goto out;
        }
    } else {
        ret = new_send_test_cmd(fmode, TYPE_SELF);
        
        if(ret < 0) {
            mp_err(PRINT_MPTEST,"Send cmd busy\n");
            goto out;
        }
        
        ret = new_send_test_cmd(fmode, TYPE_OPEN);
        
        if(ret < 0) {
            mp_err(PRINT_MPTEST,"Send cmd busy\n");
            goto out;
        }
    }
    
    deltac_data = kcalloc(MAX_MUTUAL_NUM, sizeof(int), GFP_KERNEL);
    
    if(ERR_ALLOC_MEM(deltac_data)) {
        mp_err(PRINT_MPTEST,"Failed to allocate deltac_data mem \n");
        ret = -1;
        goto out;
    }
    
    deltac_size = MAX_MUTUAL_NUM / sizeof(int);
    /* Get DeltaC */
    ret = new_get_deltac(deltac_data);
    
    if(ret == 0) {
        /* Judge values */
        if(run_short) {
            mp_test_result->short_result = ITO_TEST_OK;
            ret = p_mp_func->short_judge(deltac_data);
        } else {
            mp_test_result->open_result = ITO_TEST_OK;
            ret = p_mp_func->open_judge(deltac_data, deltac_size);
        }
        
        mp_info(PRINT_MPTEST, "*** New Judge return value = %d ***\n", ret);
    }
    
out:
    mp_kfree(deltac_data);
    return ret;
}

int ilitek_mp_new_flow_main(int item)
{
    int ret = 0;
    uint8_t data[5] = {0x53, 0x45, 0x52, 0x44, 0x42};
    run_short = item;
    mp_info(PRINT_MPTEST, "*** Running %s Test ***\n", (run_short ? "Short" : "Open"));
    ilitek_reset(HARDWARE_RESET_DELAY_TIME);
    mdelay(10);
    ilitek_data->client->addr = SLAVE_I2C_ID_DBBUS;
    /*DbBus Enter Seria lD ebug Mode*/
    ilitek_i2c_write_and_read(data, 5, 5, data, 0);
    /*DBbus stop MCU*/
    data[0] = 0x37;
    ilitek_i2c_write_and_read(data, 1, 5, data, 0);
    /*MCU force wait*/
    data[0] = 0x61;
    ilitek_i2c_write_and_read(data, 1, 5, data, 0);
    /*DBbus stop MCU*/
    data[0] = 0x37;
    ilitek_i2c_write_and_read(data, 1, 5, data, 0);
    /*DBbus I2C use bus*/
    data[0] = 0x35;
    ilitek_i2c_write_and_read(data, 1, 5, data, 0);
    /*DBbus I2C reshape*/
    data[0] = 0x71;
    ilitek_i2c_write_and_read(data, 1, 5, data, 0);
    /*DBbus access RIU*/
    data[0] = 0x80;
    ilitek_i2c_write_and_read(data, 1, 5, data, 0);
    data[0] = 0x82;
    ilitek_i2c_write_and_read(data, 1, 5, data, 0);
    data[0] = 0x84;
    ilitek_i2c_write_and_read(data, 1, 5, data, 0);
    ilitek_reg_set_16bit_value_off(0x1E08, BIT(15));
    ilitek_dbus_i2c_response_ack();
    ret = p_mp_func->enter_mp_mode();
    
    if(ret < 0) {
        mp_err(PRINT_MPTEST,"Failed to enter MP mode\n");
        goto out;
    }
    
    ret = p_mp_func->check_mp_switch();
    
    if(ret < 0) {
        mp_err(PRINT_MPTEST,"*** Switch FW Mode Busy, return fail ***\n");
        goto out;
    }
    
    if(new_clear_switch_status() < 0) {
        mp_err(PRINT_MPTEST,"*** Clear Switch status fail ***\n");
        ret = -1;
        goto out;
    }
    
    ret = new_flow_start_test();
out:
    /* Exit DBbus */
    ilitek_reg_set_16bit_value_on(0x1E04, BIT(15));
    data[0] = 0x84;
    ilitek_i2c_write_and_read(data, 1, 5, data, 0);
    ilitek_exit_ice_mode();
    ilitek_reset(HARDWARE_RESET_DELAY_TIME);
    mdelay(10);
    return ret;
}
