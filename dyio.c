/*
 * DyIO control utility.
 *
 * Copyright (C) 2015 Serge Vakulenko
 *
 * This file is distributed under the terms of the Apache License, Version 2.0.
 * See http://opensource.org/licenses/Apache-2.0 for details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include "serial.h"

const char version[] = "1.0";
const char copyright[] = "Copyright (C) 2015 Serge Vakulenko";

char *progname;
int verbose;
int dyio_debug;
uint8_t dyio_mac[6] = { 0 };
uint8_t dyio_reply_mac[6];
uint8_t dyio_reply[256];
int dyio_replylen;

struct dyio_header {
    uint8_t proto;              /* Protocol revision */
#define PROTO_VERSION   3

    uint8_t mac[6];             /* MAC address of the device */

    uint8_t type;               /* Packet type */
#define PKT_STATUS      0x00    /* Synchronous, high priority, non state changing */
#define PKT_GET         0x10    /* Synchronous, query for information, non state changing */
#define PKT_POST        0x20    /* Synchronous, device state changing */
#define PKT_CRITICAL    0x30    /* Synchronous, high priority, state changing */
#define PKT_ASYNC       0x40    /* Asynchronous, high priority, state changing */

    uint8_t id;                 /* Namespace index; high bit is response flag */
#define ID_BCS_CORE     0       /* _png, _nms */
#define ID_BCS_RPC      1       /* _rpc, args */
#define ID_BCS_IO       2       /* asyn, cchn, gacm, gacv, gchc, gchm,
                                 * gchv, gcml, sacv, schv, strm */
#define ID_BCS_SETMODE  3       /* schm, sacm */
#define ID_DYIO         4       /* _mac, _pwr, _rev */
#define ID_BCS_PID      5       /* acal, apid, cpdv, cpid, gpdc,
                                 * kpid, _pid, rpid, _vpd */
#define ID_BCS_DYPID    6       /* dpid */
#define ID_BCS_SAFE     7       /* safe */
#define ID_RESPONSE     0x80

    uint8_t datalen;            /* The length of data including the RPC */
    uint8_t hsum;               /* Sum of previous bytes */
    uint8_t rpc[4];             /* RPC call identifier */
};

/*
 * Types of method parameters.
 */
#define TYPE_I08            8   /* 8 bit integer */
#define TYPE_I16            16  /* 16 bit integer */
#define TYPE_I32            32  /* 32 bit integer */
#define TYPE_STR            37  /* first byte is number of values, next is byte values */
#define TYPE_I32STR         38  /* first byte is number of values, next is 32-bit values */
#define TYPE_ASCII          39  /* ASCII string, null terminated */
#define TYPE_FIXED100       41  /* float */
#define TYPE_FIXED1K        42  /* float */
#define TYPE_BOOL           43  /* a boolean value */
#define TYPE_FIXED1K_STR    44  /* first byte is number of values, next is floats */

/* Channel modes. */
#define MODE_NO_CHANGE              0x00
#define MODE_HIGH_IMPEDANCE         0x01
#define MODE_DI                     0x02
#define MODE_DO                     0x03
#define MODE_ANALOG_IN              0x04
#define MODE_ANALOG_OUT             0x05
#define MODE_PWM                    0x06
#define MODE_SERVO                  0x07
#define MODE_UART_TX                0x08
#define MODE_UART_RX                0x09
#define MODE_SPI_MOSI               0x0A
#define MODE_SPI_MISO               0x0B
#define MODE_SPI_SCK                0x0C
#define MODE_UNUSED                 0x0D
#define MODE_COUNTER_INPUT_INT      0x0E
#define MODE_COUNTER_INPUT_DIR      0x0F
#define MODE_COUNTER_INPUT_HOME     0x10
#define MODE_COUNTER_OUTPUT_INT     0x11
#define MODE_COUNTER_OUTPUT_DIR     0x12
#define MODE_COUNTER_OUTPUT_HOME    0x13
#define MODE_DC_MOTOR_VEL           0x14
#define MODE_DC_MOTOR_DIR           0x15
#define MODE_PPM_IN                 0x16
#define MODE_MAX                    0x17    /* limit */

