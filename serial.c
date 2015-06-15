/*
 * DyIO library: Interface to serial port.
 * These routines are internal to the DyIO library and not intended
 * to be called directly from user application.
 *
 * Copyright (C) 2015 Serge Vakulenko
 *
 * This file is distributed under the terms of the Apache License, Version 2.0.
 * See http://opensource.org/licenses/Apache-2.0 for details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "dyio.h"

#if defined(__WIN32__) || defined(WIN32)
#   include <windows.h>
#else
#   include <termios.h>
#endif

typedef struct {
    /* Generic DyIO data structure. */
    dyio_t  generic;

    /* OS dependent part. */
#if defined(__WIN32__) || defined(WIN32)
    void *fd;
    DCB saved_mode;
#else
    int fd;
    struct termios saved_mode;
#endif
} dyio_serial_t;

/*
 * Encode the speed in bits per second into bit value
 * accepted by cfsetspeed() function.
 * Return -1 when speed is not supported.
 */
static int baud_encode(int bps)
{
#if defined(__WIN32__) || defined(WIN32) || B115200 == 115200
    /* Windows, BSD, Mac OS: any baud rate allowed. */
    return bps;
#else
    /* Linux: only a limited set of values permitted. */
    switch (bps) {
#ifdef B75
    case 75: return B75;
#endif
#ifdef B110
    case 110: return B110;
#endif
#ifdef B134
    case 134: return B134;
#endif
#ifdef B150
    case 150: return B150;
#endif
#ifdef B200
    case 200: return B200;
#endif
#ifdef B300
    case 300: return B300;
#endif
#ifdef B600
    case 600: return B600;
#endif
#ifdef B1200
    case 1200: return B1200;
#endif
#ifdef B1800
    case 1800: return B1800;
#endif
#ifdef B2400
    case 2400: return B2400;
#endif
#ifdef B4800
    case 4800: return B4800;
#endif
#ifdef B9600
    case 9600: return B9600;
#endif
#ifdef B19200
    case 19200: return B19200;
#endif
#ifdef B38400
    case 38400: return B38400;
#endif
#ifdef B57600
    case 57600: return B57600;
#endif
#ifdef B115200
    case 115200: return B115200;
#endif
#ifdef B230400
    case 230400: return B230400;
#endif
#ifdef B460800
    case 460800: return B460800;
#endif
#ifdef B500000
    case 500000: return B500000;
#endif
#ifdef B576000
    case 576000: return B576000;
#endif
#ifdef B921600
    case 921600: return B921600;
#endif
#ifdef B1000000
    case 1000000: return B1000000;
#endif
#ifdef B1152000
    case 1152000: return B1152000;
#endif
#ifdef B1500000
    case 1500000: return B1500000;
#endif
#ifdef B2000000
    case 2000000: return B2000000;
#endif
#ifdef B2500000
    case 2500000: return B2500000;
#endif
#ifdef B3000000
    case 3000000: return B3000000;
#endif
#ifdef B3500000
    case 3500000: return B3500000;
#endif
#ifdef B4000000
    case 4000000: return B4000000;
#endif
    }
printf("Unknown baud\n");
    return -1;
#endif
}

/*
 * Send data to device.
 * Return number of bytes, or -1 on error.
 */
int _dyio_serial_write(dyio_t *d, unsigned char *data, int len)
{
    dyio_serial_t *s = (dyio_serial_t*) d;

#if defined(__WIN32__) || defined(WIN32)
    DWORD written;

    if (! WriteFile(s->fd, data, len, &written, 0))
        return -1;
    return len;
#else
    return write(s->fd, data, len);
#endif
}

/*
 * Receive data from device.
 * Return number of bytes, or -1 on error.
 */
int _dyio_serial_read(dyio_t *d, unsigned char *data, int len)
{
    dyio_serial_t *s = (dyio_serial_t*) d;

#if defined(__WIN32__) || defined(WIN32)
    DWORD got;

    if (! ReadFile(s->fd, data, len, &got, 0)) {
        fprintf(stderr, "serial-read: read error\n");
        exit(-1);
    }
#else
    struct timeval timeout, to2;
    long got;
    fd_set rfds;

    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    to2 = timeout;
again:
    FD_ZERO(&rfds);
    FD_SET(s->fd, &rfds);

    got = select(s->fd + 1, &rfds, 0, 0, &to2);
    if (got < 0) {
        if (errno == EINTR || errno == EAGAIN) {
            if (d->debug)
                printf("serial-read: device is not responding\n");
            goto again;
        }
        fprintf(stderr, "serial-read: select error: %s\n", strerror(errno));
        exit(1);
    }
#endif
    if (got == 0) {
        if (d->debug)
            printf("serial-read: device is not responding\n");
        return 0;
    }

#if ! defined(__WIN32__) && !defined(WIN32)
    got = read(s->fd, data, (len > 1024) ? 1024 : len);
    if (got < 0) {
        fprintf(stderr, "serial-read: read error\n");
        exit(-1);
    }
#endif
    return got;
}

