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
#include "mp_common.h"

#ifdef ILITEK_TOOL
// device data
struct dev_data {
    // device number
    dev_t devno;
    // character device
    struct cdev cdev;
    // class device
    struct class *class;
};

static struct dev_data ilitek_dev;
static struct proc_dir_entry *ilitek_proc;
struct proc_dir_entry *ilitek_proc_entry;

static char ilitek_bin_path[256] = "/data/local/tmp/ilitek.bin";

static int katoi(char *string)
{
	int result = 0;
	unsigned int digit;
	int sign;

	if (*string == '-') {
		sign = 1;
		string += 1;
	} else {
		sign = 0;
		if (*string == '+') {
			string += 1;
		}
	}

	for (;; string += 1) {
		digit = *string - '0';
		if (digit > 9)
			break;
		result = (10 * result) + digit;
	}

	if (sign) {
		return -result;
	}
	return result;
}


static int ilitek_file_open(struct inode *inode, struct file *filp)
{
    ilitek_data->operation_protection = true;
    tp_log_info("operation_protection = %d\n",
                ilitek_data->operation_protection);
    return 0;
}

static ssize_t ilitek_file_write(struct file *filp, const char *buf,
                                 size_t count, loff_t * f_pos)
{
    int ret;
    unsigned char buffer[512] = { 0 };
    
    // check the buffer size whether it exceeds the local buffer size or not
    if (count > 512) {
        tp_log_err("buffer exceed 512 bytes\n");
        //return -1;
    }
    
    // copy data from user space
    ret = copy_from_user(buffer, buf, count - 1);
    
    if (ret < 0) {
        tp_log_err("copy data from user space, failed");
        return -1;
    }
    
    if (buffer[count - 2] == 'I' && (count == 20 || count == 52)
        && buffer[0] == 0x77 && buffer[1] == 0x77) {
        tp_log_info("CMD = %d\n", buffer[2]);
        
        switch (buffer[2]) {
        case 13:
            //ilitek_irq_enable();
            tp_log_info("ilitek_irq_enable. do nothing\n");
            break;
            
        case 12:
            //ilitek_irq_disable();
            tp_log_info("ilitek_irq_disable. do nothing\n");
            break;
            
        case 19:
            ilitek_reset(HARDWARE_RESET_DELAY_TIME);
            break;
            
        case 15:
            if (buffer[3] == 0) {
                ilitek_irq_disable();
                tp_log_info("ilitek_irq_disable.\n");
            } else {
                ilitek_irq_enable();
                tp_log_info("ilitek_irq_enable.\n");
            }
            
            break;
            
        case 16:
            ilitek_data->operation_protection = buffer[3];
            tp_log_info("ilitek_data->operation_protection = %d\n",
                        ilitek_data->operation_protection);
            break;
            
        case 18:
            ret = ilitek_i2c_write(&buffer[3], 33);
            
            if (ret < 0) {
                tp_log_err
                ("i2c write error, ret %d, addr %x \n", ret,
                 ilitek_data->client->addr);
                return ret;
            }
            
            return 0;
            break;
            
        default:
            return -1;
        }
    }
    
    if (buffer[count - 2] == 'W') {
        ret = ilitek_i2c_write(buffer, count - 2);
        
        if (ret < 0) {
            tp_log_err("i2c write error, ret %d, addr %x \n", ret,
                       ilitek_data->client->addr);
            return ret;
        } 
    } else if (strcmp(buffer, "irq_status") == 0) {
        tp_log_info("gpio_get_value(i2c.irq_gpio) = %d.\n",
                    gpio_get_value(ilitek_data->irq_gpio));
    } else if (strcmp(buffer, "enable") == 0) {
        enable_irq(ilitek_data->client->irq);
        tp_log_info("irq enable\n");
    } else if (strcmp(buffer, "disable") == 0) {
        disable_irq(ilitek_data->client->irq);
        tp_log_info("irq disable\n");
    } else if (strcmp(buffer, "info") == 0) {
        ilitek_read_tp_info();
    } else if (strcmp(buffer, "reset") == 0) {
        ilitek_reset(HARDWARE_RESET_DELAY_TIME);
    }
    
    tp_log_debug(PRINT_NONE,"ilitek return count = %d\n", (int)count);
    return count;
}


