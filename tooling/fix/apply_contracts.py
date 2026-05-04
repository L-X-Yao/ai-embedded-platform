#!/usr/bin/env python3
"""Apply contract.h REQUIRE/ENSURE/INVARIANT to all L1 driver wheels."""
import os
import re

BASE = os.path.join(os.path.dirname(__file__), "..", "..", "wheels", "drivers")
BASE = os.path.normpath(BASE)

modified_wheels = []

def read_file(path):
    with open(path, 'r', encoding='utf-8') as f:
        return f.read()

def write_file(path, content):
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(content)

def add_include_contract(content):
    """Add #include "contract.h" after the main header include."""
    if 'contract.h' in content:
        return content
    m = re.search(r'(#include\s+"[^"]+\.h")', content)
    if m:
        pos = m.end()
        content = content[:pos] + '\n#include "contract.h"' + content[pos:]
    return content


def patch_bmp280():
    path = os.path.join(BASE, "bmp280", "bmp280.c")
    c = read_file(path)
    c = add_include_contract(c)
    c = c.replace(
        '    uint8_t config = 0x00;\n    if (!write_reg(dev, REG_CONFIG, config))\n    {\n        return BMP280_ERR_I2C;\n    }\n\n    return BMP280_OK;\n}',
        '    uint8_t config = 0x00;\n    if (!write_reg(dev, REG_CONFIG, config))\n    {\n        return BMP280_ERR_I2C;\n    }\n\n    INVARIANT(dev->i2c != NULL);\n    return BMP280_OK;\n}'
    )
    c = c.replace(
        '    out->pressure_pa = (float)press_raw;\n\n    return BMP280_OK;\n}',
        '    out->pressure_pa = (float)press_raw;\n\n    ENSURE(out->temperature_c == out->temperature_c);  /* not NaN */\n    ENSURE(out->pressure_pa == out->pressure_pa);        /* not NaN */\n    ENSURE(out->temperature_c >= -40.0f && out->temperature_c <= 85.0f);\n    ENSURE(out->pressure_pa >= 30000.0f && out->pressure_pa <= 110000.0f);\n    return BMP280_OK;\n}'
    )
    write_file(path, c)
    modified_wheels.append("bmp280")

def patch_bme280():
    path = os.path.join(BASE, "bme280", "bme280.c")
    c = read_file(path)
    c = add_include_contract(c)
    c = c.replace(
        '    return BME280_OK;\n}\n\nbme280_err_t bme280_set_oversampling',
        '    INVARIANT(dev->i2c != NULL);\n    return BME280_OK;\n}\n\nbme280_err_t bme280_set_oversampling'
    )
    c = c.replace(
        '    out->humidity_pct = (float)hum_raw / 1024.0f;\n\n    return BME280_OK;\n}',
        '    out->humidity_pct = (float)hum_raw / 1024.0f;\n\n    ENSURE(out->temperature_c == out->temperature_c);  /* not NaN */\n    ENSURE(out->pressure_pa == out->pressure_pa);        /* not NaN */\n    ENSURE(out->humidity_pct == out->humidity_pct);      /* not NaN */\n    ENSURE(out->temperature_c >= -40.0f && out->temperature_c <= 85.0f);\n    ENSURE(out->pressure_pa >= 30000.0f && out->pressure_pa <= 110000.0f);\n    ENSURE(out->humidity_pct >= 0.0f && out->humidity_pct <= 100.0f);\n    return BME280_OK;\n}'
    )
    write_file(path, c)
    modified_wheels.append("bme280")

