#pragma once

/**
 * Redirect ESP-IDF log output to the TinyUSB CDC ACM virtual serial port.
 * Call once after usb_composite_init() has installed TinyUSB.
 */
void usb_cdc_log_init(void);
