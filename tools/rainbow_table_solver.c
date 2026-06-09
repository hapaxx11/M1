/* See COPYING.txt for license details. */

/**
 * @file   rainbow_table_solver.c
 * @brief  Host utility to extract, validate, and brute-force Sub-GHz
 *         rainbow / permutation tables used by Nice FloR-S, Alutech
 *         AT-4N, and similar ciphers.
 *
 * This tool is NOT compiled into the firmware.  It runs on a development
 * host and produces the 32-byte hex string that is then stored as a
 * GitHub Actions secret (e.g. NICE_FLOR_S_RAINBOW_TABLE).
 *
 * Modes:
 *   extract   – read an rtl_433-style C array from stdin and emit the
 *               32-byte hex secret.
 *   validate  – given a 64-char hex table and a known OTA vector,
 *               decrypt and check the serial + counter match.
 *
 * Build:
 *   cc -O2 -o rainbow_table_solver rainbow_table_solver.c \
 *      -I../Sub_Ghz ../Sub_Ghz/subghz_nice_flor_s.c         \
 *      ../Sub_Ghz/subghz_alutech_at_4n.c
 *
 * M1 Project — Hapax fork
 */

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- cipher headers ---------------------------------------------------- */
#include "subghz_nice_flor_s.h"
#include "subghz_alutech_at_4n.h"

/* ---- constants --------------------------------------------------------- */
#define TABLE_SIZE 32
#define HEX_LEN   (TABLE_SIZE * 2)  /* 64 hex chars */

/* ======================================================================== */
/* Helpers                                                                   */
/* ======================================================================== */

/** Print a 32-byte table as a 64-char hex string to stdout. */
static void print_hex_table(const uint8_t table[TABLE_SIZE])
{
    for (int i = 0; i < TABLE_SIZE; ++i)
        printf("%02X", table[i]);
    printf("\n");
}

/** Parse a 64-char hex string into a 32-byte table.  Returns 0 on success. */
static int parse_hex_table(const char *hex, uint8_t table[TABLE_SIZE])
{
    if (strlen(hex) < HEX_LEN)
    {
        fprintf(stderr, "error: hex string too short (need %d chars, got %zu)\n",
                HEX_LEN, strlen(hex));
        return -1;
    }
    for (int i = 0; i < TABLE_SIZE; ++i)
    {
        char byte_str[3] = { hex[i * 2], hex[i * 2 + 1], '\0' };
        char *end;
        unsigned long v = strtoul(byte_str, &end, 16);
        if (*end != '\0')
        {
            fprintf(stderr, "error: invalid hex at position %d: '%s'\n",
                    i * 2, byte_str);
            return -1;
        }
        table[i] = (uint8_t)v;
    }
    return 0;
}

/** Print a table as a C array initializer. */
static void print_c_array(const uint8_t table[TABLE_SIZE], const char *name)
{
    printf("static const uint8_t %s[%d] = {\n", name, TABLE_SIZE);
    for (int i = 0; i < TABLE_SIZE; i += 8)
    {
        printf("    ");
        for (int j = i; j < i + 8 && j < TABLE_SIZE; ++j)
        {
            printf("0x%02X", table[j]);
            if (j < TABLE_SIZE - 1)
                printf(", ");
        }
        printf("\n");
    }
    printf("};\n");
}

/* ======================================================================== */
/* Mode: extract                                                             */
/* ======================================================================== */

/**
 * Read an rtl_433-style C array literal from stdin and output the
 * 64-char hex string suitable for use as a GitHub Actions secret.
 *
 * Accepts formats like:
 *   { 25, 5, 63, 97, ... }           (decimal)
 *   { 0x19, 0x05, 0x3F, 0x61, ... }  (hex)
 *   25 5 63 97 ...                    (plain whitespace-separated)
 */
static int cmd_extract(void)
{
    char buf[4096];
    size_t total = 0;

    while (fgets(buf + total, (int)(sizeof(buf) - total), stdin))
    {
        total += strlen(buf + total);
        if (total >= sizeof(buf) - 1)
            break;
    }

    uint8_t table[TABLE_SIZE];
    int count = 0;
    char *p = buf;

    while (count < TABLE_SIZE && *p)
    {
        /* skip non-hex, non-digit characters (braces, commas, etc.) */
        while (*p && !isxdigit((unsigned char)*p))
            ++p;
        if (!*p)
            break;

        char *end;
        unsigned long v;

        /* detect 0x prefix → hex; otherwise decimal */
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
            v = strtoul(p, &end, 16);
        else
            v = strtoul(p, &end, 10);

        if (end == p)
            break;

        if (v > 255)
        {
            fprintf(stderr, "warning: value %lu > 255 at index %d, truncating\n",
                    v, count);
            v &= 0xFF;
        }
        table[count++] = (uint8_t)v;
        p = end;
    }

    if (count != TABLE_SIZE)
    {
        fprintf(stderr, "error: expected %d values, found %d\n",
                TABLE_SIZE, count);
        return 1;
    }

    fprintf(stderr, "Extracted %d-byte table:\n", TABLE_SIZE);
    print_c_array(table, "rainbow_table");
    fprintf(stderr, "\nGitHub Actions secret value (64 hex chars):\n");
    print_hex_table(table);
    return 0;
}