static ssize_t ilitek_file_read(struct file *filp, char *buf, size_t count,
                                loff_t * f_pos)
{
    unsigned char *tmp;
    int ret;
    long rc;
    
    //tp_log_info("%s enter count = %d\n", __func__, count);
    
    if (count > 8192)
        count = 8192;
        
    tmp = kmalloc(count, GFP_KERNEL);
    
    if (tmp == NULL)
        return -ENOMEM;
        
    ret = ilitek_i2c_read(tmp, count);
    
    if (ret < 0) {
        tp_log_err("i2c read error, ret %d,addr %x \n", ret,
                   ilitek_data->client->addr);
    }
    
    rc = copy_to_user(buf, tmp, count);
    kfree(tmp);
    tmp = NULL;
    return ret > 0 ? count : ret;
}


static int ilitek_file_close(struct inode *inode, struct file *filp)
{
    ilitek_data->operation_protection = false;
    tp_log_info("operation_protection = %d\n",
                ilitek_data->operation_protection);
    return 0;
}

static ssize_t ilitek_update_with_bin_read(struct file *pFile,
        char __user * buf, size_t nCount,
        loff_t * pPos)
{
    int ret = 0;
    struct file *filp;
    struct inode *inode;
    unsigned char tmpbuf[256] = { 0 };
    mm_segment_t fs;
    off_t fsize;
    unsigned char *save_hex_content = NULL;
    
    if (*pPos != 0) {
        return 0;
    }
    
    if (!(strstr(ilitek_bin_path, ".bin"))) {
        tp_log_err("ilitek_bin_path is invalid ilitek_bin_path = %s\n",
                   ilitek_bin_path);
        nCount = sprintf(tmpbuf,"ilitek_bin_path is invalid ilitek_bin_path = %s\n",
                    ilitek_bin_path);
        goto out;
    } else {
        filp = filp_open(ilitek_bin_path, O_RDONLY, 0);
        
        if (IS_ERR(filp)) {
            tp_log_err("hex File Open Error ilitek_bin_path = %s\n",
                       ilitek_bin_path);
            nCount =sprintf(tmpbuf,"hex File Open Error ilitek_bin_path = %s\n",
                        ilitek_bin_path);
            goto out;
        } else {
            tp_log_info("hex File Open Right,O_RDONLY %s\n",
                        ilitek_bin_path);
                        
            if (!filp->f_op) {
                tp_log_err("File Operation Method Error\n");
                nCount =sprintf(tmpbuf, "File Operation Method Error\n");
                goto out;
            } else {
                inode = file_inode(filp);
                fsize = inode->i_size;
                tp_log_info("File size:%d \n", (int)fsize);
                save_hex_content = (unsigned char *)vmalloc((int)fsize);
                    
                if (!(save_hex_content)) {
                    tp_log_err("alloctation save_hex_content memory failed\n");
                    nCount =sprintf(tmpbuf, "alloctation save_hex_content memory failed\n");
                    goto out;
                }
                
                fs = get_fs();
                set_fs(KERNEL_DS);
                filp->f_op->read(filp, save_hex_content, fsize,
                                 &(filp->f_pos));
                set_fs(fs);
                filp_close(filp, NULL);
            }
        }
    }
    
    ilitek_irq_disable();
    ret = ilitek_read_tp_info();
    
    if (ret < 0) {
        tp_log_err("I2C error,don't upgrate %d\n", ret);
        nCount = sprintf(tmpbuf, "I2C error,don't upgrate= %d\n", ret);
        goto out;
    } else {
        target_fw_update_ptr = save_hex_content;
       
        ilitek_data->firmware_updating = true;
        ret = ilitek_upgrade_firmware(1);
        ilitek_data->firmware_updating = false;      

        if (ret < 0) {
            tp_log_err("upgrade fail ret = %d\n", ret);
            nCount = sprintf(tmpbuf, "upgrade fail ret = %d\n", ret);
            goto out;
        }
    }
    
    ret = ilitek_read_tp_info();
    
    if (ret < 0)
        tp_log_err("read TP info fail\n");
        
out:
    ilitek_irq_enable();
    
    if (save_hex_content) {
        vfree(save_hex_content);
        save_hex_content = NULL;
    }
    
    *pPos += nCount;
    return nCount;
}

