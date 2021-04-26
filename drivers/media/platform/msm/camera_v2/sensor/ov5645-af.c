#include "ov5645-af.h"
#include "msm_sd.h"
#include "camera.h"
#include "msm_cci.h"
#include "msm_camera_io_util.h"
#include "msm_camera_i2c_mux.h"
#include <linux/regulator/rpm-smd-regulator.h>
#include <linux/regulator/consumer.h>
#include "ov5645-af-firmware.h"

#undef CDBG
//#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#define CDBG(fmt, args...) pr_notice(fmt, ##args)


static int write_cmos_sensor(struct msm_camera_i2c_client *sensor_i2c_client, uint16_t address, uint8_t value) 
{
	int rc = -1;
	uint16_t reg_data;

	if (sensor_i2c_client) {
		reg_data = value;
		rc = sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, address, reg_data, MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0) {
			pr_warn("%s: write i2c address failed on i2c_write!\n", __func__);
		}
	}

	return rc;
}

static int read_cmos_sensor(struct msm_camera_i2c_client *sensor_i2c_client, uint16_t address, uint8_t* value) 
{
	int rc = -1;
	uint16_t reg_data;

	if (sensor_i2c_client && value) {
		rc = sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, address, &reg_data, MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0) {
			pr_warn("%s: read i2c address failed on i2c_read!\n", __func__);
		}
		else {
			*value = (uint8_t)reg_data;
			//CDBG("----> read_cmos_sensor address = 0x%x value=0x%x\n", address, reg_data);
		}
	}

	return  rc;
}

static uint8_t _read_cmos_sensor(struct msm_camera_i2c_client *sensor_i2c_client, uint16_t address) 
{
	uint8_t value;
	read_cmos_sensor(sensor_i2c_client, address, &value);

	return value;
}

static void ov5645_af_check_mcu(struct msm_camera_i2c_client *sensor_i2c_client)
{
    uint8_t check[13] = {0x00};
    int i = 0;

	//mcu on
    check[0] = _read_cmos_sensor(sensor_i2c_client, 0x3000);
    check[1] = _read_cmos_sensor(sensor_i2c_client, 0x3004);
	//soft reset of mcu
    check[2] = _read_cmos_sensor(sensor_i2c_client, 0x3f00);	
	//afc on
    check[3] = _read_cmos_sensor(sensor_i2c_client, 0x3001);
    check[4] = _read_cmos_sensor(sensor_i2c_client, 0x3005);
	//gpio1,gpio2
    check[5] = _read_cmos_sensor(sensor_i2c_client, 0x3018);
    check[6] = _read_cmos_sensor(sensor_i2c_client, 0x301e);
    check[7] = _read_cmos_sensor(sensor_i2c_client, 0x301b);
    check[8] = _read_cmos_sensor(sensor_i2c_client, 0x3042);
	//y0
    check[9] = _read_cmos_sensor(sensor_i2c_client, 0x3018);
    check[10] = _read_cmos_sensor(sensor_i2c_client, 0x301e);
    check[11] = _read_cmos_sensor(sensor_i2c_client, 0x301b);
    check[12] = _read_cmos_sensor(sensor_i2c_client, 0x3042);

    for(i = 0; i < 13; i++) {
		CDBG("%s: check[%d]=0x%x\n", __func__, i, check[i]);
	}
}

