#ifndef _PROJETO_H
#define _PROJETO_H

#include <stdlib.h>
#include <time.h>
#include <stdbool.h>		// Code gets much more beautiful with true/false instead of (!=0)/0
#include <minix/drivers.h>
#include "math.h"
#include "video_gr.h"
#include "vbe.h"
#include "vector2D.h"
#include "keyboard.h"
#include "vehicle.h"
#include "timer.h"

#define FPS				12
#define INTERP_PERIOD	0.07f

int racinix_start(vbe_mode_info_t *vmi);
int racinix_exit();
int racinix_start_race();
void generate_track(unsigned x, unsigned y, unsigned width, unsigned height);
int orientation(vector2D_t p, vector2D_t q, vector2D_t r);
int convexHull(vector2D_t points[], unsigned n, vector2D_t hull[]);
bool isLeft( vector2D_t P0, vector2D_t P1, vector2D_t P2 );
void swapPoints(vector2D_t *a, int i, int j);
int partition(vector2D_t *a, int left, int right, int pivot);
void quickSort(vector2D_t *a, int left, int right);
vector2D_t createCatmullRomSpline(vector2D_t P0, vector2D_t P1, vector2D_t P2, vector2D_t P3, double t);
double calculateCatmullCoordinate(double P0, double P1, double P2, double P3, double t);
double calculateCatmullDerivativeCoordinate(double P0, double P1, double P2, double P3, double t);
void pushApart(vector2D_t hull[], unsigned hull_size);
vector2D_t calculateCatmullNormal(vector2D_t P0, vector2D_t P1, vector2D_t P2, vector2D_t P3, double t);

#endif