static ssize_t ilitek_update_with_bin_write(struct file *pFile,
        const char __user * buf,
        size_t size, loff_t * pPos)
{
    int i = 0, ret = 0;
    unsigned char buffer[512] = { 0 };
    tp_log_info("size = %d\n", (int)size);
    ret = copy_from_user(buffer, buf, size);
    
    if (size > 256) {
        tp_log_err("size > 256 not support size = %d\n", (int)size);
    } else {
        for (i = 0; i < (size - 1); i++) {
            ilitek_bin_path[i] = buffer[i];
        }
        
        ilitek_bin_path[size - 1] = '\0';
        tp_log_info("ilitek_bin_path = %s\n", ilitek_bin_path);
    }
    
    return size;
}

static ssize_t ilitek_tp_information_read(struct file *pFile,
        char __user * buf, size_t nCount,
        loff_t * pPos)
{
    int ret = 0;
    unsigned char tmpbuf[256] = { 0 };
    tp_log_info("\n");
    
    if (*pPos != 0) {
        return 0;
    }
    
    ilitek_irq_disable();
    ret = ilitek_read_tp_info();
    tp_log_info("FW SW_ID = %d\n",ILITEK_UPDATE_SW_ID);
    ilitek_irq_enable();
    
    if (ret < 0) {
        tp_log_err("ilitek_read_tp_info err ret = %d\n", ret);
        nCount =sprintf(tmpbuf, "ilitek firmware version read error ret = %d\n", ret);
    } else {
        nCount = sprintf(tmpbuf, "ilitek customer version is %d.%d\n",
                    ilitek_data->customer_major,
                    ilitek_data->customer_minor);
    }
  

    
    *pPos += nCount;
    ret = copy_to_user(buf, tmpbuf, nCount);
    return nCount;
}


static ssize_t ilitek_procfs_firmware_mode_read(struct file *pFile,
        char __user * pBuffer,
        size_t nCount, loff_t * pPos)
{

    int ret = 0;
    unsigned char tmpbuf[256] = { 0 };
    
    if (*pPos != 0) {
        return 0;
    }
   
    
    if (frimware_mode == CAMARO_FIRMWARE_MODE_DEMO_MODE) {
        nCount = sprintf(tmpbuf, "ilitek firmware demo mode %d\n",frimware_mode);
    } else if (frimware_mode == CAMARO_FIRMWARE_MODE_DEBUG_MODE) {
        nCount =sprintf(tmpbuf, "ilitek firmware debug mode %d\n",frimware_mode);
    }else{
        nCount = sprintf(tmpbuf, "ilitek firmware  mode undefine\n");
    }
    
    *pPos += nCount;
    ret = copy_to_user(pBuffer, tmpbuf, nCount);
    return nCount;
}