def patch_bme680():
    path = os.path.join(BASE, "bme680", "bme680.c")
    c = read_file(path)
    c = add_include_contract(c)
    c = c.replace(
        '    return BME680_OK;\n}\n\nbme680_err_t bme680_read',
        '    INVARIANT(dev->i2c != NULL);\n    return BME680_OK;\n}\n\nbme680_err_t bme680_read'
    )
    c = c.replace(
        '    return BME680_OK;\n}\n\nbme680_err_t bme680_set_oversampling',
        '    ENSURE(data->temperature_c == data->temperature_c);  /* not NaN */\n    ENSURE(data->pressure_pa == data->pressure_pa);        /* not NaN */\n    ENSURE(data->humidity_pct == data->humidity_pct);      /* not NaN */\n    ENSURE(data->temperature_c >= -40.0f && data->temperature_c <= 85.0f);\n    ENSURE(data->pressure_pa >= 30000.0f && data->pressure_pa <= 110000.0f);\n    ENSURE(data->humidity_pct >= 0.0f && data->humidity_pct <= 100.0f);\n    return BME680_OK;\n}\n\nbme680_err_t bme680_set_oversampling'
    )
    write_file(path, c)
    modified_wheels.append("bme680")

def patch_sht30():
    path = os.path.join(BASE, "sht30", "sht30.c")
    c = read_file(path)
    c = add_include_contract(c)
    c = c.replace(
        "    out->humidity_pct   = 100.0f * (float)raw_h / 65535.0f;\n\n    return SHT30_OK;\n}",
        "    out->humidity_pct   = 100.0f * (float)raw_h / 65535.0f;\n\n    ENSURE(out->temperature_c == out->temperature_c);  /* not NaN */\n    ENSURE(out->humidity_pct == out->humidity_pct);      /* not NaN */\n    ENSURE(out->temperature_c >= -40.0f && out->temperature_c <= 125.0f);\n    ENSURE(out->humidity_pct >= 0.0f && out->humidity_pct <= 100.0f);\n    return SHT30_OK;\n}"
    )
    write_file(path, c)
    modified_wheels.append("sht30")

def patch_si7021():
    path = os.path.join(BASE, "si7021", "si7021.c")
    c = read_file(path)
    c = add_include_contract(c)
    c = c.replace(
        "    *rh = 125.0f * (float)raw / 65536.0f - 6.0f;\n    return SI7021_OK;\n}\n\nsi7021_err_t si7021_read_temperature",
        "    *rh = 125.0f * (float)raw / 65536.0f - 6.0f;\n    ENSURE(*rh == *rh);  /* not NaN */\n    return SI7021_OK;\n}\n\nsi7021_err_t si7021_read_temperature"
    )
    c = c.replace(
        "    *temp_c = 175.72f * (float)raw / 65536.0f - 46.85f;\n    return SI7021_OK;\n}\n\nsi7021_err_t si7021_read_both",
        "    *temp_c = 175.72f * (float)raw / 65536.0f - 46.85f;\n    ENSURE(*temp_c == *temp_c);  /* not NaN */\n    return SI7021_OK;\n}\n\nsi7021_err_t si7021_read_both"
    )
    write_file(path, c)
    modified_wheels.append("si7021")

def patch_dht11():
    path = os.path.join(BASE, "dht11", "dht11.c")
    c = read_file(path)
    c = add_include_contract(c)
    c = c.replace(
        "    out->temperature_dec = bytes[3];\n    return DHT11_OK;\n}",
        "    out->temperature_dec = bytes[3];\n    ENSURE(out->temperature_int <= 50);\n    ENSURE(out->humidity_int <= 90);\n    return DHT11_OK;\n}"
    )
    write_file(path, c)
    modified_wheels.append("dht11")

def patch_ds18b20():
    path = os.path.join(BASE, "ds18b20", "ds18b20.c")
    c = read_file(path)
    c = add_include_contract(c)
    c = c.replace(
        "    *temp_c = (float)raw / 16.0f;\n    return DS18B20_OK;\n}",
        "    *temp_c = (float)raw / 16.0f;\n    ENSURE(*temp_c == *temp_c);  /* not NaN */\n    ENSURE(*temp_c >= -55.0f && *temp_c <= 125.0f);\n    return DS18B20_OK;\n}"
    )
    write_file(path, c)
    modified_wheels.append("ds18b20")

def patch_tmp102():
    path = os.path.join(BASE, "tmp102", "tmp102.c")
    c = read_file(path)
    c = add_include_contract(c)
    c = c.replace(
        "    *temp_c = raw_to_c(buf[0], buf[1]);\n    return TMP102_OK;\n}",
        "    *temp_c = raw_to_c(buf[0], buf[1]);\n    ENSURE(*temp_c == *temp_c);  /* not NaN */\n    ENSURE(*temp_c >= -55.0f && *temp_c <= 150.0f);\n    return TMP102_OK;\n}"
    )
    write_file(path, c)
    modified_wheels.append("tmp102")