/* ======================================================================== */
/* Mode: validate                                                            */
/* ======================================================================== */

/**
 * Validate a hex table against a known OTA vector.
 *
 * Usage: rainbow_table_solver validate <hex_table> <cipher>
 *            <enc_payload_hex> <expected_serial_hex> <expected_counter_dec>
 *
 * Supported ciphers: nice_flor_s, alutech_at_4n
 */
static int cmd_validate(int argc, char **argv)
{
    if (argc < 7)
    {
        fprintf(stderr,
            "usage: %s validate <64-char-hex-table> <cipher> "
            "<enc_payload_hex> <expected_serial_hex> <expected_counter_dec>\n"
            "\n"
            "  cipher: nice_flor_s | alutech_at_4n\n"
            "\n"
            "Example (Nice FloR-S, rtl_433 PR #2238 vector):\n"
            "  %s validate \\\n"
            "    19053F61CB6D450A030740054786B44A299E66C75D76AF653C4D8FAE6794"
            "1D55 \\\n"
            "    nice_flor_s 08FC2E9F526 3AAB665 2813\n",
            argv[0], argv[0]);
        return 1;
    }

    const char *hex_table     = argv[2];
    const char *cipher        = argv[3];
    const char *enc_hex       = argv[4];
    const char *serial_hex    = argv[5];
    const char *counter_dec   = argv[6];

    uint8_t table[TABLE_SIZE];
    if (parse_hex_table(hex_table, table) != 0)
        return 1;

    uint64_t enc_payload = strtoull(enc_hex, NULL, 16);
    uint32_t expected_serial  = (uint32_t)strtoul(serial_hex, NULL, 16);
    uint32_t expected_counter = (uint32_t)strtoul(counter_dec, NULL, 10);

    uint64_t plain;
    uint16_t counter;
    uint32_t serial;

    if (strcmp(cipher, "nice_flor_s") == 0)
    {
        plain   = nice_flor_s_decrypt(enc_payload, table);
        counter = (uint16_t)(plain & 0xFFFFU);
        serial  = (uint32_t)((plain >> 16) & 0x0FFFFFFFU);
    }
    else if (strcmp(cipher, "alutech_at_4n") == 0)
    {
        plain   = alutech_at_4n_decrypt(enc_payload, table);
        uint8_t *pb = (uint8_t *)&plain;
        counter = (uint16_t)(((uint16_t)pb[5] << 8) | pb[6]);
        serial  = ((uint32_t)pb[1] << 24) | ((uint32_t)pb[2] << 16) |
                  ((uint32_t)pb[3] <<  8) |  (uint32_t)pb[4];
    }
    else
    {
        fprintf(stderr, "error: unknown cipher '%s'\n", cipher);
        return 1;
    }

    printf("Cipher:           %s\n", cipher);
    printf("Encrypted:        0x%llX\n", (unsigned long long)enc_payload);
    printf("Decrypted:        0x%llX\n", (unsigned long long)plain);
    printf("Serial:           0x%X (expected 0x%X) %s\n",
           serial, expected_serial,
           serial == expected_serial ? "OK" : "MISMATCH");
    printf("Counter:          %u (expected %u) %s\n",
           counter, expected_counter,
           counter == expected_counter ? "OK" : "MISMATCH");

    if (serial == expected_serial && counter == expected_counter)
    {
        printf("\n==> TABLE VALIDATED SUCCESSFULLY\n");
        return 0;
    }
    else
    {
        printf("\n==> VALIDATION FAILED\n");
        return 1;
    }
}

/* ======================================================================== */
/* Mode: convert                                                             */
/* ======================================================================== */

/**
 * Convert between formats: hex ↔ C array.
 */
