#pragma once

struct RGB {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    // 0 to 255
};

struct HSV {
    float h; // hue [0, 360]
    float s; // saturation [0, 1]
    float v; // value(brightness) [0, 1]
};

RGB HSVtoRGB(HSV hsv);