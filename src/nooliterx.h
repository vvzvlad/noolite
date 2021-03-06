#ifndef NOOLITERX_H_   /* Include guard */
#define NOOLITERX_H_

#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <time.h>

#define DEV_VID 0x16c0 //0x5824
#define DEV_PID 0x05dc //0x1500
#define DEV_CONFIG 1
#define DEV_INTF 0
#define EP_IN 0x81
#define EP_OUT 0x01

void usage(void);
void str_replace(char *ret, const char *s, const char *old, const char *new);
char* int_to_str(int num);
void cleanup(int sig);

int do_exit;

#define NSOCKET "/tmp/nooliterxd.sock"

#endif
