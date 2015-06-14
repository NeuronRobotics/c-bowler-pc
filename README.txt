
Examples of dyio utility
~~~~~~~~~~~~~~~~~~~~~~~~

Generic information about DyIO device:

    $ dyio /dev/ttyACM0
    Port name: /dev/ttyACM0
    DyIO device address: 74-f7-26-0d-05-00
    Firmware Revision: 3.13.5
    Power Input: 0.0V, Override=1
    Rail Power Source: Right=External, Left=External

Matrix of channel features and status:

    $ dyio /dev/ttyACM0 -c -v
    Port name: /dev/ttyACM0
    DyIO device address: 74-f7-26-0d-05-00

    Channel Features:                             1 1 1 1 1 1 1 1 1 1 2 2 2 2
                              0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
        Digital Input         + + + + + + + + + + + + + + + + + + + + + + + +
        Digital Output        + + + + + + + + + + + + + + + + + + + + + + + +
        Analog Input          . . . . . . . . + + + + + + + + . . . . . . . .
        Analog Output         . . . . . . . . . . . . . . . . . . . . . . . .
        PWM                   . . . . + + + + . . . . . . . . . . . . . . . .
        Servo                 + + + + + + + + + + + + + + + + + + + + + + + +
        UART Transmit         . . . . . . . . . . . . . . . . + . . . . . . .
        UART Receive          . . . . . . . . . . . . . . . . . + . . . . . .
        SPI MOSI              . . + . . . . . . . . . . . . . . . . . . . . .
        SPI MISO              . + . . . . . . . . . . . . . . . . . . . . . .
        SPI SCK               + . . . . . . . . . . . . . . . . . . . . . . .
        Counter Input INT     . . . . . . . . . . . . . . . . . + . + . + . +
        Counter Input DIR     . . . . . . . . . . . . . . . . + . + . + . + .
        Counter Input HOME    + + + + . . . . . . . . . . . . . . . . . . . .
        Counter Output INT    . . . . . . . . . . . . . . . . . + . + . + . +
        Counter Output DIR    . . . . . . . . . . . . . . . . + . + . + . + .
        Counter Output HOME   + + + + . . . . . . . . . . . . . . . . . . . .
        DC Motor VEL          . . . . + + + + . . . . . . . . . . . . . . . .
        DC Motor DIR          . . . . + + + + + + + + . . . . . . . . . . . .
        PPM Input             . . . . . . . . . . . . . . . . . . . . . . . +

    Channel Status:
         0: Digital Output       = 0
         1: Digital Output       = 1
         2: Digital Input        = 1
         3: Digital Input        = 1
         4: Servo                = 92
         5: Digital Input        = 1
         6: Digital Input        = 1
         7: Servo                = 124
         8: Digital Input        = 1
         9: Digital Input        = 1
        10: Digital Input        = 1
        11: Digital Input        = 1
        12: Analog Input         = 134
        13: Digital Input        = 1
        14: Digital Input        = 1
        15: Analog Input         = 133
        16: Digital Input        = 1
        17: Digital Input        = 1
        18: Digital Input        = 1
        19: Digital Input        = 1
        20: Digital Input        = 1
        21: Digital Input        = 1
        22: Digital Input        = 1
        23: Digital Input        = 1


Namespaces
~~~~~~~~~~

Namespace 0: bcs.core
~~~~~~~~~~~~~~~~~~~~~

    _png GET() -> POST()

        Ping the device

    _nms GET(byte) -> POST(asciiz, byte)

        Get the number of namespaces.
        Get the description of namespace.


Namespace 1: bcs.rpc
~~~~~~~~~~~~~~~~~~~~
    _rpc GET(byte, byte) -> POST(byte, byte, byte, asciiz)

        Get number of methods (RPCs) and RPC value
        by namespace index and method index.

    args GET(byte, byte) -> POST(byte, byte, byte, byte[], byte, byte[])

        Get number and types of arguments for a method and it's response
        by namespace index and method index.