def patch_mpu6050():
    path = os.path.join(BASE, "mpu6050", "mpu6050.c")
    c = read_file(path)
    c = add_include_contract(c)
    c = c.replace(
        "    dev->accel_scale = accel_scales[cfg->accel_range & 0x03];\n    dev->gyro_scale  = gyro_scales[cfg->gyro_range & 0x03];\n\n    return MPU6050_OK;\n}",
        "    dev->accel_scale = accel_scales[cfg->accel_range & 0x03];\n    dev->gyro_scale  = gyro_scales[cfg->gyro_range & 0x03];\n\n    INVARIANT(dev->i2c != NULL);\n    return MPU6050_OK;\n}"
    )
    c = c.replace(
        "    out->az_g = (float)raw_z / dev->accel_scale;\n\n    return MPU6050_OK;\n}",
        "    out->az_g = (float)raw_z / dev->accel_scale;\n\n    ENSURE(out->ax_g == out->ax_g);  /* not NaN */\n    ENSURE(out->ay_g == out->ay_g);\n    ENSURE(out->az_g == out->az_g);\n    return MPU6050_OK;\n}"
    )
    c = c.replace(
        "    out->gz_dps = (float)raw_z / dev->gyro_scale;\n\n    return MPU6050_OK;\n}",
        "    out->gz_dps = (float)raw_z / dev->gyro_scale;\n\n    ENSURE(out->gx_dps == out->gx_dps);  /* not NaN */\n    ENSURE(out->gy_dps == out->gy_dps);\n    ENSURE(out->gz_dps == out->gz_dps);\n    return MPU6050_OK;\n}"
    )
    write_file(path, c)
    modified_wheels.append("mpu6050")

def patch_qmi8658a():
    path = os.path.join(BASE, "qmi8658a", "qmi8658a.c")
    c = read_file(path)
    c = add_include_contract(c)
    c = c.replace(
        "    dev->acc_div = acc_scale[cfg->acc_range & 0x03];\n    dev->gyr_div = gyr_scale[cfg->gyr_range & 0x07];\n\n    return QMI8658A_OK;\n}",
        "    dev->acc_div = acc_scale[cfg->acc_range & 0x03];\n    dev->gyr_div = gyr_scale[cfg->gyr_range & 0x07];\n\n    INVARIANT(dev->i2c != NULL);\n    return QMI8658A_OK;\n}"
    )
    c = c.replace(
        "    out->gz_dps = (float)gz / dev->gyr_div;\n\n    return QMI8658A_OK;\n}",
        "    out->gz_dps = (float)gz / dev->gyr_div;\n\n    ENSURE(out->ax_g == out->ax_g);      /* not NaN */\n    ENSURE(out->ay_g == out->ay_g);\n    ENSURE(out->az_g == out->az_g);\n    ENSURE(out->gx_dps == out->gx_dps);\n    ENSURE(out->gy_dps == out->gy_dps);\n    ENSURE(out->gz_dps == out->gz_dps);\n    return QMI8658A_OK;\n}"
    )
    write_file(path, c)
    modified_wheels.append("qmi8658a")

def patch_lis3dh():
    path = os.path.join(BASE, "lis3dh", "lis3dh.c")
    c = read_file(path)
    c = add_include_contract(c)
    c = c.replace(
        "    dev->scale = range_scale[cfg->range & 0x03];\n    return LIS3DH_OK;\n}\n\nlis3dh_err_t lis3dh_read_accel",
        "    dev->scale = range_scale[cfg->range & 0x03];\n    INVARIANT(dev->i2c != NULL);\n    return LIS3DH_OK;\n}\n\nlis3dh_err_t lis3dh_read_accel"
    )
    c = c.replace(
        "    out->z_g = (float)rz / dev->scale;\n    return LIS3DH_OK;\n}\n\nlis3dh_err_t lis3dh_read_raw",
        "    out->z_g = (float)rz / dev->scale;\n    ENSURE(out->x_g == out->x_g);  /* not NaN */\n    ENSURE(out->y_g == out->y_g);\n    ENSURE(out->z_g == out->z_g);\n    return LIS3DH_OK;\n}\n\nlis3dh_err_t lis3dh_read_raw"
    )
    write_file(path, c)
    modified_wheels.append("lis3dh")

