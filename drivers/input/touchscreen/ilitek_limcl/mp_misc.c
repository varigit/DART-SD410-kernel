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


extern void ilitek_reg_set_16bit_value_on(u16 addr, u16 bit);
extern void ilitek_reg_set_16bit_value_off(u16 addr, u16 bit) ;
extern u8 ilitek_reg_get_lowbyte_value(u16 addr);
extern void ilitek_reg_set_lowbyte_value(uint16_t addr, uint8_t data);
extern u16 ilitek_reg_get_16bit_value(uint16_t addr);
extern u16 ilitek_reg_set_16bit_value(uint16_t addr, uint16_t data);



int ilitek_camaro_check_mp_switch(void)
{
    u16 nRegData = 0;
    int nTimeOut = 280;
    int nT = 0;
    mp_info(PRINT_MPTEST, "*** %s() ***\n", __func__);
    
    do {
        nRegData = ilitek_reg_get_16bit_value_by_address_mode(0x1401, ADDRESS_MODE_16BIT);
        mdelay(15);
        nT++;
        
        if (nT > nTimeOut) {
            return -1;
        }
        
        mp_info(PRINT_MPTEST, "nT = %d, nRegData = 0x%04x\n", nT, nRegData);
    } while (nRegData != 0x7447);
    
    return 0;
}


int ilitek_camaro_enter_mp_mode(void)
{
    mp_info(PRINT_MPTEST, "*** %s() ***\n", __func__);
    stop_mcu();
    mdelay(100);
    ilitek_reg_set_16bit_value(0X3C60, 0xAA55);    // disable watch dog
    // ilitek_reg_set_16bit_value(0X3D08, 0xFFFF);   // clear interrupt status
    // ilitek_reg_set_16bit_value(0X3D18, 0xFFFF);   // clear interrupt status
    ilitek_reg_set_16bit_value(0x1402, 0x7474);   // enter mp mode
    ilitek_reg_set_16bit_value(0x1E06, 0x0000);
    ilitek_reg_set_16bit_value(0x1E06, 0x0001);
    start_mcu();
    mdelay(300);
    return 0;
}

void ilitek_camaro_db_bus_set_default(void)
{
    u8 nParCmdSelUseCfg  = 0x7F;
    u8 nParCmdAdByteEn1  = 0x51;
    u8 nParCmdSetSelB0   = 0x81;
    u8 nParCmdSetSelB1   = 0x83;
    u8 nParCmdNSetSelB2  = 0x84;
    u8 nParCmdIicUse     = 0x35;
    u8 nParCmdNSelUseCfg = 0x7E;
    u8 nParCmdNIicUse    = 0x34;
    mp_info(PRINT_MPTEST, "*** %s() ***\n", __func__);
    ilitek_reg_set_lowbyte_value(0, 0);
    ilitek_i2c_write_and_read(&nParCmdSelUseCfg, 1, 5, &nParCmdSelUseCfg, 0);
    ilitek_i2c_write_and_read(&nParCmdAdByteEn1, 1, 5, &nParCmdAdByteEn1, 0);
    ilitek_i2c_write_and_read(&nParCmdSetSelB0, 1, 5, &nParCmdSetSelB0, 0);
    ilitek_i2c_write_and_read(&nParCmdSetSelB1, 1, 5, &nParCmdSetSelB1, 0);
    ilitek_i2c_write_and_read(&nParCmdNSetSelB2, 1, 5, &nParCmdNSetSelB2, 0);
    ilitek_i2c_write_and_read(&nParCmdIicUse, 1, 5, &nParCmdIicUse, 0);
    ilitek_i2c_write_and_read(&nParCmdNSelUseCfg, 1, 5, &nParCmdNSelUseCfg, 0);
    ilitek_i2c_write_and_read(&nParCmdNIicUse, 1, 5, &nParCmdNIicUse, 0);
}

void ilitek_camaro_db_bus_read_dq_mem_start(void)
{
    u8 nParCmdSelUseCfg = 0x7F;
    //u8 nParCmdAdByteEn0 = 0x50;
    u8 nParCmdAdByteEn2 = 0x52;
    //u8 nParCmdAdByteEn1 = 0x51;
    //u8 nParCmdDaByteEn0 = 0x54;
    u8 nParCmdUSetSelB0 = 0x80;
    u8 nParCmdUSetSelB1 = 0x82;
    u8 nParCmdSetSelB2  = 0x85;
    u8 nParCmdIicUse    = 0x35;
    //u8 nParCmdWr        = 0x10;
    mp_info(PRINT_MPTEST, "*** %s() ***\n", __func__);
    ilitek_reg_set_16bit_value_off(0x0F50, BIT(1));
    ilitek_i2c_write_and_read(&nParCmdSelUseCfg, 1, 5, &nParCmdSelUseCfg, 0);
    ilitek_i2c_write_and_read(&nParCmdAdByteEn2, 1, 5, &nParCmdAdByteEn2, 0);
    ilitek_i2c_write_and_read(&nParCmdUSetSelB0, 1, 5, &nParCmdUSetSelB0, 0);
    ilitek_i2c_write_and_read(&nParCmdUSetSelB1, 1, 5, &nParCmdUSetSelB1, 0);
    ilitek_i2c_write_and_read(&nParCmdSetSelB2, 1, 5, &nParCmdSetSelB2, 0);
    ilitek_i2c_write_and_read(&nParCmdIicUse, 1, 5, &nParCmdIicUse, 0);
}