/*
 * Send the command sequence and get back a response.
 */
void dyio_call(int type, int namespace, char *rpc, uint8_t *data, int datalen)
{
    struct dyio_header hdr;
    uint8_t *p, sum;
    int len, i, got, retry = 0;

    /*
     * Prepare header and checksum.
     */
again:
    hdr.proto     = PROTO_VERSION;
    hdr.type      = type;
    hdr.id        = namespace;
    hdr.datalen   = datalen + sizeof(hdr.rpc);
    memcpy(hdr.mac, dyio_mac, sizeof(hdr.mac));
    memcpy(hdr.rpc, rpc, sizeof(hdr.rpc));
    hdr.hsum = hdr.proto + hdr.mac[0] + hdr.mac[1] + hdr.mac[2] +
               hdr.mac[3] + hdr.mac[4] + hdr.mac[5] + hdr.type +
               hdr.id + hdr.datalen;
    sum = hdr.rpc[0] + hdr.rpc[1] + hdr.rpc[2] + hdr.rpc[3];
    for (i=0; i<datalen; i++)
        sum += data[i];

    /*
     * Send command.
     */
    if (dyio_debug) {
        printf("--- send %x-%x-%x-%x-%x-%x-%x-%x-%x-[%u]-%x-'%c%c%c%c'",
            hdr.proto, hdr.mac[0], hdr.mac[1], hdr.mac[2],
            hdr.mac[3], hdr.mac[4], hdr.mac[5], hdr.type,
            hdr.id, hdr.datalen, hdr.hsum,
            hdr.rpc[0], hdr.rpc[1], hdr.rpc[2], hdr.rpc[3]);
        for (i=0; i<datalen; i++)
            printf("-%x", data[i]);
        printf("-%x\n", sum);
    }
    if (serial_write((uint8_t*)&hdr, sizeof(hdr)) < 0) {
        fprintf(stderr, "dyio: header write error\n");
        exit(-1);
    }
    if (datalen > 0 && serial_write(data, datalen) < 0) {
        fprintf(stderr, "dyio: data write error\n");
        exit(-1);
    }
    if (serial_write(&sum, 1) < 0) {
        fprintf(stderr, "dyio: data sum write error\n");
        exit(-1);
    }

    /*
     * Get header.
     */
next:
    p = (uint8_t*) &hdr;
    len = 0;
    while (len < sizeof(hdr)) {
        got = serial_read(p, sizeof(hdr) - len);
        if (! got) {
            fprintf(stderr, "dyio: connection lost\n");
            exit(-1);
        }

        p += got;
        len += got;
    }
    if (hdr.proto != PROTO_VERSION) {
        /* Skip all incoming data. */
        unsigned char buf [300];

        if (retry)
            printf("got invalid header: %x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x\n",
                hdr.proto, hdr.mac[0], hdr.mac[1], hdr.mac[2],
                hdr.mac[3], hdr.mac[4], hdr.mac[5], hdr.type,
                hdr.id, hdr.datalen, hdr.hsum,
                hdr.rpc[0], hdr.rpc[1], hdr.rpc[2], hdr.rpc[3]);
flush_input:
        serial_read(buf, sizeof(buf));
        if (! retry) {
            retry = 1;
            goto again;
        }
        fprintf(stderr, "dyio: unable to synchronize\n");
        exit(-1);
    }
    memcpy(dyio_reply_mac, hdr.mac, sizeof(hdr.mac));

    /*
     * Get response.
     */
    dyio_replylen = hdr.datalen - sizeof(hdr.rpc);
    p = dyio_reply;
    len = 0;
    while (len <= dyio_replylen) {
        got = serial_read(p, dyio_replylen + 1 - len);
        if (! got) {
            fprintf(stderr, "dyio: connection lost\n");
            exit(-1);
        }

        p += got;
        len += got;
    }
    if (dyio_debug) {
        printf("-- reply %x-%x-%x-%x-%x-%x-%x-%x-%x-[%u]-%x-'%c%c%c%c'",
            hdr.proto, hdr.mac[0], hdr.mac[1], hdr.mac[2],
            hdr.mac[3], hdr.mac[4], hdr.mac[5], hdr.type,
            hdr.id, hdr.datalen, hdr.hsum,
            hdr.rpc[0], hdr.rpc[1], hdr.rpc[2], hdr.rpc[3]);
        for (i=0; i<=dyio_replylen; i++)
            printf("-%x", dyio_reply[i]);
        printf("\n");
    }

    /* Check header sum. */
    sum = hdr.proto + hdr.mac[0] + hdr.mac[1] + hdr.mac[2] +
          hdr.mac[3] + hdr.mac[4] + hdr.mac[5] + hdr.type +
          hdr.id + hdr.datalen;
    if (sum != hdr.hsum) {
        printf("dyio: invalid reply header sum = %02x, expected %02x \n", sum, hdr.hsum);
        goto flush_input;
    }

    /* Check data sum. */
    sum = hdr.rpc[0] + hdr.rpc[1] + hdr.rpc[2] + hdr.rpc[3];
    for (i=0; i<dyio_replylen; i++)
        sum += dyio_reply[i];
    if (sum != dyio_reply[dyio_replylen]) {
        printf("dyio: invalid reply data sum = %02x, expected %02x \n", sum, dyio_reply[dyio_replylen]);
        goto flush_input;
    }

    if (! (hdr.id & ID_RESPONSE)) {
        printf("dyio: incorrect response flag\n");
        goto next;
    }

    if (hdr.type == PKT_ASYNC) {
        goto next;
    }
}

