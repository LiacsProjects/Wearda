//
// Copyright Leiden University, Liacs 2021
//
// Author: Richard M.K. van Dijk, m.k.van.dijk@liacs.leidenuniv.nl
//

#include <tizen.h>
#include <service_app.h>
#include "sensorservice.h"

#include <sensor.h>
#include <locations.h>
#include <device/battery.h>
#include <device/power.h>
#include <time.h>
#include <device/haptic.h>
#include <stdio.h>

#define VERSION_NUMBER                     "v1.0.2"

// Testing mode for privacy circle: true means record all data but indicate outside P, inside I or unknown ? of privacy circle
#define TESTING_MODE                           true

// Sensor service states
#define WAITING                                   0
#define MEASURING                                 1

// Unique number
#define MIN_UNIQUE_IDENTIFIER_WATCH             001
#define MAX_UNIQUE_IDENTIFIER_WATCH             999
#define DEFAULT_UNIQUE_IDENTIFIER_WATCH      "0000"

// Sensor intervals in milli seconds (unsigned int)
#define MIN_INTERVAL_ACCELEROMETER                1
#define MAX_INTERVAL_ACCELEROMETER             1000
#define DEFAULT_INTERVAL_ACCELEROMETER           25

#define MIN_INTERVAL_LINEAR_ACCELEROMETER         1
#define MAX_INTERVAL_LINEAR_ACCELEROMETER      1000
#define DEFAULT_INTERVAL_LINEAR_ACCELEROMETER    25 // Zero means switched off, take gravity accelerometer

#define MIN_INTERVAL_GYROSCOPE                    1
#define MAX_INTERVAL_GYROSCOPE                 1000
#define DEFAULT_INTERVAL_GYROSCOPE               25

#define MIN_INTERVAL_BAROMETER                  100
#define MAX_INTERVAL_BAROMETER                10000
#define DEFAULT_INTERVAL_BAROMETER              100

// Sensor intervals in seconds (unsigned int)
#define MIN_INTERVAL_GPS                          1
#define MAX_INTERVAL_GPS                         10
#define DEFAULT_INTERVAL_GPS                      1

#define MIN_BASE_LATITUDE                -90.000000
#define MAX_BASE_LATITUDE                 90.000000
#define DEFAULT_BASE_LATITUDE             52.169311 // Lorentz Center @ Snellius

#define MIN_BASE_LONGITUDE              -180.000000
#define MAX_BASE_LONGITUDE               180.000000
#define DEFAULT_BASE_LONGITUDE             4.456711 // Lorentz Center @ Snellius

#define MIN_BASE_PRIVACY_DISTANCE                10
#define MAX_BASE_PRIVACY_DISTANCE             10000
#define DEFAULT_BASE_PRIVACY_DISTANCE           100

// Sensor write to sensor file interval in seconds (float)
#define MIN_INTERVAL_WRITE                    0.010
#define MAX_INTERVAL_WRITE                   10.000
#define DEFAULT_INTERVAL_WRITE                0.050
#define START_DELAY_SENSOR_WRITE              0.225 // Configurable

#define NR_SAMPLES_AGA                        10000


struct _sensor_info {
    sensor_h sensor;
    sensor_listener_h sensor_listener;
};
typedef struct _sensor_info sensorinfo_s;

/**
 *
 * @brief Global variables starting with g_
 *
 */

FILE *g_fd_aag = NULL;                          // sensor file for gravity- and linear accelerometer and gyroscope
FILE *g_fd_bar = NULL;                          // sensor file for air pressure barometer
FILE *g_fd_gps = NULL;                          // sensor file for GPS latitude, longitude

static sensorinfo_s g_sensor_info_accelerometer;
static sensorinfo_s g_sensor_info_gyroscope;
static sensorinfo_s g_sensor_info_pressure;
static sensorinfo_s g_sensor_info_linear_accelerometer;

// Accelerometer, gyroscope, air-pressure, battery
static float g_acce_x, g_acce_x_;               // acceleration in m/s^2
static float g_acce_y, g_acce_y_;               // acceleration in m/s^2
static float g_acce_z, g_acce_z_;               // acceleration in m/s^2

static float g_lin_acce_x, g_lin_acce_x_;       // acceleration in m/s^2
static float g_lin_acce_y, g_lin_acce_y_;       // acceleration in m/s^2
static float g_lin_acce_z, g_lin_acce_z_;       // acceleration in m/s^2

static float g_gyro_x, g_gyro_x_;               // rotation speed in degrees ratio per second (-2^15/+2^15)
static float g_gyro_y, g_gyro_y_;               // rotation speed in degrees ratio per second (-2^15/+2^15)
static float g_gyro_z, g_gyro_z_;               // rotation speed in degrees ratio per second (-2^15/+2^15)

static float g_pressure, g_pressure_;           // barometer air pressure in milli bar
static int   g_battery;                         // remaining power of battery in percentage of maximum capacity, 5% = low-battery, applications will switch off

static double g_time_;                          // The time when the last write timer wrote the sensor values

// GPS
static location_manager_h g_manager;

static double g_latitude;                       // horizontal coordinate dd.mm.ss.ff in degrees, minutes, seconds and fraction of a second
static double g_longitude;                      // vertical coordinate dd.mm.ss.ff in degrees, minutes, seconds and fraction of a second
static double g_altitude;                       // height in meters
static double g_climb;                          // speed of climb in km/h
static double g_direction;                      // movement direction as degree with the North as zero orientation
static double g_speed;                          // movement speed in km/h
static double g_horizontal;                     // accuracy in meters for the horizontal plain
static double g_vertical;                       // accuracy in meters for the vertical plain
static location_accuracy_level_e g_level;       // number of accuracy of location determination between 0 and 6

static location_boundary_state_e g_gps_base_bound_state = LOCATIONS_ERROR_GPS_SETTING_OFF;
static location_bounds_h g_gps_base_bounds;

// Varying globals
static double g_base_write_sensor_readings_time = 0.0;
static char g_timestring[20] = "YYYY MM DD HH mm ss";
static unsigned int g_personid = 0;
static int g_service_state = WAITING;

Ecore_Timer *g_timer_id;

/**
 *
 * @brief Vibrate wearable for 1 second, intensity 30 (0-100).
 *
 */

