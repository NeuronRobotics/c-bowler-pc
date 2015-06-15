/*
 * DyIO library: query and print information about the device.
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

static const char *pkt_name(int type)
{
    switch (type) {
    case PKT_STATUS:    return "STATUS";
    case PKT_GET:       return "GET";
    case PKT_POST:      return "POST";
    case PKT_CRITICAL:  return "CRITICAL";
    case PKT_ASYNC:     return "ASYNC";
    default:            return "UNKNOWN";
    }
}

static void print_args(int nargs, uint8_t *arg)
{
    int i;

    for (i=0; i<nargs; i++) {
        if (i)
            printf(", ");

        switch (arg[i]) {
        case TYPE_I08:          printf("byte");     break;
        case TYPE_I16:          printf("int16");    break;
        case TYPE_I32:          printf("int");      break;
        case TYPE_STR:          printf("byte[]");   break;
        case TYPE_I32STR:       printf("int[]");    break;
        case TYPE_ASCII:        printf("asciiz");   break;
        case TYPE_FIXED100:     printf("f100");     break;
        case TYPE_FIXED1K:      printf("fixed");    break;
        case TYPE_BOOL:         printf("bool");     break;
        case TYPE_FIXED1K_STR:  printf("fixed[]");  break;
        default:                printf("unknown");  break;
        }
    }
}

static const char *mode_name(int mode)
{
    switch (mode) {
    case MODE_NO_CHANGE:           return "No Change";
    case MODE_HIGH_IMPEDANCE:      return "High Impedance";
    case MODE_DI:                  return "Digital Input";
    case MODE_DO:                  return "Digital Output";
    case MODE_ANALOG_IN:           return "Analog Input";
    case MODE_ANALOG_OUT:          return "Analog Output";
    case MODE_PWM:                 return "PWM";
    case MODE_SERVO:               return "Servo";
    case MODE_UART_TX:             return "UART Transmit";
    case MODE_UART_RX:             return "UART Receive";
    case MODE_SPI_MOSI:            return "SPI MOSI";
    case MODE_SPI_MISO:            return "SPI MISO";
    case MODE_SPI_SCK:             return "SPI SCK";
    case MODE_COUNTER_INPUT_INT:   return "Counter Input INT";
    case MODE_COUNTER_INPUT_DIR:   return "Counter Input DIR";
    case MODE_COUNTER_INPUT_HOME:  return "Counter Input HOME";
    case MODE_COUNTER_OUTPUT_INT:  return "Counter Output INT";
    case MODE_COUNTER_OUTPUT_DIR:  return "Counter Output DIR";
    case MODE_COUNTER_OUTPUT_HOME: return "Counter Output HOME";
    case MODE_DC_MOTOR_VEL:        return "DC Motor VEL";
    case MODE_DC_MOTOR_DIR:        return "DC Motor DIR";
    case MODE_PPM_IN:              return "PPM Input";
    default:                       return "UNKNOWN";
    }
}

/*
 * Query and display generic information about the DyIO device.
 */
void dyio_info(dyio_t *d)
{
    int voltage;

    /* Print firmware revision. */
    dyio_call(d, PKT_GET, ID_DYIO, "_rev", 0, 0);
    if (d->reply_len < 6) {
        printf("dyio-info: incorrect _rev reply: length %u bytes\n", d->reply_len);
        exit(-1);
    }
    printf("Firmware Revision: %u.%u.%u\n",
        d->reply[0], d->reply[1], d->reply[2]);

    /* Print voltage and power status. */
    dyio_call(d, PKT_GET, ID_DYIO, "_pwr", 0, 0);
    if (d->reply_len < 5) {
        printf("dyio-info: incorrect _pwr reply: length %u bytes\n", d->reply_len);
        exit(-1);
    }
    voltage = (d->reply[2] << 8) | d->reply[3];
    printf("Power Input: %.1fV, Override=%u\n",
        voltage / 1000.0, d->reply[4]);
    printf("Rail Power Source: Right=%s, Left=%s\n",
        d->reply[0] ? "Internal" : "External",
        d->reply[1] ? "Internal" : "External");
}

/*
 * Query and display information about the namespaces.
 */
