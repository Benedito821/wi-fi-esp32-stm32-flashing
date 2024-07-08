#ifndef MPU6050_H_
#define MPU6050_H_

#include <stdint.h>
#include "hal/i2c_types.h"

#define MPU6050_ADDR 	 0x68
#define SMPLRT_DIV_REG   0x19
#define PWR_MGMT_1_REG   0x6B
#define USER_CTRL        0x6A
#define WHO_AM_I_REG     0x75
#define INT_ENABLE       0x38
#define INT_STATUS       0x3A
#define CONFIG           0x1A 
#define DLPF_FREQ_44_42  0x03 
#define DATA_RATE_1KHZ   0x00 
#define WAKE_UP_CONF     0x00


#define TEMP_OUT_H       0x41
#define TEMP_OUT_L       0x42

#define CLK_SRC_PLL_GYRO 1U<<0
#define DATA_RDY_INT     1U<<0
#define DATA_RDY_EN      1U<<0
#define DEVICE_RESET     1U<<7
#define SLEEP            1U<<6 

#define I2C_MASTER_TIMEOUT_MS       1000
#define DELAY_MS(x)                 x/portTICK_PERIOD_MS 

typedef enum
{
    MPU6050_CONFIG_SUCCESS,
    MPU6050_CONFIG_FAIL,
    MPU6050_DEVICE_FOUND,
    MPU6050_DEVICE_NOT_FOUND
}te_mpu6050_status;

typedef struct mpu
{
    te_mpu6050_status (*set_config)(struct mpu* mpu6050,i2c_port_t i2c_module);
    void (*reset_device)(i2c_port_t i2c_module);
    uint8_t (*get_interrupt_status)(i2c_port_t i2c_module,uint8_t reg);
    float (*get_temperature)(i2c_port_t i2c_module);
    void (*set_clk_src)(i2c_port_t i2c_module,uint8_t clk_src);
    void (*set_data_rate)(i2c_port_t i2c_module,uint8_t data_rate);
    void (*set_dlpf)(i2c_port_t i2c_module,uint8_t dlpf_freq);
    void (*enable_interrupts)(i2c_port_t i2c_module,uint8_t interr);
    void (*wake_up)(i2c_port_t i2c_module);
    void (*goto_sleep)(i2c_port_t i2c_module);
}ts_mpu6050;


void mpu6050_init(i2c_port_t i2c_module);
void mpu6050_suspend_readings(i2c_port_t i2c_module);
void mpu6050_resume_readings(i2c_port_t i2c_module);
ts_mpu6050* mpu6050_export_obj(void);
#endif