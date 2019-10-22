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
char ilitek_driver_information[] = {
    DERVER_VERSION_V0, DERVER_VERSION_V1, DERVER_VERSION_V2, DERVER_VERSION_V3
};

static bool ilitek_repeat_start = true;
static bool ilitek_exit_report = false;
int frimware_mode = CAMARO_FIRMWARE_MODE_DEMO_MODE;

#ifdef ILITEK_ESD_PROTECTION
static void ilitek_esd_check(struct work_struct *work)
{
    int i = 0;
    unsigned char buf[4] = { 0 };
    tp_log_debug(PRINT_ESD_CHECK,"enter.......\n");

    if (ilitek_data->esd_check == false) {
        tp_log_debug(PRINT_ESD_CHECK,
        "Not allow to do ESD check while  ilitek_data->esd_check is false\n");
        goto ilitek_esd_check_out;
    }

    if (ilitek_data->firmware_updating) {
        tp_log_debug(PRINT_ESD_CHECK,
        "Not allow to do ESD check while  ilitek_data->firmware_updating is true\n");
        goto ilitek_esd_check_out;
    }
    
    if (ilitek_data->operation_protection) {
        tp_log_debug(PRINT_ESD_CHECK,
        "Not allow to do ESD check while  ilitek_data->operation_protection is true\n");
        goto ilitek_esd_check_out;
    }

    if (ilitek_data->is_touched == true) {
        tp_log_debug(PRINT_ESD_CHECK,
        "Not allow to do ESD check while finger touch\n");
        goto ilitek_esd_check_out;
    }
    
    mutex_lock(&ilitek_data->ilitek_mutex);
    /*get tp resolution*/
    buf[0] = 0x00;
    

    for (i = 0; i < 3; i++) {
        if (ilitek_i2c_write_and_read(buf, 1, 0, buf, 0) < 0) {
            if (i == 2){
                tp_log_err ("esd i2c communication failed three times reset now\n");
                ilitek_reset(HARDWARE_RESET_DELAY_TIME);            
            }                
        } else {            
                tp_log_debug(PRINT_ESD_CHECK,"esd ilitek_ts_send_cmd successful\n");
                break;
        }
    }


ilitek_esd_check_out:
    mutex_unlock(&ilitek_data->ilitek_mutex);
    ilitek_data->esd_check = true;
    queue_delayed_work(ilitek_data->esd_wq, &ilitek_data->esd_work,
                       ilitek_data->esd_delay);
    return;
}


#endif	 
static DECLARE_WAIT_QUEUE_HEAD(waiter);
void ilitek_irq_enable(void)
{
    unsigned long irqflag = 0;
    mutex_lock(&ilitek_data->ilitek_mutex);
    spin_lock_irqsave(&ilitek_data->irq_lock, irqflag);
    
    if (!(ilitek_data->irq_status)) {
        enable_irq(ilitek_data->client->irq);
        ilitek_data->irq_status = true;
    }
    
    spin_unlock_irqrestore(&ilitek_data->irq_lock, irqflag);
    mutex_unlock(&ilitek_data->ilitek_mutex);
}

void ilitek_irq_disable(void)
{
    unsigned long irqflag = 0;
    mutex_lock(&ilitek_data->ilitek_mutex);
    spin_lock_irqsave(&ilitek_data->irq_lock, irqflag);
    
    if ((ilitek_data->irq_status)) {
        disable_irq(ilitek_data->client->irq);
        ilitek_data->irq_status = false;
        tp_log_info("\n");
    }
    
    spin_unlock_irqrestore(&ilitek_data->irq_lock, irqflag);
    mutex_unlock(&ilitek_data->ilitek_mutex);
}


#ifdef ILITEK_ENABLE_DMA
static int ilitek_dma_i2c_read(struct i2c_client *client, unsigned char *buf,
                               int len)
{
    int i = 0, err = 0;
    
    if (len < 8) {
        client->ext_flag = client->ext_flag & (~I2C_DMA_FLAG);
        return i2c_master_recv(client, buf, len);
    } else {
        client->ext_flag = client->ext_flag | I2C_DMA_FLAG;
        err =
            i2c_master_recv(client, (unsigned char *)I2CDMABuf_pa, len);
            
        if (err < 0)
            return err;
            
        for (i = 0; i < len; i++)
            buf[i] = I2CDMABuf_va[i];
            
        return err;
    }
}

static int ilitek_dma_i2c_write(struct i2c_client *client,
                                unsigned char *pbt_buf, int dw_len)
{
    int i = 0;
    
    if (dw_len <= 8) {
        client->ext_flag = client->ext_flag & (~I2C_DMA_FLAG);
        return i2c_master_send(client, pbt_buf, dw_len);
    } else {
        for (i = 0; i < dw_len; i++) {
            I2CDMABuf_va[i] = pbt_buf[i];
        }
        
        client->ext_flag = client->ext_flag | I2C_DMA_FLAG;
        //client->addr = (client->addr & I2C_MASK_FLAG) | I2C_DMA_FLAG;
        return i2c_master_send(client,(unsigned char *)I2CDMABuf_pa, dw_len);
    }
}
#endif
int ilitek_i2c_transfer(struct i2c_msg *msgs, int cnt)
{
    int ret = 0;
    struct i2c_client *client = ilitek_data->client;
    int count = ILITEK_I2C_RETRY_COUNT;
#ifdef ILITEK_ENABLE_DMA
    int i = 0;
    
    for (i = 0; i < cnt; i++) {
        while (count >= 0) {
            count -= 1;
            msgs[i].ext_flag = 0;
            
            if (msgs[i].flags == I2C_M_RD)
                ret =ilitek_dma_i2c_read(client, msgs[i].buf,
                                        msgs[i].len);
            else if (msgs[i].flags == 0)
                ret =ilitek_dma_i2c_write(client, msgs[i].buf,
                                         msgs[i].len);
                                         
            if (ret < 0) {
                tp_log_err("ilitek i2c transfer err\n");
                mdelay(20);
                continue;
            }
            
            break;
        }
    }
    
#else	 
    
    while (count >= 0) {
        count -= 1;
        ret = i2c_transfer(client->adapter, msgs, cnt);
    
        if (ret < 0) {
            tp_log_err("ilitek_i2c_transfer err");
            mdelay(20);
            continue;
        }
    
        break;
    }
    
#endif	 
    return ret;
}

int ilitek_i2c_write(uint8_t * cmd, int length)
{
    int ret = 0;
    struct i2c_client *client = ilitek_data->client;
    struct i2c_msg msgs[] = {
        {
            .addr = client->addr, .flags = 0, .len = length, .buf =
            cmd,
        }
    };
    ret = ilitek_i2c_transfer(msgs, 1);
    
    if (ret < 0)
        tp_log_err("%s, i2c write error, ret %d\n", __func__, ret);
        
    return ret;
}

