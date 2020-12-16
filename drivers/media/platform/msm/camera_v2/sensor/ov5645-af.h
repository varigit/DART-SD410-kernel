#ifndef OV5645_AF_H
#define OV5645_AF_H

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <soc/qcom/camera2.h>
#include <media/msm_cam_sensor.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ioctl.h>
#include "msm_camera_i2c.h"
#include "msm_camera_dt_util.h"
#include "msm_sd.h"


int ov5645_af_init(struct msm_camera_i2c_client *sensor_i2c_client, bool verify);
int ov5645_af_constant_focus(struct msm_camera_i2c_client *sensor_i2c_client);
int ov5645_af_cancel_focus(struct msm_camera_i2c_client *sensor_i2c_client);
bool ov5645_af_check_sensor_id(struct msm_camera_i2c_client *sensor_i2c_client, uint16_t sensor_id, bool init);

#endif
