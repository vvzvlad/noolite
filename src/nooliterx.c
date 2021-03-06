/*        Linux console utility for nooLite smart home PC receiver RX1164 (see http://www.noo.com.by/sistema-noolite.html)
        (c) Mikhail Ermolenko (ermolenkom@yandex.ru)
        (c) vvzvlad
        (c) Oleg Artamonov (oleg@olegart.ru)
*/

#include "nooliterx.h"

unsigned char COMMAND_ACTION[8] = {0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00}; //{80,0x00,0xaa,0x00,0x0,0x1,0x14,0x05}

int main(int argc, char * argv[])
{
        int i, ret;
        unsigned int level;
        libusb_device_handle * usbhandle;
        unsigned char command[1], buf[8], channel, togl;
        char param;
        char commandtxt[255];

        int daemonize = 0;
        int timeout = 250; // default timeout 250ms
        int customcommand = 0;
        int settimeout = 0;
        int ignoreconfig = 0;
        
        do_exit = 0;
        usbhandle = NULL;
        
        setlogmask(LOG_UPTO(LOG_INFO));
        
        static struct sigaction act; 
        sigemptyset (&act.sa_mask);
        act.sa_flags = 0;
        act.sa_handler = SIG_IGN;
        sigaction (SIGHUP, &act, NULL);
        act.sa_handler = cleanup;
        sigaction (SIGINT, &act, 0);
        act.sa_handler =  cleanup;
        sigaction (SIGTERM, &act, 0);
        act.sa_handler =  cleanup;
        sigaction (SIGKILL, &act, 0);
        
        FILE* config = NULL;
        char line[255] ;
        char* token;
        
        // -- socket
		mode_t mask = umask(S_IXUSR | S_IXGRP | S_IXOTH);
        int s, s2, t, len;
        struct sockaddr_un local, remote;
        if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
        {
            printf("Socket error\n");
            exit(EXIT_FAILURE);
        }
        
		int flags = fcntl(s, F_GETFL, 0);
		fcntl(s, F_SETFL, flags | O_NONBLOCK); // non-blocking operations
        local.sun_family = AF_UNIX;
        strcpy(local.sun_path, NSOCKET);
        unlink(local.sun_path);
        len = strlen(local.sun_path) + sizeof(local.sun_family);
        if (bind(s, (struct sockaddr *)&local, len) == -1)
        {
            printf("Socket bind failed\n");
        }
        if (listen(s, 5) == -1) {
            printf("Socket listen failed\n");
        }
		umask(mask);
        // -- socket end
    
        while ((i = getopt (argc, argv, "idc:t:h")) != -1)
        {
            switch (i)
            {
                case 'd':
                    daemonize = 1;
                break;
                case 't':
                    timeout = atoi(optarg);
                    settimeout = 1;
                break;
                case 'c':
                    strcpy(commandtxt, optarg);
                    customcommand = 1;
                break;
                case 'i':
                    ignoreconfig = 1;
                    break;
        case 'h':
                    usage();
                    exit (EXIT_SUCCESS);
                break;
                case '?':
                    if (optopt == 'c')
                        fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                    else if (isprint (optopt))
                        fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                    else
                        fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
                    usage();
                    exit(EXIT_SUCCESS);
            default:
            abort ();
           }
         }

        if (!ignoreconfig)
        {
            config = fopen( "/etc/noolite.conf", "r" );
            if (config)
            {
                while(fgets(line, 254, config) != NULL)
                {
                    token = strtok(line, "\t =\n\r");
                    if (token != NULL && token[0] != '#')
                    {
                        if ((!strcmp(token, "command")) && (customcommand == 0))
                        {
                            strcpy(commandtxt, strtok(NULL, "\t\n\r"));
                            while( (*commandtxt == ' ') || (*commandtxt == '=') )
                            {
                                memmove(commandtxt, commandtxt+1, strlen(commandtxt));
                            }
                            customcommand = 1;
                        }
                        if ((!strcmp(token, "timeout")) && !settimeout)
                        {
                            timeout = atoi(strtok(NULL, "=\n\r"));
                        }
                    }
                }
                fclose(config);
            }
        }

        libusb_init(NULL);
        libusb_set_debug(NULL, 3);
        usbhandle = libusb_open_device_with_vid_pid(NULL, DEV_VID, DEV_PID);
        if (usbhandle == NULL)
        {
            printf("No compatible devices were found\n");
            libusb_exit(NULL);
            exit(EXIT_FAILURE);
        }
        
        if (libusb_kernel_driver_active(usbhandle,DEV_INTF))
        {
            libusb_detach_kernel_driver(usbhandle, DEV_INTF);
        }
        
        if ((ret = libusb_set_configuration(usbhandle, DEV_CONFIG)) < 0)
        {
            printf("USB configuration error\n");
            if (ret == LIBUSB_ERROR_BUSY)
                printf("B\n");
            printf("ret:%i\n", ret);
            libusb_close(usbhandle);
            libusb_exit(NULL);
            exit(EXIT_FAILURE);
        }
        
        if (libusb_claim_interface(usbhandle, DEV_INTF) < 0)
        {
            printf("USB interface error\n");
            libusb_close(usbhandle);
            libusb_exit(NULL);
            exit(EXIT_FAILURE);
        }
      
    // fork to background if needed and create pid file
    int pidfile;
    if (daemonize)
    {
        if (daemon(0, 0))
        {
            printf("Error forking to background\n");
            libusb_close(usbhandle);
            libusb_exit(NULL);
            exit(EXIT_FAILURE);
        }
        
        char pidval[10];
        pidfile = open("/var/run/nooliterx.pid", O_CREAT | O_RDWR, 0666);
        if (lockf(pidfile, F_TLOCK, 0) == -1)
        {
            libusb_close(usbhandle);
            libusb_exit(NULL);
            exit(EXIT_FAILURE);
        }
        sprintf(pidval, "%d\n", getpid());
        write(pidfile, pidval, strlen(pidval));
    }
    
    ret = libusb_control_transfer(usbhandle, LIBUSB_REQUEST_TYPE_CLASS|LIBUSB_RECIPIENT_INTERFACE|LIBUSB_ENDPOINT_IN, 0x9, 0x300, 0, buf, 8, 1000);
    togl = (buf[0] & 128);
    
    openlog("nooliterx", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_DAEMON);
    syslog(LOG_INFO, "nooliterx started");
    
    char cmd[255];
	struct timespec timeRecv;
	long timeRecvNs = 0;
	time_t timeRecvS = 0;
	
    while (!do_exit)
    {
        s2 = accept(s, (struct sockaddr *)&remote, &t);
        if (s2 < 0) // no incoming connection on the socket, working as receiver
        {
            if (timeout)
            {
                snprintf(cmd, 255, "timeout %i %s", timeout, commandtxt);
            }
            else
            {
                snprintf(cmd, 255, "%s", commandtxt);
            }
            
            ret = libusb_control_transfer(usbhandle, LIBUSB_REQUEST_TYPE_CLASS|LIBUSB_RECIPIENT_INTERFACE|LIBUSB_ENDPOINT_IN, 0x9, 0x300, 0, buf, 8, 500);
            if ((ret == 8) && (togl!=(buf[0] & 128))) // TOGL is a 7th bit of the 1st data byte (adapter status), it toggles value every time new command received
            {
                togl = (buf[0] & 128);
                
                if (customcommand)
                {
					char repstr[255];
					static char *searchfor[8] = {"%st", "%ch", "%cm", "%df", "%d0", "%d1", "%d2", "%d3"};
					int k;
					for (k=0; k<8; k++)
					{
						int incr = 0;
						if (k == 1)
							incr = 1; // compatibility fix

						str_replace(repstr, cmd, searchfor[k], int_to_str(buf[k] + incr));
						strcpy(cmd, repstr);
					}
                }
                else
                {
                    sprintf(cmd, "echo -e 'Adapter status:\t%i\\nChannel:\t%i\\nCommand:\t%i\\nData format:\t%i\\nData:\t\t%i %i %i %i\\n\\n'", buf[0], buf[1]+1, buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
                }
                syslog(LOG_INFO, "Received: status %i, channel %i, command %i, format %i, data %i %i %i %i", buf[0], buf[1]+1, buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
                system(cmd);
            }
        }
        else // incoming connection, working as configuration tool
        {
            i = recv(s2, cmd, 25, 0);
            cmd[i] = 0; // null-terminated string
            close(s2);
            char * rxcmd[2];
            rxcmd[0] = strtok(cmd, "- \n");
            rxcmd[1] = strtok(NULL, "- \n");
            
            if (strcmp(rxcmd[0], "stop") == 0) // 2 - остановить привязку принудительно
            {
                COMMAND_ACTION[0] = 2;
            }
            else if (strcmp(rxcmd[0], "clearall") == 0) // 4 - очистить всю память 
            {
                COMMAND_ACTION[0] = 4;
            }
            else if (strcmp(rxcmd[0], "bind") == 0) // 1 - включить привязку на адрес ячейки, 30 секунд
            {
                COMMAND_ACTION[0] = 1;
            }
            else if (strcmp(rxcmd[0], "clear") == 0) // 3 - очистить ячейку
            {
                COMMAND_ACTION[0] = 3;
            }
            COMMAND_ACTION[1] = atoi(rxcmd[1]) - 1; // channel number
            
            ret = libusb_control_transfer(usbhandle, LIBUSB_REQUEST_TYPE_CLASS|LIBUSB_RECIPIENT_INTERFACE|LIBUSB_ENDPOINT_OUT, 0x9, 0x300, 0, COMMAND_ACTION, 8, 100);
            syslog(LOG_INFO, "Configuration command %s (channel %s) sent to USB receiver", rxcmd[0], rxcmd[1]);
        }
		usleep(100000);
    }
    libusb_attach_kernel_driver(usbhandle, DEV_INTF);
    libusb_close(usbhandle);
    libusb_exit(NULL);
    syslog(LOG_INFO, "nooliterx terminated");
    closelog();
    if (pidfile)
    {
        lockf(pidfile, F_ULOCK, 0);
        close(pidfile);
        remove("/var/run/nooliterx.pid");
    }
}

void usage(void)
{
    printf("Usage: nooliterx [-c command] [-t timeout] [-d] [-h]\n");
    printf("  -c\tcommand to execute. Default is to print received data to stdout.\n");
    printf("  -t\tcommand execution timeout, milliseconds (0 to disable). Default is 250 (250 ms).\n");
    printf("  -d\trun in the background.\n");
    printf("  -i\tignore /etc/noolite.conf.\n");
    printf("  -h\tprint help and exit.\n");
    printf("\nCommand examples:\n");
    printf("  echo 'Status: %%st Channel: %%ch Command: %%cm Data format: %%df Data bytes: %%d0 %%d1 %%d2 %%d3'\n");
    printf("  wget http://localhost/noolight/?script=switchNooLitePress\\&channel=%%ch\\&command=%%cm\n");
    printf("\nAvailable variables (for detailed description, see RX1164 API manual at http://www.noo.com.by/):\n");
    printf("  %%st\t RX1164 adapter status\n");
    printf("  %%ch\t Channel number\n");
    printf("  %%cm\t Command number\n");
    printf("  %%df\t Data format\n");
    printf("  %%d0\t Data (1st byte)\n");
    printf("  %%d1\t Data (2nd byte)\n");
    printf("  %%d2\t Data (3rd byte)\n");
    printf("  %%d3\t Data (4th byte)\n");
}

void str_replace(char *ret, const char *s, const char *old, const char *new)
{
    int i, count = 0;
    size_t newlen = strlen(new);
    size_t oldlen = strlen(old);

    for (i = 0; s[i] != '\0'; i++)
    {
        if (strstr(&s[i], old) == &s[i])
        {
            count++;
            i += oldlen - 1;
        }
    }
  
    i = 0;
    while (*s)
    {
        if (strstr(s, old) == s)
        {
            strcpy(&ret[i], new);
            i += newlen;
            s += oldlen;
        }
        else
        {
            ret[i++] = *s++;
        }
    }
    
    ret[i] = '\0';
}

char* int_to_str(int num)
{
    static char retstr[4];
    snprintf(retstr, 4, "%d", num);
    return retstr;
}

void cleanup(int sig)
{
    do_exit = 1;
}