def patch_qmc5883p():
    path = os.path.join(BASE, "qmc5883p", "qmc5883p.c")
    c = read_file(path)
    c = add_include_contract(c)
    c = c.replace(
        "    out->mz_gauss = (float)raw_z / SENSITIVITY_2G;\n\n    return QMC5883P_OK;\n}",
        "    out->mz_gauss = (float)raw_z / SENSITIVITY_2G;\n\n    ENSURE(out->mx_gauss == out->mx_gauss);  /* not NaN */\n    ENSURE(out->my_gauss == out->my_gauss);\n    ENSURE(out->mz_gauss == out->mz_gauss);\n    return QMC5883P_OK;\n}"
    )
    write_file(path, c)
    modified_wheels.append("qmc5883p")

def patch_hmc5883l():
    path = os.path.join(BASE, "hmc5883l", "hmc5883l.c")
    c = read_file(path)
    c = add_include_contract(c)
    c = c.replace(
        "    out->z_gauss = (float)raw.z * dev->scale;\n    return HMC5883L_OK;\n}",
        "    out->z_gauss = (float)raw.z * dev->scale;\n    ENSURE(out->x_gauss == out->x_gauss);  /* not NaN */\n    ENSURE(out->y_gauss == out->y_gauss);\n    ENSURE(out->z_gauss == out->z_gauss);\n    return HMC5883L_OK;\n}"
    )
    write_file(path, c)
    modified_wheels.append("hmc5883l")

def patch_xgzp6816d():
    path = os.path.join(BASE, "xgzp6816d", "xgzp6816d.c")
    c = read_file(path)
    c = add_include_contract(c)
    c = c.replace(
        "    out->temperature_c = (float)t_raw / 65536.0f * 190.0f - 40.0f;\n\n    return XGZP6816D_OK;\n}",
        "    out->temperature_c = (float)t_raw / 65536.0f * 190.0f - 40.0f;\n\n    ENSURE(out->pressure_pa == out->pressure_pa);        /* not NaN */\n    ENSURE(out->temperature_c == out->temperature_c);    /* not NaN */\n    ENSURE(out->pressure_pa >= 30000.0f && out->pressure_pa <= 110000.0f);\n    ENSURE(out->temperature_c >= -40.0f && out->temperature_c <= 150.0f);\n    return XGZP6816D_OK;\n}"
    )
    write_file(path, c)
    modified_wheels.append("xgzp6816d")

def patch_ina219():
    path = os.path.join(BASE, "ina219", "ina219.c")
    c = read_file(path)
    c = add_include_contract(c)
    c = c.replace(
        "    *voltage_v = (float)shifted * 0.004f;\n    return INA219_OK;\n}",
        "    *voltage_v = (float)shifted * 0.004f;\n    ENSURE(*voltage_v == *voltage_v);  /* not NaN */\n    return INA219_OK;\n}"
    )
    c = c.replace(
        "    *voltage_mv = (float)signed_raw * 0.01f;\n    return INA219_OK;\n}",
        "    *voltage_mv = (float)signed_raw * 0.01f;\n    ENSURE(*voltage_mv == *voltage_mv);  /* not NaN */\n    return INA219_OK;\n}"
    )
    c = c.replace(
        "    *current_ma = (float)signed_raw * dev->current_lsb * 1000.0f;\n    return INA219_OK;\n}",
        "    *current_ma = (float)signed_raw * dev->current_lsb * 1000.0f;\n    ENSURE(*current_ma == *current_ma);  /* not NaN */\n    return INA219_OK;\n}"
    )
    c = c.replace(
        "    *power_mw = (float)raw * 20.0f * dev->current_lsb * 1000.0f;\n    return INA219_OK;\n}",
        "    *power_mw = (float)raw * 20.0f * dev->current_lsb * 1000.0f;\n    ENSURE(*power_mw == *power_mw);  /* not NaN */\n    return INA219_OK;\n}"
    )
    write_file(path, c)
    modified_wheels.append("ina219")

