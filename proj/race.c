#include "race.h"
#include <string.h>
#include "sys/times.h"

#define PI 					3.14159265358979323846

static void race_update_vehicle(race_t *race, vehicle_t *vehicle, double delta_time);
static void race_show_info(race_t *race, unsigned fps);

race_t *race_create(track_t *track, unsigned num_players, bitmap_t **vehicle_bitmaps, vehicle_keys_t *vehicle_keys, uint16_t *vehicle_colors, double freeze_time, unsigned num_laps, vbe_mode_info_t *vbe_mode_info, font_t *font)
{
	race_t *race;
	if ((race = malloc(sizeof(race_t))) == NULL)
	{
		return NULL;
	}
	if ((race->vehicles = malloc(num_players * sizeof(vehicle_t *))) == NULL)
	{
		race_delete(race);
		return NULL;
	}
	race->track = track;
	race->num_players = num_players;
	race->vehicle_bitmaps = vehicle_bitmaps;
	race->vehicle_keys = vehicle_keys;
	race->vehicle_colors = vehicle_colors;
	race->time = -freeze_time;
	race->num_laps = num_laps;
	race->vbe_mode_info = vbe_mode_info;
	race->font = font;
	return race;
}

int race_start(race_t *race)
{
	vector2D_t starting_position_increment = vectorDivide(vectorSubtract(race->track->outside_spline[0], race->track->inside_spline[0]), race->num_players + 1);
	vector2D_t starting_position_offset, temp_vector;
	vector2D_t starting_position;
	double heading = atan2(race->track->spline[0].y - race->track->spline[race->track->spline_size - 1].y, race->track->spline[0].x - race->track->spline[race->track->spline_size - 1].x);
	temp_vector = vectorRotate(starting_position_increment, PI / 2);
	normalize(&temp_vector);
	size_t i;
	for (i = 0; i < race->num_players; ++i)
	{
		starting_position_offset = vectorMultiply(temp_vector, -VEHICLE_LENGTH / 2);
		starting_position = vectorAdd(vectorAdd(race->track->inside_spline[0], vectorMultiply(starting_position_increment, i + 1)), starting_position_offset);
		race->vehicles[i] = vehicle_create(VEHICLE_WIDTH, VEHICLE_LENGTH, &starting_position, heading, race->vehicle_bitmaps[i], race->vehicle_keys[i], race->vehicle_colors[i]);
	}
	return 0;
}

int race_tick(race_t *race, double delta_time, unsigned fps)
{
	vg_swap_mouse_buffer();
	vg_fill(RACINIX_COLOR_GRASS);
	track_draw(race->track);
	size_t i;
	if (race->num_players == 2 && race->vehicles[0]->current_checkpoint == race->vehicles[1]->current_checkpoint) // Same checkpoint
	{
		vg_draw_circle(race->track->control_points[race->vehicles[0]->current_checkpoint].x, race->track->control_points[race->vehicles[1]->current_checkpoint].y, 5, race->vehicles[0]->checkpoint_color | race->vehicles[1]->checkpoint_color);
	}
	else
	{
		for (i = 0; i < race->num_players; ++i)
		{
			vg_draw_circle(race->track->control_points[race->vehicles[i]->current_checkpoint].x, race->track->control_points[race->vehicles[i]->current_checkpoint].y, 5, race->vehicles[i]->checkpoint_color);
		}
	}
	for (i = 0; i < race->num_players; ++i)
	{
		race_update_vehicle(race, race->vehicles[i], delta_time);
	}

	// Vehicle-vehicle collision
	unsigned wheel_ID;
	size_t j;
	for (i = 0; i < race->num_players; ++i)
	{
		for (j = 0; j < race->num_players; ++j)
		{
			if (i != j)
			{
				wheel_ID = vehicle_check_vehicle_collision(race->vehicles[i], race->vehicles[j]);
				if (wheel_ID != -1)
				{
					vehicle_vehicle_collision_handler(race->vehicles[i], wheel_ID, race->vehicles[j]);
				}
			}
		}
	}
	race_show_info(race, fps);
	vg_swap_buffer();
	race->time += delta_time;
}

void race_delete(race_t *race)
{
	free(race->vehicles);
	race->vehicles = NULL;
	free(race);
	race = NULL;
}

static void race_update_vehicle(race_t *race, vehicle_t *vehicle, double delta_time)
{
	double drag = VEHICLE_DRAG;
	size_t i;
	for(i = 0; i < VEHICLE_NUM_WHEELS; ++i)
	{
		drag += track_get_point_drag(race->track, (int)vehicle->wheels[i].x, (int)vehicle->wheels[i].y, race->vbe_mode_info->XResolution, race->vbe_mode_info->YResolution);
	}
	vehicle_tick(vehicle, race->track, race->vbe_mode_info, delta_time, drag);
}

static void race_show_info(race_t *race, unsigned fps)
{
	char string[100];
	sprintf(string, "%d", (int)floor(race->time));
	font_show_string(race->font, string, 10, race->vbe_mode_info->XResolution - 11, 11, FONT_ALIGNMENT_RIGHT, VIDEO_GR_WHITE, 2);
	sprintf(string, "FPS: %d", fps);
	font_show_string(race->font, string, 20, 11, race->vbe_mode_info->YResolution - 31, FONT_ALIGNMENT_LEFT, VIDEO_GR_WHITE, 2);
}