void
vibrate()
{
	haptic_device_h device_handle;
	int device_number;

	int err = device_haptic_get_count(&device_number);

	dlog_print(DLOG_INFO, LOG_TAG,  "Vibrate device number %d %d", device_number, err);

	device_haptic_open(device_number, &device_handle);
	device_haptic_vibrate(device_handle, 1000, 30, NULL); // TODO: search for effect examples
	device_haptic_stop(device_handle, NULL);

	return;
}

/**
 *
 * @brief Timer callback to align the time and sensor readings.
 *
 * @details ISO 8601 time format YYYY:MM:DD HH:mm:ss, 24h format,
 * but without : because Tizen Device Manager "pull file" does not accept this.
 *
 * Reference: http://www.cplusplus.com/reference/ctime/strftime/
 *
 */

void
get_timestring()
{
	time_t rawtime;
	time (&rawtime);

	struct tm * timeinfo;
	timeinfo = localtime (&rawtime);

	strftime (g_timestring, 20, "%Y %m %d %H %M %S", timeinfo);
	return;
}

/**
 *
 * @brief Execute a Linux command.
 *
 * @detailed The command is given on the root dir because change of directory is not supported.
 * So, to get a list of files from a certain dir can be done with the command "ls -l /opt/var/tmp".
 *
 * If you are not interested in the output, use system("ls -l /opt/var/tmp") != NULL;
 *
 * To remove a file use for example "rm /opt/var/tmp/000 * aga.dat"
 *
 * NOTE: You can only remove a file with the app which created this file.
 *
 * To make a file use "ls -l /opt/var/tmp > /opt/var/tmp/output.txt"
 *
 * The directory /opt/var/tmp has set all permissions (read, write and execute).
 *
 *
 * @return 0 if okay, -1 if command failed
 */

int
linux_command(const char* command, char* linux_output)
{
	FILE *fd = popen(command, "r");

	if (fd < 0) {
		dlog_print(DLOG_ERROR, LOG_TAG, "linux command %s failed %d", command, fd);
		return -1;
	}

	char line[256];
	line[0] = 0;
	linux_output[0] = 0;

	while(fgets(line, sizeof(line), fd) != NULL)
	{
		strcat(linux_output, line);

		//dlog_print(DLOG_INFO, LOG_TAG, "%s", line);
	}
	strcat(linux_output, "\n");

	pclose(fd);

	return 0;
}

/**
 *
 * @brief Read and write the configuration file.
 *
 * @details
 *
 *  line1 - unique_identifier_watch_int <value in %3d><\n>
 *  line2 - accelerometer_interval_ms <value in %3d><\n>
 *  line3 - linear_accelerometer_interval_ms <value in %3d><\n>
 *  line4 - gyroscope_interval_ms <value in %3d><\n>
 *  line5 - barometer_interval_ms <value in %3d><\n>
 *  line6 - gps_interval_seconds <value in %2d><\n>
 *  line7 - write_interval_ms <value in %3d><\n>
 *  line8 - gps_base_point_latitude <value in %2.6f>  _longitude <value in %2.6f><\n>
 *  line9 - gps_base_privacy_distance <><\n>
 *
 * If the parameters have the value of zero, the sensor or service will be disabled.
 *
 */

static char g_unique_identifier_watch[32]           = DEFAULT_UNIQUE_IDENTIFIER_WATCH;
static unsigned int g_accelerometer_interval_ms     = DEFAULT_INTERVAL_ACCELEROMETER;
static unsigned int g_lin_accelerometer_interval_ms = DEFAULT_INTERVAL_LINEAR_ACCELEROMETER;
static unsigned int g_gyroscope_interval_ms         = DEFAULT_INTERVAL_GYROSCOPE;
static unsigned int g_barometer_interval_ms         = DEFAULT_INTERVAL_BAROMETER;
static unsigned int g_gps_interval_seconds          = DEFAULT_INTERVAL_GPS;
static double       g_gps_base_point_latitude       = DEFAULT_BASE_LATITUDE;
static double       g_gps_base_point_longitude      = DEFAULT_BASE_LONGITUDE;
static unsigned int g_gps_base_privacy_distance     = DEFAULT_BASE_PRIVACY_DISTANCE;

static double g_write_interval_seconds = DEFAULT_INTERVAL_WRITE;

/**
 *
 * @brief If a parameter is zero, let it be, it is used to disable to corresponding sensor.
 *
 * @details The validation does not throw errors but corrects invalid values by default settings (robust validation type).
 *
 * The configuration file will be read from /opt/var/tmp/ folder. This folder permits the developer account to write
 * a configuration file to while the data folder of the sensor service does not. The file cannot be deleted by the developer
 * account. This is good because the configuration file contains the unique identifier of the watch.
 *
 * However, it can be over-written. Do take care not to change the unique identifier of the watch.
 *
 */

static void
validate_configuration_file_contents()
{
    if(strcmp(g_unique_identifier_watch, "") == 0)
    	snprintf(g_unique_identifier_watch, sizeof(g_unique_identifier_watch), "%s\n", DEFAULT_UNIQUE_IDENTIFIER_WATCH);

    if(g_accelerometer_interval_ms != 0)
		if(!(MIN_INTERVAL_ACCELEROMETER <= g_accelerometer_interval_ms && g_accelerometer_interval_ms <= MAX_INTERVAL_ACCELEROMETER))
			g_accelerometer_interval_ms = DEFAULT_INTERVAL_ACCELEROMETER;

    if(g_lin_accelerometer_interval_ms != 0)
		if(!(MIN_INTERVAL_ACCELEROMETER <= g_lin_accelerometer_interval_ms && g_lin_accelerometer_interval_ms <= MAX_INTERVAL_ACCELEROMETER))
			g_lin_accelerometer_interval_ms = DEFAULT_INTERVAL_LINEAR_ACCELEROMETER;

    if(g_gyroscope_interval_ms != 0)
		if(!(MIN_INTERVAL_GYROSCOPE <= g_gyroscope_interval_ms && g_gyroscope_interval_ms <= MAX_INTERVAL_GYROSCOPE))
			g_gyroscope_interval_ms = DEFAULT_INTERVAL_GYROSCOPE;

    if(g_barometer_interval_ms != 0)
		if(!(MIN_INTERVAL_BAROMETER <= g_barometer_interval_ms && g_barometer_interval_ms <= MAX_INTERVAL_BAROMETER))
			g_barometer_interval_ms = DEFAULT_INTERVAL_BAROMETER;

    if(g_gps_interval_seconds != 0)
		if(!(MIN_INTERVAL_GPS <= g_gps_interval_seconds && g_gps_interval_seconds <= MAX_INTERVAL_GPS))
			g_gps_interval_seconds = DEFAULT_INTERVAL_GPS;

    if(g_gps_base_point_latitude != 0)
		if(!(MIN_BASE_LATITUDE <= g_gps_base_point_latitude && g_gps_base_point_latitude <= MAX_BASE_LATITUDE))
			g_gps_base_point_latitude = DEFAULT_BASE_LATITUDE;

    if(g_gps_base_point_longitude != 0)
		if(!(MIN_BASE_LONGITUDE <= g_gps_base_point_longitude && g_gps_base_point_longitude <= MAX_BASE_LONGITUDE))
			g_gps_base_point_longitude = DEFAULT_BASE_LONGITUDE;

    if(g_gps_base_privacy_distance != 0)
		if(!(MIN_BASE_PRIVACY_DISTANCE <= g_gps_base_privacy_distance && g_gps_base_privacy_distance <= MAX_BASE_PRIVACY_DISTANCE))
			g_gps_base_privacy_distance = DEFAULT_BASE_PRIVACY_DISTANCE;

    if(!(MIN_INTERVAL_WRITE <= g_write_interval_seconds && g_write_interval_seconds <= MAX_INTERVAL_WRITE))
    	g_write_interval_seconds = DEFAULT_INTERVAL_WRITE;

	return;
}