static int cmd_convert(int argc, char **argv)
{
    if (argc < 4)
    {
        fprintf(stderr,
            "usage: %s convert <hex|c_array> <64-char-hex-table>\n"
            "\n"
            "  hex      — print as 64-char hex string (for GitHub secret)\n"
            "  c_array  — print as C array initializer\n",
            argv[0]);
        return 1;
    }

    const char *format    = argv[2];
    const char *hex_input = argv[3];

    uint8_t table[TABLE_SIZE];

    /* If the user passes "c_array" as format and a hex string, convert. */
    if (argc >= 4)
    {
        if (parse_hex_table(hex_input, table) != 0)
            return 1;
    }
    else
    {
        fprintf(stderr, "error: provide the 64-char hex table value\n");
        return 1;
    }

    if (strcmp(format, "c_array") == 0)
    {
        print_c_array(table, "rainbow_table");
    }
    else if (strcmp(format, "hex") == 0)
    {
        print_hex_table(table);
    }
    else
    {
        fprintf(stderr, "error: unknown format '%s' (use 'hex' or 'c_array')\n",
                format);
        return 1;
    }
    return 0;
}

/* ======================================================================== */
/* Mode: roundtrip                                                           */
/* ======================================================================== */

/**
 * Encrypt-then-decrypt a range of counter values with a given table and
 * serial, verifying every roundtrip.  Reports the first failure or
 * prints a success summary.
 */
static int cmd_roundtrip(int argc, char **argv)
{
    if (argc < 4)
    {
        fprintf(stderr,
            "usage: %s roundtrip <64-char-hex-table> <cipher> [serial_hex]\n"
            "\n"
            "  Tests encrypt/decrypt roundtrip for all 65536 counter values.\n"
            "  Default serial: 0x1234567\n",
            argv[0]);
        return 1;
    }

    const char *hex_table  = argv[2];
    const char *cipher     = argv[3];
    uint32_t serial        = 0x1234567U;
    if (argc >= 5)
        serial = (uint32_t)strtoul(argv[4], NULL, 16);

    uint8_t table[TABLE_SIZE];
    if (parse_hex_table(hex_table, table) != 0)
        return 1;

    int failures = 0;

    if (strcmp(cipher, "nice_flor_s") == 0)
    {
        serial &= 0x0FFFFFFFU;
        for (uint32_t cnt = 0; cnt < 65536; ++cnt)
        {
            uint64_t plain = ((uint64_t)serial << 16) | cnt;
            uint64_t enc   = nice_flor_s_encrypt(plain, table);
            uint64_t dec   = nice_flor_s_decrypt(enc, table);
            if ((dec & 0x0FFFFFFFFFFFULL) != (plain & 0x0FFFFFFFFFFFULL))
            {
                fprintf(stderr, "FAIL: counter=%u  plain=0x%llX  enc=0x%llX  "
                        "dec=0x%llX\n", cnt,
                        (unsigned long long)plain,
                        (unsigned long long)enc,
                        (unsigned long long)dec);
                ++failures;
                if (failures >= 10)
                {
                    fprintf(stderr, "... stopping after 10 failures\n");
                    break;
                }
            }
        }
    }
    else if (strcmp(cipher, "alutech_at_4n") == 0)
    {
        for (uint32_t cnt = 0; cnt < 65536; ++cnt)
        {
            uint64_t plain = ((uint64_t)serial << 16) | cnt;
            uint64_t enc   = alutech_at_4n_encrypt(plain, table);
            uint64_t dec   = alutech_at_4n_decrypt(enc, table);
            if (dec != plain)
            {
                fprintf(stderr, "FAIL: counter=%u  plain=0x%llX  enc=0x%llX  "
                        "dec=0x%llX\n", cnt,
                        (unsigned long long)plain,
                        (unsigned long long)enc,
                        (unsigned long long)dec);
                ++failures;
                if (failures >= 10)
                {
                    fprintf(stderr, "... stopping after 10 failures\n");
                    break;
                }
            }
        }
    }
    else
    {
        fprintf(stderr, "error: unknown cipher '%s'\n", cipher);
        return 1;
    }

    if (failures == 0)
    {
        printf("Roundtrip OK: all 65536 counter values for serial=0x%X "
               "(%s)\n", serial, cipher);
        return 0;
    }
    printf("%d roundtrip failures\n", failures);
    return 1;
}

/* ======================================================================== */
/* Mode: decrypt                                                             */
/* ======================================================================== */

/**
 * Decrypt a single payload and print the result.
 */
