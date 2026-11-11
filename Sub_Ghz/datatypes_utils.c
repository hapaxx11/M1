/* See COPYING.txt for license details. */

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "datatypes_utils.h"

char *hexStrToBinStr(char *hexStr)
{
    if (hexStr == NULL)
        return NULL;

    const size_t hexstr_len = strlen(hexStr);
    if (hexstr_len == 0 || (hexstr_len & 1U) != 0U ||
        hexstr_len > (SIZE_MAX - 1U) / 8U)
        return NULL;

    char *binStr = malloc(hexstr_len * 8U + 1U);
    if (binStr == NULL)
        return NULL;

    size_t binstr_cnt = 0;
    for (size_t i = 0; i < hexstr_len; i += 2U)
    {
        const char high = hexStr[i];
        const char low = hexStr[i + 1U];
        const bool valid_high = (high >= '0' && high <= '9') ||
                                (high >= 'A' && high <= 'F') ||
                                (high >= 'a' && high <= 'f');
        const bool valid_low = (low >= '0' && low <= '9') ||
                               (low >= 'A' && low <= 'F') ||
                               (low >= 'a' && low <= 'f');
        if (!valid_high || !valid_low)
        {
            free(binStr);
            return NULL;
        }

        const uint8_t value = (uint8_t)((hexCharToDecimal(high) << 4) |
                                        hexCharToDecimal(low));
        for (uint8_t mask = 0x80U; mask != 0U; mask >>= 1U)
            binStr[binstr_cnt++] = (value & mask) ? '1' : '0';
    }

    binStr[binstr_cnt] = '\0';
    return binStr;
} // char *hexStrToBinStr(char *hexStr)




// Converts a Hex char to decimal
uint8_t hexCharToDecimal(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    else if (c >= 'A' && c <= 'F')
    {
        return c - 'A' + 10;
    }
    else if (c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }
    return 0;
} // uint8_t hexCharToDecimal(char c)



// Convert a Hex string to decimal
uint32_t hexStringToDecimal(const char *hexString)
{
    if (hexString == NULL)
        return 0;

    const size_t len = strlen(hexString);
    if ((len & 1U) != 0U)
        return 0;

    uint32_t decimal = 0;
    for (size_t i = 0; i < len; i += 2U)
    {
        const char high = hexString[i];
        const char low = hexString[i + 1U];
        const bool valid_high = (high >= '0' && high <= '9') ||
                                (high >= 'A' && high <= 'F') ||
                                (high >= 'a' && high <= 'f');
        const bool valid_low = (low >= '0' && low <= '9') ||
                               (low >= 'A' && low <= 'F') ||
                               (low >= 'a' && low <= 'f');
        if (!valid_high || !valid_low)
            return 0;

        decimal = (decimal << 8) |
                  (hexCharToDecimal(high) << 4) | hexCharToDecimal(low);
    }

    return decimal;
} // uint32_t hexStringToDecimal(const char *hexString)



char *dec2binWzerofill(uint64_t Dec, unsigned int bitLength)
{
    if (bitLength > 64U)
        return NULL;

    char *bin = (char *)malloc(bitLength + 1U);
    if (bin == NULL)
        return NULL;

    bin[bitLength] = '\0';
    for (unsigned int i = bitLength; i > 0U; i--)
    {
        bin[i - 1U] = (Dec & 1U) ? '1' : '0';
        Dec >>= 1U;
    }

    return bin;
} // char *dec2binWzerofill(uint64_t Dec, unsigned int bitLength)


/*
String hexToStr(uint8_t *data, uint8_t len, char separator)
{
    String str = "";

    for (size_t i = 0; i < len; i++)
    {
        str += separator;
        if (data[i] < 0x10)
        	str += '0';
        str += String(data[i], HEX);
    }

    str.trim();
    str.toUpperCase();
    return str;
}
*/