static void
read_configuration_file()
{
	char* data_path = NULL;
	char configurationfilename[256];

	data_path = "/opt/var/tmp/"; // This folder has write permission for the developer account used by the host
	snprintf(configurationfilename, 256, "%sconfiguration.dat", data_path);
	dlog_print(DLOG_INFO, LOG_TAG, "Data path + configuration filename for read: %s", configurationfilename);

    FILE *fd = fopen(configurationfilename, "r");
	if(fd == NULL) {
		dlog_print(DLOG_ERROR, LOG_TAG, "Could not open configuration file for read");
		return;
    }

	fscanf(fd, "unique_identifier_watch_str %s\n", &g_unique_identifier_watch);
    fscanf(fd, "accelerometer_interval_ms_int %u\n", &g_accelerometer_interval_ms);
    fscanf(fd, "linear_accelerometer_interval_ms_int %u\n", &g_lin_accelerometer_interval_ms);
    fscanf(fd, "gyroscope_interval_ms_int %u\n", &g_gyroscope_interval_ms);
    fscanf(fd, "barometer_interval_ms_int %u\n", &g_barometer_interval_ms);
    fscanf(fd, "gps_interval_seconds_int %u\n", &g_gps_interval_seconds);
    fscanf(fd, "write_interval_seconds_float %lf\n", &g_write_interval_seconds);
    fscanf(fd, "gps_base_point_latitude %lf _longitude %lf\n", &g_gps_base_point_latitude, &g_gps_base_point_longitude);
    fscanf(fd, "gps_base_privacy_distance_meter_int %u\n", &g_gps_base_privacy_distance);

    fclose(fd);

    validate_configuration_file_contents();

    return;
}

static void
write_configuration_file()
{
	char* data_path = NULL;
	char configurationfilename[256];

	data_path = app_get_data_path();
	snprintf(configurationfilename, 256, "%s%03d %s %s con.dat", data_path, g_personid, g_timestring, g_unique_identifier_watch);
	dlog_print(DLOG_INFO, LOG_TAG, "Data path + configuration filename for write: %s", configurationfilename);

	FILE *fd = fopen(configurationfilename, "w");
	if(fd == NULL) {
		dlog_print(DLOG_ERROR, LOG_TAG, "Could not open current settings file for write");
		return;
	}

	fprintf(fd, "version number_str %s\n", VERSION_NUMBER);
    fprintf(fd, "unique_identifier_watch_str %s\n", g_unique_identifier_watch);
    fprintf(fd, "accelerometer_interval_ms_int %3u\n", g_accelerometer_interval_ms);
    fprintf(fd, "linear_accelerometer_interval_ms_int %3u\n", g_lin_accelerometer_interval_ms);
    fprintf(fd, "gyroscope_interval_ms_int %3u\n", g_gyroscope_interval_ms);
    fprintf(fd, "barometer_interval_ms_int %3u\n", g_barometer_interval_ms);
    fprintf(fd, "gps_interval_seconds_int %2u\n", g_gps_interval_seconds);
    fprintf(fd, "write_interval_seconds_float %2.3f\n", g_write_interval_seconds);
    fprintf(fd, "gps_base_point_latitude %2.6f _longitude %2.6f\n", g_gps_base_point_latitude, g_gps_base_point_longitude);
    fprintf(fd, "gps_base_privacy_distance_meter_int %4u\n", g_gps_base_privacy_distance);
    fprintf(fd, "\n");
    fprintf(fd, "Notes:\n");
    fprintf(fd, " Lorentz Center @ Snellius Leiden, latitude %2.6f longitude %2.6f\n", DEFAULT_BASE_LATITUDE, DEFAULT_BASE_LONGITUDE);
    fprintf(fd, " Vossenberg @ Kaatsheuvel, latitude 51.654439 longitude 5.047266\n");
    fprintf(fd, " Zilkerduinweg 8 @ De Zilk, latitude 52.292638 longitude 4.527435\n");
    fprintf(fd, " Treslong hof @ Hillegom, latitude 52.298330, longitude 4.583271\n");
    fprintf(fd, " Maartensheem @ Hillegom, latitude 52.299163, longitude 4.584793\n");
    fprintf(fd, " ...\n");

    fclose(fd);
    return;
}

/**
 *
 * @brief Open and close the sensor files (aag = accelerometer+gyro, bar = barometer, gps = gps data).
 *
 */

