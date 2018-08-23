#include <stm32f4xx_hal.h>
#include "bme280.h"
#include "bme280_defs.h"

static I2C_HandleTypeDef  bme280I2cHandle;
static struct bme280_dev bmeDev;

static void Bme280Init(void);
static int8_t Bme280SetNormalMode(struct bme280_dev *dev);

void Bme280ShimInit(void)
{
	GPIO_InitTypeDef GPIO_InitStruct;

	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_I2C3_CLK_ENABLE();

	// Common GPIO settings
	GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF4_I2C3;

	// SDA
	GPIO_InitStruct.Pin = GPIO_PIN_9;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	// SCL
	GPIO_InitStruct.Pin = GPIO_PIN_8;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	// What the fuck.
	// https://electronics.stackexchange.com/questions/272427/stm32-busy-flag-is-set-after-i2c-initialization
	__HAL_RCC_I2C3_FORCE_RESET();
	HAL_Delay(2);
	__HAL_RCC_I2C3_RELEASE_RESET();

	// Init I2C
	bme280I2cHandle.Instance = I2C3;
	bme280I2cHandle.Init.ClockSpeed = 400000;
	bme280I2cHandle.Init.DutyCycle = I2C_DUTYCYCLE_2;
	bme280I2cHandle.Init.OwnAddress1 = 0;
	bme280I2cHandle.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	bme280I2cHandle.Init.DualAddressMode = I2C_DUALADDRESS_DISABLED;
	bme280I2cHandle.Init.OwnAddress2 = 0;
	bme280I2cHandle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLED;
	bme280I2cHandle.Init.NoStretchMode = I2C_NOSTRETCH_DISABLED;
	HAL_I2C_Init(&bme280I2cHandle);

	// Bring up the BME280
	Bme280Init();
}

static int8_t Bme280Write(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len)
{
	if (HAL_I2C_Mem_Write(&bme280I2cHandle, dev_id << 1, reg_addr, I2C_MEMADD_SIZE_8BIT, reg_data, len, 10000) != HAL_OK)
	{
		return -1;
	}
	
	return BME280_OK;
}

static int8_t Bme280Read(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len)
{
	if (HAL_I2C_Mem_Read(&bme280I2cHandle, dev_id << 1, reg_addr, I2C_MEMADD_SIZE_8BIT, reg_data, len, 10000) != HAL_OK)
	{
		return -1;
	}
	
	return BME280_OK;
}

static void Bme280Delay(uint32_t msek)
{
	HAL_Delay(msek);
}

static void Bme280Init(void)
{
	// Fill struct
	bmeDev.dev_id = BME280_I2C_ADDR_PRIM;
	bmeDev.intf = BME280_I2C_INTF;
	bmeDev.read = Bme280Read;
	bmeDev.write = Bme280Write;
	bmeDev.delay_ms = Bme280Delay;
	
	// Init the device
	bme280_init(&bmeDev);

	// Set the sensor normal mode
	Bme280SetNormalMode(&bmeDev);
}

static int8_t Bme280SetNormalMode(struct bme280_dev *dev)
{
	int8_t rslt;
	uint8_t settings_sel;
	struct bme280_data comp_data;

	dev->settings.osr_h = BME280_OVERSAMPLING_1X;
	dev->settings.osr_p = BME280_OVERSAMPLING_16X;
	dev->settings.osr_t = BME280_OVERSAMPLING_2X;
	dev->settings.filter = BME280_FILTER_COEFF_16;
	dev->settings.standby_time = BME280_STANDBY_TIME_62_5_MS;

	settings_sel = BME280_OSR_PRESS_SEL;
	settings_sel |= BME280_OSR_TEMP_SEL;
	settings_sel |= BME280_OSR_HUM_SEL;
	settings_sel |= BME280_STANDBY_SEL;
	settings_sel |= BME280_FILTER_SEL;
	rslt = bme280_set_sensor_settings(BME280_ALL_SETTINGS_SEL, dev);
	rslt = bme280_set_sensor_mode(BME280_NORMAL_MODE, dev);

	return rslt;
}

const uint8_t Bme280ShimGetTph(float* temperature, float* pressure, float* humidity)
{
	struct bme280_data compData;
	
	// Perform the xAction
	if (bme280_get_sensor_data(BME280_ALL, &compData, &bmeDev))
	{
		return 0;
	}

	// Sucessfuly obtained data, assign back values
	*temperature = compData.temperature;
	*pressure = compData.pressure;
	*humidity = compData.humidity;

	return 1;
}
