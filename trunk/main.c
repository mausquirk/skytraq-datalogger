/*

    Control program for SkyTraq GPS data logger.

    Copyright (C) 2008  Jesper Zedlitz, jesper@zedlitz.de

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111 USA

 */

#include "datalogger.h"
#include "lowlevel.h"

enum{ NO_ACTION, ACTION_INFO, ACTION_DELETE, ACTION_DUMP, ACTION_CONFIG,
      ACTION_SET_SPEED, ACTION_OUTPUT_OFF, ACTION_OUTPUT_NMEA, ACTION_OUTPUT_BINARY
    };

#define verbose(fmt, args...) fprintf(stderr, fmt"\n", ##args)

int main(int argc, char *argv[])  {
    int fd, i, baud_rate = 0, action=NO_ACTION, serial_speed=0;
    int min_time=-1, max_time=-1, min_dist=-1, max_dist=-1, min_speed=-1, max_speed=-1, enable=0, disable=0, gpx_format=0;
    int mode_fifo = 0, mode_stop = 0;
    int permanent = 0;
    int success;
    char* device = "/dev/ttyUSB0";

    for ( i = 1; i < argc; i++ ) {
        if (!strcmp(argv[i], "--info" ) ) {
            action = ACTION_INFO;
        } else if ( !strcmp(argv[i], "--delete" ) ) {
            action = ACTION_DELETE;
        } else if ( !strcmp(argv[i], "--dump" ) ) {
            action = ACTION_DUMP;
        } else if ( !strcmp(argv[i], "--set-config" ) ) {
            action = ACTION_CONFIG;
        } else if ( !strcmp(argv[i], "--set-baud-rate" ) ) {
            action = ACTION_SET_SPEED;
	    if ( argc>i+1) serial_speed = atoi(argv[++i]);
        } else if ( !strcmp(argv[i], "--device" ) ) {
            if ( argc>i+1) device = argv[++i];
        } else if ( !strcmp(argv[i], "--time" ) ) {
            if ( argc>i+1) min_time = atoi(argv[++i]);
        } else if ( !strcmp(argv[i], "--max-time" ) ) {
            if ( argc>i+1) max_time = atoi(argv[++i]);
        } else if ( !strcmp(argv[i], "--dist" ) ) {
            if ( argc>i+1) min_dist = atoi(argv[++i]);
        } else if ( !strcmp(argv[i], "--max-dist" ) ) {
            if ( argc>i+1) max_dist = atoi(argv[++i]);
        } else if ( !strcmp(argv[i], "--speed" ) ) {
            if ( argc>i+1) min_speed = atoi(argv[++i]);
        } else if ( !strcmp(argv[i], "--max-speed" ) ) {
            if ( argc>i+1) max_speed = atoi(argv[++i]);
        } else if ( !strcmp(argv[i], "--enable-log" ) ) {
            enable = 1;
        } else if ( !strcmp(argv[i], "--disable-log" ) ) {
            disable = 1;
        } else if ( !strcmp(argv[i], "--mode-fifo" ) ) {
            mode_fifo = 1;
        } else if ( !strcmp(argv[i], "--mode-stop" ) ) {
            mode_stop = 1;
        } else if ( !strcmp(argv[i], "--gpx" ) ) {
            gpx_format = 1;
        } else if ( !strcmp(argv[i], "--permanent" ) ) {
            permanent = 1;
        } else if ( !strcmp(argv[i], "--set-output-off" ) ) {
            action = ACTION_OUTPUT_OFF;
        } else if ( !strcmp(argv[i], "--set-output-nmea" ) ) {
            action = ACTION_OUTPUT_NMEA;
        } else if ( !strcmp(argv[i], "--set-output-bin" ) ) {
            action = ACTION_OUTPUT_BINARY;
        } else if ( !strcmp(argv[i], "--baud-rate" ) ) {
	    if ( argc>i+1) baud_rate = atoi(argv[++i]);
	}
    }

    if ( action == NO_ACTION ) {
        fprintf(stderr, "USAGE: %s <OPTIONS> ACTION \n", argv[0]);
        fprintf(stderr, " ACTION is one of:\n");
        fprintf(stderr, "  --info             get information about software version and configuration\n");
        fprintf(stderr, "  --delete           delete all track lists from the data logger\n");
        fprintf(stderr, "  --dump             dump track lists to STDOUT\n");
        fprintf(stderr, "  --set-config       change configuration of the data logger\n");
        fprintf(stderr, "  --set-baud-rate    configure speed of the device's serial port\n");
        fprintf(stderr, "  --set-output-off   disable output for GPS data\n");
        fprintf(stderr, "  --set-output-nmea  enable output for GPS data in NMEA format\n");
        fprintf(stderr, "  --set-output-bin   enable output for GPS data in binary format\n");
        fprintf(stderr, " OPTIONS:\n");
        fprintf(stderr, "  --device <DEV>        name of the device, default is /dev/ttyUSB0\n");
        fprintf(stderr, "  --permanent           write serial port speed to FLASH\n");
	fprintf(stderr, "  --baud-rate           set baud-rate manually\n");
        fprintf(stderr, " OPTIONS for dump output:\n");
        fprintf(stderr, "  --gpx                 dump track in gpx format\n");
        fprintf(stderr, " OPTIONS for configuration:\n");
        fprintf(stderr, "  --time <SECONDS>      log every <SECONDS> seconds\n");
        fprintf(stderr, "  --max-time <SECONDS>  \n");
        fprintf(stderr, "  --dist <METERS>       log every <METERS> meters\n");
        fprintf(stderr, "  --max-dist <METERS>   \n");
        fprintf(stderr, "  --speed <KMPH>        only log if faster than <KMPH> km/h\n");
        fprintf(stderr, "  --max-speed <KMPH>   \n");
        fprintf(stderr, "  --enable-log          turn on logging\n");
        fprintf(stderr, "  --disable-log         turn off logging\n");
        fprintf(stderr, "  --mode-fifo           overwrite oldest entries when no space is left\n");
        fprintf(stderr, "  --mode-stop           stop logging when no space is left\n");
        return 2;
    }

    fd = open_port(device);
    if ( fd == -1 ) {
        fprintf(stderr,"Failed to open device %s\n", device);
        return 1;
    }

    /* detect device and speed */
    if( baud_rate == 0 ) {
	    baud_rate = skytraq_determine_speed(fd);
	    if ( baud_rate == 0 ) {
	        fprintf(stderr,"Could not find data logger at port %s\n", device);
	        return 1;
	    }
    }

    /* get status and config from GPS data logger */
    skytraq_config* info = malloc( sizeof(skytraq_config));
    success = skytraq_read_datalogger_config(fd,info);
    
    if( success != SUCCESS ) {
    	fprintf(stderr, "No response from datalogger.\n");
	return 1;
    }

    if ( action == ACTION_INFO ) {
        int agps_days, agps_hours;
        skytraq_read_software_version(fd);
	skytraq_read_agps_status(fd, info);
	
	agps_days = info->agps_hours_left / 24;
	agps_hours = info->agps_hours_left % 24;
	
        printf("log_wr_ptr:      %ld\n",info->log_wr_ptr );
        printf("total sectors:   %d\n", info->total_sectors);
        printf("sectors left:    %d\n", info->sectors_left);
        printf("max time:        %ld s\n", info->max_time);
        printf("min time:        %ld s\n", info->min_time);
        printf("max distance:    %ld m\n", info->max_distance);
        printf("min distance:    %ld m\n",info->min_distance );
        printf("max speed:       %ld km/h\n", info->max_speed);
        printf("min speed:       %ld km/h\n", info->min_speed);
        printf("datalog enable:  %d\n",info->datalog_enable );
        printf("log fifo mode:   %d\n", info->log_fifo_mode);
	printf("AGPS enabled:    %d\n", info->agps_enabled);
	printf("AGPS data left:  ");
	if( agps_days == 1 ) {
	  printf("1 day ");
	} else if( agps_days > 0 ) {
	  printf("%d days ", agps_days);
	}
	if( agps_hours == 1 ) {
	  printf("1 hour");
	} else if( agps_hours > 0 ) {
	  printf("%d hours ", agps_hours);
	}
	printf("\n");
	printf("baud-rate:       %d bps\n", baud_rate);
    } else if ( action == ACTION_DELETE ) {
        skytraq_clear_datalog(fd);
    } else if ( action == ACTION_DUMP ) {
        int used_sectors;

        used_sectors = info->total_sectors - info->sectors_left + 1;

	if (gpx_format) {
	    printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<gpx>\n<trk>\n<trkseg>\n");
	}
        for ( i = 0; i< used_sectors ; i++ ) {
            int len, retries_left = 3;
            gbuint8* buf = malloc(4100);
            len= skytraq_read_datalog_sector(fd,i,buf);
            while ( len == -1 && retries_left > 3 ) {
                /* retry to read the sector */
                len= skytraq_read_datalog_sector(fd,i,buf);
                retries_left--;
            }
            process_buffer(buf,len,gpx_format);
            free(buf);

            sleep(1);
            /* close(fd);
             fd = open("/dev/ttyUSB0",O_RDWR); */
        }
	if (gpx_format) {
	    printf("</trkseg>\n</trk>\n</gpx>\n");
	}
    } else if ( action  == ACTION_CONFIG ) {
        if ( min_time > -1 ) info->min_time = min_time;
        if ( max_time > -1 ) info->max_time = max_time;
        if ( min_dist > -1 ) info->min_distance = min_dist;
        if ( max_dist > -1 ) info->max_distance = max_dist;
        if ( min_speed > -1 ) info->min_speed = min_speed;
        if ( max_speed > -1 ) info->max_speed = max_speed;
        if ( disable ) info->datalog_enable = 0;
        if ( enable ) info->datalog_enable = 1;
        if ( mode_stop ) info->log_fifo_mode = 0;
        if ( mode_fifo ) info->log_fifo_mode = 1;
        skytraq_write_datalogger_config(fd,info);
    } else if( action == ACTION_SET_SPEED ) {
    	unsigned requested_speed = skytraq_mkspeed( serial_speed );
	if( requested_speed != ERROR ) {
		skytraq_set_serial_speed(fd,requested_speed,permanent);
	} else {
	   fprintf( stderr, "unknown speed %d\n", serial_speed);
	   return 1;
	}
    } else if( action == ACTION_OUTPUT_OFF ) {
    	skytraq_output_disable(fd);
    } else if( action == ACTION_OUTPUT_NMEA ) {
    	skytraq_output_enable_nmea(fd);    
    } else if( action == ACTION_OUTPUT_BINARY ) {
       	skytraq_output_enable_binary(fd);
    }
    free(info);
    close(fd);

    return 0;
}
