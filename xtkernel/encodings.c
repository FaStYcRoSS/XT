#include <xt/encodings.h>
#include <xt/kernel.h>

XTResult XTEXPORT xtUTF8toUTF16(const uint8_t* utf8, uint16_t* utf16) {
    size_t out = 0;
    while (*utf8) {
        uint32_t cp = 0;
        uint8_t b0 = utf8[0];
        if ((b0 & 0x80) == 0) { // 1 byte
            cp = b0;
            utf8 += 1;
        } else if ((b0 & 0xE0) == 0xC0) { // 2 bytes
            cp = ((b0 & 0x1F) << 6) | (utf8[1] & 0x3F);
            utf8 += 2;
        } else if ((b0 & 0xF0) == 0xE0) { // 3 bytes
            cp = ((b0 & 0x0F) << 12) | ((utf8[1] & 0x3F) << 6) | (utf8[2] & 0x3F);
            utf8 += 3;
        } else if ((b0 & 0xF8) == 0xF0) { // 4 bytes
            cp = ((b0 & 0x07) << 18) | ((utf8[1] & 0x3F) << 12) | ((utf8[2] & 0x3F) << 6) | (utf8[3] & 0x3F);
            utf8 += 4;
        } else {
            // невалидный UTF-8, можно пропустить или вернуть ошибку
            utf8 += 1;
            continue;
        }

        if (cp <= 0xFFFF) {
            utf16[out++] = (uint16_t)cp;
        } else {
            cp -= 0x10000;
            utf16[out++] = (uint16_t)((cp >> 10) + 0xD800);
            utf16[out++] = (uint16_t)((cp & 0x3FF) + 0xDC00);
        }
    }
    utf16[out] = 0;
    return XT_SUCCESS;
}

XTResult XTEXPORT xtUTF16toUTF8(const uint16_t* utf16, uint8_t* utf8) {
    int outPos = 0;
    for (int i = 0; utf16[i]; i++) {
        uint32_t cp;
        uint16_t high = utf16[i];
        if (high >= 0xD800 && high <= 0xDBFF) { // суррогатная пара
            uint16_t low = utf16[++i];
            cp = 0x10000 + ((high - 0xD800) << 10) | (low - 0xDC00);
        } else {
            cp = high;
        }
        if (cp <= 0x7F) {
            utf8[outPos++] = (uint8_t)cp;
        } else if (cp <= 0x7FF) {
            utf8[outPos++] = (uint8_t)((cp >> 6) | 0xC0);
            utf8[outPos++] = (uint8_t)((cp & 0x3F) | 0x80);
        } else if (cp <= 0xFFFF) {
            utf8[outPos++] = (uint8_t)((cp >> 12) | 0xE0);
            utf8[outPos++] = (uint8_t)(((cp >> 6) & 0x3F) | 0x80);
            utf8[outPos++] = (uint8_t)((cp & 0x3F) | 0x80);
        } else {
            utf8[outPos++] = (uint8_t)((cp >> 18) | 0xF0);
            utf8[outPos++] = (uint8_t)(((cp >> 12) & 0x3F) | 0x80);
            utf8[outPos++] = (uint8_t)(((cp >> 6) & 0x3F) | 0x80);
            utf8[outPos++] = (uint8_t)((cp & 0x3F) | 0x80);
        }
    }
    utf8[outPos] = 0;
    return XT_SUCCESS;
}