def patch_bh1750():
    path = os.path.join(BASE, "bh1750", "bh1750.c")
    c = read_file(path)
    c = add_include_contract(c)
    c = c.replace(
        "        *lux = (float)raw / 1.2f;\n\n    return BH1750_OK;\n}",
        "        *lux = (float)raw / 1.2f;\n\n    ENSURE(*lux == *lux);  /* not NaN */\n    ENSURE(*lux >= 0.0f);\n    return BH1750_OK;\n}"
    )
    write_file(path, c)
    modified_wheels.append("bh1750")

def patch_veml7700():
    path = os.path.join(BASE, "veml7700", "veml7700.c")
    c = read_file(path)
    c = add_include_contract(c)
    c = c.replace(
        "    *lux = (float)raw * resolution(dev->gain, dev->it);\n    return VEML7700_OK;\n}",
        "    *lux = (float)raw * resolution(dev->gain, dev->it);\n    ENSURE(*lux == *lux);  /* not NaN */\n    ENSURE(*lux >= 0.0f);\n    return VEML7700_OK;\n}"
    )
    write_file(path, c)
    modified_wheels.append("veml7700")

def patch_opt3001():
    path = os.path.join(BASE, "opt3001", "opt3001.c")
    c = read_file(path)
    c = add_include_contract(c)
    c = c.replace(
        "    *lux = 0.01f * (float)(1u << exponent) * (float)mantissa;\n    return OPT3001_OK;\n}",
        "    *lux = 0.01f * (float)(1u << exponent) * (float)mantissa;\n    ENSURE(*lux == *lux);  /* not NaN */\n    ENSURE(*lux >= 0.0f);\n    return OPT3001_OK;\n}"
    )
    write_file(path, c)
    modified_wheels.append("opt3001")

def patch_mlx90614():
    path = os.path.join(BASE, "mlx90614", "mlx90614.c")
    c = read_file(path)
    c = add_include_contract(c)
    c = c.replace(
        "    *temp_c = (float)raw * 0.02f - 273.15f;\n    return MLX90614_OK;\n}",
        "    *temp_c = (float)raw * 0.02f - 273.15f;\n    ENSURE(*temp_c == *temp_c);  /* not NaN */\n    return MLX90614_OK;\n}"
    )
    c = c.replace(
        "    *emissivity = (float)raw / 65535.0f;\n    return MLX90614_OK;\n}",
        "    *emissivity = (float)raw / 65535.0f;\n    ENSURE(*emissivity == *emissivity);  /* not NaN */\n    ENSURE(*emissivity >= 0.0f && *emissivity <= 1.0f);\n    return MLX90614_OK;\n}"
    )
    write_file(path, c)
    modified_wheels.append("mlx90614")

def patch_max31855():
    path = os.path.join(BASE, "max31855", "max31855.c")
    c = read_file(path)
    c = add_include_contract(c)
    c = c.replace(
        "    if (thermocouple_c) *thermocouple_c = decode_thermocouple(raw);\n    if (internal_c)     *internal_c     = decode_internal(raw);\n    return MAX31855_OK;\n}",
        "    if (thermocouple_c) {\n        *thermocouple_c = decode_thermocouple(raw);\n        ENSURE(*thermocouple_c == *thermocouple_c);  /* not NaN */\n    }\n    if (internal_c) {\n        *internal_c = decode_internal(raw);\n        ENSURE(*internal_c == *internal_c);  /* not NaN */\n    }\n    return MAX31855_OK;\n}"
    )
    write_file(path, c)
    modified_wheels.append("max31855")