/*
 * Establish a connection to the DyIO device.
 */
void dyio_connect(const char *devname)
{
    /* Open serial port */
    if (serial_open(devname, 115200) < 0) {
        /* failed to open serial port */
        exit(-1);
    }

    /* Ping the device. */
    dyio_call(PKT_GET, ID_BCS_CORE, "_png", 0, 0);
    if (dyio_debug)
        printf("dyio-connect: OK\n");

    if (verbose)
        printf("DyIO device address: %02x-%02x-%02x-%02x-%02x-%02x\n",
            dyio_reply_mac[0], dyio_reply_mac[1], dyio_reply_mac[2],
            dyio_reply_mac[3], dyio_reply_mac[4], dyio_reply_mac[5]);
}

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
void dyio_info()
{
    int voltage;

    /* Print firmware revision. */
    dyio_call(PKT_GET, ID_DYIO, "_rev", 0, 0);
    if (dyio_replylen < 6) {
        printf("dyio-info: incorrect _rev reply: length %u bytes\n", dyio_replylen);
        exit(-1);
    }
    printf("Firmware Revision: %u.%u.%u\n",
        dyio_reply[0], dyio_reply[1], dyio_reply[2]);

    /* Print voltage and power status. */
    dyio_call(PKT_GET, ID_DYIO, "_pwr", 0, 0);
    if (dyio_replylen < 5) {
        printf("dyio-info: incorrect _pwr reply: length %u bytes\n", dyio_replylen);
        exit(-1);
    }
    voltage = (dyio_reply[2] << 8) | dyio_reply[3];
    printf("Power Input: %.1fV, Override=%u\n",
        voltage / 1000.0, dyio_reply[4]);
    printf("Rail Power Source: Right=%s, Left=%s\n",
        dyio_reply[0] ? "Internal" : "External",
        dyio_reply[1] ? "Internal" : "External");
}

/*
 * Query and display information about the namespaces.
 */
