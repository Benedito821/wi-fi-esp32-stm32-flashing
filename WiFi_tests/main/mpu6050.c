#include "mpu6050.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include <stdlib.h>
#include <stddef.h>
#include "tasks_common.h"

static const char TAG[] = "mpu6050"; 
ts_mpu6050* mpu6050 = NULL;

ts_mpu6050* mpu6050_create(void);
te_mpu6050_status set_config(ts_mpu6050* mpu6050,i2c_port_t i2c_module);
static esp_err_t register_write_byte(i2c_port_t i2c_module,uint8_t dev_adr,uint8_t reg_addr, uint8_t data);
void reset_device(i2c_port_t i2c_module);
uint8_t get_interrupt_status(i2c_port_t i2c_module,uint8_t reg);
float get_temperature(i2c_port_t i2c_module);      
static te_mpu6050_status mpu6050_find_device(i2c_port_t i2c_module);
static esp_err_t mpu9250_register_read(i2c_port_t i2c_module,uint8_t dev_adr,uint8_t reg_addr, uint8_t *data, size_t len);
void set_clk_src(i2c_port_t i2c_module,uint8_t clk_src);
void set_data_rate(i2c_port_t i2c_module,uint8_t data_rate);
void set_dlpf(i2c_port_t i2c_module,uint8_t dlpf_freq);
void enable_interrupts(i2c_port_t i2c_module,uint8_t interr);
static void mpu6050_task(void* pvParameters);
void wake_up(i2c_port_t i2c_module);
void goto_sleep(i2c_port_t i2c_module);

ts_mpu6050* mpu6050_create(void)
{
    ts_mpu6050* mpu6050 = (ts_mpu6050*)malloc(sizeof(ts_mpu6050));
    if(NULL == mpu6050)
    {
        ESP_LOGI(TAG,"MEMORY FAIL");
        return NULL;
    }

    *mpu6050 = (ts_mpu6050){ 
        .set_config = set_config,
        .reset_device = reset_device,
        .get_interrupt_status = get_interrupt_status,
        .get_temperature = get_temperature,
        .set_clk_src = set_clk_src,
        .set_data_rate = set_data_rate,
        .set_dlpf = set_dlpf,
        .enable_interrupts = enable_interrupts,
        .wake_up = wake_up,
        .goto_sleep = goto_sleep
    };
    return mpu6050;
}

void mpu6050_init(i2c_port_t i2c_module)
{  
    mpu6050 =  mpu6050_create();
    if(NULL == mpu6050)
        return;
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 21,         // select SDA GPIO specific to your project
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = 22,         // select SCL GPIO specific to your project
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,  // select frequency specific to your project
        .clk_flags = 0               // optional; you can use I2C_SCLK_SRC_FLAG_* flags to choose I2C source clock here
    };
    ESP_ERROR_CHECK(i2c_param_config(i2c_module,&conf));
    ESP_ERROR_CHECK(i2c_driver_install(i2c_module, I2C_MODE_MASTER, 0, 0, 0));

    mpu6050->set_config(mpu6050,i2c_module);

    xTaskCreatePinnedToCore(&mpu6050_task,"mpu6050_task",MPU6050_APP_TASK_STACK_SIZE,NULL,MPU6050_APP_TASK_PRIORITY,NULL,MPU6050_APP_TASK_CORE_ID);
    
}

te_mpu6050_status set_config(ts_mpu6050* mpu6050, i2c_port_t i2c_module)
{
    if(MPU6050_DEVICE_NOT_FOUND == mpu6050_find_device(i2c_module))
        return MPU6050_CONFIG_FAIL;
    mpu6050->reset_device(i2c_module);
    vTaskDelay(DELAY_MS(100));
    mpu6050->wake_up(i2c_module);
    mpu6050->set_clk_src(i2c_module,CLK_SRC_PLL_GYRO);
    mpu6050->set_data_rate(i2c_module,DATA_RATE_1KHZ);
    mpu6050->set_dlpf(i2c_module,DLPF_FREQ_44_42);
    mpu6050->enable_interrupts(i2c_module,DATA_RDY_EN);
    return MPU6050_CONFIG_SUCCESS;
}

void wake_up(i2c_port_t i2c_module)
{
    uint8_t get_conf;
    mpu9250_register_read(i2c_module,MPU6050_ADDR,PWR_MGMT_1_REG, &get_conf, 1);
    get_conf &= ~SLEEP;
    if(ESP_OK == register_write_byte(i2c_module,MPU6050_ADDR,PWR_MGMT_1_REG, get_conf))
    {    
        ESP_LOGI(TAG,"WAKE UP SUCCESS");
        return;
    }
    ESP_LOGI(TAG,"WAKE UP FAIL");
}

void goto_sleep(i2c_port_t i2c_module)
{
    uint8_t get_conf;
    mpu9250_register_read(i2c_module,MPU6050_ADDR,PWR_MGMT_1_REG, &get_conf, 1);
    get_conf |= SLEEP;
    if(ESP_OK == register_write_byte(i2c_module,MPU6050_ADDR,PWR_MGMT_1_REG, get_conf))
    {    
        ESP_LOGI(TAG,"SLEEP SUCCESS");
        return;
    }
    ESP_LOGI(TAG,"SLEEP FAIL");
}