void ilitek_camaro_db_bus_read_dq_mem_end(void)
{
    /*u8 nParCmdNSelUseCfg = 0x7E;
    u8 nParCmdNSelUseCfg = 0x7F;*/
    mp_info(PRINT_MPTEST, "*** %s() ***\n", __func__);
    ilitek_camaro_db_bus_set_default();
    //IicWriteData(SLAVE_I2C_ID_DBBUS, &nParCmdNSelUseCfg, 1);
    ilitek_reg_set_16bit_value_on(0x0F50, BIT(1));
}


void start_mcu(void)
{
    mp_info(PRINT_MPTEST, "*** %s *** \n", __func__);
    ilitek_reg_set_lowbyte_value(0x0FE6, 0x00); //bank:mheg5, addr:h0073
}

void stop_mcu(void)
{
    mp_info(PRINT_MPTEST, "*** %s *** \n", __func__);
    ilitek_reg_set_lowbyte_value(0x0FE6, 0x01); //bank:mheg5, addr:h0073
}

void debug_show_array2(void *pBuf, u16 nLen, int nDataType, int nCarry, int nChangeLine)
{
    int i;
    char szStrTmp[10] = {0};
    char szStrBuf[512] = {0};
    u8 * pU8Buf = NULL;
    s8 * pS8Buf = NULL;
    u16 * pU16Buf = NULL;
    s16 * pS16Buf = NULL;
    u32 * pU32Buf = NULL;
    s32 * pS32Buf = NULL;
    mp_info(PRINT_MPTEST, "*** %s ***\n", __func__);
    
    if(nDataType == 8)
        pU8Buf = (u8 *)pBuf;
    else if(nDataType == -8)
        pS8Buf = (s8 *)pBuf;
    else if(nDataType == 16)
        pU16Buf = (u16 *)pBuf;
    else if(nDataType == -16)
        pS16Buf = (s16 *)pBuf;
    else if(nDataType == 32)
        pU32Buf = (u32 *)pBuf;
    else if(nDataType == -32)
        pS32Buf = (s32 *)pBuf;
        
    for(i = 0; i < nLen; i++) {
        if(nCarry == 16) {
            if(nDataType == 8)
                sprintf(szStrTmp, "%02X ", pU8Buf[i]);
            else if(nDataType == -8)
                sprintf(szStrTmp, "%02X ", pS8Buf[i]);
            else if(nDataType == 16)
                sprintf(szStrTmp, "%04X ", pU16Buf[i]);
            else if(nDataType == -16)
                sprintf(szStrTmp, "%04X ", pS16Buf[i]);
            else if(nDataType == 32)
                sprintf(szStrTmp, "%08X ", pU32Buf[i]);
            else if(nDataType == -32)
                sprintf(szStrTmp, "%08X ", pS32Buf[i]);
        } else if(nCarry == 10) {
            if(nDataType == 8)
                sprintf(szStrTmp, "%6u ", pU8Buf[i]);
            else if(nDataType == -8)
                sprintf(szStrTmp, "%6d ", pS8Buf[i]);
            else if(nDataType == 16)
                sprintf(szStrTmp, "%6u ", pU16Buf[i]);
            else if(nDataType == -16)
                sprintf(szStrTmp, "%6d ", pS16Buf[i]);
            else if(nDataType == 32)
                sprintf(szStrTmp, "%6u ", pU32Buf[i]);
            else if(nDataType == -32)
                sprintf(szStrTmp, "%6d ", pS32Buf[i]);
        }
        
        strcat(szStrBuf, szStrTmp);
        memset(szStrTmp, 0, 10);
        
        if(i % nChangeLine == nChangeLine - 1) {
            printk("%s\n", szStrBuf);
            memset(szStrBuf, 0, 512);
        }
    }
    
    printk("\n");
}

int ilitek_camaro_covert_r_value(int dump_time, s32 deltaR)
{
    s32 result = 0;
    
    if (deltaR >= IIR_MAX)
        return 0;
        
    if (deltaR == 0)
        deltaR = 1;
        
    if(deltaR > -1000)
        result = (int)(73318 * dump_time * 1000) / (55 * (abs(deltaR) - 0));
    else
        result = (int)(53248 * dump_time * 1000) / (55 * ((deltaR) - 0));
        
    return result;
}


/* global vars for open */
u16 normal_test_result_check[MAX_MUTUAL_NUM];
u16 normal_test_fail_check_deltac[MAX_MUTUAL_NUM];
u16 normal_test_fail_check_ratio[MAX_MUTUAL_NUM];
int ratio_border[MAX_MUTUAL_NUM];
int ratio_move[MAX_MUTUAL_NUM];
int ratio[MAX_MUTUAL_NUM];

