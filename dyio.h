/*
 * C interface to DyIO device.
 *
 * Copyright (C) 2015 Serge Vakulenko
 *
 * This file is distributed under the terms of the Apache License, Version 2.0.
 * See http://opensource.org/licenses/Apache-2.0 for details.
 */

#define MAX_CHANNELS    64          /* Max channels per device */

/*
 * Data structure describing a connection to a DyIO device.
 */
typedef struct {
    /* User visible part. */
    unsigned char   mac[6];         /* Inique address of the device */
    unsigned char   reply[256];     /* Bytes of last reply */
    int             reply_len;      /* Number of bytes */
    unsigned char   reply_mac[6];   /* Address extracted from reply */
    int             debug;          /* Trace USB protocol */

    /* Actually more data are allocated.
     * Here comes an OS-dependent stuff, hidden from the user. */
} dyio_t;

/*
 * Establish a connection to the DyIO device.
 */
dyio_t *dyio_connect(const char *devname, int debug);

/*
 * Close the connection and deallocate device object.
 */
void dyio_close(dyio_t *d);

/*
 * Set channel mode.
 */
void dyio_set_mode(dyio_t *d, int ch, int mode);

/*
 * Get current channel value.
 */
int dyio_get_value(dyio_t *d, int ch);

/*
 * Set channel value.
 */
void dyio_set_value(dyio_t *d, int ch, int value);

/*
 * Set channel value with additional timing parameter.
 */
void dyio_set_value_msec(dyio_t *d, int ch, int value, int msec);

/*
 * Query and display generic information about the DyIO device.
 */
void dyio_info(dyio_t *d);

/*
 * Query and display information about the namespaces.
 */
void dyio_print_namespaces(dyio_t *d);

/*
 * Query and print a table of channel capabilities.
 */
void dyio_print_channel_features(dyio_t *d);

/*
 * Query and display current status of the channels.
 */
void dyio_print_channels(dyio_t *d);

/*
 * Send the command sequence and get back a response.
 */
void dyio_call(dyio_t *d, int type, int namespace, char *rpc,
    unsigned char *data, int datalen);

/*
 * Packet types.
 */
#define PKT_STATUS      0x00    /* Synchronous, high priority, non state changing */
#define PKT_GET         0x10    /* Synchronous, query for information, non state changing */
#define PKT_POST        0x20    /* Synchronous, device state changing */
#define PKT_CRITICAL    0x30    /* Synchronous, high priority, state changing */
#define PKT_ASYNC       0x40    /* Asynchronous, high priority, state changing */

/*
 * Method IDs.
 */
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

/*
 * Channel modes.
 */
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
#define MAX_MODES                   0x17    /* limit */

/*
 * Open the serial port.
 * Return -1 on error.
 */
dyio_t *_dyio_serial_open(const char *devname, int baud_rate);

/*
 * Close the serial port.
 */
void _dyio_serial_close(dyio_t *device);

/*
 * Send data to device.
 * Return number of bytes, or -1 on error.
 */
int _dyio_serial_write(dyio_t *device, unsigned char *data, int len);

/*
 * Receive data from device.
 * Return number of bytes, or -1 on error.
 */
int _dyio_serial_read(dyio_t *device, unsigned char *data, int len);