static void
open_new_sensor_files()
{
	char* data_path = NULL;
	char aagfilename[256];
	char barfilename[256];
	char gpsfilename[256];

	get_timestring();
	g_base_write_sensor_readings_time = 0.0;

	// AAG sensor file
	data_path = app_get_data_path();

	snprintf(aagfilename, 256, "%s%03d %s %s aag.dat", data_path, g_personid, g_timestring, g_unique_identifier_watch);
	dlog_print(DLOG_INFO, LOG_TAG, "Data path + aag filename: %s", aagfilename);

	g_fd_aag = fopen(aagfilename, "w");

	fprintf(g_fd_aag, "%03d %s %s\n", g_personid, g_unique_identifier_watch, g_timestring);
	if(g_lin_accelerometer_interval_ms == 0)
		fprintf(g_fd_aag, "time, acce_x, acce_y, acce_z, gyro_x, gyro_y, gyro_z, private\n");
	else
		fprintf(g_fd_aag, "time, acce_x, acce_y, acce_z, lin_acce_x, lin_acce_y, lin_acce_z, gyro_x, gyro_y, gyro_z, private\n");


	// BAR sensor file
	snprintf(barfilename, 256, "%s%03d %s %s bar.dat", data_path, g_personid, g_timestring, g_unique_identifier_watch);
	dlog_print(DLOG_INFO, LOG_TAG, "Data path + bar filename: %s", barfilename);

	g_fd_bar = fopen(barfilename, "w");

	fprintf(g_fd_bar, "%03d %s %s\n", g_personid, g_unique_identifier_watch, g_timestring);
	fprintf(g_fd_bar, "time, baro, battery\n");


	// GPS sensor file
	snprintf(gpsfilename, 256, "%s%03d %s %s gps.dat", data_path, g_personid, g_timestring, g_unique_identifier_watch);
	dlog_print(DLOG_INFO, LOG_TAG, "Data path + gps filename: %s", gpsfilename);

	g_fd_gps = fopen(gpsfilename, "w");

	fprintf(g_fd_gps, "%03d %s %s\n", g_personid, g_unique_identifier_watch, g_timestring);
	fprintf(g_fd_gps, "time, latitude, longitude, accuracy, private\n");

	return;
}

static void
close_sensor_files()
{
	fclose(g_fd_aag);
	fclose(g_fd_bar);
	fclose(g_fd_gps);

	dlog_print(DLOG_INFO, LOG_TAG, "closed all sensor files");
}

/**
 *
 * @brief When the GPS position is updated this callback is called every 1-10 seconds.
 *
 * If the privacy circle is set, the GPS position is not recorded if the position is outside of
 * this circle. The base point and privacy distance can be set by the configuration file.
 *
 */

static void
write_gps_position_cb(double latitude, double longitude, double altitude, time_t timestamp, void *user_data)
{
	double time = ecore_time_unix_get();

  	location_manager_get_location(g_manager,
		&g_altitude, &g_latitude, &g_longitude,
        &g_climb, &g_direction, &g_speed,
		&g_level, &g_horizontal, &g_vertical,
		&timestamp);

    if(g_gps_base_privacy_distance != 0 &&
       g_gps_base_bound_state == LOCATIONS_BOUNDARY_IN)
    {
    	fprintf(g_fd_gps, "%0.1f,"
    		"%0.6f,%0.6f,"
    		"%0.1f,"
    		"I\n",
    		time - g_base_write_sensor_readings_time,
    		g_latitude, g_longitude,
			g_horizontal);
    }

    if(TESTING_MODE &&
       g_gps_base_privacy_distance != 0 &&
       g_gps_base_bound_state == LOCATIONS_BOUNDARY_OUT)
    {
    	fprintf(g_fd_gps, "%0.1f,"
    		"%0.6f,%0.6f,"
    		"%0.1f,"
    		"P\n",
    		time - g_base_write_sensor_readings_time,
    		g_latitude, g_longitude,
			g_horizontal);
    }

    if (g_gps_base_privacy_distance == 0 ||
       (g_gps_base_bound_state != LOCATIONS_BOUNDARY_OUT &&
        g_gps_base_bound_state != LOCATIONS_BOUNDARY_IN))
	{
    	fprintf(g_fd_gps, "%0.1f,"
    		"%0.6f,%0.6f,"
    		"%0.1f,"
    		"?\n",
    		time - g_base_write_sensor_readings_time,
    		g_latitude, g_longitude,
			g_horizontal);
    }
}

/**
 *
 * @brief Write the sensor values of gravity + linear accelerometer + gyroscope to file every x ms.
 *
 */