Namespace 2: bcs.io
~~~~~~~~~~~~~~~~~~~
    gchc GET() -> POST(int)

        Get number of i/o channels.

    gcml GET(byte) -> POST(byte[])

        Get a list of channel modes.

    gchm GET(byte) -> POST(byte, byte)

        Get channel mode.

    gacm GET() -> POST(byte[])

        Get current modes for all channels.

    gchv GET(byte) -> POST(byte, int)

        Get channel value.

    gacv GET() -> POST(int[])

        Get current values of all channels.

    asyn GET(byte) -> POST(byte, byte)

        Query whether the channel can generate asynchronous data
        (counter and servo modes).

    strm GET(byte) -> POST(byte, byte[])

        Read data from input stream.
        (UART receive, SPI receive, PPM input)

    strm POST(byte, byte[]) -> POST(byte, byte)

        Write data to output stream.
        (UART transmit, SPI transmit, PPM configuration)

    schv POST(byte, int, int) -> POST(byte, byte)

        Set channel value.
        Third parameter is time in milliseconds, for counter output.

    schv CRITICAL(byte, int[]) -> POST()

        Set channel value (deprecated version).

    sacv POST(int, int[]) -> POST(int[])

        Set values of all channels.
        First parameter is time in milliseconds, for counter output.

    cchn CRITICAL(byte, bool, int[]) -> POST(int[])

        Configure channel.
        Looks buggy.

    asyn CRITICAL(pin: byte, mode: byte, time: int, value: int, edge: byte) -> POST()

        Set the pin to advanced async mode.
        Parameters:
            pin   - channel number
            mode  - one of AUTOSAMP, NOTEQUAL, DEADBAND or THRESHHOLD
            time  -
            value - for THRESHOLD mode
            edge  - for THRESHOLD mode

Namespace 3: bcs.io.setmode
~~~~~~~~~~~~~~~~~~~~~~~~~~~
    schm POST(byte, byte, byte) -> POST(byte[])

        Set channel mode. Third parameter ignored.
        Return a list of all channel modes.

    sacm POST(byte[]) -> POST(byte[])

        Set all channel modes.
        Return a list of all channel modes.

Namespace 4: neuronrobotics.dyio
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    _rev GET() -> POST(byte, byte, byte, byte, byte, byte)

        Get firmware revision numbers.

    _pwr GET() -> POST(byte, byte, int16, bool)

        Get external power input voltage and
        power source for right and left rails.

    _mac CRITICAL(byte, byte, byte, byte, byte, byte) -> POST(byte, byte)

    _pwr CRITICAL(byte) -> POST(byte, byte)


Namespace 5: bcs.pid
~~~~~~~~~~~~~~~~~~~~
    apid GET() -> POST(int[])

    _pid GET(byte) -> POST(byte, int)

    cpid GET(byte) -> POST(byte, byte, byte, byte, f100, f100, f100,
                           int, byte, byte, fixed, fixed, fixed)

    cpdv GET(byte) -> POST(byte, f100, f100)

    gpdc GET() -> POST(int)

    apid POST(int, int[]) -> STATUS(byte, byte)

    _pid POST(byte, int, int) -> STATUS(byte, byte)

    _vpd POST(byte, int, int) -> STATUS(byte, byte)

    rpid POST(byte, int) -> STATUS(byte, byte)

    kpid CRITICAL() -> STATUS(byte, byte)

    cpid CRITICAL(byte, byte, byte, byte, f100, f100, f100, int,
                  byte, byte, fixed, fixed, fixed) -> STATUS(byte, byte)

    cpdv CRITICAL(byte, f100, f100) -> STATUS(byte, byte)

    acal CRITICAL(byte) -> STATUS(byte, byte)


Namespace 6: bcs.pid.dypid
~~~~~~~~~~~~~~~~~~~~~~~~~~
    dpid GET(byte) -> POST(byte, byte, byte, byte, byte)

    dpid CRITICAL(byte, byte, byte, byte, byte) -> POST()


Namespace 7: bcs.safe
~~~~~~~~~~~~~~~~~~~~~
    safe GET() -> POST(byte, int16)

    safe POST(byte, int16) -> POST(byte, byte)