/* global vars for short */
s8 normal_test_fail[TEST_ITEM_NUM] = {0};//0:golden    1:ratio
u16 normal_test_fail_check[TEST_ITEM_NUM][MAX_MUTUAL_NUM];//6:max subframe    14:max afe
int ilitek_21117_gr_test_pins[14] = {0};
int gr_test_pins[13] = {0};


u16 ilitek_camaro_read_test_pins(u16 itemID, int *testPins)
{
    int nSize = 0, i;
    char str[512] = {0};
    mp_info(PRINT_MPTEST, "*** %s() ***\n", __func__);
    
    switch (itemID) {
    case 0:
    case 10:
        ilitek_ms_get_ini_data("SHORT_TEST_N1", "TEST_PIN", str);
        nSize = ilitek_ms_ini_split_int_array(str, testPins);
        mp_info(PRINT_MPTEST, "SHORT_TEST_N1 nSize = %d\n", nSize);
        break;
        
    case 1:
    case 11:
        ilitek_ms_get_ini_data("SHORT_TEST_N2", "TEST_PIN", str);
        nSize = ilitek_ms_ini_split_int_array(str, testPins);
        mp_info(PRINT_MPTEST, "SHORT_TEST_N2 nSize = %d\n", nSize);
        break;
        
    case 2:
    case 12:
        ilitek_ms_get_ini_data("SHORT_TEST_S1", "TEST_PIN", str);
        nSize = ilitek_ms_ini_split_int_array(str, testPins);
        mp_info(PRINT_MPTEST, "SHORT_TEST_S1 nSize = %d\n", nSize);
        break;
        
    case 3:
    case 13:
        ilitek_ms_get_ini_data("SHORT_TEST_S2", "TEST_PIN", str);
        nSize = ilitek_ms_ini_split_int_array(str, testPins);
        mp_info(PRINT_MPTEST, "SHORT_TEST_S2 nSize = %d\n", nSize);
        break;
        
    case 4:
    case 14:
        if (ilitek_ms_get_ini_data("SHORT_TEST_N3", "MUX_MEM_20_3E", str) != 0) {
            ilitek_ms_get_ini_data("SHORT_TEST_N3", "TEST_PIN", str);
            nSize = ilitek_ms_ini_split_int_array(str, testPins);
            mp_info(PRINT_MPTEST, "SHORT_TEST_N3 nSize = %d\n", nSize);
        } else if (ilitek_ms_get_ini_data("SHORT_TEST_S3", "MUX_MEM_20_3E", str) != 0) {
            ilitek_ms_get_ini_data("SHORT_TEST_S3", "TEST_PIN", str);
            nSize = ilitek_ms_ini_split_int_array(str, testPins);
            mp_info(PRINT_MPTEST, "SHORT_TEST_S3 nSize = %d\n", nSize);
        } else if (ilitek_ms_get_ini_data("SHORT_TEST_GR", "MUX_MEM_20_3E", str) != 0) {
            if (mp_test_data->sensor_info.num_gr == 0)
                nSize = 0;
            else
                nSize = mp_test_data->sensor_info.num_gr;
                
            for (i = 0; i < sizeof(ilitek_21117_gr_test_pins) / sizeof(ilitek_21117_gr_test_pins[0]); i++)
                testPins[i] = ilitek_21117_gr_test_pins[i];
                
            mp_info(PRINT_MPTEST, "SHORT_TEST_GR nSize = %d\n", nSize);
        }
        
        break;
        
    default:
        return 0;
    }
    
    for (i = nSize; i < MAX_CHANNEL_NUM_30XX; i++) {
        testPins[i] = PIN_NO_ERROR;
    }
    
    return nSize;
}


s32 ilitek_camaro_short_test_judge(int *deltac_data, u8 nItemID, s8 *TestFail, u16 TestFail_check[][MAX_MUTUAL_NUM])
{
    int nRet = 1, i, count_test_pin = 0;
    int testPins[MAX_CHANNEL_NUM_30XX] = {0};
    mp_info(PRINT_MPTEST, "*** %s() ***\n", __func__);
    ilitek_camaro_read_test_pins(nItemID, testPins);
    
    for(i = 0; i < ARRAY_SIZE(testPins); i++)
        mp_info(PRINT_MPTEST, "testPins[%d] = %d\n", i , testPins[i]);
        
    for (i = 0; i < sizeof(testPins) / sizeof(testPins[0]); i++) {
        if (testPins[i] != PIN_NO_ERROR)
            count_test_pin++;
    }
    
    for (i = 0; i < count_test_pin; i++) {
        TestFail_check[nItemID][i] = testPins[i];
        
        if (0 == check_thrs(deltac_data[i + nItemID * 14], mp_test_data->sensor_info.thrs_short, -mp_test_data->sensor_info.thrs_short)) {
            TestFail[nItemID] = 1;
            mp_test_result->short_result = ITO_TEST_FAIL;
            mp_info(PRINT_MPTEST, "*** (%d) ***\n", __LINE__);
            nRet = 0;
        }
    }
    
    return nRet;
}