/*
 * Close the serial port.
 */
void _dyio_serial_close(dyio_t *d)
{
    dyio_serial_t *s = (dyio_serial_t*) d;

#if defined(__WIN32__) || defined(WIN32)
    SetCommState(s->fd, &s->saved_mode);
    CloseHandle(s->fd);
#else
    tcsetattr(s->fd, TCSANOW, &s->saved_mode);
    close(s->fd);
#endif
}

/*
 * Open the serial port.
 * Return -1 on error.
 */
dyio_t *_dyio_serial_open(const char *devname, int baud_rate)
{
#if defined(__WIN32__) || defined(WIN32)
    DCB new_mode;
    COMMTIMEOUTS ctmo;
#else
    struct termios new_mode;
#endif
    dyio_serial_t *s;

    s = calloc(1, sizeof(dyio_serial_t));
    if (! s) {
        fprintf(stderr, "dyio: Out of memory\n");
        return 0;
    }

#if defined(__WIN32__) || defined(WIN32)
    /* Open port */
    s->fd = CreateFile(devname, GENERIC_READ | GENERIC_WRITE,
        0, 0, OPEN_EXISTING, 0, 0);
    if (s->fd == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "%s: Cannot open\n", devname);
        free(s);
        return 0;
    }

    /* Set serial attributes */
    memset(&s->saved_mode, 0, sizeof(s->saved_mode));
    if (! GetCommState(s->fd, &s->saved_mode)) {
        fprintf(stderr, "%s: Cannot get state\n", devname);
        CloseHandle(s->fd);
        free(s);
        return 0;
    }

    new_mode = s->saved_mode;
    new_mode.BaudRate = baud_encode(baud_rate);
    new_mode.ByteSize = 8;
    new_mode.StopBits = ONESTOPBIT;
    new_mode.Parity = 0;
    new_mode.fParity = FALSE;
    new_mode.fOutX = FALSE;
    new_mode.fInX = FALSE;
    new_mode.fOutxCtsFlow = FALSE;
    new_mode.fOutxDsrFlow = FALSE;
    new_mode.fRtsControl = RTS_CONTROL_ENABLE;
    new_mode.fNull = FALSE;
    new_mode.fAbortOnError = FALSE;
    new_mode.fBinary = TRUE;
    if (! SetCommState(s->fd, &new_mode)) {
        fprintf(stderr, "%s: Cannot set state\n", devname);
        CloseHandle(s->fd);
        free(s);
        return 0;
    }

    memset(&ctmo, 0, sizeof(ctmo));
    ctmo.ReadIntervalTimeout = 0;
    ctmo.ReadTotalTimeoutMultiplier = 0;
    ctmo.ReadTotalTimeoutConstant = 5000;
    if (! SetCommTimeouts(s->fd, &ctmo)) {
        fprintf(stderr, "%s: Cannot set timeouts\n", devname);
        CloseHandle(s->fd);
        free(s);
        return 0;
    }
#else
    /* Encode baud rate. */
    int baud_code = baud_encode(baud_rate);
    if (baud_code < 0) {
        fprintf(stderr, "%s: Bad baud rate %d\n", devname, baud_rate);
        free(s);
        return 0;
    }

    /* Open port */
    s->fd = open(devname, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (s->fd < 0) {
        perror(devname);
        free(s);
        return 0;
    }

    /* Set serial attributes */
    memset(&s->saved_mode, 0, sizeof(s->saved_mode));
    tcgetattr(s->fd, &s->saved_mode);

    /* 8n1, ignore parity */
    memset(&new_mode, 0, sizeof(new_mode));
    new_mode.c_cflag = CS8 | CLOCAL | CREAD;
    new_mode.c_iflag = IGNBRK;
    new_mode.c_oflag = 0;
    new_mode.c_lflag = 0;
    new_mode.c_cc[VTIME] = 0;
    new_mode.c_cc[VMIN]  = 1;
    cfsetispeed(&new_mode, baud_code);
    cfsetospeed(&new_mode, baud_code);
    tcflush(s->fd, TCIFLUSH);
    tcsetattr(s->fd, TCSANOW, &new_mode);

    /* Clear O_NONBLOCK flag. */
    int flags = fcntl(s->fd, F_GETFL, 0);
    if (flags >= 0)
        fcntl(s->fd, F_SETFL, flags & ~O_NONBLOCK);
#endif
    return &s->generic;
}