Eina_Bool
write_sensor_readings_cb(void *data)
{
	// Time of Watch in seconds and fraction of a second from January first of 1970 (Unix base time)
	double time = ecore_time_unix_get();

	// Set the base time for sensors and gps, if not set.
    if(g_base_write_sensor_readings_time == 0.0)
		g_base_write_sensor_readings_time = time;

    // Remove duplicates based on minimal time difference with last write (0.002 seconds)
    if(time - g_time_ < 0.002)
        return ECORE_CALLBACK_RENEW;

    g_time_ = time;

    // Remove duplicates based on identical sensor values with last write
    if( fabsf(g_acce_x - g_acce_x_) < 0.0001 &&
    	fabsf(g_acce_y - g_acce_y_) < 0.0001 &&
    	fabsf(g_acce_z - g_acce_z_) < 0.0001 &&
		fabsf(g_lin_acce_x - g_lin_acce_x_) < 0.0001 &&
		fabsf(g_lin_acce_y - g_lin_acce_y_) < 0.0001 &&
		fabsf(g_lin_acce_z - g_lin_acce_z_) < 0.0001 &&
    	fabsf(g_gyro_x - g_gyro_x_) < 0.0001 &&
    	fabsf(g_gyro_y - g_gyro_y_) < 0.0001 &&
    	fabsf(g_gyro_z - g_gyro_z_) < 0.0001 )
    	return ECORE_CALLBACK_RENEW;

    g_acce_x_ = g_acce_x;
    g_acce_y_ = g_acce_y;
    g_acce_z_ = g_acce_z;

    g_lin_acce_x_ = g_lin_acce_x;
    g_lin_acce_y_ = g_lin_acce_y;
    g_lin_acce_z_ = g_lin_acce_z;

    g_gyro_x_ = g_gyro_x;
    g_gyro_y_ = g_gyro_y;
    g_gyro_z_ = g_gyro_z;

    // If gps is set on and boundary set as well, switch in privacy mode
    if(g_gps_interval_seconds != 0 &&
       g_gps_base_privacy_distance != 0 &&
	   g_gps_base_bound_state == LOCATIONS_BOUNDARY_IN)
    {
    	if(g_lin_accelerometer_interval_ms == 0) {
    		fprintf(g_fd_aag, "%0.3f,"
    				"%0.4f,%0.4f,%0.4f,"
    				"%0.4f,%0.4f,%0.4f,"
    				"I\n",
					time - g_base_write_sensor_readings_time,
					g_acce_x, g_acce_y, g_acce_z,
					g_gyro_x, g_gyro_y, g_gyro_z);
    	}
    	else {
    		fprintf(g_fd_aag, "%0.3f,"
    				"%0.4f,%0.4f,%0.4f,"
    				"%0.4f,%0.4f,%0.4f,"
    				"%0.4f,%0.4f,%0.4f,"
    				"I\n",
					time - g_base_write_sensor_readings_time,
					g_acce_x, g_acce_y, g_acce_z,
					g_lin_acce_x, g_lin_acce_y, g_lin_acce_z,
					g_gyro_x, g_gyro_y, g_gyro_z);
    	}
    }

    if(TESTING_MODE &&
       g_gps_interval_seconds != 0 &&
       g_gps_base_privacy_distance != 0 &&
	   g_gps_base_bound_state == LOCATIONS_BOUNDARY_OUT)
    {
    	if(g_lin_accelerometer_interval_ms == 0) {
    		fprintf(g_fd_aag, "%0.3f,"
    				"%0.4f,%0.4f,%0.4f,"
    				"%0.4f,%0.4f,%0.4f,"
    				"P\n",
					time - g_base_write_sensor_readings_time,
					g_acce_x, g_acce_y, g_acce_z,
					g_gyro_x, g_gyro_y, g_gyro_z);
    	}
    	else {
    		fprintf(g_fd_aag, "%0.3f,"
    				"%0.4f,%0.4f,%0.4f,"
    				"%0.4f,%0.4f,%0.4f,"
    				"%0.4f,%0.4f,%0.4f,"
    				"P\n",
					time - g_base_write_sensor_readings_time,
					g_acce_x, g_acce_y, g_acce_z,
					g_lin_acce_x, g_lin_acce_y, g_lin_acce_z,
					g_gyro_x, g_gyro_y, g_gyro_z);
    	}
    }

    // If gps is switched off the privacy mode cannot be maintained or bound state is not defined.
    if( g_gps_interval_seconds == 0 ||
       (g_gps_base_bound_state != LOCATIONS_BOUNDARY_IN &&
	    g_gps_base_bound_state != LOCATIONS_BOUNDARY_OUT))
    {
    	if(g_lin_accelerometer_interval_ms == 0) {
    		fprintf(g_fd_aag, "%0.3f,"
    				"%0.4f,%0.4f,%0.4f,"
    				"%0.4f,%0.4f,%0.4f,"
    				"?\n",
					time - g_base_write_sensor_readings_time,
					g_acce_x, g_acce_y, g_acce_z,
					g_gyro_x, g_gyro_y, g_gyro_z);
    	}
    	else {
    		fprintf(g_fd_aag, "%0.3f,"
    				"%0.4f,%0.4f,%0.4f,"
    				"%0.4f,%0.4f,%0.4f,"
    				"%0.4f,%0.4f,%0.4f,"
    				"?\n",
					time - g_base_write_sensor_readings_time,
					g_acce_x, g_acce_y, g_acce_z,
					g_lin_acce_x, g_lin_acce_y, g_lin_acce_z,
					g_gyro_x, g_gyro_y, g_gyro_z);
    	}
    }

	return ECORE_CALLBACK_RENEW;
}

/**
 *
 * @brief Write the sensor value of the barometer to file every x ms.
 *
 */

static void
write_barometer_readings(void *data)
{
	// Time of Watch in seconds and fraction of a second from January first of 1970 (Unix base time)
	double time = ecore_time_unix_get();

	// Set the base time for sensors and gps, if not set.
    if(g_base_write_sensor_readings_time == 0.0)
		g_base_write_sensor_readings_time = time;

    // Remove duplicates based on minimal time difference with last write (0.002 seconds)
    if(time - g_time_ < 0.002)
        return;

    g_time_ = time;

    // Remove duplicates based on identical sensor values with last write
    if( fabsf(g_pressure - g_pressure_) < 0.0001 )
    	return;

    g_pressure_ = g_pressure;

    // If gps is set on and boundary set as well, switch in privacy mode
    if(g_gps_interval_seconds != 0 &&
       g_gps_base_privacy_distance != 0 &&
	   g_gps_base_bound_state == LOCATIONS_BOUNDARY_IN)
    {
    	fprintf(g_fd_bar, "%0.3f,"
    		"%0.3f,%d,"
 			"I\n",
    		time - g_base_write_sensor_readings_time,
			g_pressure, g_battery);
    }

    if(TESTING_MODE &&
       g_gps_interval_seconds != 0 &&
       g_gps_base_privacy_distance != 0 &&
	   g_gps_base_bound_state == LOCATIONS_BOUNDARY_OUT)
    {
    	fprintf(g_fd_bar, "%0.3f,"
    		"%0.3f,%d,"
 			"P\n",
    		time - g_base_write_sensor_readings_time,
			g_pressure, g_battery);
    }

    // If gps is switched off the privacy mode cannot be maintained or bound state is not defined.
    if( g_gps_interval_seconds == 0 ||
       (g_gps_base_bound_state != LOCATIONS_BOUNDARY_IN &&
	    g_gps_base_bound_state != LOCATIONS_BOUNDARY_OUT))
    {
    	fprintf(g_fd_bar, "%0.3f,"
    		"%0.3f,%d,"
 			"?\n",
    		time - g_base_write_sensor_readings_time,
			g_pressure, g_battery);
    }

	return;
}

/**
 *
 * @brief Sensoring the values.
 *
 */

static void
_get_new_accelerometer_value(sensor_h sensor, sensor_event_s *events, void *user_data)
{
	g_acce_x = events->values[0];
	g_acce_y = events->values[1];
	g_acce_z = events->values[2];

	return;
}

static void
_get_new_gyroscope_value(sensor_h sensor, sensor_event_s *events, void *user_data)
{
	g_gyro_x = events->values[0];
	g_gyro_y = events->values[1];
	g_gyro_z = events->values[2];

	return;
}

static void
_get_new_pressure_value(sensor_h sensor, sensor_event_s *events, void *user_data)
{
	device_battery_get_percent(&g_battery);
	g_pressure = events->values[0];

	write_barometer_readings(user_data);

	return;
}

static void
_get_new_linear_accelerometer_value(sensor_h sensor, sensor_event_s *events, void *user_data)
{
	g_lin_acce_x = events->values[0];
	g_lin_acce_y = events->values[1];
	g_lin_acce_z = events->values[2];

	return;
}

/**
 *
 * @brief Start a particular sensor listener.
 *
 */

