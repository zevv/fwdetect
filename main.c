#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>

#include "biquad.h"

struct sample {
	float v[2];
};

static const float srate = 8000.0;
static const float buf_time = 0.05;
static const float rec_duration = 3.0;
static const float rec_threshold = -70.0;
static const float rec_threshold_delta = 40.0;
static const char *amixer_control = "Mic";
static const float amixer_level = 40.0;
static const float filter_hp_freq = 30.0;

static bool do_graph = true;
struct biquad filter[2];

static float rec_timer = 0.0;
static FILE *rec_fd = NULL;
static FILE *f_log = NULL;


void debug(const char *msg, ...)
{
	if(f_log == NULL) {
		printf("open\n");
		f_log = fopen("fwdetect.log", "a");
	}

	char ts[64];
	time_t now = time(NULL);
	strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));
	va_list args;
	va_start(args, msg);

	fprintf(stderr, "%s ", ts);
	vfprintf(stderr, msg, args);
	fprintf(stderr, "\e[K\n");

	fprintf(f_log, "%s ", ts);
	vfprintf(f_log, msg, args);
	fprintf(f_log, "\n");
	fflush(f_log);

	va_end(args);
}


void rec_start(void)
{
	if(rec_fd == NULL) {
		char fname[256];
		time_t now = time(NULL);
		strftime(fname, sizeof(fname), "rec/%Y%m%d-%H%M%S", localtime(&now));
		rec_fd = fopen(fname, "w");
		debug("rec start %s", fname);
	}
}


void rec_stop(void)
{
	if(rec_fd) {
		fclose(rec_fd);
		rec_fd = NULL;
		debug("rec stop");
	}
}


void rec_write(struct sample *sample, int nsamples)
{
	if(rec_fd) {
		fwrite((void *)sample, sizeof(*sample), nsamples, rec_fd);
	}
}


float find_peak(struct sample *sample, int nsamples)
{
	float max = 0.0;

	for(int i = 0; i < nsamples; i++) {
		float amp = fabs(sample[i].v[0]);
		if(amp > max) max = amp;
	}

	float level = 20.0 * logf(max);
	return level;
}


void process(struct sample *sample, int nsamples)
{
	// High pass filter to reduce wind noise
	for(int i=0; i<nsamples; i++) {
		for(int j=0; j<2; j++) {
			sample[i].v[j] = biquad_run(&filter[j], sample[i].v[j]);
		}
	}

	static float level_prev = 0.0;
	float level = find_peak(sample, nsamples);
	float level_delta = level - level_prev;
	level_prev = level;

	if(level > rec_threshold && level_delta > rec_threshold_delta) {
		debug("boom abs: %1f dB, rel %.1f dB", level, level_delta);
		rec_start();
		rec_timer = rec_duration;
	}


	if(rec_timer > 0.0) {
		rec_timer -= buf_time;
		if(rec_timer <= 0) {
			rec_stop();
			rec_timer = 0;
		}
	}

	rec_write(sample, nsamples);

	if(do_graph) {
		printf("%+4.0f %+4.0f [", level, level_delta);
		for(int i=-120; i<0; i+=2) {
			putchar((i < level) ? '*' : ' ');
		}
		printf("]\r");
		fflush(stdout);
	}
}


int main(int argc, char **argv)
{
	debug("start");

	char cmd[256];
	snprintf(cmd, sizeof(cmd), "amixer -q -c1 sset %s %.1f", amixer_control, amixer_level);
	system(cmd);
	
	for(int i=0; i<2; i++) {
		biquad_init(&filter[i], srate);
		biquad_config(&filter[i], BIQUAD_TYPE_HP, filter_hp_freq, 0.737);
	}

	snprintf(cmd, sizeof(cmd), "parec --channels=2 --format=float32 --rate=%d --latency-msec=10 --process-time-msec=10", (int)srate);
	FILE *f = popen(cmd, "r");

	int nsamples = srate * buf_time;
	struct sample sample[nsamples];

	for(;;) {
		int r = fread((void *)sample, sizeof(sample), 1, f);
		if(r != 1) break;

		process(sample, nsamples);
	}

	return 0;
}