def patch_max31865():
    path = os.path.join(BASE, "max31865", "max31865.c")
    c = read_file(path)
    c = add_include_contract(c)
    c = c.replace(
        "    *temp_c = rtd_to_temperature(r_rtd, dev->cfg.r_nom);\n\n    return MAX31865_OK;\n}",
        "    *temp_c = rtd_to_temperature(r_rtd, dev->cfg.r_nom);\n\n    ENSURE(*temp_c == *temp_c);  /* not NaN */\n    ENSURE(*temp_c >= -240.0f && *temp_c <= 850.0f);\n    return MAX31865_OK;\n}"
    )
    write_file(path, c)
    modified_wheels.append("max31865")

def patch_hx711():
    path = os.path.join(BASE, "hx711", "hx711.c")
    c = read_file(path)
    c = add_include_contract(c)
    c = c.replace(
        "    *value = (float)(raw - dev->offset) / dev->scale;\n    return HX711_OK;\n}",
        "    *value = (float)(raw - dev->offset) / dev->scale;\n    ENSURE(*value == *value);  /* not NaN */\n    return HX711_OK;\n}"
    )
    write_file(path, c)
    modified_wheels.append("hx711")

def patch_ads1115():
    path = os.path.join(BASE, "ads1115", "ads1115.c")
    c = read_file(path)
    c = add_include_contract(c)
    c = c.replace(
        "    *voltage_mv = (float)raw * pga_scale_mv[pga_idx];\n    return ADS1115_OK;\n}",
        "    *voltage_mv = (float)raw * pga_scale_mv[pga_idx];\n    ENSURE(*voltage_mv == *voltage_mv);  /* not NaN */\n    return ADS1115_OK;\n}"
    )
    write_file(path, c)
    modified_wheels.append("ads1115")

def patch_ads1015():
    path = os.path.join(BASE, "ads1015", "ads1015.c")
    c = read_file(path)
    c = add_include_contract(c)
    c = c.replace(
        "    *voltage_mv = (float)raw * pga_scale_mv[idx];\n    return ADS1015_OK;\n}",
        "    *voltage_mv = (float)raw * pga_scale_mv[idx];\n    ENSURE(*voltage_mv == *voltage_mv);  /* not NaN */\n    return ADS1015_OK;\n}"
    )
    write_file(path, c)
    modified_wheels.append("ads1015")

def patch_as5600():
    path = os.path.join(BASE, "as5600", "as5600.c")
    c = read_file(path)
    c = add_include_contract(c)
    c = c.replace(
        "    *degrees = (float)raw * (FULL_TURN_DEG / FULL_TURN_CNT);\n    return AS5600_OK;\n}",
        "    *degrees = (float)raw * (FULL_TURN_DEG / FULL_TURN_CNT);\n    ENSURE(*degrees == *degrees);  /* not NaN */\n    ENSURE(*degrees >= 0.0f && *degrees < 360.0f);\n    return AS5600_OK;\n}"
    )
    write_file(path, c)
    modified_wheels.append("as5600")

def patch_ds3231():
    path = os.path.join(BASE, "ds3231", "ds3231.c")
    c = read_file(path)
    c = add_include_contract(c)
    c = c.replace(
        "    *temp_c = (float)msb + (float)frac * 0.25f;\n\n    return DS3231_OK;\n}",
        "    *temp_c = (float)msb + (float)frac * 0.25f;\n\n    ENSURE(*temp_c == *temp_c);  /* not NaN */\n    ENSURE(*temp_c >= -40.0f && *temp_c <= 85.0f);\n    return DS3231_OK;\n}"
    )
    write_file(path, c)
    modified_wheels.append("ds3231")

def patch_vl53l0x():
    path = os.path.join(BASE, "vl53l0x", "vl53l0x.c")
    c = read_file(path)
    c = add_include_contract(c)
    c = c.replace(
        "            *range_mm = range;\n            return VL53L0X_OK;",
        "            *range_mm = range;\n            ENSURE(range < 8190u);\n            return VL53L0X_OK;"
    )
    write_file(path, c)
    modified_wheels.append("vl53l0x")