int ilitek_camaro_short_judge(int *deltac_data)
{
    int i, j, shift;
    int *senseR = NULL, sense = mp_test_data->sensor_info.num_sen;
    int *driveR = NULL, drive = mp_test_data->sensor_info.num_drv;
    int *GRR = NULL, gr = mp_test_data->sensor_info.num_gr;
    int count_test_pin = 0;
    int thrs = 0, dump_time;
    u16 Temp_20_3E_Settings[16] = {0};
    u16 nTestItemLoop = 6 - 1;
    u16 nTestItem = 0;
    u32 nRetVal = 1;
    char str[512] = {0};
    int datalen1 = 0;
    int *deltaC = NULL;
    int sum = 0, cunt = 0, avg = 0, convert[MAX_AFENUMs_30XX * MAX_SUBFRAMES_30XX] = {0};
    deltaC = kcalloc(MAX_MUTUAL_NUM, sizeof(int), GFP_KERNEL);
    senseR = kcalloc(sense, sizeof(int), GFP_KERNEL);
    driveR = kcalloc(drive, sizeof(int), GFP_KERNEL);
    GRR = kcalloc(gr, sizeof(int), GFP_KERNEL);
    
    if(ERR_ALLOC_MEM(senseR) || ERR_ALLOC_MEM(driveR) || ERR_ALLOC_MEM(GRR)) {
        mp_err(PRINT_MPTEST,"Failed to allocate senseR/driveR/GRR mem\n");
        nRetVal = -1;
        goto ITO_TEST_END;
    }
    
    dump_time = (((mp_test_data->short_dump1 + 1) * 4 * 2) * 100) / 5166;
    mp_info(PRINT_MPTEST, "%s, dump_time = %d, short_thrs = %d\n", __func__, dump_time, mp_test_data->sensor_info.thrs_short);
    thrs = ilitek_camaro_covert_r_value(dump_time, mp_test_data->sensor_info.thrs_short);
    
    for (i = 0; i < sense; i++) senseR[i] = thrs;
    
    for (i = 0; i < drive; i++) driveR[i] = thrs;
    
    for (i = 0; i < gr; i++) GRR[i] = thrs;
    
    for (i = 0; i < 6; i++)  {//max 6 subframe
        for (j = 0; j < 14; j++) {// max 14 AFE
            if ((i * 14 + j) < MAX_CHANNEL_NUM_30XX)
                mp_test_result->p_short_fail_channel[i * 14 + j] = (u32)PIN_UN_USE;
                
            normal_test_fail_check[i][j] = PIN_UN_USE;
        }
    }
    
    memset(normal_test_fail, 0, TEST_ITEM_NUM * sizeof(s8));
    datalen1 = MAX_AFENUMs_30XX * MAX_SUBFRAMES_30XX;
    
    for (i = 0; i < datalen1; i++) { // 2 AFE * 28 subframe,but AFE0 un-use
        deltaC[i] = deltac_data[i];
        
        if ((deltaC[i] > -1000) && (deltaC[i] < 1000)) {
            sum += deltaC[i];
            cunt++;
        }
        
        mp_info(PRINT_MPTEST, "*** deltaC[%d] = %d ***\n", i, deltaC[i]);
        //result += string.Format("deltaC[{0}]={1}\n", i, deltaC[i]);
    }
    
    if (cunt != 0) {
        avg = (int)(sum / cunt);
        mp_info(PRINT_MPTEST, "avg = %d, sum = %d, cunt = %d\n", avg, sum, cunt);
        
        //int datalen2 = MAX_AFENUMs_30XX * MAX_SUBFRAMES_30XX;
        for (i = 0; i < datalen1; i++) {
            deltac_data[i] = deltaC[i] - avg;
            mp_info(PRINT_MPTEST, "*** deltac_data[%d] = %d ***\n", i, deltac_data[i]);
        }
    }
    
    /* New judge - not verified yet */
    for(i = 0; i < datalen1; i++) {
        convert[i] = ilitek_camaro_covert_r_value(dump_time, deltac_data[i]);
        mp_info(PRINT_MPTEST, "*** convert[%d] = %d ***\n", i, convert[i]);
        
        if(mp_test_data->pad2_sense != NULL) {
            for(j = 0; j < p_mp_func->sense_len; j++) {
                if(i == mp_test_data->pad2_sense[j]) {
                    mp_test_result->short_r_data[j] = convert[i];
                    mp_test_result->short_result_data[j] = deltac_data[i];
                    
                    if (convert[i] >= 10000)
                        mp_test_result->short_r_data[j] = 10000;
                        
                    if (0 == check_thrs(deltac_data[i], mp_test_data->sensor_info.thrs_short, -mp_test_data->sensor_info.thrs_short)) {
                        nRetVal = -1;
                        mp_test_result->short_result = ITO_TEST_FAIL;
                        mp_test_result->p_short_fail_channel[j] = ITO_TEST_FAIL;
                    }
                }
            }
        }
        
        if(mp_test_data->pad2_drive != NULL) {
            for(j = 0; j < p_mp_func->drive_len; j++) {
                if(i == mp_test_data->pad2_drive[j]) {
                    mp_test_result->short_r_data[p_mp_func->sense_len + j] = convert[i];
                    mp_test_result->short_result_data[p_mp_func->sense_len + j] = deltac_data[i];
                    
                    if (convert[i] >= 10000)
                        mp_test_result->short_r_data[p_mp_func->sense_len + j] = 10000;
                        
                    if (0 == check_thrs(deltac_data[i], mp_test_data->sensor_info.thrs_short, -mp_test_data->sensor_info.thrs_short)) {
                        nRetVal = -1;
                        mp_test_result->short_result = ITO_TEST_FAIL;
                        mp_test_result->p_short_fail_channel[p_mp_func->sense_len + j] = -1;
                    }
                }
            }
        }
        
        if(mp_test_data->pad2_gr != NULL) {
            for(j = 0; j < p_mp_func->gr_len; j++) {
                if(i == mp_test_data->pad2_gr[j]) {
                    mp_test_result->short_r_data[p_mp_func->sense_len + p_mp_func->drive_len + j] = convert[i];
                    mp_test_result->short_result_data[p_mp_func->sense_len + p_mp_func->drive_len + j] = deltac_data[i];
                    
                    if (convert[i] >= 10000)
                        mp_test_result->short_r_data[p_mp_func->sense_len + p_mp_func->drive_len + j] = 10000;
                        
                    if (0 == check_thrs(deltac_data[i], mp_test_data->sensor_info.thrs_short, -mp_test_data->sensor_info.thrs_short)) {
                        nRetVal = -1;
                        mp_test_result->short_result = ITO_TEST_FAIL;
                        mp_test_result->p_short_fail_channel[p_mp_func->sense_len + p_mp_func->drive_len + j] = -1;
                    }
                }
            }
        }
    }
    
    //nRetVal = -1;
    mp_info(PRINT_MPTEST, "Short Judge done \n");
    goto ITO_TEST_END;
    
    /*
     * Here is a new judge flow copied from older one.
     * The given data is sorted by PAD TAble written in INI.
     */
    for (nTestItem = 0; nTestItem < nTestItemLoop; nTestItem++) {
        if (!ilitek_camaro_short_test_judge(deltac_data, nTestItem, normal_test_fail, normal_test_fail_check)) {
            mp_err(PRINT_MPTEST,"*** New ITO Short Judge failed! ***\n");
            nRetVal = -1;
            //goto ITO_TEST_END;
        }
        
        count_test_pin = 0;
        
        for (i = 0; i < 14; i++) {
            //printk("normal_test_fail_check[%d][%d] = %x", nTestItem, i, normal_test_fail_check[nTestItem][i]);
            if (normal_test_fail_check[nTestItem][i] != PIN_UN_USE)
                count_test_pin++;
        }
        
        mp_info(PRINT_MPTEST, "nTestItem = %d, count_test_pin = %d\n", nTestItem, count_test_pin);
        memset(Temp_20_3E_Settings, 0, sizeof(Temp_20_3E_Settings));
        ilitek_ms_get_ini_data("SHORT_TEST_N3", "MUX_MEM_20_3E", str);
        
        if (nTestItem == 0 || nTestItem == 1 || (nTestItem == 4 && (ilitek_ms_ini_split_u16_array(str, Temp_20_3E_Settings)))) {
            mp_info(PRINT_MPTEST, "NEW SHORT_TEST_N3\n");
            
            for (i = 0; i < count_test_pin; i++) {
                for (j = 0; j < sense; j++) {
                    mp_info(PRINT_MPTEST, "check[%d][%d] = %d, pad2Sens[%d] = %d\n", nTestItem, i, normal_test_fail_check[nTestItem][i], j, mp_test_data->pad2_sense[j]);
                    
                    if (normal_test_fail_check[nTestItem][i] == mp_test_data->pad2_sense[j]) {
                        shift = (i + nTestItem  * 14);
                        senseR[j] = ilitek_camaro_covert_r_value(dump_time, deltac_data[shift]);
                        mp_info(PRINT_MPTEST, "senseR[%d] = %.2d, deltac_data[%d] = %d\n", j, senseR[j], shift, deltac_data[shift]);
                        mp_test_result->short_r_data[j] = senseR[j];
                        mp_test_result->short_result_data[j] = deltac_data[shift];
                        mp_info(PRINT_MPTEST, "short_r_data[%d] = %.2d, short_result_data[%d] = %d\n", j, mp_test_result->short_r_data[j], j, mp_test_result->short_result_data[j]);
                        
                        if (senseR[j] >= 10000)
                            mp_test_result->short_r_data[j] = 10000;
                            
                        if (0 == check_thrs(deltac_data[shift], mp_test_data->sensor_info.thrs_short, -mp_test_data->sensor_info.thrs_short))
                            mp_test_result->p_short_fail_channel[j] = (u32)normal_test_fail_check[nTestItem][i];
                    }
                }
            }
        }
        
        memset(Temp_20_3E_Settings, 0, sizeof(Temp_20_3E_Settings));
        ilitek_ms_get_ini_data("SHORT_TEST_S3", "MUX_MEM_20_3E", str);
        
        if (nTestItem == 2 || nTestItem == 3 || (nTestItem == 4 && (ilitek_ms_ini_split_u16_array(str, Temp_20_3E_Settings)))) {
            mp_info(PRINT_MPTEST, "NEW SHORT_TEST_S3\n");
            
            for (i = 0; i < count_test_pin; i++) {
                for (j = 0; j < drive; j++) {
                    //mp_info(PRINT_MPTEST, "check[%d][%d] = %d, pad2_drive[%d] = %d\n",nTestItem,i,normal_test_fail_check[nTestItem][i],j,mp_test_data->pad2_drive[j]);
                    if (normal_test_fail_check[nTestItem][i] == mp_test_data->pad2_drive[j]) {
                        shift = (i + nTestItem  * 14);
                        driveR[j] = ilitek_camaro_covert_r_value(dump_time, deltac_data[shift]);
                        mp_info(PRINT_MPTEST, "driveR[%d] = %.2d, deltac_data[%d] = %d\n", j, driveR[j], shift, deltac_data[shift]);
                        mp_test_result->short_r_data[sense + j] = driveR[j];
                        mp_test_result->short_result_data[sense + j] = deltac_data[shift];
                        mp_info(PRINT_MPTEST, "short_r_data[%d] = %.2d, short_result_data[%d] = %d\n", j, mp_test_result->short_r_data[sense + j], j, mp_test_result->short_result_data[sense + j]);
                        
                        if (driveR[j] >= 10000)
                            mp_test_result->short_r_data[sense + j] = 10000;
                            
                        if (0 == check_thrs(deltac_data[shift], mp_test_data->sensor_info.thrs_short, -mp_test_data->sensor_info.thrs_short))
                            mp_test_result->p_short_fail_channel[sense + j] = (u32)normal_test_fail_check[nTestItem][i];
                    }
                }
            }
        }
        
        memset(Temp_20_3E_Settings, 0, sizeof(Temp_20_3E_Settings));
        ilitek_ms_get_ini_data("SHORT_TEST_GR", "MUX_MEM_20_3E", str);
        
        if (nTestItem == 4 && (ilitek_ms_ini_split_u16_array(str, Temp_20_3E_Settings))) {
            for (i = 0; i < count_test_pin; i++) {
                for (j = 0; j < gr; j++) {
                    if (normal_test_fail_check[nTestItem][i] == mp_test_data->pad2_gr[j]) {
                        shift = (i + nTestItem  * 14);
                        GRR[j] = ilitek_camaro_covert_r_value(dump_time, deltac_data[shift]);
                        mp_test_result->short_r_data[sense + drive + j] = GRR[j];
                        mp_test_result->short_result_data[sense + drive + j] = deltac_data[shift];
                        mp_info(PRINT_MPTEST, "GR[%d] = %.2d, deltac_data[%d] = %d\n", j, GRR[j], shift, deltac_data[shift]);
                        
                        if (GRR[j] >= 10000)
                            mp_test_result->short_r_data[sense + drive + j] = 10000;
                            
                        if (0 == check_thrs(deltac_data[shift], mp_test_data->sensor_info.thrs_short, -mp_test_data->sensor_info.thrs_short))
                            mp_test_result->p_short_fail_channel[sense + drive + j] = (u32)normal_test_fail_check[nTestItem][i];
                    }
                }
            }
        }
        
        if (normal_test_fail[nTestItem]) {
            mp_test_result->p_check_fail[3] = normal_test_fail[nTestItem];// ito short fail
            nRetVal = -1;
        }
    }
    
ITO_TEST_END:
    mp_kfree(senseR);
    mp_kfree(driveR);
    mp_kfree(GRR);
    mp_kfree(deltaC);
    return nRetVal;
}