int ilitek_i2c_read(uint8_t * data, int length)
{
    int ret = 0;
    struct i2c_client *client = ilitek_data->client;
    struct i2c_msg msgs_ret[] = {
        {
            .addr = client->addr, .flags = I2C_M_RD, .len = length, .buf =
            data,
        }
    };
    ret = ilitek_i2c_transfer(msgs_ret, 1);
    
    if (ret < 0)
        tp_log_err("%s, i2c read error, ret %d\n", __func__, ret);
        
    return ret;
}

int ilitek_i2c_write_and_read(uint8_t * cmd,
                              int write_len, int delay, uint8_t * data,
                              int read_len)
{
    int ret = 0;
    struct i2c_client *client = ilitek_data->client;
    struct i2c_msg msgs_send[] = {
        {
            .addr = client->addr,
            .flags = 0,
            .len = write_len, .buf =cmd,
        }, 
        {
            .addr = client->addr,
            .flags = I2C_M_RD,
            .len =read_len,
            .buf = data,
        }
    };
    struct i2c_msg msgs_receive[] = {
        {
            .addr = client->addr,
            .flags = I2C_M_RD,
            .len =read_len,
            .buf = data,
        }
    };
    
    if (ilitek_repeat_start) {
        if (read_len == 0) {
            if (write_len > 0) {
                ret = ilitek_i2c_transfer(msgs_send, 1);
                
                if (ret < 0) {
                    tp_log_err
                    ("%s, i2c write error, ret = %d\n",__func__, ret);
                }
            }
            
            if (delay > 0) {
                mdelay(delay);
            }
        } else if (write_len == 0) {
            if (read_len > 0) {
                ret = ilitek_i2c_transfer(msgs_receive, 1);
                
                if (ret < 0)
                    tp_log_err
                    ("%s, i2c read error, ret = %d\n", __func__, ret);
                     
                if (delay > 0)
                    mdelay(delay);
            }
        } else if (delay > 0) {
            if (write_len > 0) {
                ret = ilitek_i2c_transfer(msgs_send, 1);
                
                if (ret < 0) {
                    tp_log_err
                    ("%s, i2c write error, ret = %d\n", __func__, ret);
                }
            }
            
            if (delay > 0) {
                mdelay(delay);
            }
            
            if (read_len > 0) {
                ret = ilitek_i2c_transfer(msgs_receive, 1);
                
                if (ret < 0) {
                    tp_log_err
                    ("%s, i2c read error, ret = %d\n",__func__, ret);
                }
            }
        } else {
            ret = ilitek_i2c_transfer(msgs_send, 2);
            
            if (ret < 0) {
                tp_log_err
                ("%s, i2c repeat start error, ret = %d\n", __func__, ret);
            }
        }
    } else {
        if (write_len > 0) {
            ret = ilitek_i2c_transfer(msgs_send, 1);
            
            if (ret < 0) {
                tp_log_err("%s, i2c write error, ret = %d\n",__func__, ret);
            }
        }
        
        if (delay > 0) {
            mdelay(delay);
        }
        
        if (read_len > 0) {
            ret = ilitek_i2c_transfer(msgs_receive, 1);
            
            if (ret < 0) {
                tp_log_err("%s, i2c read error, ret = %d\n",__func__, ret);
            }
        }
    }
    
    return ret;
}

int ilitek_poll_int(void)
{
    return gpio_get_value(ilitek_data->irq_gpio);
}

void ilitek_reset(int delay)
{
    tp_log_info("delay = %d\n", delay);
    
    if (ilitek_data->reset_gpio >= 0) {
        gpio_direction_output(ilitek_data->reset_gpio, 0);
        mdelay(10);
        gpio_direction_output(ilitek_data->reset_gpio, 1);
        mdelay(delay);
    } else {
        tp_log_err("reset pin is invalid\n");
    }
}


#ifdef ILITEK_ENABLE_REGULATOR_POWER_ON
void ilitek_regulator_release(void)
{
    if (ilitek_data->vdd) {
        regulator_put(ilitek_data->vdd);
    }
    
    if (ilitek_data->vdd_i2c) {
        regulator_put(ilitek_data->vdd_i2c);
    }
}


#endif	 
int ilitek_free_gpio(void)
{
    return 0;
}