def patch_tcs34725():
    path = os.path.join(BASE, "tcs34725", "tcs34725.c")
    c = read_file(path)
    c = add_include_contract(c)
    c = c.replace(
        "    *lux = l;\n    return TCS34725_OK;\n}",
        "    *lux = l;\n    ENSURE(*lux == *lux);  /* not NaN */\n    ENSURE(*lux >= 0.0f);\n    return TCS34725_OK;\n}"
    )
    write_file(path, c)
    modified_wheels.append("tcs34725")

def patch_apds9960():
    path = os.path.join(BASE, "apds9960", "apds9960.c")
    c = read_file(path)
    c = add_include_contract(c)
    c = c.replace(
        "    err = write_reg(dev, REG_CONTROL, 0x00u);\n    if (err != APDS9960_OK) return err;\n\n    return APDS9960_OK;\n}",
        "    err = write_reg(dev, REG_CONTROL, 0x00u);\n    if (err != APDS9960_OK) return err;\n\n    INVARIANT(dev->i2c != NULL);\n    return APDS9960_OK;\n}"
    )
    write_file(path, c)
    modified_wheels.append("apds9960")

def patch_simple(name):
    """For wheels with no float sensor output: just add #include contract.h."""
    path = os.path.join(BASE, name, name + ".c")
    c = read_file(path)
    c = add_include_contract(c)
    write_file(path, c)
    modified_wheels.append(name)

def patch_with_invariant(name, old_str, new_str):
    """Add include + one INVARIANT."""
    path = os.path.join(BASE, name, name + ".c")
    c = read_file(path)
    c = add_include_contract(c)
    c = c.replace(old_str, new_str)
    write_file(path, c)
    modified_wheels.append(name)


# Run all patches
patch_bmp280()
patch_bme280()
patch_bme680()
patch_sht30()
patch_si7021()
patch_dht11()
patch_ds18b20()
patch_tmp102()
patch_mpu6050()
patch_qmi8658a()
patch_lis3dh()
patch_qmc5883p()
patch_hmc5883l()
patch_xgzp6816d()
patch_ina219()
patch_bh1750()
patch_veml7700()
patch_opt3001()
patch_mlx90614()
patch_max31855()
patch_max31865()
patch_hx711()
patch_ads1115()
patch_ads1015()
patch_as5600()
patch_ds3231()
patch_vl53l0x()
patch_tcs34725()
patch_apds9960()

# Simple include-only (no float sensor output)
for name in ["w25qxx", "sd_spi", "oled_ssd1306", "max7219", "gpio_button",
             "gpio_led", "rotary_encoder", "keypad_matrix", "ws2812",
             "nrf24l01", "lora_sx1276", "enc28j60", "mcp2515", "mcp23017",
             "pcf8574", "tca9548a", "pca9685"]:
    patch_simple(name)

# With INVARIANT on HAL pointer
patch_with_invariant("at24cxx",
    "    dev->page_size = page_size_for(model);\n    return AT24CXX_OK;\n}",
    "    dev->page_size = page_size_for(model);\n    INVARIANT(dev->i2c != NULL);\n    return AT24CXX_OK;\n}")
patch_with_invariant("servo",
    "    dev->current_angle = 0;\n    return SERVO_OK;\n}",
    "    dev->current_angle = 0;\n    INVARIANT(dev->pwm != NULL);\n    return SERVO_OK;\n}")
patch_with_invariant("stepper",
    "    s->current_delay_us = 0;\n    return STEPPER_OK;\n}",
    "    s->current_delay_us = 0;\n    INVARIANT(s->hal != NULL);\n    return STEPPER_OK;\n}")
patch_with_invariant("drv8833",
    "    dev->max_duty = max_duty;\n    return DRV8833_OK;\n}",
    "    dev->max_duty = max_duty;\n    INVARIANT(dev->hal != NULL);\n    return DRV8833_OK;\n}")
patch_with_invariant("drv2605",
    "    return DRV2605_OK;\n}\n\ndrv2605_err_t drv2605_set_actuator",
    "    INVARIANT(dev->i2c != NULL);\n    return DRV2605_OK;\n}\n\ndrv2605_err_t drv2605_set_actuator")

print(f"\nModified {len(modified_wheels)} wheels:")
for w in sorted(modified_wheels):
    print(f"  {w}")