int ilitek_camaro_open_judge(int *deltac_data, int deltac_size)
{
    u16 i, j, k;
    u16 nRowNum = 0, nColumnNum = 0;
    u16 nCSub, nCfb;
    s32 nRetVal = 1, bg_per_csub;
    int ratioAvg = 0, ratioAvg_max = 0, ratioAvg_min = 0, passCount = 0;
    int ratioAvg_border = 0, ratioAvg_border_max = 0, ratioAvg_border_min = 0, passCount1 = 0;
    int ratioAvg_move = 0, ratioAvg_border_move = 0;
    int sense_num = 0, drv_num = 0, *result_buf = NULL;
    s8 nNormalTestResult[2] = {0};
    mp_info(PRINT_MPTEST, "*** %s() ***\n", __func__);
    sense_num = mp_test_data->sensor_info.num_sen;
    drv_num = mp_test_data->sensor_info.num_drv;
    result_buf = mp_test_result->open_result_data;
    nCSub = mp_test_data->open_test_csub;
    
    /* if open mode is sine mode, Csub must be zero. */
    if ((mp_test_data->open_mode == 1))
        nCSub = 0;
        
    if (!mp_test_data->open_test_cfb)
        nCfb = 2;
    else
        nCfb = mp_test_data->open_test_cfb;
        
    bg_per_csub = (int)(92012 / (11 * nCfb));
    
    for (i = 0; i < sense_num * drv_num; i++) {
        if (deltac_data[i] > 31000)
            return -1;
            
        result_buf[i] = bg_per_csub * nCSub - deltac_data[i];
        
        // For mutual key, last column if not be used, show number "one".
        if ((mp_test_data->mutual_key == 1 || mp_test_data->mutual_key == 2) && (mp_test_data->sensor_info.num_key != 0)) {
            if (mp_test_data->pattern_type == 5) {
                // key_ch = 1, it mean keys in same drive. Current one key project only key_ch = 1 type.
                if (mp_test_data->sensor_info.key_ch != mp_test_data->sensor_info.num_key) {
                    if (!((i + 1) % drv_num)) {
                        result_buf[i] = NULL_DATA;
                        
                        for (k = 0; k < mp_test_data->sensor_info.num_key; k++)
                            if ((i + 1) / drv_num == mp_test_data->key_sen[k]) {
                                // if (_gMsg30xxsssqrt_en)
                                //     result_buf[i] = deltac_data[i];
                                // else
                                result_buf[i] = bg_per_csub * nCSub - deltac_data[i];
                            }
                    }
                } else {
                    if (i > ((sense_num - 1) * drv_num - 1)) {
                        result_buf[i] = NULL_DATA;
                        
                        for (k = 0; k < mp_test_data->sensor_info.num_key; k++)
                            if (((i + 1) - (sense_num - 1) * drv_num) == mp_test_data->key_sen[k]) {
                                // if (_gMsg30xxsssqrt_en)
                                //     result_buf[i] = deltac_data[i];
                                // else
                                result_buf[i] = bg_per_csub * nCSub - deltac_data[i];
                            }
                    }
                }
            } else {
                if ((sense_num < drv_num) && ((i + 1) % drv_num == 0)) {
                    result_buf[i] = NULL_DATA;
                    
                    for (k = 0; k < mp_test_data->sensor_info.num_key; k++)
                        if ((i + 1) / drv_num == mp_test_data->key_sen[k]) {
                            // if (_gMsg30xxsssqrt_en)
                            //     result_buf[i] = deltac_data[i];
                            // else
                            result_buf[i] = bg_per_csub * nCSub - deltac_data[i];
                        }
                }
                
                if ((sense_num > drv_num) && (i > (sense_num - 1) * drv_num - 1)) {
                    result_buf[i] = NULL_DATA;
                    
                    for (k = 0; k < mp_test_data->sensor_info.num_key; k++) {
                        if (((i + 1) - (sense_num - 1) * drv_num) == mp_test_data->key_sen[k]) {
                            // if (_gMsg30xxsssqrt_en)
                            //     result_buf[i] = deltac_data[i];
                            // else
                            result_buf[i] = bg_per_csub * nCSub - deltac_data[i];
                        }
                    }
                }
            }
        }
    }
    
    memset(normal_test_fail_check_deltac, 0xFFFF, sizeof(normal_test_fail_check_deltac));
    memset(normal_test_fail_check_ratio, 0xFFFF, sizeof(normal_test_fail_check_ratio));
    memset(ratio, 0, sizeof(ratio));
    memset(ratio_border, 0, sizeof(ratio_border));
    memset(ratio_move, 0, sizeof(ratio_move));
    nRowNum = drv_num;
    nColumnNum = sense_num;
    mp_info(PRINT_MPTEST, "*** Msg30xx Open Test# Show _gResult ***\n");
    debug_show_array2(result_buf, nRowNum * nColumnNum, -32, 10, nColumnNum);
    mp_info(PRINT_MPTEST, "*** Msg30xx Open Test# Show Goldensample ***\n");
    debug_show_array2(mp_test_data->goldensample_ch_0, nRowNum * nColumnNum, -32, 10, nColumnNum);
    
    for (k = 0; k < deltac_size; k++) {
        if (0 == mp_test_data->goldensample_ch_0[k]) {
            if (k == 0)
                nNormalTestResult[0] = 1;	// no golden sample
                
            break;
        }
        
        if (result_buf[k] != NULL_DATA) {
            ratio[k] = (result_buf[k] * 1000) / mp_test_data->goldensample_ch_0[k];
            
            if (0 == check_thrs(result_buf[k], mp_test_data->goldensample_ch_0_max[k], mp_test_data->goldensample_ch_0_min[k])) {
                nNormalTestResult[0] = 1;
                normal_test_result_check[k] = (u16)(((k / drv_num) + 1) * 100 + ((k % drv_num) + 1));
            } else {
                normal_test_result_check[k] = PIN_NO_ERROR;
                
                if ((mp_test_data->pattern_type == 3) && (mp_test_data->sensor_info.num_key == 0) && ((k % drv_num == 0 ) || ((k + 1) % drv_num == 0))) {
                    ratioAvg_border += ratio[k];
                    passCount1 += 1;
                } else if ((mp_test_data->pattern_type == 3) && (mp_test_data->sensor_info.num_key != 0) && ((k % drv_num == 0) || ((k + 2) % drv_num == 0))) {
                    ratioAvg_border += ratio[k];
                    passCount1 += 1;
                } else {
                    ratioAvg += ratio[k];
                    passCount += 1;
                }
            }
        } else {
            normal_test_result_check[k] = PIN_NO_ERROR;
        }
        
        normal_test_fail_check_deltac[k] = normal_test_result_check[k];
    }
    
    mp_info(PRINT_MPTEST, "*** Msg30xx Open Test# normal_test_fail_check_deltac Channel ***\n");
    ratioAvg_max = (int)(100000 + (mp_test_data->toast_info.persent_dc_va_ratio * 1000) + (mp_test_data->toast_info.persent_dc_va_ratio_up * 1000)) / 100;
    ratioAvg_min = (int)(100000 - (mp_test_data->toast_info.persent_dc_va_ratio * 1000)) / 100;
    ratioAvg_border_max = (int)(100000 + (mp_test_data->toast_info.persent_dc_border_ratio * 1000) + (mp_test_data->toast_info.persent_dc_va_ratio_up * 1000)) / 100;
    ratioAvg_border_min = (int)(100000 - (mp_test_data->toast_info.persent_dc_border_ratio * 1000)) / 100;
    
    if (passCount != 0) {
        if (passCount1 != 0) {
            ratioAvg_border_move = ratioAvg_border / passCount1;
            ratioAvg_move = ratioAvg / passCount;
            
            for (i = 0; i < sizeof(ratio) / sizeof(ratio[0]); i++) {
                if ((mp_test_data->sensor_info.num_key == 0) && ((i % drv_num == 0) || ((i + 1) % drv_num == 0))) {
                    ratio_move[i] = ratio[i] - ratioAvg_border_move + 1000;
                } else if ((mp_test_data->sensor_info.num_key != 0) && ((i % drv_num == 0) || ((i + 2) % drv_num == 0))) {
                    ratio_move[i] = ratio[i] - ratioAvg_border_move + 1000;
                } else {
                    ratio_move[i] = ratio[i] - ratioAvg_move + 1000;
                }
            }
        } else {
            ratioAvg_move = ratioAvg / passCount;
            
            for (i = 0; i < sizeof(ratio) / sizeof(ratio[0]); i++) {
                ratio_move[i] = ratio[i] - ratioAvg_move + 1000;
            }
        }
    } else {
        memcpy(ratio, ratio_move, sizeof(ratio));
    }
    
    /* Based on the size of deltac_data for th loop */
    for (j = 0; j < deltac_size; j++) {
        if (0 == mp_test_data->goldensample_ch_0[j]) {
            if (j == 0)
                nNormalTestResult[1] = 1;	// no golden sample
                
            break;
        }
        
        if (PIN_NO_ERROR == normal_test_result_check[j]) {
            if (result_buf[j] != NULL_DATA) {
                if ((mp_test_data->pattern_type == 3) && (mp_test_data->sensor_info.num_key == 0) && ((j % drv_num == 0) || ((j + 1) % drv_num == 0))) {
                    if (0 == check_thrs(ratio_move[j], ratioAvg_border_max, ratioAvg_border_min)) {
                        nNormalTestResult[1] = 1;
                        normal_test_result_check[j] = (u16)(((j / drv_num) + 1) * 100 + ((j % drv_num) + 1));
                    } else {
                        normal_test_result_check[j] = PIN_NO_ERROR;
                    }
                } else if ((mp_test_data->pattern_type == 3) && (mp_test_data->sensor_info.num_key != 0) && ((j % drv_num == 0) || ((j + 2) % drv_num == 0))) {
                    if (0 == check_thrs(ratio_move[j], ratioAvg_border_max, ratioAvg_border_min)) {
                        nNormalTestResult[1] = 1;
                        normal_test_result_check[j] = (u16)(((j / drv_num) + 1) * 100 + ((j % drv_num) + 1));
                    } else {
                        normal_test_result_check[j] = PIN_NO_ERROR;
                    }
                } else {
                    if (0 == check_thrs(ratio_move[j], ratioAvg_max, ratioAvg_min)) {
                        nNormalTestResult[1] = 1;
                        normal_test_result_check[j] = (u16)(((j / drv_num) + 1) * 100 + ((j % drv_num) + 1));
                    } else {
                        normal_test_result_check[j] = PIN_NO_ERROR;
                    }
                }
            } else {
                normal_test_result_check[j] = PIN_NO_ERROR;
            }
        } else {
            normal_test_fail_check_ratio[j] = normal_test_result_check[j];
            continue;
        }
        
        normal_test_fail_check_ratio[j] = normal_test_result_check[j];
    }
    
    for (k = 0; k < MAX_MUTUAL_NUM; k++) {
        if (0 == mp_test_data->goldensample_ch_0[k]) {
            normal_test_result_check[k] = PIN_NO_ERROR;
            normal_test_fail_check_deltac[k] = PIN_NO_ERROR;
            normal_test_fail_check_ratio[k] = PIN_NO_ERROR;
        }
    }
    
    if ((nNormalTestResult[0] != 0) || (nNormalTestResult[1] != 0))
        nRetVal = -1;
        
    for (i = 0; i < 2; i++) {
        mp_test_result->p_check_fail[i] = nNormalTestResult[i];
    }
    
    for (i = 0; i < sizeof(normal_test_fail_check_deltac) / sizeof(normal_test_fail_check_deltac[0]); i++) {	// reduce memory operation instead of memcpy
        mp_test_result->open_fail_channel[i] = normal_test_fail_check_deltac[i];
        mp_test_result->open_ratio_fail_channel[i] = normal_test_fail_check_ratio[i];
        mp_test_result->p_golden_ch_max_avg[i] = ratio_move[i];
    }
    
    return nRetVal;
}
