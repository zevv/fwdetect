#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <time.h>

struct sample {
	float v[2];
};

static const float srate = 8000.0;
static const float buf_time = 0.05;
static const float rec_duration = 3.0;
static const float rec_threshold = 20.0;

static float rec_timer = 0.0;
static FILE *rec_fd = NULL;


void debug(const char *msg, ...)
{
	char ts[64];
	time_t now = time(NULL);
	strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));
	fprintf(stderr, "%s ", ts);
	va_list args;
	va_start(args, msg);
	vfprintf(stderr, msg, args);
	va_end(args);

}


void rec_start(void)
{
	if(rec_fd == NULL) {
		char fname[256];
		time_t now = time(NULL);
		strftime(fname, sizeof(fname), "rec/%Y%m%d-%H%M%S", localtime(&now));
		rec_fd = fopen(fname, "w");
		debug("rec start %s\n", fname);
	}
}


void rec_stop(void)
{
	if(rec_fd) {
		fclose(rec_fd);
		rec_fd = NULL;
		debug("rec stop\n");
	}
}


void rec_write(struct sample *sample, int nsamples)
{
	if(rec_fd) {
		fwrite((void *)sample, sizeof(sample), nsamples, rec_fd);
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
	static float level_prev = 0.0;
	float level = find_peak(sample, nsamples);
	float level_delta = level - level_prev;
	level_prev = level;

	if(level_delta > rec_threshold) {
		debug("boom %.1f dB\n", level_delta);
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


	if(rec_fd) {
		printf("%.2f %+3.0f ", rec_timer, level);
		for(int i=-100; i<0; i++) {
			putchar((i < level) ? '*' : ' ');
		}
		printf("\n");
		fflush(stdout);
	}
}


int main(int argc, char **argv)
{
	char cmd[256];
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