static ssize_t ilitek_procfs_firmware_mode_write(struct file *pFile,
        const char __user * pBuffer,
        size_t nCount, loff_t * pPos)
{
    uint32_t mode;
    uint8_t data[10], ret = 0, i = 0;
    char buf[16] = { 0 };
    
    if (copy_from_user(buf, pBuffer, nCount)) {
        tp_log_err("copy_from_user() failed\n");
        return -EFAULT;
    }
    
    if (buf != NULL) {
        sscanf(buf, "%x", &mode);
        tp_log_info("firmware mode = 0x%x\n", mode);
        
        ilitek_irq_disable();	//Skip finger touch ISR handling temporarily for device driver can send change firmware mode i2c command to firmware.
        mutex_lock(&ilitek_data->ilitek_mutex);
        if (mode == CAMARO_FIRMWARE_MODE_DEMO_MODE || mode == CAMARO_FIRMWARE_MODE_DEBUG_MODE) {
            data[0] = 0x02;
            data[1] = (u8) mode;
            
            while (i < 5) {
                ret =ilitek_i2c_write_and_read(data, 2, 5, data,0);
                                              
                if (ret > 0)
                    break;
                    
                i++;
            }
            
            if (i == 5)
                tp_log_err("Change firmware mode failed\n");
            else
                tp_log_info("Change firmware mode success\n");
                            
        }

        data[0] = 0x01;
        ret = ilitek_i2c_write_and_read(data, 1, 5, data, 10);

        if (data[1] == CAMARO_FIRMWARE_MODE_DEBUG_MODE && data[2] == 0xA7
        && data[5] == ILITEK_PACKET_TYPE_TOOTH_PATTERN) {
        
            frimware_mode = CAMARO_FIRMWARE_MODE_DEBUG_MODE;
            tp_log_info("*** Firmware Debug Mode = 0x%x ***\n",frimware_mode);
            ilitek_data->debug_mode_len = (data[3] << 8) + data[4];
            ilitek_data->mc_y_channel_num = data[6];
            ilitek_data->mc_x_channel_num = data[7];
            ilitek_data->sc_y_channel_num = data[8];
            ilitek_data->sc_x_channel_num = data[9];
            tp_log_info("Mode PacketHeader = 0x%x\n", data[2]);
            tp_log_info("PacketLength = %d\n", ilitek_data->debug_mode_len);
            tp_log_info("Type = 0x%x\n", data[5]);
            tp_log_info("mc_y_channel_num = %d\n",
            ilitek_data->mc_y_channel_num);
            tp_log_info("mc_x_channel_num = %d\n",
            ilitek_data->mc_x_channel_num);
            tp_log_info("sc_y_channel_num = %d\n",
            ilitek_data->sc_y_channel_num);
            tp_log_info("sc_x_channel_num = %d\n",
            ilitek_data->sc_x_channel_num);
        } else if (data[1] == CAMARO_FIRMWARE_MODE_DEMO_MODE
            && data[2] == 0x5A) {
            frimware_mode = CAMARO_FIRMWARE_MODE_DEMO_MODE;
            tp_log_info("*** Firmware Demo Mode = 0x%x ***\n",
            frimware_mode);
        }

        mutex_unlock(&ilitek_data->ilitek_mutex);
        ilitek_irq_enable();


        }

    return nCount;
}

static ssize_t ilitek_proc_mp_test_write(struct file *pFile,
        const char __user * pBuffer,
        size_t nCount, loff_t * pPos)
{
    return nCount;
}

static ssize_t ilitek_proc_debug_level_read(struct file *filp, char __user *buff, size_t size, loff_t *pPos)
{
    int res = 0;
    uint32_t len = 0;
    char buf[16] = { 0 };

    if (*pPos != 0)
    	return 0;

    len = sprintf(buf, "%d", print_debug_level);

    tp_log_info("Current DEBUG Level = %d\n", print_debug_level);
    tp_log_info("You can set one of levels for debug as below:\n");
    tp_log_info("PRINT_NONE = %d\n", PRINT_NONE);
    tp_log_info("PRINT_FINGER_REPORT = %d\n", PRINT_FINGER_REPORT);
    tp_log_info("PRINT_KEY_REPORT = %d\n", PRINT_KEY_REPORT);
    tp_log_info("PRINT_FIRMWARE_UPGRATE = %d\n", PRINT_FIRMWARE_UPGRATE);
    tp_log_info("PRINT_MPTEST = %d\n", PRINT_MPTEST);
    tp_log_info("PRINT_I2C_IRQ = %d\n", PRINT_I2C_IRQ);    
    tp_log_info("PRINT_ESD_CHECK = %d\n", PRINT_ESD_CHECK);
    tp_log_info("PRINT_ALL = %d\n", PRINT_ALL);

    res = copy_to_user((uint32_t *) buff, &print_debug_level, len);
    if (res < 0) {
    	tp_log_info("Failed to copy data to user space\n");
    }

    *pPos = len;

    return len;
}

