#ifndef _COLOR_UTILS_H_
#define _COLOR_UTILS_H_

#include <stdint.h>

/**
 * @brief Structure to hold the 16-bit CIE 1931 x and y coordinates.
 * These coordinates range from 0 to 65535, as defined by Zigbee ZCL.
 */
typedef struct {
    uint16_t x;
    uint16_t y;
} ColorXY;

/**
 * @brief Converts a Zigbee Hue value (0-254) to CIE 1931 X/Y coordinates (0-65535).
 * * This conversion assumes maximum saturation (254/254) and maximum value (brightness V=1.0).
 * * @param hue The 8-bit Hue value (0-254) from the Color Control Cluster.
 * @return ColorXY structure containing the resulting 16-bit x and y coordinates.
 */
ColorXY get_xy_from_hue(uint8_t hue);

#endif // _COLOR_UTILS_H_