static void
create_and_start_accelerometer()
{
    sensor_get_default_sensor(SENSOR_ACCELEROMETER, &g_sensor_info_accelerometer.sensor);
    sensor_create_listener(g_sensor_info_accelerometer.sensor, &g_sensor_info_accelerometer.sensor_listener);
    sensor_listener_set_event_cb(g_sensor_info_accelerometer.sensor_listener, g_accelerometer_interval_ms, (sensor_event_cb)_get_new_accelerometer_value, NULL);
    sensor_listener_set_option(g_sensor_info_accelerometer.sensor_listener, SENSOR_OPTION_ALWAYS_ON); // SENSOR_OPTION_ON_IN_POWERSAVE_MODE

    sensor_error_e err = SENSOR_ERROR_NONE;
    err = sensor_listener_start(g_sensor_info_accelerometer.sensor_listener);

    dlog_print(DLOG_INFO, LOG_TAG, "Accelerometer sensor listener started with interval %d ms %d", g_accelerometer_interval_ms, err);

    return;
}

static void
create_and_start_gyroscope()
{
	sensor_get_default_sensor(SENSOR_GYROSCOPE, &g_sensor_info_gyroscope.sensor);
    sensor_create_listener(g_sensor_info_gyroscope.sensor, &g_sensor_info_gyroscope.sensor_listener);
    sensor_listener_set_event_cb(g_sensor_info_gyroscope.sensor_listener, g_gyroscope_interval_ms, (sensor_event_cb)_get_new_gyroscope_value, NULL);
    sensor_listener_set_option(g_sensor_info_gyroscope.sensor_listener, SENSOR_OPTION_ALWAYS_ON); // SENSOR_OPTION_ON_IN_POWERSAVE_MODE

    sensor_error_e err = SENSOR_ERROR_NONE;
    err = sensor_listener_start(g_sensor_info_gyroscope.sensor_listener);

	dlog_print(DLOG_INFO, LOG_TAG, "Gyroscope sensor listener started with interval %d ms %d", g_gyroscope_interval_ms, err);

    return;
}

static void
create_and_start_barometer()
{
	sensor_get_default_sensor(SENSOR_PRESSURE, &g_sensor_info_pressure.sensor);
    sensor_create_listener(g_sensor_info_pressure.sensor, &g_sensor_info_pressure.sensor_listener);
    sensor_listener_set_event_cb(g_sensor_info_pressure.sensor_listener, g_barometer_interval_ms, (sensor_event_cb)_get_new_pressure_value, NULL);
    sensor_listener_set_option(g_sensor_info_pressure.sensor_listener, SENSOR_OPTION_ALWAYS_ON); // SENSOR_OPTION_ON_IN_POWERSAVE_MODE

    sensor_error_e err = SENSOR_ERROR_NONE;
    err = sensor_listener_start(g_sensor_info_pressure.sensor_listener);

    dlog_print(DLOG_INFO, LOG_TAG, "Pressure sensor listener started with interval %d ms %d", g_barometer_interval_ms, err);

	return;
}

static void
create_and_start_linear_accelerometer()
{
    sensor_get_default_sensor(SENSOR_LINEAR_ACCELERATION, &g_sensor_info_linear_accelerometer.sensor);
    sensor_create_listener(g_sensor_info_linear_accelerometer.sensor, &g_sensor_info_linear_accelerometer.sensor_listener);
    sensor_listener_set_event_cb(g_sensor_info_linear_accelerometer.sensor_listener, g_lin_accelerometer_interval_ms, (sensor_event_cb)_get_new_linear_accelerometer_value, NULL);
    sensor_listener_set_option(g_sensor_info_linear_accelerometer.sensor_listener, SENSOR_OPTION_ALWAYS_ON); // SENSOR_OPTION_ON_IN_POWERSAVE_MODE

    sensor_error_e err = SENSOR_ERROR_NONE;
    err = sensor_listener_start(g_sensor_info_linear_accelerometer.sensor_listener);


    dlog_print(DLOG_INFO, LOG_TAG, "Linear accelerometer sensor listener started with interval %d ms %d", g_lin_accelerometer_interval_ms, err);

	return;
}

/**
 *
 * @brief Destroy sensor listeners according to the configuration file in order to save power.
 *
 */

static void
stop_and_destroy_sensor_listeners()
{
	if(g_accelerometer_interval_ms == 0)
		sensor_destroy_listener(g_sensor_info_accelerometer.sensor_listener);

	if(g_lin_accelerometer_interval_ms == 0)
		sensor_destroy_listener(g_sensor_info_linear_accelerometer.sensor_listener);

	if(g_gyroscope_interval_ms == 0)
		sensor_destroy_listener(g_sensor_info_gyroscope.sensor_listener);

	if(g_barometer_interval_ms == 0)
		sensor_destroy_listener(g_sensor_info_pressure.sensor_listener);

	return;
}

/**
 *
 * @brief Create, start, stop and destroy write timer which writes the sensor values to the sensor files.
 *
 */

static void
create_and_start_write_timer()
{
	g_timer_id = ecore_timer_add(START_DELAY_SENSOR_WRITE, write_sensor_readings_cb, NULL);
	ecore_timer_interval_set(g_timer_id, g_write_interval_seconds);

	dlog_print(DLOG_INFO, LOG_TAG, "Sensor timer writer started with interval %0.3f seconds", g_write_interval_seconds);

	return;
}

static void
stop_and_destroy_write_timer()
{
	ecore_timer_del(g_timer_id);

	dlog_print(DLOG_INFO, LOG_TAG, "Sensor timer writer deleted");

	return;
}

/**
 *
 * @brief The callback is called when the watch in entering of leaving the circle.
 *
 * It produces a state which can be LOCATIONS_BOUNDARY_IN (entering the circle) or _OUT (leaving the circle).
 *
 */

static void
enter_or_leave_measurement_circle_cb(location_boundary_state_e state, void *user_data)
{
    g_gps_base_bound_state = state;
}

static void
set_gps_base_privacy_distance()
{
	location_coords_s center;
	center.latitude = g_gps_base_point_latitude;
	center.longitude = g_gps_base_point_longitude;

	location_bounds_create_circle(center, g_gps_base_privacy_distance, &g_gps_base_bounds);
	location_bounds_set_state_changed_cb(g_gps_base_bounds, enter_or_leave_measurement_circle_cb, NULL);

	location_manager_add_boundary(g_manager, g_gps_base_bounds);

	return;
}

static void
unset_gps_base_privacy_distance()
{
	location_bounds_destroy(g_gps_base_bounds);

	return;
}