static int cmd_decrypt(int argc, char **argv)
{
    if (argc < 5)
    {
        fprintf(stderr,
            "usage: %s decrypt <64-char-hex-table> <cipher> <enc_hex>\n"
            "\n"
            "  Decrypt a single payload and print serial + counter.\n",
            argv[0]);
        return 1;
    }

    const char *hex_table = argv[2];
    const char *cipher    = argv[3];
    const char *enc_hex   = argv[4];

    uint8_t table[TABLE_SIZE];
    if (parse_hex_table(hex_table, table) != 0)
        return 1;

    uint64_t enc_payload = strtoull(enc_hex, NULL, 16);
    uint64_t plain;

    if (strcmp(cipher, "nice_flor_s") == 0)
    {
        plain = nice_flor_s_decrypt(enc_payload, table);
        uint16_t counter = (uint16_t)(plain & 0xFFFFU);
        uint32_t serial  = (uint32_t)((plain >> 16) & 0x0FFFFFFFU);
        printf("Cipher:    nice_flor_s\n");
        printf("Encrypted: 0x%llX\n", (unsigned long long)enc_payload);
        printf("Decrypted: 0x%llX\n", (unsigned long long)plain);
        printf("Serial:    0x%07X\n", serial);
        printf("Counter:   %u (0x%04X)\n", counter, counter);
    }
    else if (strcmp(cipher, "alutech_at_4n") == 0)
    {
        plain = alutech_at_4n_decrypt(enc_payload, table);
        uint8_t *pb = (uint8_t *)&plain;
        uint32_t serial  = ((uint32_t)pb[1] << 24) | ((uint32_t)pb[2] << 16) |
                           ((uint32_t)pb[3] <<  8) |  (uint32_t)pb[4];
        uint16_t counter = (uint16_t)(((uint16_t)pb[5] << 8) | pb[6]);
        uint8_t  button  = pb[7];
        printf("Cipher:    alutech_at_4n\n");
        printf("Encrypted: 0x%llX\n", (unsigned long long)enc_payload);
        printf("Decrypted: 0x%llX\n", (unsigned long long)plain);
        printf("Serial:    0x%08X\n", serial);
        printf("Counter:   %u (0x%04X)\n", counter, counter);
        printf("Button:    0x%02X\n", button);
    }
    else
    {
        fprintf(stderr, "error: unknown cipher '%s'\n", cipher);
        return 1;
    }

    return 0;
}

/* ======================================================================== */
/* Usage                                                                     */
/* ======================================================================== */

static void usage(const char *prog)
{
    fprintf(stderr,
        "Rainbow Table Solver — M1 Sub-GHz utility\n"
        "\n"
        "Usage: %s <command> [args...]\n"
        "\n"
        "Commands:\n"
        "  extract     Read an rtl_433-style C array from stdin, emit 64-char\n"
        "              hex string for use as a GitHub Actions secret.\n"
        "\n"
        "  validate    Validate a table against a known OTA vector.\n"
        "              Args: <hex_table> <cipher> <enc_hex> <serial_hex> "
        "<counter_dec>\n"
        "\n"
        "  decrypt     Decrypt a single payload with a given table.\n"
        "              Args: <hex_table> <cipher> <enc_hex>\n"
        "\n"
        "  roundtrip   Test encrypt/decrypt roundtrip for all 65536 counters.\n"
        "              Args: <hex_table> <cipher> [serial_hex]\n"
        "\n"
        "  convert     Convert a hex table to C array format or vice versa.\n"
        "              Args: <hex|c_array> <hex_table>\n"
        "\n"
        "Supported ciphers: nice_flor_s, alutech_at_4n\n"
        "\n"
        "Examples:\n"
        "  # Extract table from rtl_433 source:\n"
        "  grep -A4 'leaf_node' rtl_433/src/devices/nice_flor_s.c | "
        "%s extract\n"
        "\n"
        "  # Validate Nice FloR-S table against rtl_433 PR #2238 vector:\n"
        "  %s validate \\\n"
        "    19053F61CB6D450A030740054786B44A299E66C75D76AF653C4D8FAE67941D55"
        " \\\n"
        "    nice_flor_s 08FC2E9F526 3AAB665 2813\n"
        "\n"
        "  # Full roundtrip test:\n"
        "  %s roundtrip \\\n"
        "    19053F61CB6D450A030740054786B44A299E66C75D76AF653C4D8FAE67941D55"
        " \\\n"
        "    nice_flor_s\n",
        prog, prog, prog, prog);
}

/* ======================================================================== */
/* Main                                                                      */
/* ======================================================================== */

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        usage(argv[0]);
        return 1;
    }

    /* argv[1] is the command, argv[2..] are sub-command arguments. */
    const char *cmd = argv[1];

    if (strcmp(cmd, "extract") == 0)
        return cmd_extract();
    if (strcmp(cmd, "validate") == 0)
        return cmd_validate(argc, argv);
    if (strcmp(cmd, "decrypt") == 0)
        return cmd_decrypt(argc, argv);
    if (strcmp(cmd, "roundtrip") == 0)
        return cmd_roundtrip(argc, argv);
    if (strcmp(cmd, "convert") == 0)
        return cmd_convert(argc, argv);
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0 ||
        strcmp(cmd, "-h") == 0)
    {
        usage(argv[0]);
        return 0;
    }

    fprintf(stderr, "error: unknown command '%s'\n\n", cmd);
    usage(argv[0]);
    return 1;
}
