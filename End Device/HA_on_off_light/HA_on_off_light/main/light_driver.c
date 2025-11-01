/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: LicenseRef-Included
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Espressif Systems
 * integrated circuit in a product or a software update for such product,
 * must reproduce the above copyright notice, this list of conditions and
 * the following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * 4. Any software provided in binary form under this license must not be reverse
 * engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "esp_log.h"
#include "led_strip.h"
#include "light_driver.h"
#include "math.h"

static led_strip_handle_t s_led_strip;
static uint8_t s_base_red = 255, s_base_green = 255, s_base_blue = 255;
static uint8_t s_level = 255; // Brightness level (0-255)
static bool s_power = false; // On/Off state
static uint8_t s_hue = 0;       // Current Hue (0-254)

#define HUE_SCALE_FACTOR 255
#define HUE_MAX_ZIGBEE 254
#define ZIGBEE_MAX_COLOR_VALUE 65535.0f

/**
 * @brief Helper function to apply the current state (color * power * level) to the LED strip.
 */
static void light_driver_update_led(void)
{
    uint32_t final_r = 0, final_g = 0, final_b = 0;
    
    if (s_power && s_level > 0) {
        // Calculate the final color intensity by multiplying the base color
        // by the current brightness level (0-255) and normalizing by 255.
        // This scales the base RGB color according to the dimming level.
        final_r = (uint32_t)s_base_red * s_level / 255;
        final_g = (uint32_t)s_base_green * s_level / 255;
        final_b = (uint32_t)s_base_blue * s_level / 255;
    }
    
    ESP_ERROR_CHECK(led_strip_set_pixel(s_led_strip, 0, final_r, final_g, final_b));
    ESP_ERROR_CHECK(led_strip_refresh(s_led_strip));
}

/**
 * @brief Sets the On/Off state (ON/OFF Cluster)
 */
void light_driver_set_power(bool power)
{
    s_power = power;
    light_driver_update_led();
}

/**
 * @brief Sets the brightness level (LEVEL CONTROL Cluster)
 */
void light_driver_set_level(uint8_t level)
{
    s_level = level;
    light_driver_update_led();
}

/**
 * @brief Sets the base RGB color (Called by COLOR CONTROL Cluster handlers)
 * Note: Actual Zigbee implementation would use Hue/Sat or X/Y and convert to RGB here.
 */
void light_driver_set_rgb_base(uint8_t red, uint8_t green, uint8_t blue)
{
    s_base_red = red;
    s_base_green = green;
    s_base_blue = blue;
    // Only update the LED if power is on to avoid waking the system unnecessarily
    if (s_power) {
        light_driver_update_led();
    }
}

/**
 * @brief Driver initialization
 */
void light_driver_init(bool power)
{
    led_strip_config_t led_strip_conf = {
        .max_leds = CONFIG_EXAMPLE_STRIP_LED_NUMBER,
        .strip_gpio_num = CONFIG_EXAMPLE_STRIP_LED_GPIO,
    };
    led_strip_rmt_config_t rmt_conf = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&led_strip_conf, &rmt_conf, &s_led_strip));
    
    // Set initial state
    s_power = power;
    // Note: We don't call light_driver_update_led here, light_driver_set_power handles the first refresh
    light_driver_set_power(power);
}

/**
 * @brief Converts Zigbee Hue (0-254) to RGB and updates the base color
 */
 void light_driver_set_hue(uint8_t hue)
{
    // Scale hue from 0-254 (Zigbee) to 0-255 (for 8-bit math)
    uint8_t h_scaled = (uint8_t)(((uint16_t)hue * HUE_SCALE_FACTOR) / HUE_MAX_ZIGBEE);

    // Standard 8-bit HSV-to-RGB conversion (for S=255, V=255)
    uint8_t region = h_scaled / 43;      // 6 regions, 43 units each (43*6=258)
    uint8_t remainder = (h_scaled % 43) * 6; // Scale remainder up to 0-255

    uint8_t q = 255 - remainder;
    uint8_t t = remainder;

    switch (region) {
        case 0: // Red -> Yellow
            s_base_red = 255; s_base_green = t; s_base_blue = 0;
            break;
        case 1: // Yellow -> Green
            s_base_red = q; s_base_green = 255; s_base_blue = 0;
            break;
        case 2: // Green -> Cyan
            s_base_red = 0; s_base_green = 255; s_base_blue = t;
            break;
        case 3: // Cyan -> Blue
            s_base_red = 0; s_base_green = q; s_base_blue = 255;
            break;
        case 4: // Blue -> Magenta
            s_base_red = t; s_base_green = 0; s_base_blue = 255;
            break;
        default: // Magenta -> Red
            s_base_red = 255; s_base_green = 0; s_base_blue = q;
            break;
    }

    light_driver_update_led();
}

void light_driver_set_xy(uint16_t x, uint16_t y)
{
    // 1. Normalize 16-bit Zigbee values (0-65535) to float coordinates (0.0-1.0)
    float fx = (float)x / ZIGBEE_MAX_COLOR_VALUE;
    float fy = (float)y / ZIGBEE_MAX_COLOR_VALUE;

    if (fy == 0.0f) {
        // Handle division by zero: if Y is zero, the point is outside the gamut (or black).
        light_driver_set_rgb_base(0, 0, 0);
        return;
    }

    // 2. Convert normalized x, y coordinates to CIE XYZ space.
    // Assume maximum luminance (Y=1.0) for the conversion.
    float Y = 1.0f; // Luminance
    float X = (fx / fy) * Y;
    float Z = ((1.0f - fx - fy) / fy) * Y;

    // 3. Convert CIE XYZ to sRGB (using standard D65 white point matrix)
    // R = 3.2404542 X - 1.5371385 Y - 0.4985314 Z
    // G = -0.9692660 X + 1.8760633 Y + 0.0415560 Z
    // B = 0.0556434 X - 0.2040259 Y + 1.0573110 Z

    float r = X * 3.2404542f - Y * 1.5371385f - Z * 0.4985314f;
    float g = X * (-0.9692660f) + Y * 1.8760633f + Z * 0.0415560f;
    float b = X * 0.0556434f - Y * 0.2040259f + Z * 1.0573110f;

    // 4. Apply a simplified gamma correction (if value is < 0.0031308, use linear, otherwise power 2.4)
    // Note: Since this is often complex for embedded, we skip full gamma correction 
    // and rely on clipping and direct scaling for speed and simplicity.

    // 5. Clip and Scale RGB to 8-bit (0-255)
    uint8_t final_r = (uint8_t)(fminf(fmaxf(r, 0.0f), 1.0f) * 255.0f);
    uint8_t final_g = (uint8_t)(fminf(fmaxf(g, 0.0f), 1.0f) * 255.0f);
    uint8_t final_b = (uint8_t)(fminf(fmaxf(b, 0.0f), 1.0f) * 255.0f);

    light_driver_set_rgb_base(final_r, final_g, final_b);
}