/**
 *
 * @brief Create, start, stop and destroy GPS sensors
 *
 */

static void
create_and_start_gps()
{
    location_manager_create(LOCATIONS_METHOD_GPS, &g_manager); // LOCATIONS_METHOD_HYBRID -> results in instable aga values
    location_manager_set_position_updated_cb(g_manager, write_gps_position_cb, g_gps_interval_seconds, NULL);

    if(g_gps_base_privacy_distance != 0)
    	set_gps_base_privacy_distance();
    else
    	unset_gps_base_privacy_distance();

    sensor_error_e err = SENSOR_ERROR_NONE;
    err = location_manager_start(g_manager);

    // If location connection on settings watch is off, do not set a privacy circle
    if(err<0)
    	g_gps_base_privacy_distance = 0;

	dlog_print(DLOG_INFO, LOG_TAG, "GPS sensor manager started with interval %d seconds %d", g_gps_interval_seconds, err);

    return;
}

static void
stop_and_destroy_gps()
{
	unset_gps_base_privacy_distance();

	location_manager_destroy(g_manager);
	g_manager = NULL;

	dlog_print(DLOG_INFO, LOG_TAG, "GPS sensor manager stopped and destroyed");

	return;
}

/**
 *
 * @brief Start or stop all sensors based on the configuration file.
 *
 */
static void
start_sensors()
{
    if(g_barometer_interval_ms != 0)
    	create_and_start_barometer();

    if(g_accelerometer_interval_ms != 0)
    	create_and_start_accelerometer();

    if(g_lin_accelerometer_interval_ms != 0)
    	create_and_start_linear_accelerometer();

    if(g_gyroscope_interval_ms != 0)
    	create_and_start_gyroscope();

    if(g_gps_interval_seconds != 0)
    	create_and_start_gps();

    if(g_write_interval_seconds > 0.0)
    	create_and_start_write_timer();

	// Let CPU run independent of display mode (do not terminate after power saving on).
	device_power_request_lock(POWER_LOCK_CPU, 0); // TODO: tests show this has no effect, test separately

	return;
}

static void
stop_sensors()
{
	if(g_write_interval_seconds > 0.0)
	    stop_and_destroy_write_timer();

	if(g_accelerometer_interval_ms != 0)
		sensor_destroy_listener(g_sensor_info_accelerometer.sensor_listener);

	if(g_lin_accelerometer_interval_ms != 0)
		sensor_destroy_listener(g_sensor_info_linear_accelerometer.sensor_listener);

	if(g_gyroscope_interval_ms != 0)
		sensor_destroy_listener(g_sensor_info_gyroscope.sensor_listener);

	if(g_barometer_interval_ms != 0)
		sensor_destroy_listener(g_sensor_info_pressure.sensor_listener);

	if(g_gps_interval_seconds != 0)
		stop_and_destroy_gps();

	return;
}

/**
 *
 * @brief Pause/resume the main timer, location manager and sensor listeners temporary.
 *
 */

static void
pause_sensors()
{
	ecore_timer_freeze(g_timer_id);

	location_manager_stop(g_manager);

	sensor_listener_stop(g_sensor_info_linear_accelerometer.sensor_listener);
	sensor_listener_stop(g_sensor_info_accelerometer.sensor_listener);
	sensor_listener_stop(g_sensor_info_gyroscope.sensor_listener);
	sensor_listener_stop(g_sensor_info_pressure.sensor_listener);

	return;
}

static void
resume_sensors()
{
	// Resume from pause the main timer and location manager
	sensor_listener_stop(g_sensor_info_linear_accelerometer.sensor_listener);
	sensor_listener_stop(g_sensor_info_accelerometer.sensor_listener);
	sensor_listener_stop(g_sensor_info_gyroscope.sensor_listener);
	sensor_listener_stop(g_sensor_info_pressure.sensor_listener);

	location_manager_start(g_manager);
	ecore_timer_thaw(g_timer_id);

	return;
}

static void
pause_sensors_and_close_sensor_files()
{
	pause_sensors();
	sleep(1);
	close_sensor_files();

	return;
}

static void
resume_sensors_and_open_new_sensor_files()
{
	open_new_sensor_files();
	resume_sensors();

	return;
}

/**
 *
 * @brief Standard service application callbacks.
 *
 */

bool service_app_create(void *data)
{
	dlog_print(DLOG_INFO, LOG_TAG, "SensorService created");

    return true;
}

void service_app_terminate(void *data)
{
	dlog_print(DLOG_INFO, LOG_TAG, "SensorService terminated");

	pause_sensors_and_close_sensor_files();

	sleep(1); // wait 1 second for closing files

    return;
}

/**
 *
 * @brief Process the restart message sent by the sensor application.
 *
 */

static void
process_restart_message()
{
    // Validate patient identifier, if wrong set to 000
    if(g_personid > 999)
    {
    	g_personid = 0;
        dlog_print(DLOG_ERROR, LOG_TAG, "Patient identifier was out of range. Set to %03d", g_personid);
    }

    dlog_print(DLOG_INFO, LOG_TAG, "Patient identifier = %03d", g_personid);

    if( g_service_state == WAITING )
    {
    	read_configuration_file();
    	open_new_sensor_files();
    	write_configuration_file();
    	start_sensors();

    	vibrate();

    	g_service_state = MEASURING;

    	dlog_print(DLOG_INFO, LOG_TAG, "Measurement started with patient identifier = %03d", g_personid);

    	return;
    }

    if( g_service_state == MEASURING )
    {
    	// Pause current measurement, start new measurement with new person identifier
    	stop_sensors();
    	close_sensor_files();

    	read_configuration_file();
    	open_new_sensor_files();
    	write_configuration_file();
    	start_sensors();

    	vibrate();

    	g_service_state = MEASURING;

    	dlog_print(DLOG_INFO, LOG_TAG, "Measurement stopped and restarted with patient identifier = %03d", g_personid);

    	return;
    }
}

/**
 *
 * @brief Process the clean message sent by the sensor application.
 *
 * All sensor- and config files are deleted found in the data folder of the sensor service.
 *
 */