static ssize_t ilitek_proc_debug_level_write(struct file *filp, const char *buff, size_t size, loff_t *pPos)
{
    int res = 0;
    char cmd[10] = { 0 };

    if (buff != NULL) {
    	res = copy_from_user(cmd, buff, size - 1);
    	if (res < 0) {
    		tp_log_info("copy data from user space, failed\n");
    		return -1;
    	}
    }

    print_debug_level = katoi(cmd);

   tp_log_info("debug_level = %d\n", print_debug_level);

    return size;
}


static ssize_t ilitek_proc_idle_mode_read(struct file *filp, char __user *buff, size_t size, loff_t *pPos)
{
        int res = 0;
        uint32_t len = 0;
        uint8_t data[2];    
        if (*pPos != 0)
                return 0;

        mutex_lock(&ilitek_data->ilitek_mutex);  
        data[0] = 0x6D;
        res = ilitek_i2c_write_and_read(data,1, 5, data,1);
        mutex_unlock(&ilitek_data->ilitek_mutex);

        if(data[0] == 0)
	tp_log_info("Current state is disable %d\n",data[0]);
        else
	tp_log_info("Current state is eisable %d\n",data[0]);
        
        *pPos = len;

    return len;
}

static ssize_t ilitek_proc_idle_mode_write(struct file *filp, const char *buff, size_t size, loff_t *pPos)
{
    int res = 0,timeout=100,temp;
    char cmd[10] = { 0 };
    uint8_t data[2];

    if (buff != NULL) {
    	res = copy_from_user(cmd, buff, size - 1);
    	if (res < 0) {
    		tp_log_info("copy data from user space, failed\n");
    		return -1;
    	}
    }
        
        temp= katoi(cmd);

/*0: disable  1:enable*/
        if(temp !=0 && temp !=1)
        {
               tp_log_err("Idle mode setting data fail = %d\n",temp);
               return size;
        }                

        mutex_lock(&ilitek_data->ilitek_mutex);  
        while(data[0] != temp)
        {
                 data[0] = 0x6C;
                 data[1] = temp+1; 
                 res = ilitek_i2c_write_and_read(data,2, 5, data,0);
                 data[0] = 0x6D;
                 res = ilitek_i2c_write_and_read(data,1, 5, data,1);
                 timeout -- ;
                 if(timeout == 0)
                 {
                        tp_log_err("Idle mode setting fail\n");
                        return size;
                 }
        }
        mutex_unlock(&ilitek_data->ilitek_mutex);
        
        if(data[0] == 0)
	tp_log_info("Idle mode disable %d\n",data[0]);
        else if(data[0] == 1)
              tp_log_info("Idle mode enable %d\n",data[0]);


    return size;
}


static ssize_t ilitek_proc_mp_test_read(struct file *pFile,
                                        char __user * pBuffer, size_t nCount,
                                        loff_t * pPos)
{
    u32 length = 0;
    u8 result[16];
    int ret = 0;
    char *ini_path = INI_PATH;
    
    if (*pPos != 0)
        return 0;
    ilitek_irq_disable();
    ilitek_data->operation_protection =  true;   
    ret = ilitek_mp_start_test(CHIP_TYPE_CAMARO, ini_path);
    tp_log_info("MP Test Result = %d \n", ret);
    length = sprintf(result, "%d", ret);
    *pPos += length;
    tp_log_info("end\n");
    ilitek_irq_enable();
    ilitek_data->operation_protection =  false;
    return length;
}

// declare file operations
struct file_operations ilitek_fops = {
    .read = ilitek_file_read,
    .write = ilitek_file_write,
    .open = ilitek_file_open,
    .release = ilitek_file_close,
};

const struct file_operations ilitek_proc_mp_test = {
    .read = ilitek_proc_mp_test_read,
    .write = ilitek_proc_mp_test_write,
};

static const struct file_operations ilitek_proc_firmware_mode = {
    .read = ilitek_procfs_firmware_mode_read,
    .write = ilitek_procfs_firmware_mode_write,
};

static const struct file_operations ilitek_proc_fops_tp_information = {
    .read = ilitek_tp_information_read,
    .write = NULL,
};

static const struct file_operations ilitek_proc_fops_fwupdate = {
    .read = ilitek_update_with_bin_read,
    .write = ilitek_update_with_bin_write,
};

