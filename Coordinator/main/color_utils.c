#include "color_utils.h"
#include <math.h>

// --- Zigbee and Color Constants ---
#define HUE_MAX_ZIGBEE 254.0f
#define ZCL_MAX_VALUE 65535.0f

// sRGB to CIE XYZ Transformation Matrix (Adapted from sRGB D65)
// This matrix is used for the conversion from RGB (Step 2)
#define M11 0.4124564f
#define M12 0.3575761f
#define M13 0.1804375f
#define M21 0.2126729f
#define M22 0.7151522f
#define M23 0.0721750f
#define M31 0.0193339f
#define M32 0.1191920f
#define M33 0.9503041f

/**
 * @brief Converts a Zigbee Hue value (0-254) to CIE 1931 X/Y coordinates (0-65535).
 * * This conversion assumes maximum saturation and brightness (Value V=1.0).
 * * @param hue The 8-bit Hue value (0-254).
 * @return ColorXY structure containing the resulting 16-bit x and y coordinates.
 */
ColorXY get_xy_from_hue(uint8_t hue)
{
    ColorXY result = {0, 0};

    // --- Step 1: Hue/Sat to Normalized RGB (0.0 - 1.0) ---
    // Since only HUE is provided, we assume SATURATION = 1.0 and VALUE = 1.0
    float h = (float)hue * 360.0f / HUE_MAX_ZIGBEE; // Hue in degrees (0-360)
    float s = 1.0f;                                // Saturation (Max)
    float v = 1.0f;                                // Value/Brightness (Max)

    float r_norm, g_norm, b_norm;
    
    // HSV to RGB standard conversion
    int i = floor(h / 60.0f);
    float f = h / 60.0f - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);
    
    switch (i % 6) {
        case 0: r_norm = v; g_norm = t; b_norm = p; break;
        case 1: r_norm = q; g_norm = v; b_norm = p; break;
        case 2: r_norm = p; g_norm = v; b_norm = t; break;
        case 3: r_norm = p; g_norm = q; b_norm = v; break;
        case 4: r_norm = t; g_norm = p; b_norm = v; break;
        default: r_norm = v; g_norm = p; b_norm = q; break;
    }
    
    // --- Step 2: Normalized RGB to CIE XYZ ---
    float X_float = r_norm * M11 + g_norm * M12 + b_norm * M13;
    float Y_float = r_norm * M21 + g_norm * M22 + b_norm * M23;
    float Z_float = r_norm * M31 + g_norm * M32 + b_norm * M33;

    // --- Step 3: XYZ to Chromaticity XY and Scale ---
    float denominator = X_float + Y_float + Z_float;
    
    if (denominator > 0.0f) {
        float normalized_x = X_float / denominator;
        float normalized_y = Y_float / denominator;

        // Scale back to 16-bit integer (0-65535)
        result.x = (uint16_t)roundf(normalized_x * ZCL_MAX_VALUE);
        result.y = (uint16_t)roundf(normalized_y * ZCL_MAX_VALUE);
    }
    
    return result;
}
