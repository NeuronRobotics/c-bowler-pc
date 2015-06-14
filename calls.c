/*
 * DyIO library: basic API calls.
 *
 * Copyright (C) 2015 Serge Vakulenko
 *
 * This file is distributed under the terms of the Apache License, Version 2.0.
 * See http://opensource.org/licenses/Apache-2.0 for details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "dyio.h"

/*
 * Set channel mode.
 */
void dyio_set_mode(dyio_t *d, int ch, int mode)
{
    uint8_t query[3];

    query[0] = ch;
    query[1] = mode;
    query[2] = 0;
    dyio_call(d, PKT_POST, ID_BCS_SETMODE, "schm", query, 3);
    if (d->reply_len < 1) {
        printf("dyio-info: incorrect schm[%u] reply\n", ch);
        exit(-1);
    }
}

/*
 * Set channel value.
 */
void dyio_set_value(dyio_t *d, int ch, int value)
{
    dyio_set_value_msec(d, ch, value, 0);
}

/*
 * Set channel value.
 */
void dyio_set_value_msec(dyio_t *d, int ch, int value, int msec)
{
    uint8_t query[9];

    query[0] = ch;
    query[1] = value >> 24;
    query[2] = value >> 16;
    query[3] = value >> 8;
    query[4] = value;
    query[5] = msec >> 24;
    query[6] = msec >> 16;
    query[7] = msec >> 8;
    query[8] = msec;
    dyio_call(d, PKT_POST, ID_BCS_IO, "schv", query, 9);
    if (d->reply_len < 2) {
        printf("dyio-info: incorrect schv[%u] reply\n", ch);
        exit(-1);
    }
}

/*
 * Get current channel value.
 */
int dyio_get_value(dyio_t *d, int ch)
{
    uint8_t query[1];
    int value;

    query[0] = ch;
    dyio_call(d, PKT_GET, ID_BCS_IO, "gchv", query, 1);
    if (d->reply_len < 5) {
        printf("dyio-info: incorrect gchv[%u] reply\n", ch);
        exit(-1);
    }
    value = (d->reply[1] << 24) | (d->reply[2] << 16) |
            (d->reply[3] << 8) | d->reply[4];
    return value;
}
