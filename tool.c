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
#include "dyio.h"

const char version[] = "1.0."GITVERSION;
const char copyright[] = "Copyright (C) 2015 Serge Vakulenko";

char *progname;
int verbose;

/*
 * Simple test of digital inputs and outputs.
 * Input sensor (button) is connected to channel 23.
 * Two LEDs are connected to channels 00 and 01.
 * While button is idle, LED1 is off and LED2 is on.
 * When button is pressed, LED1 is turned on, and LED2 turned off.
 */
void test1(dyio_t *d)
{
    int led, button;

    printf("Test 1: button at channel 23, two LEDs at channels 00 and 01.\n");
    dyio_set_mode(d, 23, MODE_DI);
    dyio_set_mode(d, 0, MODE_DO);
    dyio_set_mode(d, 1, MODE_DO);
    led = 0;
    dyio_set_value(d, 0, 0);
    dyio_set_value(d, 1, 1);
    for (;;) {
        button = !dyio_get_value(d, 23);
        if (button != led) {
            printf(button ? "#" : ".");
            led = button;
            dyio_set_value(d, 0, button);
            dyio_set_value(d, 1, !button);
        }
        fflush(stdout);
        usleep(10000);
    }
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

int main(int argc, char **argv)
{
    char *devname;
    int iflag = 0, nflag = 0, cflag = 0, tflag = 0;
    int debug = 0;
    dyio_t *d;

    progname = *argv;
    for (;;) {
        switch (getopt(argc, argv, "vdinct:")) {
        case EOF:
            break;
        case 'v':
            verbose++;
            continue;
        case 'd':
            debug++;
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


    if (argc < 1)
        usage();
    devname = argv[0];

    if (verbose)
        printf("Port name: %s\n", devname);

    d = dyio_connect(devname, debug);
    if (! d) {
        printf("Failed to open port %s\n", devname);
        exit(-1);
    }

    if (verbose)
        printf("DyIO device address: %02x-%02x-%02x-%02x-%02x-%02x\n",
            d->reply_mac[0], d->reply_mac[1], d->reply_mac[2],
            d->reply_mac[3], d->reply_mac[4], d->reply_mac[5]);

    if (iflag)
        dyio_info(d);

    if (nflag)
        dyio_print_namespaces(d);

    if (cflag) {
        if (verbose)
            dyio_print_channel_features(d);
        dyio_print_channels(d);
    }

    switch (tflag) {
    case 1:
        test1(d);
        break;

    /* TODO: add more tests here.
    case 2:
        test1(d);
        break;
    */
    }

	if (argc == 4){
		printf("\n%s\n",argv[1]);
		if (strcmp(argv[1], "mode")==0){

			int ch = strtol(argv[2], (char **)NULL, 10);
			int mode = strtol(argv[3], (char **)NULL, 10);
			printf("Set ch %d to mode %d\n",ch,mode);		
			dyio_set_mode(d, ch, mode);
		} else if(strcmp(argv[1], "value")==0) {

			int ch = strtol(argv[2], (char **)NULL, 10);
			int val = strtol(argv[3], (char **)NULL, 10);
			printf("Set ch %d to value %d\n",ch,val);
			dyio_set_value(d, ch, val);	
		}
	}
	
    dyio_close(d);
    return 0;
}
