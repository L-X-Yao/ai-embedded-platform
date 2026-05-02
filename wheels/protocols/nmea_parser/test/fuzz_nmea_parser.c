// SPDX-License-Identifier: MIT
// Fuzz harness for nmea_parser — parse random strings as NMEA sentences
#include "nmea_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

int main(void) {
    srand(42);

    nmea_parser_data_t data;

    // Random byte streams (NUL-terminated)
    for (int i = 0; i < 10000; i++) {
        int len = rand() % 1024;
        // +1 for NUL
        char buf[1025];
        for (int j = 0; j < len; j++) {
            buf[j] = (char)(rand() % 255 + 1); // avoid embedded NUL
        }
        buf[len] = '\0';

        nmea_parser_data_init(&data);
        nmea_parser_parse(buf, &data);
        nmea_parser_verify_checksum(buf);
        nmea_parser_identify(buf);
    }

    // Boundary: empty string
    {
        nmea_parser_data_init(&data);
        nmea_parser_parse("", &data);
        nmea_parser_verify_checksum("");
        nmea_parser_identify("");
    }

    // Boundary: NULL input
    {
        nmea_parser_data_init(&data);
        nmea_parser_parse(NULL, &data);
    }

    // Boundary: just "$" prefix
    {
        nmea_parser_data_init(&data);
        nmea_parser_parse("$", &data);
        nmea_parser_parse("$GPGGA", &data);
    }

    // Boundary: all commas
    {
        char buf[256];
        memset(buf, ',', 255);
        buf[255] = '\0';
        nmea_parser_data_init(&data);
        nmea_parser_parse(buf, &data);
    }

    // Boundary: malformed with * but no checksum digits
    {
        nmea_parser_data_init(&data);
        nmea_parser_parse("$GPGGA,*", &data);
        nmea_parser_parse("$GPGGA,*XX", &data);
        nmea_parser_parse("$GPRMC,*FF", &data);
    }

    // Boundary: very long field values
    {
        char buf[1024];
        snprintf(buf, sizeof(buf), "$GPGGA,%s,N,12345.678,W,1,08,0.9,545.4,M,47.0,M,,*47",
                 "99999999999999999999999999999999999999999999999999");
        nmea_parser_data_init(&data);
        nmea_parser_parse(buf, &data);
    }

    printf("OK: nmea_parser fuzz 10000 random + boundary inputs, no crash\n");
    return 0;
}
