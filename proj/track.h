#ifndef _TRACK_H
#define _TRACK_H

#include <stdlib.h>
#include "vector2D.h"
#include <stdbool.h>
#include "math.h"
#include "video_gr.h"
#include <stdbool.h>

#define TRACK_INTERP_PERIOD	0.07

typedef struct
{
	bool *track_points;
	unsigned width;
	unsigned height;
	vector2D_t* spline;
	vector2D_t* inside_spline;
	vector2D_t* outside_spline;
	unsigned spline_size;
	vector2D_t* control_points;
	unsigned num_control_points;
} track_t;

track_t *track_generate(unsigned width, unsigned height, unsigned long seed);

void track_draw(track_t *track, unsigned width, unsigned height);

double track_get_point_drag(track_t *track, int x, int y, unsigned width, unsigned height);

void track_delete(track_t *track);

#endif
