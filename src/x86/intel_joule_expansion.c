/*
 * Author: Brendan Le Foll <brendan.le.foll@intel.com>
 * Author: Mihai Tudor Panu <mihai.tudor.panu@intel.com>
 * Copyright (c) 2016 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <mraa/i2c.h>

// Debug
#include <syslog.h>
#include <sys/errno.h>

#include "common.h"
#include "x86/intel_joule_expansion.h"

#define PLATFORM_NAME "INTEL JOULE EXPANSION"

typedef enum {NO_SHIELD, DFROBOT, GROVE} shield_t;

static mraa_result_t
mraa_joule_expansion_board_get_valid_fp(mraa_aio_context dev)
{
    char file_path[64] = "";

    // 4 channels per ADC
    snprintf(file_path, 64, "/sys/bus/iio/devices/iio:device%d/in_voltage%d_raw", dev->channel / 4, dev->channel % 4);

    dev->adc_in_fp = open(file_path, O_RDONLY);
    if (dev->adc_in_fp == -1) {
        syslog(LOG_ERR, "aio: Failed to open input raw file %s for reading!", file_path);
        return MRAA_ERROR_INVALID_RESOURCE;
    }

    return MRAA_SUCCESS;
}

mraa_board_t*
mraa_joule_expansion_board()
{
    shield_t shield = NO_SHIELD;
    int pincount = MRAA_INTEL_JOULE_EXPANSION_PINCOUNT;

    mraa_board_t* b = (mraa_board_t*) calloc(1, sizeof(mraa_board_t));
    if (b == NULL) {
        return NULL;
    }

    b->adv_func = (mraa_adv_func_t*) calloc(1, sizeof(mraa_adv_func_t));
    if (b->adv_func == NULL) {
        free(b->pins);
        goto error;
    }

    b->platform_name = PLATFORM_NAME;
    b->gpio_count = pincount;

    b->pwm_default_period = 5000;
    b->pwm_max_period = 218453;
    b->pwm_min_period = 1;

    b->i2c_bus_count = 0;

    int i2c_bus_num = mraa_find_i2c_bus_pci("0000:00", "0000:00:16.0", "i2c_designware.0");
    int i2c_aio_bus = i2c_bus_num;
    if (i2c_bus_num != -1) {
        b->i2c_bus[0].bus_id = i2c_bus_num;
        b->i2c_bus[0].sda = 11;
        b->i2c_bus[0].scl = 13;
        b->i2c_bus_count++;
    }

    i2c_bus_num = mraa_find_i2c_bus_pci("0000:00", "0000:00:16.1", "i2c_designware.1");
    if (i2c_bus_num != -1) {
        b->i2c_bus[b->i2c_bus_count].bus_id = i2c_bus_num;
        b->i2c_bus[b->i2c_bus_count].sda = 71;
        b->i2c_bus[b->i2c_bus_count].scl = 73;
        b->i2c_bus_count++;
    }

    i2c_bus_num = mraa_find_i2c_bus_pci("0000:00", "0000:00:16.2", "i2c_designware.2");
    if (i2c_bus_num != -1) {
        b->i2c_bus[b->i2c_bus_count].bus_id = i2c_bus_num;
        b->i2c_bus[b->i2c_bus_count].sda = 75;
        b->i2c_bus[b->i2c_bus_count].scl = 77;
        b->i2c_bus_count++;
    }

    i2c_bus_num = mraa_find_i2c_bus_pci("0000:00", "0000:00:17.1", "i2c_designware.5");
    if (i2c_bus_num != -1) {
        b->i2c_bus[b->i2c_bus_count].bus_id = i2c_bus_num;
        b->i2c_bus[b->i2c_bus_count].sda = 15;
        b->i2c_bus[b->i2c_bus_count].scl = 17;
        b->i2c_bus_count++;
    }

    i2c_bus_num = mraa_find_i2c_bus_pci("0000:00", "0000:00:17.2", "i2c_designware.6");
    if (i2c_bus_num != -1) {
        b->i2c_bus[b->i2c_bus_count].bus_id = i2c_bus_num;
        b->i2c_bus[b->i2c_bus_count].sda = 19;
        b->i2c_bus[b->i2c_bus_count].scl = 21;
        b->i2c_bus_count++;
    }

    /**
     * Old i2c detection method, very poor, avoid, but keep as fallback if
     * above failed We check for /dev/i2c-0 because we can assume i2c-dev is
     * not loaded if we haven't enumerated a single i2c-dev node
     */
    if (b->i2c_bus_count == 0) {
        if (mraa_file_exist("/dev/i2c-0")) {
            syslog(LOG_WARNING, "joule: Failed to detect i2c buses, making wild assumptions!");
            b->i2c_bus_count = 3;
            b->i2c_bus[0].bus_id = 0;
            b->i2c_bus[0].sda = 11;
            b->i2c_bus[0].scl = 13;

            if (mraa_find_i2c_bus("designware", 5) != 5) {
                b->i2c_bus[1].bus_id = 9;
                b->i2c_bus[2].bus_id = 10;
            } else {
                b->i2c_bus[1].bus_id = 5;
                b->i2c_bus[2].bus_id = 6;
            }
            b->i2c_bus[1].sda = 15;
            b->i2c_bus[1].scl = 17;
            b->i2c_bus[2].sda = 19;
            b->i2c_bus[2].scl = 21;
        } else {
            syslog(LOG_WARNING, "joule: Failed to detect any i2c buses, is i2c-dev loaded?");
        }
    }

    b->def_i2c_bus = b->i2c_bus[0].bus_id;

    b->spi_bus_count = 5;
    b->def_spi_bus = 0;
    b->spi_bus[0].bus_id = 32766;
    b->spi_bus[0].slave_s = 0;
    b->spi_bus[1].bus_id = 32766;
    b->spi_bus[1].slave_s = 1;
    b->spi_bus[2].bus_id = 32766;
    b->spi_bus[2].slave_s = 2;
    b->spi_bus[3].bus_id = 32765;
    b->spi_bus[3].slave_s = 0;
    b->spi_bus[4].bus_id = 32765;
    b->spi_bus[4].slave_s = 2;

    b->uart_dev_count = 2;
    b->def_uart_dev = 0;
    b->uart_dev[0].device_path = "/dev/ttyS0";
    b->uart_dev[0].rx = 68;
    b->uart_dev[0].tx = 7;
    b->uart_dev[1].device_path = "/dev/ttyS1";
    b->uart_dev[1].rx = 24;
    b->uart_dev[1].tx = 22;

    // Aio shield detection
    // Only if i2c_designware.0 was successfully found
    if (i2c_aio_bus != -1) {

        syslog(LOG_NOTICE, "Attempting shield autodetection on I2C bus %d...", i2c_aio_bus);
        mraa_i2c_context i2c = mraa_i2c_init_raw(i2c_aio_bus);

        if (i2c == NULL)
            syslog(LOG_ERR, "Failed to open I2C bus: %d", i2c_aio_bus);
        else {
            if (mraa_i2c_address(i2c, 0x49) != MRAA_SUCCESS)
                syslog(LOG_ERR, "Failed to set I2C address: %d", 0x49);
            if (mraa_i2c_read_word_data(i2c, 0x01) < 0) {
                syslog(LOG_NOTICE, "No device at I2C address 0x49");

                // no DFRobot shield, try Grove
                if (mraa_i2c_address(i2c, 0x48) != MRAA_SUCCESS)
                    syslog(LOG_ERR, "Failed to set I2C address: %d", 0x48);
                if (mraa_i2c_read_word_data(i2c, 0x01) < 0)
                    syslog(LOG_NOTICE, "No device at I2C address 0x48");
                    // no shield

                else {
                    // load ads1015 at 0x48
                    syslog(LOG_NOTICE, "Loading ti-ads1015 module for Grove Shield");
                    if (system("modprobe ti-ads1015") < 0) {
                        syslog(LOG_NOTICE, "Failed, are you running the latest Joule kernel?");
                    } else {
                        char command[128];
                        snprintf(command, 128, "echo ads1015 0x48 >/sys/bus/i2c/devices/i2c-%d/new_device", i2c_aio_bus);
                        if (system(command) < 0) {
                            syslog(LOG_ERR, "Failed to add ads1015 device");
                        } else {
                            // success, setup aio pins
                            b->aio_count = 4;
                            b->adc_raw = 11; // 12-bit ads1015 - sign bit
                            b->adc_supported = 10;
                            pincount += b->aio_count;
                            shield = GROVE;

                            // max sample rate and 6.144 V reference
                            // additional aio functions can be added to make up for loss of precision at 3V3, 5V, etc
                            int i;
                            for (i = 0; i < b->aio_count; i++) {
                                snprintf(command, 128, "echo 3300 >/sys/bus/iio/devices/iio:device0/in_voltage%d_sampling_frequency", i);
                                system(command);
                                snprintf(command, 128, "echo 3 >/sys/bus/iio/devices/iio:device0/in_voltage%d_scale", i);
                                system(command);
                            }
                        }
                    }
                }
            } else {
                // load ads1115 at 0x48 and 0x49
                syslog(LOG_NOTICE, "Loading ti-ads1015 module for DFRobot Shield");
                if (system("modprobe ti-ads1015") < 0) {
                    syslog(LOG_NOTICE, "Failed, are you running the latest Joule kernel?");
                } else {
                    char command[128];
                    snprintf(command, 128, "echo ads1115 0x48 >/sys/bus/i2c/devices/i2c-%d/new_device", i2c_aio_bus);
                    if (system(command) < 0)
                        syslog(LOG_ERR, "Failed to add ads1115 device");
                    else {
                        snprintf(command, 128, "echo ads1115 0x49 >/sys/bus/i2c/devices/i2c-%d/new_device", i2c_aio_bus);
                        if (system(command) < 0)
                            syslog(LOG_ERR, "Failed to add ads1115 device");
                        else {
                            // success, setup aio pins
                            b->aio_count = 8;
                            b->adc_raw = 15; // 16-bit ads1115 - sign bit
                            b->adc_supported = 10;
                            b->adv_func->aio_get_valid_fp = &mraa_joule_expansion_board_get_valid_fp;
                            pincount += b->aio_count;
                            shield = DFROBOT;

                            // max sample rate and 6.144 V reference
                            // additional aio functions can be added to make up for loss of precision at 3V3, 5V, etc
                            int i;
                            for (i = 0; i < b->aio_count; i++) {
                                snprintf(command, 128, "echo 860 >/sys/bus/iio/devices/iio:device%d/in_voltage%d_sampling_frequency", i / 4, i % 4);
                                system(command);
                                snprintf(command, 128, "echo 3 >/sys/bus/iio/devices/iio:device%d/in_voltage%d_scale", i / 4, i % 4);
                                system(command);
                            }
                        }
                    }
                }
            }
        }
    }

    // initialize pins
    b->phy_pin_count = pincount;

    b->pins = (mraa_pininfo_t*) calloc(pincount, sizeof(mraa_pininfo_t));
    if (b->pins == NULL) {
        goto error;
    }

    int pos = 0;

    strncpy(b->pins[pos].name, "INVALID", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 0, 0, 0, 0, 0, 0, 0, 0 };
    pos++;

    // pin 1
    strncpy(b->pins[pos].name, "GPIO22", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 451;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "SPP1RX", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 1, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 421;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "PMICRST", 8);
    // disabled as this pin causes a reset
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 366;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "SPP1TX", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 1, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 422;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "19.2mhz", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 356;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "SPP1FS0", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 1, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 417;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "UART0TX", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 1 };
    b->pins[pos].gpio.pinmap = 468;
    b->pins[pos].gpio.mux_total = 0;
    b->pins[pos].uart.pinmap = 0;
    b->pins[pos].uart.parent_id = 0;
    b->pins[pos].uart.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "SPP1FS2", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 1, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 419;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "PWRGD", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 0, 0 };
    // not configured as GPI0 - does read work?
    //b->pins[pos].gpio.pinmap = 365;
    //b->pins[pos].gpio.mux_total = 0;
    pos++;

    // pin 10
    strncpy(b->pins[pos].name, "SPP1CLK", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 1, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 416;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "I2C0SDA", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 1, 0, 0 };
    b->pins[pos].gpio.pinmap = 315;
    b->pins[pos].gpio.mux_total = 0;
    b->pins[pos].i2c.mux_total = 0;
    b->pins[pos].i2c.pinmap = 0;
    pos++;

    strncpy(b->pins[pos].name, "I2S1SDI", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 381;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "I2C0SCL", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 1, 0, 0 };
    b->pins[pos].gpio.pinmap = 316;
    b->pins[pos].gpio.mux_total = 0;
    b->pins[pos].i2c.mux_total = 0;
    b->pins[pos].i2c.pinmap = 0;
    pos++;

    strncpy(b->pins[pos].name, "I2S1SDO", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 382;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "II0SDA", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 1, 0, 0 };
    b->pins[pos].gpio.pinmap = 331;
    b->pins[pos].gpio.mux_total = 0;
    b->pins[pos].i2c.mux_total = 0;
    b->pins[pos].i2c.pinmap = 0;
    pos++;

    strncpy(b->pins[pos].name, "I2S1WS", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 380;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "IIC0SCL", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 1, 0, 0 };
    b->pins[pos].gpio.pinmap = 332;
    b->pins[pos].gpio.mux_total = 0;
    b->pins[pos].i2c.pinmap = 0;
    b->pins[pos].i2c.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "I2S1CLK", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 379;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "IIC1SDA", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 1, 0, 0 };
    b->pins[pos].gpio.pinmap = 333;
    b->pins[pos].gpio.mux_total = 0;
    b->pins[pos].i2c.pinmap = 0;
    b->pins[pos].i2c.mux_total = 0;
    pos++;

    // pin 20
    strncpy(b->pins[pos].name, "I2S1MCL", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 378;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "IIC1SCL", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 1, 0, 0 };
    b->pins[pos].gpio.pinmap = 334;
    b->pins[pos].gpio.mux_total = 0;
    b->pins[pos].i2c.mux_total = 0;
    b->pins[pos].i2c.pinmap = 0;
    pos++;

    strncpy(b->pins[pos].name, "UART1TX", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 1 };
    b->pins[pos].gpio.pinmap = 472;
    b->pins[pos].gpio.mux_total = 0;
    b->pins[pos].uart.pinmap = 0;
    b->pins[pos].uart.parent_id = 0;
    b->pins[pos].uart.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "ISH_IO6", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 343;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "UART1RX", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 1 };
    b->pins[pos].gpio.pinmap = 471;
    b->pins[pos].gpio.mux_total = 0;
    b->pins[pos].uart.pinmap = 0;
    b->pins[pos].uart.parent_id = 0;
    b->pins[pos].uart.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "ISH_IO5", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 342;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "PWM0", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 1, 0, 0, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 463;
    b->pins[pos].gpio.mux_total = 0;
    b->pins[pos].pwm.pinmap = 0;
    b->pins[pos].pwm.parent_id = 0;
    b->pins[pos].pwm.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "ISH_IO4", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 341;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "PWM1", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 1, 0, 0, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 464;
    b->pins[pos].gpio.mux_total = 0;
    b->pins[pos].pwm.pinmap = 1;
    b->pins[pos].pwm.parent_id = 0;
    b->pins[pos].pwm.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "ISH_IO3", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 };
    // High level will be V1P8 - VBE on MBT3904D
    b->pins[pos].gpio.pinmap = 340;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    // pin 30
    strncpy(b->pins[pos].name, "PWM2", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 1, 0, 0, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 465;
    b->pins[pos].gpio.mux_total = 0;
    b->pins[pos].pwm.pinmap = 2;
    b->pins[pos].pwm.parent_id = 0;
    b->pins[pos].pwm.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "ISH_IO2", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 };
    // High level will be V1P8 - VBE on MBT3904D
    b->pins[pos].gpio.pinmap = 339;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "PWM3", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 1, 0, 0, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 466;
    b->pins[pos].gpio.mux_total = 0;
    b->pins[pos].pwm.pinmap = 3;
    b->pins[pos].pwm.parent_id = 0;
    b->pins[pos].pwm.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "ISH_IO1", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 };
    // High level will be V1P8 - VBE on MBT3904D
    b->pins[pos].gpio.pinmap = 338;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "1.8V", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 0, 0 };
    pos++;

    strncpy(b->pins[pos].name, "ISH_IO0", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 };
    // High level will be V1P8 - VBE on MBT3904D
    b->pins[pos].gpio.pinmap = 337;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "GND", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 0, 0 };
    pos++;

    strncpy(b->pins[pos].name, "GND", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 0, 0 };
    pos++;

    strncpy(b->pins[pos].name, "GND", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 0, 0 };
    pos++;

    strncpy(b->pins[pos].name, "GND", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 0, 0 };
    pos++;

    // pin 40
    strncpy(b->pins[pos].name, "3.3V", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 0, 0 };
    pos++;

    // second header
    strncpy(b->pins[pos].name, "GND", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 0, 0 };
    pos++;

    strncpy(b->pins[pos].name, "5V", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 0, 0 };
    pos++;

    strncpy(b->pins[pos].name, "GND", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 0, 0 };
    pos++;

    strncpy(b->pins[pos].name, "5V", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 0, 0 };
    pos++;

    strncpy(b->pins[pos].name, "GND", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 0, 0 };
    pos++;

    strncpy(b->pins[pos].name, "3.3V", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 0, 0 };
    pos++;

    strncpy(b->pins[pos].name, "GND", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 0, 0 };
    pos++;

    strncpy(b->pins[pos].name, "3.3V", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 0, 0 };
    pos++;

    strncpy(b->pins[pos].name, "GND", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 0, 0 };
    pos++;

    // pin 50
    strncpy(b->pins[pos].name, "1.8V", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 0, 0 };
    pos++;

    strncpy(b->pins[pos].name, "GPIO", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 456;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "1.8V", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 0, 0 };
    pos++;

    strncpy(b->pins[pos].name, "PANEL", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 270;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "GND", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 0, 0 };
    pos++;

    strncpy(b->pins[pos].name, "PANEL", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 271;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "CAMERA", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 0, 0 };
    pos++;

    strncpy(b->pins[pos].name, "PANEL", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 272;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "CAMERA", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 0, 0 };
    pos++;

    strncpy(b->pins[pos].name, "SPP0FS0", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 1, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 411;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    // pin 60
    strncpy(b->pins[pos].name, "CAMERA", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 0, 0 };
    pos++;

    strncpy(b->pins[pos].name, "SPP0FS1", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 1, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 412;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "SPI_DAT", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 385;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "SPP0FS2", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 1, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 413;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "SPICLKB", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 384;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "SPP0CLK", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 1, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 410;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "SPICLKA", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 383;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "SPP0TX", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 1, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 414;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "UART0RX", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 467;
    b->pins[pos].gpio.mux_total = 0;
    b->pins[pos].uart.pinmap = 0;
    b->pins[pos].uart.parent_id = 0;
    b->pins[pos].uart.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "SPP0RX", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 1, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 415;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    // pin 70
    strncpy(b->pins[pos].name, "UART0RT", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 1 };
    b->pins[pos].gpio.pinmap = 469;
    b->pins[pos].gpio.mux_total = 0;
    b->pins[pos].uart.pinmap = 0;
    b->pins[pos].uart.parent_id = 0;
    b->pins[pos].uart.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "I2C1SDA", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 1, 0, 0 };
    b->pins[pos].gpio.pinmap = 317;
    b->pins[pos].gpio.mux_total = 0;
    b->pins[pos].i2c.pinmap = 0;
    b->pins[pos].i2c.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "UART0CT", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 1 };
    b->pins[pos].gpio.pinmap = 470;
    b->pins[pos].gpio.mux_total = 0;
    b->pins[pos].uart.pinmap = 0;
    b->pins[pos].uart.parent_id = 0;
    b->pins[pos].uart.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "I2C1SCL", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 1, 0, 0 };
    b->pins[pos].gpio.pinmap = 318;
    b->pins[pos].gpio.mux_total = 0;
    b->pins[pos].i2c.pinmap = 0;
    b->pins[pos].i2c.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "IURT0TX", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 1 };
    b->pins[pos].gpio.pinmap = 480;
    b->pins[pos].gpio.mux_total = 0;
    b->pins[pos].uart.pinmap = 0;
    b->pins[pos].uart.parent_id = 0;
    b->pins[pos].uart.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "I2C2SDA", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 1, 0, 0 };
    b->pins[pos].gpio.pinmap = 319;
    b->pins[pos].gpio.mux_total = 0;
    b->pins[pos].i2c.pinmap = 0;
    b->pins[pos].i2c.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "IURT0RX", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 1 };
    b->pins[pos].gpio.pinmap = 479;
    b->pins[pos].gpio.mux_total = 0;
    b->pins[pos].uart.pinmap = 0;
    b->pins[pos].uart.parent_id = 0;
    b->pins[pos].uart.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "I2C2SCL", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 1, 0, 0 };
    b->pins[pos].gpio.pinmap = 320;
    b->pins[pos].gpio.mux_total = 0;
    b->pins[pos].i2c.pinmap = 0;
    b->pins[pos].i2c.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "IURT0RT", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 1 };
    b->pins[pos].gpio.pinmap = 481;
    b->pins[pos].gpio.mux_total = 0;
    b->pins[pos].uart.pinmap = 0;
    b->pins[pos].uart.parent_id = 0;
    b->pins[pos].uart.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "RTC_CLK", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 367;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    // pin 80
    strncpy(b->pins[pos].name, "IURT0CT", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 1 };
    b->pins[pos].gpio.pinmap = 482;
    b->pins[pos].uart.pinmap = 0;
    b->pins[pos].uart.parent_id = 0;
    b->pins[pos].uart.mux_total = 0;
    pos++;

    while (pos != 100) {
        b->pins[pos].capabilities = (mraa_pincapabilities_t){ 0, 0, 0, 0, 0, 0, 0, 0 };
        pos++;
    }

    strncpy(b->pins[pos].name, "LED100", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 337;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "LED101", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 338;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "LED102", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 339;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "LED103", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 340;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "LEDBT", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 438;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    strncpy(b->pins[pos].name, "LEDWIFI", 8);
    b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 };
    b->pins[pos].gpio.pinmap = 439;
    b->pins[pos].gpio.mux_total = 0;
    pos++;

    switch (shield) {

        case DFROBOT:

            strncpy(b->pins[pos + 4].name, "A4", 8);
            b->pins[pos + 4].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 1, 0 };
            b->pins[pos + 4].aio.pinmap = 4;
            b->pins[pos + 4].aio.mux_total = 0;

            strncpy(b->pins[pos + 5].name, "A5", 8);
            b->pins[pos + 5].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 1, 0 };
            b->pins[pos + 5].aio.pinmap = 5;
            b->pins[pos + 5].aio.mux_total = 0;

            strncpy(b->pins[pos + 6].name, "A6", 8);
            b->pins[pos + 6].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 1, 0 };
            b->pins[pos + 6].aio.pinmap = 6;
            b->pins[pos + 6].aio.mux_total = 0;

            strncpy(b->pins[pos + 5].name, "A7", 8);
            b->pins[pos + 7].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 1, 0 };
            b->pins[pos + 7].aio.pinmap = 7;
            b->pins[pos + 7].aio.mux_total = 0;

        case GROVE:

            strncpy(b->pins[pos].name, "A0", 8);
            b->pins[pos].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 1, 0 };
            b->pins[pos].aio.pinmap = 0;
            b->pins[pos].aio.mux_total = 0;

            strncpy(b->pins[pos + 1].name, "A1", 8);
            b->pins[pos + 1].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 1, 0 };
            b->pins[pos + 1].aio.pinmap = 1;
            b->pins[pos + 1].aio.mux_total = 0;

            strncpy(b->pins[pos + 2].name, "A2", 8);
            b->pins[pos + 2].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 1, 0 };
            b->pins[pos + 2].aio.pinmap = 2;
            b->pins[pos + 2].aio.mux_total = 0;

            strncpy(b->pins[pos + 3].name, "A3", 8);
            b->pins[pos + 3].capabilities = (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 1, 0 };
            b->pins[pos + 3].aio.pinmap = 3;
            b->pins[pos + 3].aio.mux_total = 0;

            break;

        case NO_SHIELD:

            b->aio_count = 0;
            b->adc_raw = 0;
            b->adc_supported = 0;
            break;

        default: break;
    }

    return b;

error:
    syslog(LOG_CRIT, "Intel Joule Expansion: Platform failed to initialise");
    free(b);
    return NULL;
}
