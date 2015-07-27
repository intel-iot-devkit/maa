/*
 * Author: Brendan Le Foll <brendan.le.foll@intel.com>
 * Author: Thomas Ingleby <thomas.c.ingleby@intel.com>
 * Copyright © 2014 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#pragma once

#include "types.h"

#define MRAA_PLATFORM_NAME_MAX_SIZE 64
#define MRAA_PIN_NAME_SIZE 12

#define MRAA_SUB_PLATFORM_BIT_SHIFT 9
#define MRAA_SUB_PLATFORM_MASK (1<<MRAA_SUB_PLATFORM_BIT_SHIFT)



/** @file
 *
 * This file defines the basic shared values for libmraa
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * MRAA boolean type
 * 1 For TRUE
 */
typedef unsigned int mraa_boolean_t;

/**
 * Initialise MRAA
 *
 * Detects running platform and attempts to use included pinmap, this is run on
 * module/library init/load but is handy to rerun to check board initialised
 * correctly. MRAA_SUCCESS inidicates correct (first time) initialisation
 * whilst MRAA_ERROR_PLATFORM_ALREADY_INITIALISED indicates the board is
 * already initialised correctly
 *
 * @return Result of operation
 */
#if (defined SWIGPYTHON) || (defined SWIG)
mraa_result_t mraa_init();
#else
// this sets a compiler attribute (supported by GCC & clang) to have mraa_init()
// be called as a constructor make sure your libc supports this!  uclibc needs
// to be compiled with UCLIBC_CTOR_DTOR
mraa_result_t mraa_init() __attribute__((constructor));
#endif

/**
 * De-Initilise MRAA
 *
 * This is not a strict requirement but useful to test memory leaks and for
 * people who like super clean code. If dynamically loading & unloading
 * libmraa you need to call this before unloading the library.
 */
void mraa_deinit();

/**
 * Checks if a pin is able to use the passed in mode.
 *
 * @param pin Physical Pin to be checked.
 * @param mode the mode to be tested.
 * @return boolean if the mode is supported, 0=false.
 */
mraa_boolean_t mraa_pin_mode_test(int pin, mraa_pinmodes_t mode);

/**
 * Check the board's  bit size when reading the value
 *
 * @return raw bits being read from kernel module. zero if no ADC
 */
unsigned int mraa_adc_raw_bits();

/**
 * Return value that the raw value should be shifted to. Zero if no ADC
 *
 * @return return actual bit size the adc value should be understood as.
 */
unsigned int mraa_adc_supported_bits();

/**
 * Sets the log level to use from 0-7 where 7 is very verbose. These are the
 * syslog log levels, see syslog(3) for more information on the levels.
 *
 * @return Result of operation
 */
mraa_result_t mraa_set_log_level(int level);

/**
 * Return the Platform's Name, If no platform detected return NULL
 *
 * @return platform name
 */
char* mraa_get_platform_name();

/**
 * This function attempts to set the mraa process to a given priority and the
 * scheduler to SCHED_RR. Highest * priority is typically 99 and minimum is 0.
 * This function * will set to MAX if * priority is > MAX. Function will return
 * -1 on failure.
 *
 * @param priority Value from typically 0 to 99
 * @return The priority value set
 */
int mraa_set_priority(const unsigned int priority);

/** Get the version string of mraa autogenerated from git tag
 *
 * The version returned may not be what is expected however it is a reliable
 * number associated with the git tag closest to that version at build time
 *
 * @return version string from version.h
 */
const char* mraa_get_version();

/**
 * Print a textual representation of the mraa_result_t
 *
 * @param result the result to print
 */
void mraa_result_print(mraa_result_t result);

/**
 * Get platform type, board must be initialised.
 *
 * @return mraa_platform_t Platform type enum
 */
mraa_platform_t mraa_get_platform_type();

/**
 * Get platform pincount, board must be initialised.
 *
 * @return uint of physical pin count on the in-use platform
 */
unsigned int mraa_get_pin_count();

/**
 * Get platform usable I2C bus count, board must be initialised.
 *
 * @return number f usable I2C bus count on the current platform. Function will
 * return -1 on failure
 */
int mraa_get_i2c_bus_count();

/**
 * Get I2C adapter number in sysfs.
 *
 * @param i2c_bus the logical I2C bus number
 * @return I2C adapter number in sysfs. Function will return -1 on failure
 */
int mraa_get_i2c_bus_id(unsigned int i2c_bus);

/**
* Get name of pin, board must be initialised.
*
* @param pin number
* @return char* of pin name
*/
char* mraa_get_pin_name(int pin);

/**
 * Get default i2c bus, board must be initialised.
 *
 * @return int default i2c bus index
 */
int mraa_get_default_2c_bus();

/**
 * Select main platform for platform info calls.
 *
 * @return mraa_boolean_t 1 if main platform is available, 0 otherwise
 */
mraa_boolean_t mraa_select_main_platform();

/**
 * Select sub platform for platform info calls.
 *
 * @return mraa_boolean_t 1 if sub platform is available, 0 otherwise
 */
mraa_boolean_t mraa_select_sub_platform();
	
/**
 * Check if sub platform is currently available and selected for platform info calls.
 *
 * @return mraa_boolean_t 1 if sub platform is selected, 0 otherwise
 */
mraa_boolean_t mraa_is_sub_platform_selected();

/**
 * Check if pin or bus id includes sub platform mask.
 *
 * @param int pin or bus number
 *
 * @return mraa_boolean_t 1 if pin or bus is for sub platform, 0 otherwise
 */
mraa_boolean_t mraa_is_on_sub_platform(int pin_or_bus);

/**
 * Convert pin or bus id to corresponding sub platform id.
 *
 * @param int pin or bus number
 *
 * @return int sub platform pin or bus number
 */
int mraa_use_sub_platform(int pin_or_bus);

/**
 * Convert pin or bus sub platform id to base platform id.
 *
 * @param int sub platform pin or bus number
 *
 * @return int base platform pin or bus number
 */
int mraa_get_sub_platform_index(int pin_or_bus);



#ifdef __cplusplus
}
#endif