int ov5645_af_init(struct msm_camera_i2c_client *sensor_i2c_client, bool verify)
{
    uint8_t state=0x8F;
    uint32_t  iteration = 100;
    int totalCnt = 0; 
    int sentCnt = 0; 
	const uint16_t MCUFirwareBytesToVerify = 100;
    uint16_t addr = 0;
	bool update_it = false;	
	int rv = 0;

	CDBG("%s:%d\n", __func__, __LINE__);
	
	write_cmos_sensor(sensor_i2c_client, 0x3103, 0x11);
	write_cmos_sensor(sensor_i2c_client, 0x3008, 0x82);

	mdelay(20);

	write_cmos_sensor(sensor_i2c_client, 0x3004, 0xff);
	
	write_cmos_sensor(sensor_i2c_client, 0x3000, 0x20);
    totalCnt = ARRAY_SIZE(OV5645_AF_Firmware); 
	
	update_it = false;
	if (verify) {
		addr = OV5645_AF_FIRMWARE_ADDRESS;
		CDBG("%s:%d current firmware/config verification - update needed: %d \n", __func__, __LINE__, update_it);
		for (sentCnt = 0; sentCnt < totalCnt && sentCnt < MCUFirwareBytesToVerify; sentCnt++) {
			if( _read_cmos_sensor(sensor_i2c_client, addr) != OV5645_AF_Firmware[sentCnt]) {
				update_it = true;
				break;
			}
			addr++;
		}
		CDBG("%s:%d current firmware/config verification - update needed: %d \n", __func__, __LINE__, update_it);
		if (update_it) {
			int sentCntStop = totalCnt - MCUFirwareBytesToVerify - 1;
			addr = OV5645_AF_FIRMWARE_ADDRESS + totalCnt - 1;
			for (sentCnt = totalCnt - 1; sentCnt >= 0 && sentCnt >= sentCntStop; sentCnt--) {
				if( _read_cmos_sensor(sensor_i2c_client, addr) != OV5645_AF_Firmware[sentCnt]) {
					update_it = true;
					break;
				}
				addr--;
			}
		}
		CDBG("%s:%d verifing firmware/config... update needed: %d \n", __func__, __LINE__, update_it);
	}
	else {
		update_it = true;
	}
	if (update_it) {
		CDBG("%s:%d updating...\n", __func__, __LINE__);
		addr = OV5645_AF_FIRMWARE_ADDRESS;
		for (sentCnt = 0; sentCnt < totalCnt; sentCnt++) {
			write_cmos_sensor(sensor_i2c_client, addr, OV5645_AF_Firmware[sentCnt]);
			addr++;
		}
		CDBG("%s:%d update completed!\n", __func__, __LINE__);
	}
	mdelay(10);
    write_cmos_sensor(sensor_i2c_client, 0x3022, 0x00);
    write_cmos_sensor(sensor_i2c_client, 0x3023, 0x00);
    write_cmos_sensor(sensor_i2c_client, 0x3024, 0x00);
    write_cmos_sensor(sensor_i2c_client, 0x3025, 0x00);
    write_cmos_sensor(sensor_i2c_client, 0x3026, 0x00);
    write_cmos_sensor(sensor_i2c_client, 0x3027, 0x00);
    write_cmos_sensor(sensor_i2c_client, 0x3028, 0x00);
    write_cmos_sensor(sensor_i2c_client, 0x3029, 0x7f);
	write_cmos_sensor(sensor_i2c_client, 0x3000, 0x00);
	
    do {
		read_cmos_sensor(sensor_i2c_client, 0x3029,&state);
		//CDBG("----> state=0x%x\n", state);
		mdelay(5);
        if (iteration-- == 0) {
			pr_warn("%s: af init failed\n", __func__);
			rv = -1;
			break;
        }
	}
	while (state != 0x70);

	CDBG("%s: completed with rc = %d\n", __func__, rv);
	ov5645_af_check_mcu(sensor_i2c_client);
    return rv;    
}

int ov5645_af_constant_focus(struct msm_camera_i2c_client *sensor_i2c_client)
{
	CDBG("%s:%d\n", __func__, __LINE__);
    
    write_cmos_sensor(sensor_i2c_client, 0x3023, 0x01);
    write_cmos_sensor(sensor_i2c_client, 0x3022, 0x04);

	return 0;
}

int ov5645_af_cancel_focus(struct msm_camera_i2c_client *sensor_i2c_client)
{
    uint8_t state = 0x8F;   
    int iteration = 100;
	
	CDBG("%s:%d\n", __func__, __LINE__);

    write_cmos_sensor(sensor_i2c_client, 0x3023, 0x01);
    write_cmos_sensor(sensor_i2c_client, 0x3022, 0x08);
	
    iteration = 100;  
    do {
        state = _read_cmos_sensor(sensor_i2c_client, 0x3023);	
        if (state == 0x00) {
			break;
		}
        msleep(10);
        iteration--;
		
    }
	while(iteration);

	return 0;
}

bool ov5645_af_check_sensor_id(struct msm_camera_i2c_client *sensor_i2c_client, uint16_t sensor_id, bool init)
{
	uint16_t sensor_id_read = 0;
	uint8_t retry;
	bool rv = false;

	CDBG("%s:%d\n", __func__, __LINE__);
	
	// check if sensor ID correct
    retry = 3; 
    do {
        sensor_id_read = (((uint16_t)_read_cmos_sensor(sensor_i2c_client, 0x300A) << 8) | (uint16_t)_read_cmos_sensor(sensor_i2c_client, 0x300B)); 
		CDBG("%s: read sensor ID = 0x%04x\n", __func__, sensor_id_read); 
        if (sensor_id_read == sensor_id) {
			rv = true;
            break; 
		}
        pr_warn("%s: wrong sensor ID = 0x%04x read; expected sensor ID = 0x%04x\n", __func__, sensor_id_read, sensor_id); 
        retry--; 
    } 
	while (retry > 0);

	if (rv == false && init) {
		write_cmos_sensor(sensor_i2c_client, 0x3103, 0x11);
		write_cmos_sensor(sensor_i2c_client, 0x3008, 0x82);
	}

	return rv;
}   
