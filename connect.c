/*
 * DyIO library: implementation of the low level serial protocol.
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

#define PROTO_VERSION   3   /* Revision of the current protocol */

struct dyio_header {
    uint8_t proto;          /* Protocol revision */
    uint8_t mac[6];         /* MAC address of the device */
    uint8_t type;           /* Packet type */
    uint8_t id;             /* Namespace index; high bit is response flag */
    uint8_t datalen;        /* The length of data including the RPC */
    uint8_t hsum;           /* Sum of previous bytes */
    uint8_t rpc[4];         /* RPC call identifier */
};

/*
 * Send the command sequence and get back a response.
 */
void dyio_call(dyio_t *d, int type, int namespace, char *rpc, uint8_t *data, int datalen)
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
    memcpy(hdr.mac, d->mac, sizeof(hdr.mac));
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
    if (d->debug) {
        printf("--- send %x-%x-%x-%x-%x-%x-%x-%x-%x-[%u]-%x-'%c%c%c%c'",
            hdr.proto, hdr.mac[0], hdr.mac[1], hdr.mac[2],
            hdr.mac[3], hdr.mac[4], hdr.mac[5], hdr.type,
            hdr.id, hdr.datalen, hdr.hsum,
            hdr.rpc[0], hdr.rpc[1], hdr.rpc[2], hdr.rpc[3]);
        for (i=0; i<datalen; i++)
            printf("-%x", data[i]);
        printf("-%x\n", sum);
    }
    if (_dyio_serial_write(d, (uint8_t*)&hdr, sizeof(hdr)) < 0) {
        fprintf(stderr, "dyio: header write error\n");
        exit(-1);
    }
    if (datalen > 0 && _dyio_serial_write(d, data, datalen) < 0) {
        fprintf(stderr, "dyio: data write error\n");
        exit(-1);
    }
    if (_dyio_serial_write(d, &sum, 1) < 0) {
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
        got = _dyio_serial_read(d, p, sizeof(hdr) - len);
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
        _dyio_serial_read(d, buf, sizeof(buf));
        if (! retry) {
            retry = 1;
            goto again;
        }
        fprintf(stderr, "dyio: unable to synchronize\n");
        exit(-1);
    }
    memcpy(d->reply_mac, hdr.mac, sizeof(hdr.mac));

    /*
     * Get response.
     */
    d->reply_len = hdr.datalen - sizeof(hdr.rpc);
    p = d->reply;
    len = 0;
    while (len <= d->reply_len) {
        got = _dyio_serial_read(d, p, d->reply_len + 1 - len);
        if (! got) {
            fprintf(stderr, "dyio: connection lost\n");
            exit(-1);
        }

        p += got;
        len += got;
    }
    if (d->debug) {
        printf("-- reply %x-%x-%x-%x-%x-%x-%x-%x-%x-[%u]-%x-'%c%c%c%c'",
            hdr.proto, hdr.mac[0], hdr.mac[1], hdr.mac[2],
            hdr.mac[3], hdr.mac[4], hdr.mac[5], hdr.type,
            hdr.id, hdr.datalen, hdr.hsum,
            hdr.rpc[0], hdr.rpc[1], hdr.rpc[2], hdr.rpc[3]);
        for (i=0; i<=d->reply_len; i++)
            printf("-%x", d->reply[i]);
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
    for (i=0; i<d->reply_len; i++)
        sum += d->reply[i];
    if (sum != d->reply[d->reply_len]) {
        printf("dyio: invalid reply data sum = %02x, expected %02x \n", sum, d->reply[d->reply_len]);
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
dyio_t *dyio_connect(const char *devname, int debug)
{
    dyio_t *d;

    /* Open serial port */
    d = _dyio_serial_open(devname, 115200);
    if (! d) {
        /* Failed to open serial port. */
        return 0;
    }

    /* Set debug option. */
    d->debug = debug;

    /* Ping the device. */
    dyio_call(d, PKT_GET, ID_BCS_CORE, "_png", 0, 0);
    if (d->debug)
        printf("dyio-connect: OK\n");
    return d;
}

/*
 * Close the connection and deallocate device object.
 */
void dyio_close(dyio_t *d)
{
    _dyio_serial_close(d);
}