static void
process_clean_message()
{
	if( g_service_state == MEASURING ) {
		pause_sensors_and_close_sensor_files();
		sleep(1);
	}

	char* data_path = NULL;
	data_path = app_get_data_path();

	char linux_command[256];
	snprintf(linux_command, 256, "rm %s*aag.dat", data_path);
	dlog_print(DLOG_INFO, LOG_TAG, "Linux: %s", linux_command);
	system(linux_command);

	snprintf(linux_command, 256, "rm %s*bar.dat", data_path);
	dlog_print(DLOG_INFO, LOG_TAG, "Linux: %s", linux_command);
	system(linux_command);

	snprintf(linux_command, 256, "rm %s*con.dat", data_path);
	dlog_print(DLOG_INFO, LOG_TAG, "Linux: %s", linux_command);
	system(linux_command);

	snprintf(linux_command, 256, "rm %s*gps.dat", data_path);
	dlog_print(DLOG_INFO, LOG_TAG, "Linux: %s", linux_command);
	system(linux_command);

	if( g_service_state == MEASURING )
		resume_sensors_and_open_new_sensor_files();

	vibrate();

	dlog_print(DLOG_INFO, LOG_TAG, "Sensor files deleted");

	return;
}

/**
 *
 * @brief In the call back service_app_control the message requests are handled.
 *
 * @detailed Message which can be received are:
 *
 * 1. Launch request to start the sensor service. This message can be ignored.
 * 2. Restart request with person identifier. This must restart the measurement.
 * 3. Clean request. This must delete all sensor files.
 *
 */

void service_app_control(app_control_h app_control, void *user_data)
{
    // Handle the request of another application.
	dlog_print(DLOG_INFO, LOG_TAG, "SensorService controlled");

	char *operation;
    char *uri;
    char *app_id;

    app_control_get_operation(app_control, &operation);
	dlog_print(DLOG_INFO, LOG_TAG, "SensorService control requested with operation %s", operation);

	app_control_get_uri(app_control, &uri);
	dlog_print(DLOG_INFO, LOG_TAG, "SensorService control requested with uri %s", uri);

	app_control_get_app_id(app_control, &app_id);
	dlog_print(DLOG_INFO, LOG_TAG, "SensorService control requested with appid %s", app_id);

    // Check if the request is restart or clean
    if (!strcmp(operation, APP_CONTROL_OPERATION_SEND))
    {
    	char command[16];
    	char parameter[16];

    	sscanf(uri, "%s %s", &command, &parameter);

    	if(!strcmp("restart", command)) {
    		sscanf(parameter, "%d", &g_personid);
    		process_restart_message();
    		return;
    	}

    	if(!strcmp("clean", command)) {
    		process_clean_message();
    		return;
    	}
    }

    return;
}

/**
 *
 * @brief Low battery and memory callbacks called by the Tizen framework.
 *
 */

static void
service_app_low_battery(app_event_info_h event_info, void *user_data)
{
	// APP_EVENT_LOW_BATTERY
	dlog_print(DLOG_INFO, LOG_TAG, "SensorService low battery");

	pause_sensors_and_close_sensor_files();

	return;
}

static void
service_app_low_memory(app_event_info_h event_info, void *user_data)
{
	// APP_EVENT_LOW_MEMORY
	dlog_print(DLOG_INFO, LOG_TAG, "SensorService low memory");

	pause_sensors_and_close_sensor_files();

	return;
}

/**
 *
 * @brief Check for support of particular sensor.
 *
 */

static void
check_supported_sensors()
{
    sensor_error_e err = SENSOR_ERROR_NONE;
    sensorinfo_s sensor_info;

    err = sensor_get_default_sensor(SENSOR_ACCELEROMETER, &sensor_info.sensor);
    dlog_print(DLOG_INFO, LOG_TAG, "Accelerometer sensor support = %d", err);

    char *vendor;
    err = sensor_get_vendor(sensor_info.sensor, &vendor);
    dlog_print(DLOG_INFO, LOG_TAG, "Accelerometer sensor vendor = %s %d", vendor, err);

    err = sensor_get_default_sensor(SENSOR_LINEAR_ACCELERATION, &sensor_info.sensor);
    dlog_print(DLOG_INFO, LOG_TAG, "Linear accelerometer sensor support = %d", err);

    err = sensor_get_default_sensor(SENSOR_GYROSCOPE, &sensor_info.sensor);
    dlog_print(DLOG_INFO, LOG_TAG, "Gyroscope sensor support = %d", err);

    err = sensor_get_default_sensor(SENSOR_PRESSURE, &sensor_info.sensor);
    dlog_print(DLOG_INFO, LOG_TAG, "Barometer sensor support = %d", err);

    err = sensor_get_default_sensor(SENSOR_TEMPERATURE, &sensor_info.sensor);
    dlog_print(DLOG_INFO, LOG_TAG, "Temperature sensor support = %d", err);

	err = sensor_get_default_sensor(SENSOR_MAGNETIC, &sensor_info.sensor);
	dlog_print(DLOG_INFO, LOG_TAG, "Magnetic sensor support = %d", err);

	err = sensor_get_default_sensor(SENSOR_HUMIDITY, &sensor_info.sensor);
	dlog_print(DLOG_INFO, LOG_TAG, "Humidity sensor support = %d", err);

	err = sensor_get_default_sensor(SENSOR_PROXIMITY, &sensor_info.sensor);
	dlog_print(DLOG_INFO, LOG_TAG, "Proximity sensor support = %d", err);

	err = sensor_get_default_sensor(SENSOR_PROXIMITY_NEAR, &sensor_info.sensor);
	dlog_print(DLOG_INFO, LOG_TAG, "Proximity near sensor support = %d", err);

	err = sensor_get_default_sensor(SENSOR_PROXIMITY_FAR, &sensor_info.sensor);
	dlog_print(DLOG_INFO, LOG_TAG, "Proximity far sensor support = %d", err);

    return;
}

/**
 *
 * Start of the sensor service application
 *
 */

int main(int argc, char* argv[])
{
    check_supported_sensors();

    char ad[50] = {0,};
	service_app_lifecycle_callback_s event_callback;
	app_event_handler_h handlers[5] = {NULL, };

	event_callback.create = service_app_create;
	event_callback.terminate = service_app_terminate;
	event_callback.app_control = service_app_control;

	service_app_add_event_handler(&handlers[APP_EVENT_LOW_BATTERY], APP_EVENT_LOW_BATTERY, service_app_low_battery, &ad);
	service_app_add_event_handler(&handlers[APP_EVENT_LOW_MEMORY], APP_EVENT_LOW_MEMORY, service_app_low_memory, &ad);

	return service_app_main(argc, argv, &event_callback, ad);
}