void dyio_print_namespaces()
{
    int query_type, resp_type, num_args, num_resp;
    int num_spaces, ns, num_methods, m;
    uint8_t query[2], *args, *resp;
    char rpc[5];

    /* Query the number of namespaces. */
    dyio_call(PKT_GET, ID_BCS_CORE, "_nms", 0, 0);
    if (dyio_replylen < 1) {
        printf("dyio-info: incorrect _nms reply: length %u bytes\n", dyio_replylen);
        exit(-1);
    }
    num_spaces = dyio_reply[0];

    /* Print info about every namespace. */
    for (ns=0; ns<num_spaces; ns++) {
        query[0] = ns;
        dyio_call(PKT_GET, ID_BCS_CORE, "_nms", query, 1);
        if (dyio_replylen < 1) {
            printf("dyio-info: incorrect _nms[%u] reply\n", ns);
            exit(-1);
        }
        printf("Namespace %u: %s\n", ns, dyio_reply);

        /* Print available methods. */
        num_methods = 1;
        for (m=0; m<num_methods; m++) {
            /* Get method name (RPC). */
            query[0] = ns;
            query[1] = m;
            dyio_call(PKT_GET, ID_BCS_RPC, "_rpc", query, 2);
            if (dyio_replylen < 7) {
                printf("dyio-info: incorrect _rpc[%u] reply\n", ns);
                exit(-1);
            }
            num_methods = dyio_reply[2];
            rpc[0] = dyio_reply[3];
            rpc[1] = dyio_reply[4];
            rpc[2] = dyio_reply[5];
            rpc[3] = dyio_reply[6];
            rpc[4] = 0;

            /* Get method args. */
            query[0] = ns;
            query[1] = m;
            dyio_call(PKT_GET, ID_BCS_RPC, "args", query, 2);
            if (dyio_replylen < 6) {
                printf("dyio-info: incorrect args[%u] reply\n", ns);
                exit(-1);
            }
            query_type = dyio_reply[2];
            num_args = dyio_reply[3];
            args = &dyio_reply[4];
            resp_type = dyio_reply[4 + num_args];
            num_resp = dyio_reply[5 + num_args];
            resp = &dyio_reply[6 + num_args];

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
void print_channel_features()
{
    int num_channels, c, num_modes, i, m;
    uint8_t query[1], chan_feature[64][MODE_MAX];

    /* Get number of channels. */
    dyio_call(PKT_GET, ID_BCS_IO, "gchc", 0, 0);
    if (dyio_replylen < 4) {
        printf("dyio-info: incorrect gchc reply: length %u bytes\n", dyio_replylen);
        exit(-1);
    }
    num_channels = dyio_reply[3];
    memset(chan_feature, 0, sizeof(chan_feature));

    /* Build a matrix of channel features. */
    for (c=0; c<num_channels; c++) {
        /* Get a list of channel modes. */
        query[0] = c;
        dyio_call(PKT_GET, ID_BCS_IO, "gcml", query, 1);
        if (dyio_replylen < 1) {
            printf("dyio-info: incorrect gcml[%u] reply\n", c);
            exit(-1);
        }
        num_modes = dyio_reply[0];

        for (i=0; i<num_modes; i++) {
            m = dyio_reply[1+i];
            chan_feature[c][m] = 1;
        }
    }

    printf("\n");
    printf("Channel Features:                             1 1 1 1 1 1 1 1 1 1 2 2 2 2\n");
    printf("                          0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3\n");
    for (m=MODE_DI; m<MODE_MAX; m++) {
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
void dyio_print_channels()
{
    int num_channels, c;
    uint8_t chan_mode[64], *p;
    int chan_value[64];

    if (verbose)
        print_channel_features();

    /* Get current channel modes. */
    dyio_call(PKT_GET, ID_BCS_IO, "gacm", 0, 0);
    if (dyio_replylen < 1) {
        printf("dyio-info: incorrect gacm reply: length %u bytes\n", dyio_replylen);
        exit(-1);
    }
    num_channels = dyio_reply[0];
    memcpy(chan_mode, &dyio_reply[1], num_channels);

    /* Get current pin values. */
    dyio_call(PKT_GET, ID_BCS_IO, "gacv", 0, 0);
    if (dyio_replylen < 1) {
        printf("dyio-info: incorrect gacv reply: length %u bytes\n", dyio_replylen);
        exit(-1);
    }
    for (c=0; c<num_channels; c++) {
        p = &dyio_reply[1 + c*4];
        chan_value[c] = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
    }

    printf("\nChannel Status:\n");
    for (c=0; c<num_channels; c++) {
        printf("    %2u: %-20s = %u\n", c,
            mode_name(chan_mode[c]), chan_value[c]);
    }
}

/*
 * Set channel mode.
 */
void dyio_set_mode(int ch, int mode)
{
    uint8_t query[3];

    query[0] = ch;
    query[1] = mode;
    query[2] = 0;
    dyio_call(PKT_POST, ID_BCS_SETMODE, "schm", query, 3);
    if (dyio_replylen < 1) {
        printf("dyio-info: incorrect schm[%u] reply\n", ch);
        exit(-1);
    }
}

/*
 * Set channel value.
 */
void dyio_set_value(int ch, int value, int msec)
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
    dyio_call(PKT_POST, ID_BCS_IO, "schv", query, 9);
    if (dyio_replylen < 2) {
        printf("dyio-info: incorrect schv[%u] reply\n", ch);
        exit(-1);
    }
}

/*
 * Set channel value.
 */
int dyio_get_value(int ch)
{
    uint8_t query[1];
    int value;

    query[0] = ch;
    dyio_call(PKT_GET, ID_BCS_IO, "gchv", query, 1);
    if (dyio_replylen < 5) {
        printf("dyio-info: incorrect gchv[%u] reply\n", ch);
        exit(-1);
    }
    value = (dyio_reply[1] << 24) | (dyio_reply[2] << 16) |
            (dyio_reply[3] << 8) | dyio_reply[4];
    return value;
}

void usage()
{
    printf("DyIO utility, Version %s, %s\n", version, copyright);
    printf("Usage:\n\t%s [-vdinc] -t#] portname\n", progname);
    printf("Options:\n");
    printf("\t-v\tverbose mode\n");
    printf("\t-i\tdisplay generic information about DyIO device\n");
    printf("\t-n\tshow namespaces and RPC calls\n");
    printf("\t-c\tshow channel status\n");
    printf("\t-d\tprint debug trace of the USB protocol\n");
    printf("\t-t num\trun test with given number\n");
    exit(-1);
}

/*
 * Simple test of digital inputs and outputs.
 * Input sensor (button) is connected to channel 23.
 * Two LEDs are connected to channels 00 and 01.
 * While button is idle, LED1 is off and LED2 is on.
 * When button is pressed, LED1 is turned on, and LED2 turned off.
 */
void test1()
{
    int led0, led1, button;

    printf("Test 1: button at channel 23, two LEDs at channels 00 and 01.\n");
    dyio_set_mode(23, MODE_DI);
    dyio_set_mode(0, MODE_DO);
    dyio_set_mode(1, MODE_DO);
    led0 = 0;
    led1 = 0;
    for (;;) {
        button = !dyio_get_value(23);
        if (button) {
            if (! led0) {
                printf("#");
                led0 = 1;
                dyio_set_value(0, led0, 0);
            }
            if (led1) {
                led1 = 0;
                dyio_set_value(1, led1, 0);
            }
        } else {
            if (led0) {
                printf(".");
                led0 = 0;
                dyio_set_value(0, led0, 0);
            }
            if (! led1) {
                led1 = 1;
                dyio_set_value(1, led1, 0);
            }
        }
        fflush(stdout);
        usleep(10000);
    }
}

int main(int argc, char **argv)
{
    char *devname;
    int iflag = 0, nflag = 0, cflag = 0, tflag = 0;

    progname = *argv;
    for (;;) {
        switch (getopt(argc, argv, "vdinct:")) {
        case EOF:
            break;
        case 'v':
            verbose++;
            continue;
        case 'd':
            dyio_debug++;
            continue;
        case 'i':
            iflag++;
            continue;
        case 'n':
            nflag++;
            continue;
        case 'c':
            cflag++;
            continue;
        case 't':
            tflag = strtol(optarg, 0, 0);
            continue;
            usage();
        }
        break;
    }
    argc -= optind;
    argv += optind;
    if (! iflag && ! nflag && ! cflag && !tflag) {
        /* By default, print generic information. */
        iflag++;
        verbose++;
    }

    if (argc != 1)
        usage();
    devname = argv[0];

    if (verbose)
        printf("Port name: %s\n", devname);

    dyio_connect(devname);

    if (iflag)
        dyio_info();
    if (nflag)
        dyio_print_namespaces();
    if (cflag)
        dyio_print_channels();
    switch (tflag) {
    case 1:
        test1();
    }
    return 0;
}