const struct file_operations proc_debug_level_fops = {
	.write = ilitek_proc_debug_level_write,
	.read = ilitek_proc_debug_level_read,
};

const struct file_operations proc_idle_mode_fops = {
	.write = ilitek_proc_idle_mode_write,
	.read = ilitek_proc_idle_mode_read,
};

typedef struct {
	char *name;
	struct proc_dir_entry *node;
	const struct file_operations *fops;
	bool isCreated;
} proc_node_t;

proc_node_t proc_table[] = {
	{"tp_information", NULL, &ilitek_proc_fops_tp_information, false},
	{"update_firmware", NULL, &ilitek_proc_fops_fwupdate, false},
	{"mp_test", NULL, &ilitek_proc_mp_test, false},
	{"firmware_mode", NULL, &ilitek_proc_firmware_mode, false},
	{"debug_level", NULL, &proc_debug_level_fops, false},
	{"idle_mode", NULL, &proc_idle_mode_fops, false},                
};

int ilitek_create_tool_node(void)
{
    int ret = 0;
    int i = 0;
    // allocate character device driver buffer
    ret = alloc_chrdev_region(&ilitek_dev.devno, 0, 1, "ilitek_file");
    
    if (ret) {
        tp_log_err("can't allocate chrdev\n");
        //return ret;
    } else {
        tp_log_info("register chrdev(%d, %d)\n",
                    MAJOR(ilitek_dev.devno), MINOR(ilitek_dev.devno));
        // initialize character device driver
        cdev_init(&ilitek_dev.cdev, &ilitek_fops);
        ilitek_dev.cdev.owner = THIS_MODULE;
        ret = cdev_add(&ilitek_dev.cdev, ilitek_dev.devno, 1);
        
        if (ret < 0) {
            tp_log_err("add character device error, ret %d\n", ret);
        } else {
            ilitek_dev.class = class_create(THIS_MODULE, "ilitek_file");
                
            if (IS_ERR(ilitek_dev.class)) {
                tp_log_err("create class, error\n");
            }
            
            device_create(ilitek_dev.class, NULL, ilitek_dev.devno, NULL, "ilitek_ctrl");
        }
    }
    
    ilitek_proc = proc_create("ilitek_ctrl", 0666, NULL, &ilitek_fops);
    
    if (ilitek_proc == NULL) {
        tp_log_err
        ("proc_create(ilitek_ctrl, 0666, NULL, &ilitek_fops) fail\n");
    }
    
    ilitek_proc_entry = proc_mkdir("ilitek", NULL);

    for (; i < ARRAY_SIZE(proc_table); i++) {
        proc_table[i].node = proc_create(proc_table[i].name, 0666, ilitek_proc_entry, proc_table[i].fops);

        if (proc_table[i].node == NULL) {
            proc_table[i].isCreated = false;
            tp_log_err("Failed to create %s under /proc\n", proc_table[i].name);
            ret = -ENODEV;
        } else {
            proc_table[i].isCreated = true;
            tp_log_info("Succeed to create %s under /proc\n", proc_table[i].name);
        }
    }    



    
    return 0;
}

int ilitek_remove_tool_node(void)
{
    int i = 0;
    
    cdev_del(&ilitek_dev.cdev);
    unregister_chrdev_region(ilitek_dev.devno, 1);
    device_destroy(ilitek_dev.class, ilitek_dev.devno);
    class_destroy(ilitek_dev.class);
    
    if (ilitek_proc) {
        tp_log_info("remove procfs ilitek_ctrl.\n");
        remove_proc_entry("ilitek_ctrl", NULL);
        ilitek_proc = NULL;
    }

	for (; i < ARRAY_SIZE(proc_table); i++) {
		if (proc_table[i].isCreated == true) {
			tp_log_info("Removed %s under /proc\n", proc_table[i].name);
			remove_proc_entry(proc_table[i].name, ilitek_proc_entry);
		}
	}
	remove_proc_entry("ilitek", NULL);
        
        ilitek_proc_entry = NULL;
    
    return 0;
}
#endif