static int ilitek_set_input_param(void)
{
    int ret = 0;
    int i = 0;
    struct input_dev *input = ilitek_data->input_dev;
    tp_log_info("ilitek_set_input_param\n");
    __set_bit(INPUT_PROP_DIRECT, input->propbit);
    input->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
    input->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
#if !ILITEK_ROTATE_FLAG
    input_set_abs_params(input, ABS_MT_POSITION_X,
                         ilitek_data->screen_min_x,
                         ilitek_data->screen_max_x, 0, 0);
    input_set_abs_params(input, ABS_MT_POSITION_Y,
                         ilitek_data->screen_min_y,
                         ilitek_data->screen_max_y, 0, 0);
#else	 
    input_set_abs_params(input, ABS_MT_POSITION_X,
                         ilitek_data->screen_min_y,
                         ilitek_data->screen_max_y, 0, 0);
    input_set_abs_params(input, ABS_MT_POSITION_Y,
                         ilitek_data->screen_min_x,
                         ilitek_data->screen_max_x, 0, 0);
#endif	 
#if !ILITEK_ROTATE_FLAG
    input_set_abs_params(input, ABS_MT_POSITION_X,
                         ilitek_data->screen_min_x,
                         ilitek_data->screen_max_x, 0, 0);
    input_set_abs_params(input, ABS_MT_POSITION_Y,
                         ilitek_data->screen_min_y,
                         ilitek_data->screen_max_y, 0, 0);
#else	 
    input_set_abs_params(input, ABS_MT_POSITION_X,
                         ilitek_data->screen_min_y,
                         ilitek_data->screen_max_y, 0, 0);
    input_set_abs_params(input, ABS_MT_POSITION_Y,
                         ilitek_data->screen_min_x,
                         ilitek_data->screen_max_x, 0, 0);
#endif	 
    input_set_abs_params(input, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
    input_set_abs_params(input, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
    input->name = ILITEK_TS_NAME;
    input->id.bustype = BUS_I2C;
    input->dev.parent = &(ilitek_data->client)->dev;
#ifdef ILITEK_TOUCH_PROTOCOL_B
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
    input_mt_init_slots(input, MUTUAL_MAX_TOUCH_NUM, INPUT_MT_DIRECT);
#else	 
    input_mt_init_slots(input, MUTUAL_MAX_TOUCH_NUM);
#endif	 
#else	 
    input_set_abs_params(input, ABS_MT_TRACKING_ID, 0,
                         MUTUAL_MAX_TOUCH_NUM, 0, 0);
#endif	 
#ifdef ILITEK_REPORT_PRESSURE
    input_set_abs_params(input, ABS_MT_PRESSURE, 0, 255, 0, 0);
#endif	 
    
    for (i = 0; i < ilitek_data->keycount; i++) {
        set_bit(ilitek_data->keyinfo[i].id & KEY_MAX, input->keybit);
    }
    
#ifdef ILITEK_CLICK_WAKEUP
    input_set_capability(input, EV_KEY, KEY_POWER);
#endif	 
    ret = input_register_device(ilitek_data->input_dev);
    
    if (ret < 0) {
        tp_log_err("register input device, error\n");
    }
    
    return ret;
}

static int ilitek_touch_down(int id, int x, int y, int pressure)
{
    struct input_dev *input = ilitek_data->input_dev;
    input_report_key(input, BTN_TOUCH, 1);
#ifdef ILITEK_TOUCH_PROTOCOL_B
    input_mt_slot(input, id);
    input_mt_report_slot_state(input, MT_TOOL_FINGER, true);
#endif	 
#if !ILITEK_ROTATE_FLAG
    input_event(input, EV_ABS, ABS_MT_POSITION_X, x);
    input_event(input, EV_ABS, ABS_MT_POSITION_Y, y);
#else	 
    input_event(input, EV_ABS, ABS_MT_POSITION_X, y);
    input_event(input, EV_ABS, ABS_MT_POSITION_Y, x);
#endif	 
    input_event(input, EV_ABS, ABS_MT_TOUCH_MAJOR, 128);
#ifdef ILITEK_REPORT_PRESSURE
    input_event(input, EV_ABS, ABS_MT_PRESSURE, pressure);
#endif	 
#ifndef ILITEK_TOUCH_PROTOCOL_B
    input_event(input, EV_ABS, ABS_MT_TRACKING_ID, id);
    input_mt_sync(input);
#endif	 
    return 0;
}

static int ilitek_touch_release(int id)
{
    struct input_dev *input = ilitek_data->input_dev;
#ifdef ILITEK_TOUCH_PROTOCOL_B
    
    if (ilitek_data->touch_flag[id] == 1) {
        tp_log_debug(PRINT_FINGER_REPORT,"release point id = %d\n", id);
        input_mt_slot(input, id);
        input_mt_report_slot_state(input, MT_TOOL_FINGER, false);
    }
    
#else	 
    input_report_key(input, BTN_TOUCH, 0);
    input_mt_sync(input);
#endif	 
    ilitek_data->touch_flag[id] = 0;
    return 0;
}

static int ilitek_touch_release_all_point(void)
{
    struct input_dev *input = ilitek_data->input_dev;
    int i = 0;
#ifdef ILITEK_TOUCH_PROTOCOL_B
    input_report_key(input, BTN_TOUCH, 0);
    
    for (i = 0; i < MUTUAL_MAX_TOUCH_NUM; i++) {
        ilitek_touch_release(i);
    }
    
#else
    
    for (i = 0; i < MUTUAL_MAX_TOUCH_NUM; i++) {
        ilitek_data->touch_flag[i] = 0;
    }
    
    ilitek_touch_release(0);
#endif
    ilitek_data->is_touched = false;
    input_sync(input);
    return 0;
}

static int ilitek_check_key_down(int x, int y)
{
    int j = 0;
    
    for (j = 0; j < ilitek_data->keycount; j++) {
        if ((x >= ilitek_data->keyinfo[j].x
            && x <= ilitek_data->keyinfo[j].x + ilitek_data->key_xlen)
            && (y >= ilitek_data->keyinfo[j].y
            && y <=ilitek_data->keyinfo[j].y + ilitek_data->key_ylen)) {
            
            input_report_key(ilitek_data->input_dev, ilitek_data->keyinfo[j].id, 1);
            ilitek_data->keyinfo[j].status = 1;
            ilitek_data->touch_key_hold_press = true;
            ilitek_data->is_touched = true;
            tp_log_debug
            (PRINT_KEY_REPORT,"Key, Keydown ID=%d, X=%d, Y=%d, key_status=%d\n",
             ilitek_data->keyinfo[j].id, x, y,
             ilitek_data->keyinfo[j].status);
            break;
        }
    }
    
    return 0;
}

static int ilitek_check_key_release(int x, int y, int check_point)
{
    int j = 0;
    
    for (j = 0; j < ilitek_data->keycount; j++) {
        if (check_point) {
            if ((ilitek_data->keyinfo[j].status == 1)&&  
                (x < ilitek_data->keyinfo[j].x || 
                x > ilitek_data->keyinfo[j].x +ilitek_data->key_xlen || 
                y < ilitek_data->keyinfo[j].y|| 
                y >ilitek_data->keyinfo[j].y +  ilitek_data->key_ylen)) {
                
                input_report_key(ilitek_data->input_dev,
                                 ilitek_data->keyinfo[j].id,
                                 0);
                ilitek_data->keyinfo[j].status = 0;
                ilitek_data->touch_key_hold_press = false;
                tp_log_debug
                (PRINT_KEY_REPORT,"Key, Keyout ID=%d, X=%d, Y=%d, key_status=%d\n",
                 ilitek_data->keyinfo[j].id, x, y,
                 ilitek_data->keyinfo[j].status);
                break;
            }
        } else {
            if ((ilitek_data->keyinfo[j].status == 1)) {
                input_report_key(ilitek_data->input_dev,
                                 ilitek_data->keyinfo[j].id,
                                 0);
                ilitek_data->keyinfo[j].status = 0;
                ilitek_data->touch_key_hold_press = false;
                tp_log_debug
                (PRINT_KEY_REPORT,"Key, Keyout ID=%d, X=%d, Y=%d, key_status=%d\n",
                 ilitek_data->keyinfo[j].id, x, y,
                 ilitek_data->keyinfo[j].status);
                break;
            }
        }
    }
    
    return 0;
}

static irqreturn_t ilitek_i2c_isr(int irq, void *dev_id)
{
    unsigned long irqflag = 0;

    if (ilitek_data->firmware_updating) {
        tp_log_err("firmware_updating return\n");
        return IRQ_HANDLED;
    }
    tp_log_debug(PRINT_I2C_IRQ,"Enter i2c IRQ\n");    
    spin_lock_irqsave(&ilitek_data->irq_lock, irqflag);
    
    if (ilitek_data->irq_status) {
        ilitek_data->irq_status = false;
        disable_irq_nosync(ilitek_data->client->irq);
        ilitek_data->irq_trigger = true;
        wake_up_interruptible(&waiter);
    }
    
    spin_unlock_irqrestore(&ilitek_data->irq_lock, irqflag);
    return IRQ_HANDLED;
}

static int ilitek_request_irq(void)
{
    int ret = 0;
    spin_lock_init(&ilitek_data->irq_lock);
    ilitek_data->irq_status = true;
    ilitek_data->client->irq = gpio_to_irq(ilitek_data->irq_gpio);
    tp_log_info("ilitek_data->client->irq = %d\n", ilitek_data->client->irq);
                
    if (ilitek_data->client->irq > 0) {
        //ret = request_irq(ilitek_data->client->irq, ilitek_i2c_isr, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "ilitek_i2c_irq", ilitek_data);
        ret =
            request_threaded_irq(ilitek_data->client->irq, NULL,
                                 ilitek_i2c_isr,
                                 /*IRQF_TRIGGER_FALLING */
                                 IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                                 "touch,irq-gpio", ilitek_data);
                                 
        if (ret != 0) {
            tp_log_err("ilitek_request_irq, error\n");
        }
    } else {
        tp_log_err("ilitek_request_irq pin error\n");
        ret = -1;
    }
    
    if (ret >= 0)
        ilitek_data->irq_status = true;
        
    return ret;
}

static u8 ilitek_calculate_check_sum(uint8_t * p_msg, u32 length)
{
    int32_t check_sum = 0;
    uint32_t i;
    
    for (i = 0; i < length; i++) {
        check_sum += p_msg[i];
    }
    
    return (uint8_t) ((-check_sum) & 0xFF);
}

static s32 ilitek_read_data_and_report_camaro(void)
{
    uint8_t buf[1000] = {
        0
    };
    uint8_t check_sum = 0;
    uint16_t read_packet_len = 0;
    int16_t temp;
    uint8_t point_len = 0;
    uint8_t data_shift = 0;
    int ret = 0;
    int report_max_point = MUTUAL_MAX_TOUCH_NUM;
    int release_point = 0;
    uint16_t i = 0;
    int x = 0;
    int y = 0;
    struct input_dev *input = ilitek_data->input_dev;
    
    if (frimware_mode == CAMARO_FIRMWARE_MODE_DEBUG_MODE) {
        read_packet_len = ilitek_data->debug_mode_len;
        point_len = 3;
        data_shift = 5;
    } else {
        read_packet_len = 43;
        point_len = 4;
        data_shift = 1;
    }
    
    ret = ilitek_i2c_read(buf, read_packet_len);
    
    if (ret < 0) {
        tp_log_err("get touch information err\n");
        
        if (ilitek_data->is_touched) {
            ilitek_touch_release_all_point();
            ilitek_check_key_release(x, y, 0);
            return ret;
        }
    }
    
    if ((frimware_mode == CAMARO_FIRMWARE_MODE_DEBUG_MODE)
        && ((buf[0] & 0xff) != ILITEK_DEBUG_MODE
            || buf[3] != ILITEK_PACKET_TYPE_TOOTH_PATTERN)) {
        tp_log_err
        ("Debue mode data format not correct  header=0x%x, type =0x%x \n",
         buf[0], buf[3]);
        return -1;
    }
    
    check_sum = ilitek_calculate_check_sum(buf, (read_packet_len - 1));
    
    if (check_sum != buf[read_packet_len - 1]) {
        tp_log_err("check sum error\n");
        return -1;
    }
    
    for (i = 0; i < report_max_point; i++) {
        if ((buf[(point_len * i) + data_shift] == 0xFF)
            && (buf[(point_len * i) + data_shift + 1] == 0xFF)
            && (buf[(point_len * i) + data_shift + 2] == 0xFF)) {
            release_point++;
#ifdef ILITEK_TOUCH_PROTOCOL_B
            ilitek_touch_release(i);
#endif
            continue;
        }
        
        ilitek_data->touch_flag[i] = 1;
        x = (((buf[(point_len * i) + data_shift] & 0xF0) << 4) |
             (buf[(point_len * i) + data_shift + 1])) *ilitek_data->screen_max_x / TPD_WIDTH;
        y = (((buf[(point_len * i) + data_shift] & 0x0F) << 8) |
             (buf[(point_len * i) + data_shift + 2])) * ilitek_data->screen_max_y / TPD_HEIGHT;
            
        //tp_log_debug(PRINT_NONE,"ilitek x = %d y = %d\n", x, y);
        if (ilitek_data->system_suspend) {
            tp_log_info("system is suspend not report point\n");
        } else {
            if (!(ilitek_data->is_touched)) {
                ilitek_check_key_down(x, y);
            }
            
            if (!(ilitek_data->touch_key_hold_press)) {
                ilitek_data->is_touched = true;
                
                if (ILITEK_REVERT_X)
                    x = ilitek_data->screen_max_x - x +
                        ilitek_data->screen_min_x;
                        
                if (ILITEK_REVERT_Y)
                    y = ilitek_data->screen_max_y - y +
                        ilitek_data->screen_min_y;
                        
                tp_log_debug
                (PRINT_FINGER_REPORT,"Point, ID=%02X, X=%04d, Y=%04d\n", i, x, y);
                ilitek_touch_down(i, x, y, 10);
                
                if (frimware_mode ==CAMARO_FIRMWARE_MODE_DEBUG_MODE)
                    printk(" Pnt[%d]=(0x%x ,0x%x)\n ", i,x, y);
            }
        }
    }
    
    if (frimware_mode == CAMARO_FIRMWARE_MODE_DEBUG_MODE) {
        tp_log_info("Read Raw data:\n");
        
        for (i = 0; i <(ilitek_data->mc_x_channel_num * ilitek_data->mc_y_channel_num); i++) {
            if (i % ilitek_data->mc_x_channel_num == 0)
                printk("\n");
                
            temp =((buf[35 + (i * 2)] << 8) + buf[36 + (i * 2)]);
            printk("%d, ", temp);
        }
    }
    
    tp_log_debug(PRINT_FINGER_REPORT,"release point counter =  %d", release_point);
    
    if (release_point == report_max_point) {
        if (ilitek_data->touch_before_suspend == 0) {
            if (ilitek_data->is_touched)
                ilitek_touch_release_all_point();
                
            ilitek_check_key_release(x, y, 0);
            ilitek_data->is_touched = false;
#ifdef ILITEK_CLICK_WAKEUP
            
            if (ilitek_data->system_suspend == true) {
                input_report_key(input, KEY_POWER, 1);
                input_sync(input);
                input_report_key(input, KEY_POWER, 0);
                input_sync(input);
            }
            
#endif
        } else {
            ilitek_data->touch_before_suspend = 0;
        }
    }
    
    if (ilitek_data->system_suspend == false)
        input_sync(input);
        
    return ret;
}

static int ilitek_i2c_process_and_report(void)
{
    int ret = 0;
    mutex_lock(&ilitek_data->ilitek_mutex);
    ret = ilitek_read_data_and_report_camaro();
    mutex_unlock(&ilitek_data->ilitek_mutex);
    return ret;
}


#ifdef ILITEK_UPDATE_FW
static int ilitek_update_thread(void *arg)
{
    int ret = 0;
    tp_log_info("\n");
    
    if (kthread_should_stop()) {
        tp_log_info("ilitek_update_thread, stop\n");
        return -1;
    }
    
    mdelay(100);
    
    ilitek_data->firmware_updating = true;
    ret = ilitek_upgrade_firmware(0);
    ret = ilitek_read_tp_info();    
    ilitek_data->firmware_updating = false;
  
    
    if (ret < 0) {
        tp_log_err("read info error \n");
        goto out;
    }
    
    ret = ilitek_set_input_param();
    
    if (ret < 0) {
        tp_log_err("register input device, error\n");
        goto out;
    }
    
    ret = ilitek_request_irq();
    
    if (ret < 0) {
        tp_log_err("ilitek_request_irq, error\n");
        goto out;
    }
    
out:
    return ret;
}


#endif
static int ilitek_irq_handle_thread(void *arg)
{
    int ret = 0;
    struct sched_param param = {.sched_priority = 4 };
    sched_setscheduler(current, SCHED_RR, &param);
    tp_log_debug(PRINT_I2C_IRQ,"%s, enter\n", __func__);
    
    while (!kthread_should_stop() && !ilitek_exit_report) {
        set_current_state(TASK_INTERRUPTIBLE);
        wait_event_interruptible(waiter, ilitek_data->irq_trigger);
        ilitek_data->irq_trigger = false;
        set_current_state(TASK_RUNNING);
        
        if (ilitek_i2c_process_and_report() < 0) {
            tp_log_err("I2C report point process error\n");
        }
        
        ilitek_irq_enable();
    }
    
    tp_log_err("%s, exit\n", __func__);
    return ret;
}

void ilitek_suspend(void)
{    
    if (ilitek_data->operation_protection || ilitek_data->firmware_updating) {
        tp_log_info
        ("operation_protection or firmware_updating return\n");
        return;
    }

#ifdef ILITEK_ESD_PROTECTION
    ilitek_data->esd_check = false;
    cancel_delayed_work_sync(&ilitek_data->esd_work);
#endif    
    
#ifndef ILITEK_CLICK_WAKEUP
    ilitek_irq_disable();
#endif
    
    if (ilitek_data->is_touched == false)
        ilitek_data->touch_before_suspend = 0;
    else
        ilitek_data->touch_before_suspend = 1;
        
    ilitek_data->system_suspend = true;
}

void ilitek_resume(void)
{
    tp_log_info("Resume\n");
    
    if (ilitek_data->operation_protection || ilitek_data->firmware_updating) {
        tp_log_info
        ("operation_protection or firmware_updating return\n");
        return;
    }
    
    ilitek_reset(HARDWARE_RESET_DELAY_TIME);
#ifndef ILITEK_CLICK_WAKEUP
    ilitek_irq_enable();
#endif
    ilitek_data->system_suspend = false;
#ifdef ILITEK_ESD_PROTECTION
    ilitek_data->esd_check = true;
    
    if (ilitek_data->esd_wq) {
        queue_delayed_work(ilitek_data->esd_wq, &ilitek_data->esd_work,
                           ilitek_data->esd_delay);
    }
    
#endif
    ilitek_touch_release_all_point();
    ilitek_check_key_release(0, 0, 0);
}


#ifdef CONFIG_FB
static int ilitek_fb_notifier_callback(struct notifier_block *self,
                                       unsigned long event, void *data)
{
    struct fb_event *ev_data = data;
    int *blank;
    
    if (ev_data && ev_data->data && event == FB_EVENT_BLANK) {
        blank = ev_data->data;
        
        if (*blank == FB_BLANK_POWERDOWN) {
            ilitek_suspend();
        } else if (*blank == FB_BLANK_UNBLANK
                   || *blank == FB_BLANK_NORMAL) {
            ilitek_resume();
        }
    }
    
    return 0;
}


#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void ilitek_early_suspend(struct early_suspend *h)
{
    ilitek_suspend();
}
static void ilitek_late_resume(struct early_suspend *h)
{
    ilitek_resume();
}
#endif
int ilitek_get_gpio_num(void)
{
    int ret = 0;
#ifdef GET_DTS_GPIO_NUM
#ifdef CONFIG_OF
    struct device *dev = &(ilitek_data->client->dev);
    struct device_node *np = dev->of_node;
    ilitek_data->reset_gpio =
        of_get_named_gpio(np, "touch,reset-gpio", 0);
        
    if (ilitek_data->reset_gpio < 0) {
        tp_log_err("reset_gpio = %d\n", ilitek_data->reset_gpio);
        ret = -1;
    }
    
    ilitek_data->irq_gpio = of_get_named_gpio(np, "touch,irq-gpio", 0);
    
    if (ilitek_data->irq_gpio < 0) {
        tp_log_err("irq_gpio = %d\n", ilitek_data->irq_gpio);
        ret = -1;
    }
    
#endif
#else
    ilitek_data->reset_gpio = ILITEK_RESET_GPIO;
    ilitek_data->irq_gpio = ILITEK_IRQ_GPIO;
#endif
    tp_log_info("reset_gpio = %d irq_gpio = %d\n",
                ilitek_data->reset_gpio, ilitek_data->irq_gpio);
    return ret;
}

int ilitek_request_gpio(void)
{
    int ret = -1;
    ilitek_get_gpio_num();
    
    if (ilitek_data->reset_gpio > 0) {
        ret = gpio_request(ilitek_data->reset_gpio, "touch,reset-gpio");
            
        if (ret < 0) {
            tp_log_err
            ("Failed to request reset_gpio so free retry\n");
            gpio_free(ilitek_data->reset_gpio);
            ret = gpio_request(ilitek_data->reset_gpio, "touch,reset-gpio");
                             
            if (ret < 0) {
                tp_log_err("Failed to request reset_gpio \n");
            }
        }
        
        if (ret == 0) {
            ret = gpio_direction_output(ilitek_data->reset_gpio, 1);
                
            if (ret < 0)
                tp_log_err ("Failed to direction output rest gpio err\n");
        }
    }
    
    if (ilitek_data->irq_gpio > 0) {
        ret = gpio_request(ilitek_data->irq_gpio, "touch,irq-gpio");
        
        if (ret < 0) {
            tp_log_err ("Failed to request irq_gpio so free retry\n");
            gpio_free(ilitek_data->irq_gpio);
            ret = gpio_request(ilitek_data->irq_gpio, "touch,irq-gpio");
                             
            if (ret < 0)
                tp_log_err("Failed to request irq_gpio \n");
        }
        
        if (ret == 0) {
            ret = gpio_direction_input(ilitek_data->irq_gpio);
            
            if (ret < 0)
                tp_log_err
                ("Failed to direction input irq gpio err\n");
        }
    }
    
    return ret;
}

void ilitek_power_on(bool status)
{
#ifdef ILITEK_ENABLE_REGULATOR_POWER_ON
    int ret = 0;
    tp_log_info("%s\n", status ? "POWER ON" : "POWER OFF");
    
    if (status) {
        if (ilitek_data->vdd) {
            ret = regulator_enable(ilitek_data->vdd);
            
            if (ret < 0)
                tp_log_err("regulator_enable vdd fail\n");
        }
        
        if (ilitek_data->vdd_i2c) {
            ret = regulator_enable(ilitek_data->vdd_i2c);
            
            if (ret < 0)
                tp_log_err("regulator_enable vdd_i2c fail\n");
        }
    } else {
        if (ilitek_data->vdd) {
            ret = regulator_disable(ilitek_data->vdd);
            
            if (ret < 0)
                tp_log_err("regulator_enable vdd fail\n");
        }
        
        if (ilitek_data->vdd_i2c) {
            ret = regulator_disable(ilitek_data->vdd_i2c);
            
            if (ret < 0)
                tp_log_err("regulator_enable vdd_i2c fail\n");
        }
    }
    
#endif
}

static int ilitek_create_esdandcharge_workqueue(void)
{
#ifdef ILITEK_ESD_PROTECTION
    INIT_DELAYED_WORK(&ilitek_data->esd_work, ilitek_esd_check);
    ilitek_data->esd_wq = create_singlethread_workqueue("ilitek_esd_wq");
    
    if (!ilitek_data->esd_wq) {
        tp_log_err("create workqueue esd work err\n");
    } else {
        ilitek_data->esd_check = true;
        ilitek_data->esd_delay = 2 * HZ;
        queue_delayed_work(ilitek_data->esd_wq, &ilitek_data->esd_work,
                           ilitek_data->esd_delay);
    }
    
#endif
    return 0;
}

static int ilitek_register_resume_suspend(void)
{
    int ret = 0;
#ifdef CONFIG_FB
    ilitek_data->fb_notif.notifier_call = ilitek_fb_notifier_callback;
    ret = fb_register_client(&ilitek_data->fb_notif);
    
    if (ret) {
        tp_log_err("Unable to register fb_notifier: %d\n", ret);
    }
    
#elif defined(CONFIG_HAS_EARLYSUSPEND)
    ilitek_data->early_suspend.level =
        EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
    ilitek_data->early_suspend.suspend = ilitek_early_suspend;
    ilitek_data->early_suspend.resume = ilitek_late_resume;
    register_early_suspend(&ilitek_data->early_suspend);
#endif
    return ret;
}

u8 ilitek_reg_get_lowbyte_value(u16 addr)
{
    u8 data[3] = {0x10, (addr >> 8) & 0xFF, addr & 0xFF};
    ilitek_i2c_write_and_read(data, 3, 5, data, 1);
    return (data[0]);
}

void ilitek_reg_set_lowbyte_value(uint16_t addr, uint8_t data)
{
    uint8_t tx_data[4] = { 0x10, (addr >> 8) & 0xFF, addr & 0xFF, data };
    ilitek_i2c_write_and_read(tx_data, 4, 5, tx_data, 0);
}
u16 ilitek_reg_get_16bit_value(uint16_t addr)
{
    uint8_t data[3] = { 0x10, (addr >> 8) & 0xFF, addr & 0xFF };
    ilitek_i2c_write_and_read(data, 3, 5, data, 2);
    return (data[1] << 8 | data[0]);
}

u16 ilitek_reg_set_16bit_value(uint16_t addr, uint16_t data)
{
    s32 rc = 0;
    uint8_t tx_data[5] = {
        0x10, (addr >> 8) & 0xFF, addr & 0xFF, data & 0xFF, data >> 8
    };
    rc = ilitek_i2c_write_and_read(tx_data, 5, 5, tx_data, 0);
    return rc;
}

void ilitek_reg_set_16bit_value_on(u16 addr, u16 bit)	//Set bit on nData from 0 to 1
{
    u16 temp = ilitek_reg_get_16bit_value(addr);
    temp |= bit;
    ilitek_reg_set_16bit_value(addr, temp);
}
void ilitek_reg_set_16bit_value_off(u16 addr, u16 bit)	//Set bit on nData from 1 to 0
{
    u16 temp = ilitek_reg_get_16bit_value(addr);
    temp &= (~bit);
    ilitek_reg_set_16bit_value(addr, temp);
}
u16 ilitek_reg_get_16bit_value_by_address_mode(u16 addr,
        address_mode_e
        address_mode)
{
    u16 nData = 0;
    
    if (address_mode == ADDRESS_MODE_16BIT) {
        addr = addr - (addr & 0xFF) + ((addr & 0xFF) << 1);
    }
    
    nData = ilitek_reg_get_16bit_value(addr);
    return nData;
}

void ilitek_reg_set_16bit_value_by_address_mode(u16 addr, u16 nData,
        address_mode_e address_mode)

{
    if (address_mode == ADDRESS_MODE_16BIT) {
        addr = addr - (addr & 0xFF) + ((addr & 0xFF) << 1);
    }
    
    ilitek_reg_set_16bit_value(addr, nData);
}

void ilitek_reg_mask_16bit_value(u16 addr, u16 mask, u16 data,
                                 address_mode_e address_mode)
{
    /*0:8bit ,1:16 bit */
    u16 tmp_data = 0;
    
    if (data > mask) {
        return;
    }
    
    tmp_data = ilitek_reg_get_16bit_value_by_address_mode(addr, address_mode);
    tmp_data = (tmp_data & (~mask));
    tmp_data = (tmp_data | data);
    ilitek_reg_set_16bit_value_by_address_mode(addr, tmp_data,
            address_mode);
}

void ilitek_enter_ice_mode(void)
{
    uint8_t data[5] = {
        0x53, 0x45, 0x52, 0x44, 0x42
    };
    ilitek_data->client->addr = SLAVE_I2C_ID_DBBUS;
    /* Erase TP Flash first */
    /*DbBus Enter Seria lD ebug Mode */
    ilitek_i2c_write_and_read(data, 5, 5, data, 0);
    /*DBbus stop MCU */
    data[0] = 0x37;
    ilitek_i2c_write_and_read(data, 1, 5, data, 0);
    /*DBbus I2C use bus */
    data[0] = 0x35;
    ilitek_i2c_write_and_read(data, 1, 5, data, 0);
    /*DBbus I2C reshape */
    data[0] = 0x71;
    ilitek_i2c_write_and_read(data, 1, 5, data, 0);
}
void ilitek_exit_ice_mode(void)
{
    uint8_t data[5];
    /*DBbus i2c not use bus */
    data[0] = 0x34;
    ilitek_i2c_write_and_read(data, 1, 5, data, 0);
    /*DBbus not stop MCU */
    data[0] = 0x36;
    ilitek_i2c_write_and_read(data, 1, 5, data, 0);
    /*DBbus exit serial debug mode */
    data[0] = 0x46;
    ilitek_i2c_write_and_read(data, 1, 5, data, 0);
    ilitek_data->client->addr = SLAVE_I2C_ID_DWI2C;
}
int ilitek_get_chip_type(void)
{
    u16 chip_type = 0;
    int ret = 0;
    /*enter ice mode */
    ilitek_enter_ice_mode();
    /* Stop MCU */
    ilitek_reg_set_lowbyte_value(0x0FE6, 0x01);
    /*Set Password */
    ilitek_reg_set_lowbyte_value(0x1616, 0xAA);
    ilitek_reg_set_lowbyte_value(0x1617, 0x55);
    ilitek_reg_set_lowbyte_value(0x1618, 0xA5);
    ilitek_reg_set_lowbyte_value(0x1619, 0x5A);
    /*disable cpu read, in,tial); read */
    ilitek_reg_set_lowbyte_value(0x1608, 0x20);
    ilitek_reg_set_lowbyte_value(0x1606, 0x20);
    ilitek_reg_set_lowbyte_value(0x1607, 0x00);
    /*set info block */
    ilitek_reg_set_lowbyte_value(0x1607, 0x08);
    /* set info double buffer */
    ilitek_reg_set_lowbyte_value(0x1604, 0x01);
    /*set eflash mode to read mode */
    ilitek_reg_set_lowbyte_value(0x1606, 0x01);
    ilitek_reg_set_lowbyte_value(0x1610, 0x01);
    ilitek_reg_set_lowbyte_value(0x1611, 0x00);
    /*set read address */
    ilitek_reg_set_lowbyte_value(0x1600, 0x05);
    ilitek_reg_set_lowbyte_value(0x1601, 0x00);
    chip_type = ilitek_reg_get_16bit_value(0x160A) & 0xFFFF;
    /*Star MCU */
    ilitek_reg_set_lowbyte_value(0x0FE6, 0x00);
    tp_log_info
    ("----------------------ILI Chip Type=0x%x-------------------------\n",
     chip_type);
    /*read chip id */
    chip_type = ilitek_reg_get_16bit_value(0x1ECC) & 0xFF;
    tp_log_info
    ("----------------------ILI Chip Type=0x%x-------------------------\n",
     chip_type);
    /*exit ice mode */
    ilitek_exit_ice_mode();
    ilitek_reset(HARDWARE_RESET_DELAY_TIME);
    
    if (chip_type != CHIP_TYPE_CAMARO)
        ret = -1;
        
    return ret;
}

int ilitek_get_platform_firmware_version(void)
{
    uint8_t data[10];
    int ret = 0;
    unsigned char *p_platform_fw_ver;
    data[0] = 0x0C;
    mutex_lock(&ilitek_data->ilitek_mutex);
    ret = ilitek_i2c_write_and_read(data, 1, 5, data, 10);
    p_platform_fw_ver = kzalloc(sizeof(u8) * 10, GFP_KERNEL);
    mutex_unlock(&ilitek_data->ilitek_mutex);
    ilitek_data->p_platform_fw_ver = p_platform_fw_ver;
    sprintf(p_platform_fw_ver, "%.10s", data);
    tp_log_info
    ("platform_firmware 0x%x.0x%x.0x%x.0x%x.0x%x.0x%x.0x%x.0x%x.0x%x.0x%x\n",
     p_platform_fw_ver[0], p_platform_fw_ver[1], p_platform_fw_ver[2],
     p_platform_fw_ver[3], p_platform_fw_ver[4], p_platform_fw_ver[5],
     p_platform_fw_ver[6], p_platform_fw_ver[7], p_platform_fw_ver[8],
     p_platform_fw_ver[9]);
    tp_log_info("Platform_firmware = %s \n", p_platform_fw_ver);
    
    if (p_platform_fw_ver[0] * 100000 + (p_platform_fw_ver[1]) * 100 +
        p_platform_fw_ver[2] >= 101003) {
        ilitek_data->fw_version_flag = 1;
    }
    
    return ret;
}

int ilitek_get_customer_firmware_version(void)
{
    uint8_t data[4];
    uint16_t major = 0, minor = 0;
    int ret = 0;
    unsigned char *p_customer_fw_ver;
    data[0] = 0x03;
    mutex_lock(&ilitek_data->ilitek_mutex);
    ret = ilitek_i2c_write_and_read(data, 1, 5, data, 4);
    p_customer_fw_ver = kzalloc(sizeof(u8) * 11, GFP_KERNEL);
    mutex_unlock(&ilitek_data->ilitek_mutex);
    ilitek_data->p_customer_fw_ver = p_customer_fw_ver;
    ilitek_data->customer_major = (data[1] << 8) + data[0];
    ilitek_data->customer_minor = (data[3] << 8) + data[2];
    sprintf(p_customer_fw_ver, "%05d.%05d", major, minor);
    
    if (ilitek_data->fw_version_flag) {
        tp_log_info
        ("Customer_ver Major = %d, Minor = %d.%d, _gFwVersion = %s\n",
         ilitek_data->customer_major,
         (ilitek_data->customer_minor & 0xFF),
         ((ilitek_data->customer_minor >> 8) & 0xFF),
         p_customer_fw_ver);
    } else {
        tp_log_info
        ("Customer_ver Major = %d, Minor = %d, _gFwVersion = %s\n",
         ilitek_data->customer_major, ilitek_data->customer_minor,
         p_customer_fw_ver);
    }
    
    return ret;
}

void ilitek_get_pannel_info(void)
{
#ifndef ILITEK_MANUAL_SET_RESOLUTION
    uint8_t data[4];
    int ret = 0;
#endif
	ilitek_data->screen_min_x = 0;
    ilitek_data->screen_min_y = 0;
#ifdef ILITEK_MANUAL_SET_RESOLUTION
    ilitek_data->screen_max_x = TOUCH_SCREEN_X_MAX;
    ilitek_data->screen_max_y = TOUCH_SCREEN_Y_MAX;
    tp_log_info("use LCM resolution(%d, %d)\n", ilitek_data->screen_max_x,
                ilitek_data->screen_max_y);
#else
    data[0] = 0x17;
    mutex_lock(&ilitek_data->ilitek_mutex);
    ret = ilitek_i2c_write_and_read(data, 1, 5, data, 4);
    mutex_unlock(&ilitek_data->ilitek_mutex);
    ilitek_data->screen_max_x = (data[0] << 8) + data[1];
    ilitek_data->screen_max_y = (data[2] << 8) + data[3];
                
    if (ilitek_data->screen_max_x == 0 || ilitek_data->screen_max_y == 0 || ilitek_data->screen_max_x == 0xff || ilitek_data->screen_max_y == 0xff) {
        /*If return data isn't correct, setting to default value */
        ilitek_data->screen_max_x = TOUCH_SCREEN_X_MAX;
        ilitek_data->screen_max_y = TOUCH_SCREEN_Y_MAX;
    }
                
    tp_log_info("Resolution(%d, %d)\n", ilitek_data->screen_max_x,
                ilitek_data->screen_max_y);
#endif
}

int ilitek_read_tp_info(void)
{
    int ret = 0;
    tp_log_info("driver version %d.%d.%d.%d\n",
                ilitek_driver_information[0],
                ilitek_driver_information[1],
                ilitek_driver_information[2],
                ilitek_driver_information[3]);
    ret = ilitek_get_chip_type();
    
    if (ret < 0)
        return ret;
        
    ret = ilitek_get_platform_firmware_version();
    
    if (ret < 0)
        tp_log_err("get platform firmware version I2C error\n");
        
    ret = ilitek_get_customer_firmware_version();
    
    if (ret < 0)
        tp_log_err("get customer firmware version I2C error\n");
        
    ilitek_get_pannel_info();


    return 0;

    
}


#ifdef ILITEK_ENABLE_DMA
static int ilitek_alloc_dma(void)
{
    tpd->dev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
    I2CDMABuf_va =
        (u8 *) dma_alloc_coherent(&tpd->dev->dev, 4096, &I2CDMABuf_pa,
                                  GFP_KERNEL);
                                  
    if (!I2CDMABuf_va) {
        tp_log_err
        ("ilitek [TPD] tpd->dev->dev dma_alloc_coherent error \n");
        I2CDMABuf_va =
            (u8 *) dma_alloc_coherent(NULL, 4096, &I2CDMABuf_pa,GFP_KERNEL);
                                      
        if (!I2CDMABuf_va) {
            tp_log_err
            ("ilitek [TPD] NULL dma_alloc_coherent error\n");
            return -1;
        }
    }
    
    memset(I2CDMABuf_va, 0, 4096);
    //ilitek_data->client->ext_flag |= I2C_DMA_FLAG;
    return 0;
}


#endif
int ilitek_main_probe(struct ilitek_ts_data *ilitek_ts_data)
{
    int ret = 0;
    ilitek_data->client->addr = SLAVE_I2C_ID_DWI2C;
    tp_log_info("default client->addr = 0x%x client->irq = %d\n",
                ilitek_data->client->addr, ilitek_data->client->irq);
    mutex_init(&ilitek_data->ilitek_mutex);
#ifdef ILITEK_ENABLE_DMA
    ilitek_alloc_dma();
#endif
    ilitek_power_on(true);
    ret = ilitek_request_gpio();
    ret = ilitek_read_tp_info();
    
    if (ret != 0) {
        goto read_info_err;
    }
    
    ilitek_data->input_dev = input_allocate_device();
    
    if (ilitek_data->input_dev == NULL) {
        tp_log_err("allocate input device, error\n");
        goto read_info_err;
    }
    
#ifndef ILITEK_UPDATE_FW
    ret = ilitek_set_input_param();
    
    if (ret)
        goto input_register_err;
        
    ret = ilitek_request_irq();
    
    if (ret)
        goto input_register_err;
        
#endif
    ilitek_data->irq_thread =
        kthread_run(ilitek_irq_handle_thread, NULL, "ilitek_irq_thread");
        
    if (ilitek_data->irq_thread == (struct task_struct *)ERR_PTR) {
        ilitek_data->irq_thread = NULL;
        tp_log_err("kthread create ilitek_irq_handle_thread, error\n");
        goto kthread_run_irq_thread_err;
    }
    
#ifdef ILITEK_UPDATE_FW
    ilitek_data->update_thread =
        kthread_run(ilitek_update_thread, NULL, "ilitek_update_thread");
        
    if (ilitek_data->update_thread == (struct task_struct *)ERR_PTR) {
        ilitek_data->update_thread = NULL;
        tp_log_err("kthread create ilitek_update_thread, error\n");
        goto kthread_run_irq_thread_err;
    }
    
#endif
    ilitek_register_resume_suspend();
#ifdef ILITEK_TOOL
    ilitek_create_tool_node();
#endif
    ilitek_create_esdandcharge_workqueue();
    device_init_wakeup(&ilitek_data->client->dev, 1);
    return 0;
kthread_run_irq_thread_err:
#ifndef ILITEK_UPDATE_FW
    free_irq(ilitek_data->client->irq, ilitek_data);
input_register_err:
    input_free_device(ilitek_data->input_dev);
#endif
read_info_err:
#ifdef ILITEK_ENABLE_REGULATOR_POWER_ON
    ilitek_regulator_release();
#endif
    ilitek_free_gpio();
    kfree(ilitek_data);
    ilitek_data = NULL;
    tp_log_err("return -ENODEV\n");
    return -ENODEV;
}

int ilitek_main_remove(struct ilitek_ts_data *ilitek_data)
{
    tp_log_info("\n");
    
    if (ilitek_data != NULL) {
        free_irq(ilitek_data->client->irq, ilitek_data);
#ifdef CONFIG_FB
        fb_unregister_client(&ilitek_data->fb_notif);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
        unregister_early_suspend(&ilitek_data->early_suspend);
#endif
        
        if (ilitek_data->irq_thread != NULL) {
            tp_log_info("irq_thread\n");
            ilitek_exit_report = true;
            ilitek_data->irq_trigger = true;
            wake_up_interruptible(&waiter);
            kthread_stop(ilitek_data->irq_thread);
            ilitek_data->irq_thread = NULL;
        }
        
        if (ilitek_data->input_dev) {
            input_unregister_device(ilitek_data->input_dev);
            ilitek_data->input_dev = NULL;
        }
        
#ifdef ILITEK_TOOL
        ilitek_remove_tool_node();
#endif
#ifdef ILITEK_ESD_PROTECTION
        
        if (ilitek_data->esd_wq) {
            destroy_workqueue(ilitek_data->esd_wq);
            ilitek_data->esd_wq = NULL;
        }
        
#endif
#ifdef ILITEK_ENABLE_REGULATOR_POWER_ON
        ilitek_regulator_release();
#endif
        ilitek_free_gpio();
#ifdef ILITEK_ENABLE_DMA
        
        if (I2CDMABuf_va) {
            dma_free_coherent(&tpd->dev->dev, 250, I2CDMABuf_va, I2CDMABuf_pa);
            I2CDMABuf_va = NULL;
            I2CDMABuf_pa = 0;
        }
        
#endif
        kfree(ilitek_data);
        ilitek_data = NULL;
    }
    
    return 0;
}