void dyio_print_namespaces(dyio_t *d)
{
    int query_type, resp_type, num_args, num_resp;
    int num_spaces, ns, num_methods, m;
    uint8_t query[2], *args, *resp;
    char rpc[5];

    /* Query the number of namespaces. */
    dyio_call(d, PKT_GET, ID_BCS_CORE, "_nms", 0, 0);
    if (d->reply_len < 1) {
        printf("dyio-info: incorrect _nms reply: length %u bytes\n", d->reply_len);
        exit(-1);
    }
    num_spaces = d->reply[0];

    /* Print info about every namespace. */
    for (ns=0; ns<num_spaces; ns++) {
        query[0] = ns;
        dyio_call(d, PKT_GET, ID_BCS_CORE, "_nms", query, 1);
        if (d->reply_len < 1) {
            printf("dyio-info: incorrect _nms[%u] reply\n", ns);
            exit(-1);
        }
        printf("Namespace %u: %s\n", ns, d->reply);

        /* Print available methods. */
        num_methods = 1;
        for (m=0; m<num_methods; m++) {
            /* Get method name (RPC). */
            query[0] = ns;
            query[1] = m;
            dyio_call(d, PKT_GET, ID_BCS_RPC, "_rpc", query, 2);
            if (d->reply_len < 7) {
                printf("dyio-info: incorrect _rpc[%u] reply\n", ns);
                exit(-1);
            }
            num_methods = d->reply[2];
            rpc[0] = d->reply[3];
            rpc[1] = d->reply[4];
            rpc[2] = d->reply[5];
            rpc[3] = d->reply[6];
            rpc[4] = 0;

            /* Get method args. */
            query[0] = ns;
            query[1] = m;
            dyio_call(d, PKT_GET, ID_BCS_RPC, "args", query, 2);
            if (d->reply_len < 6) {
                printf("dyio-info: incorrect args[%u] reply\n", ns);
                exit(-1);
            }
            query_type = d->reply[2];
            num_args = d->reply[3];
            args = &d->reply[4];
            resp_type = d->reply[4 + num_args];
            num_resp = d->reply[5 + num_args];
            resp = &d->reply[6 + num_args];

            printf("    %s %s(", rpc, pkt_name(query_type));
            print_args(num_args, args);
            printf(") -> %s(", pkt_name(resp_type));
            print_args(num_resp, resp);
            printf(")\n");
        }
    }
}

/*
 * Query and print a table of channel capabilities.
 */
void dyio_print_channel_features(dyio_t *d)
{
    int num_channels, c, num_modes, i, m;
    uint8_t query[1], chan_feature[MAX_CHANNELS][MAX_MODES];

    /* Get number of channels. */
    dyio_call(d, PKT_GET, ID_BCS_IO, "gchc", 0, 0);
    if (d->reply_len < 4) {
        printf("dyio-info: incorrect gchc reply: length %u bytes\n", d->reply_len);
        exit(-1);
    }
    num_channels = d->reply[3];
    memset(chan_feature, 0, sizeof(chan_feature));

    /* Build a matrix of channel features. */
    for (c=0; c<num_channels; c++) {
        /* Get a list of channel modes. */
        query[0] = c;
        dyio_call(d, PKT_GET, ID_BCS_IO, "gcml", query, 1);
        if (d->reply_len < 1) {
            printf("dyio-info: incorrect gcml[%u] reply\n", c);
            exit(-1);
        }
        num_modes = d->reply[0];

        for (i=0; i<num_modes; i++) {
            m = d->reply[1+i];
            chan_feature[c][m] = 1;
        }
    }

    printf("\n");
    printf("Channel Features:                             1 1 1 1 1 1 1 1 1 1 2 2 2 2\n");
    printf("                          0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3\n");
    for (m=MODE_DI; m<MAX_MODES; m++) {
        if (m == MODE_UNUSED)
            continue;

        printf("    %-22s", mode_name(m));
        for (c=0; c<num_channels; c++) {
            printf("%c ", chan_feature[c][m] ? '+' : '.');
        }
        printf("\n");
    }
}

/*
 * Query and display information about the channels.
 */
void dyio_print_channels(dyio_t *d)
{
    int num_channels, c;
    uint8_t chan_mode[MAX_CHANNELS], *p;
    int chan_value[MAX_CHANNELS];

    /* Get current channel modes. */
    dyio_call(d, PKT_GET, ID_BCS_IO, "gacm", 0, 0);
    if (d->reply_len < 1) {
        printf("dyio-info: incorrect gacm reply: length %u bytes\n", d->reply_len);
        exit(-1);
    }
    num_channels = d->reply[0];
    memcpy(chan_mode, &d->reply[1], num_channels);

    /* Get current pin values. */
    dyio_call(d, PKT_GET, ID_BCS_IO, "gacv", 0, 0);
    if (d->reply_len < 1) {
        printf("dyio-info: incorrect gacv reply: length %u bytes\n", d->reply_len);
        exit(-1);
    }
    for (c=0; c<num_channels; c++) {
        p = &d->reply[1 + c*4];
        chan_value[c] = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
    }

    printf("\nChannel Status:\n");
    for (c=0; c<num_channels; c++) {
        printf("    %2u: %-20s = %u\n", c,
            mode_name(chan_mode[c]), chan_value[c]);
    }
}
