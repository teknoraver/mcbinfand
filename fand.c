/*
 * mcbinfand - fan daemon for the MACCHIATObin board
 * Copyright (C) 2020 Matteo Croce <mcroce@microsoft.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <time.h>
#include <gpiod.h>

#define GPIO_DEV "/dev/gpiochip1"
#define THERM_PATH "/sys/class/thermal/thermal_zone0/temp"

#define MIN_TEMP 40000
#define MAX_TEMP 55000

/* 1 kHz consumes around 0.7% of a CPU */
#define PWM_USECS 1000

// #define DEBUG

static jmp_buf loopjmp;

static void sighandler(int _unused)
{
	longjmp(loopjmp, 1);
}

static int read_temp()
{
	char buf[16] = { 0 };
	static FILE *ftemp;
	int temp;

	if (!ftemp)
		ftemp = fopen(THERM_PATH, "r");

	/* Protect from burning if temp can't be read */
	if (!ftemp)
		return MAX_TEMP;

	fseek(ftemp, 0, SEEK_SET);
	fread(buf, 1, sizeof(buf), ftemp);

	/* Same here */
	if (ferror(ftemp)) {
		perror("fread");
		fclose(ftemp);
		ftemp = NULL;
		return MAX_TEMP;
	}
	temp = atoi(buf);

	return temp;
}

static int duty_cycle(int temp)
{
	if (temp < MIN_TEMP)
		return 0;

	if (temp >= MAX_TEMP)
		return PWM_USECS;

	/* Scale the duty cycle from MIN_TEMP to MAX_TEMP */
	return (temp - MIN_TEMP) * PWM_USECS / (MAX_TEMP - MIN_TEMP);
}

static struct gpiod_line_request * get_request()
{
	struct gpiod_request_config *req_cfg;
	struct gpiod_line_settings *settings;
	struct gpiod_line_request *request;
	struct gpiod_line_config *line_cfg;
	struct gpiod_chip *chip;
	unsigned int offset = 16;
	int ret;

	chip = gpiod_chip_open(GPIO_DEV);
	if (!chip) {
		perror("gpiod_chip_open");
		return NULL;
	}

	settings = gpiod_line_settings_new();
	if (!settings) {
		perror("gpiod_line_settings_new");
		return NULL;
	}

	gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
	gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_ACTIVE);

	line_cfg = gpiod_line_config_new();
	if (!line_cfg) {
		perror("gpiod_line_config_new");
		return NULL;
	}

	ret = gpiod_line_config_add_line_settings(line_cfg, &offset, 1, settings);
	if (ret) {
		perror("gpiod_line_config_add_line_settings");
		return NULL;
	}

	req_cfg = gpiod_request_config_new();
	if (!req_cfg) {
		perror("gpiod_request_config_new");
		return NULL;
	}

	gpiod_request_config_set_consumer(req_cfg, "fand");

	request = gpiod_chip_request_lines(chip, req_cfg, line_cfg);
	if (!request) {
		perror("gpiod_chip_get_line");
		gpiod_chip_close(chip);
		return NULL;
	}

	gpiod_request_config_free(req_cfg);
	gpiod_line_config_free(line_cfg);
	gpiod_line_settings_free(settings);
	gpiod_chip_close(chip);

	return request;
}

int main(int argc, char *argv[])
{
	struct gpiod_line_request *request;
	struct timespec oldtp;
	int off = 0;
	int on = PWM_USECS;

	request = get_request();

	signal(SIGINT, sighandler);
	signal(SIGQUIT, sighandler);
	signal(SIGTERM, sighandler);

	clock_gettime(CLOCK_MONOTONIC, &oldtp);

	while (!setjmp(loopjmp)) {
		struct timespec now;

		clock_gettime(CLOCK_MONOTONIC, &now);
		if (now.tv_sec > oldtp.tv_sec) {
			int temp = read_temp();

			oldtp = now;
			on = duty_cycle(temp);
			off = PWM_USECS - on;

#ifdef DEBUG
			printf("temp: %0.1f, duty cycle: %d%%: (%d on, %d off)\n",
			       temp / 1000.0, on * 100 / PWM_USECS, on, off);
#endif
		}

		if (on) {
			gpiod_line_request_set_value(request, off, GPIOD_LINE_VALUE_ACTIVE);
			usleep(on);
		}
		if (off) {
			gpiod_line_request_set_value(request, off, GPIOD_LINE_VALUE_INACTIVE);
			usleep(off);
		}
	}

	puts("exiting...");

	/* Don't leave the fan off and burn the CPU */
	usleep(100000);
	gpiod_line_request_set_value(request, off, GPIOD_LINE_VALUE_ACTIVE);

	return 0;
}
