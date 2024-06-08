#include <color_spaces.h>
#include <cmath>

RGB HSVtoRGB(HSV hsv) {
    float r, g, b;

    float h = hsv.h;
    float s = hsv.s;
    float v = hsv.v;

    float c = v * s; // Chroma
    float x = c * (1 - std::fabs(fmod(h / 60.0, 2) - 1));
    float m = v - c;

    if (0 <= h && h < 60) {
        r = c;
        g = x;
        b = 0;
    } else if (60 <= h && h < 120) {
        r = x;
        g = c;
        b = 0;
    } else if (120 <= h && h < 180) {
        r = 0;
        g = c;
        b = x;
    } else if (180 <= h && h < 240) {
        r = 0;
        g = x;
        b = c;
    } else if (240 <= h && h < 300) {
        r = x;
        g = 0;
        b = c;
    } else if (300 <= h && h < 360) {
        r = c;
        g = 0;
        b = x;
    } else {
        r = 0;
        g = 0;
        b = 0;
    }

    r = (r + m) * 255;
    g = (g + m) * 255;
    b = (b + m) * 255;

    return {
        static_cast<unsigned char>(std::round(r)),
        static_cast<unsigned char>(std::round(g)),
        static_cast<unsigned char>(std::round(b))
    };
}