void reset_device( i2c_port_t i2c_module)
{
    wake_up(i2c_module);
    if(ESP_OK == register_write_byte(i2c_module,MPU6050_ADDR,PWR_MGMT_1_REG,DEVICE_RESET ))
    {    
        ESP_LOGI(TAG,"RESET SUCCESSFULY");
        return;
    }
    ESP_LOGI(TAG,"RESET FAIL");
}

uint8_t get_interrupt_status(i2c_port_t i2c_module,uint8_t reg)
{
    uint8_t get_inter_st;
    mpu9250_register_read(i2c_module,MPU6050_ADDR,INT_STATUS, &get_inter_st, 1);
    return get_inter_st;
}

float get_temperature(i2c_port_t i2c_module)
{
    uint8_t temp_buf[2];
    int16_t temp_raw_data;
    float temp_meas;
    if(1 == get_interrupt_status(i2c_module,DATA_RDY_INT))
    {
        mpu9250_register_read(i2c_module,MPU6050_ADDR,TEMP_OUT_H, temp_buf,2);
        temp_raw_data = (int16_t)(((uint16_t)temp_buf[0])<<8 | temp_buf[1]);
        temp_meas = ((float)temp_raw_data)/340.0f + 36.53f;
        return temp_meas;
    }
    ESP_LOGI(TAG,"DEVICE NOT READY");
    return 0.0;
}

te_mpu6050_status mpu6050_find_device(i2c_port_t i2c_module)
{
    uint8_t get_id;
    mpu9250_register_read(i2c_module,MPU6050_ADDR,WHO_AM_I_REG, &get_id, 1);
    if(MPU6050_ADDR == get_id){
        ESP_LOGI(TAG,"DEVICE FOUND SUCCESSFULY, ID = 0x%x",get_id);
        return MPU6050_DEVICE_FOUND;
    }        
    ESP_LOGI(TAG,"DEVICE NOT FOUND");
    return MPU6050_DEVICE_NOT_FOUND;
}

void mpu6050_suspend_readings(i2c_port_t i2c_module)
{
    goto_sleep(i2c_module);
    vTaskSuspend(xTaskGetHandle("mpu6050_task"));
}

void mpu6050_resume_readings(i2c_port_t i2c_module)
{
    vTaskResume(xTaskGetHandle("mpu6050_task"));
    wake_up(i2c_module);
}

void set_data_rate(i2c_port_t i2c_module,uint8_t data_rate)
{
     if(ESP_OK == register_write_byte(i2c_module,MPU6050_ADDR,SMPLRT_DIV_REG, data_rate))
     {
        ESP_LOGI(TAG,"DATA RATE SET SUCCESSFULY");
        return;
     }
    ESP_LOGI(TAG,"DATA RATE SET FAIL");
}

void set_clk_src(i2c_port_t i2c_module,uint8_t clk_src)
{
    uint8_t get_conf;
    wake_up(i2c_module);
    mpu9250_register_read(i2c_module,MPU6050_ADDR,PWR_MGMT_1_REG, &get_conf, 1);
    get_conf |= clk_src;
    if(ESP_OK == register_write_byte(i2c_module,MPU6050_ADDR,PWR_MGMT_1_REG, get_conf))
    {
        ESP_LOGI(TAG,"CLOCK SOURCE SET SUCCESSFULY");
        return;
    }        
    ESP_LOGI(TAG,"CLOCK SOURCE SET FAIL");
}

void set_dlpf(i2c_port_t i2c_module,uint8_t dlpf_freq)
{
    if(ESP_OK == register_write_byte(i2c_module,MPU6050_ADDR,CONFIG, dlpf_freq))
    {
        ESP_LOGI(TAG,"DIGITAL LOW PASS FILTER SET SUCCESSFULY");
        return;
    }        
    ESP_LOGI(TAG,"DIGITAL LOW PASS FILTER SET FAIL");
}

void mpu6050_task(void* pvParameters)
{
    ESP_LOGI(TAG,"Starting MPU6050 task ...");
    while(1)
    {
        // ESP_LOGI(TAG,"temperature = %.2f ºC",get_temperature(I2C_NUM_0)); //print the measurements on the monitor
         vTaskDelay(DELAY_MS(1000));
    }
}

void enable_interrupts(i2c_port_t i2c_module,uint8_t interr)
{
    if(ESP_OK == register_write_byte(i2c_module,MPU6050_ADDR,INT_ENABLE, interr))
    {
        ESP_LOGI(TAG,"INTERRUPT ENABLE SUCCESS");
        return;
    }
    ESP_LOGI(TAG,"INTERRUPT ENABLE FAIL");
}

ts_mpu6050* mpu6050_export_obj(void)
{
    return mpu6050;
}


static esp_err_t register_write_byte(i2c_port_t i2c_module,uint8_t dev_addr,uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data};
    esp_err_t ret;
    ret = i2c_master_write_to_device(i2c_module, dev_addr, write_buf, sizeof(write_buf), DELAY_MS(100));  
    return ret;
}
    
static esp_err_t mpu9250_register_read(i2c_port_t i2c_module,uint8_t dev_adr,uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(i2c_module, dev_adr, &reg_addr, 1, data, len, DELAY_MS(100));
}