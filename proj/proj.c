#include "proj.h"

#define MOUSE_NUM_TRIES		10
#define MOUSE_HOOK_BIT	12
#define PI 					3.14159265358979323846

#define BIT(n) (0x01<<(n))

static vector2D_t mouse_position;
static vbe_mode_info_t vmi;
static race_t *race;
static vehicle_keys_t vehicle_keys[2];
static uint16_t vehicle_colors[2];
static bitmap_t *vehicle_bitmaps[2];
static unsigned num_players;
static bool serial_port;

// Bitmaps
static bitmap_t *background;
static bitmap_t *mouse_cursor;
static bitmap_t *logo;
static bitmap_t *bitmap_red_car;
static bitmap_t *bitmap_blue_car;
static bitmap_t *bitmap_credits;

// Fonts
static font_t *font_impact;

int main(int argc, char **argv) {

	/* Initialize service */
	sef_startup();

	if (racinix_start())
	{
		racinix_exit();
		printf("Racinix: An error occurred and the program was stopped.\n");
	}

	racinix_exit();

	return 0;
}

int racinix_start()
{
	srand(time(NULL));
	//vg_exit(); // Fix darker colors first time the mode is changed.
	vg_init(RACINIX_VIDEO_MODE);
	vbe_get_mode_info(RACINIX_VIDEO_MODE, &vmi);

	mouse_position = vectorCreate(vmi.XResolution / 2, vmi.YResolution / 2);

	background = bitmap_load(RACINIX_FOLDER_IMAGES "background.bmp");
	if (background == NULL)
	{
		return 1;
	}

	mouse_cursor = bitmap_load(RACINIX_FOLDER_IMAGES "cursor.bmp");
	if (mouse_cursor == NULL)
	{
		return 1;
	}

	logo = bitmap_load(RACINIX_FOLDER_IMAGES "logo.bmp");
	if (logo == NULL)
	{
		return 1;
	}

	bitmap_red_car = bitmap_load(RACINIX_FOLDER_IMAGES "red_car.bmp");
	if (bitmap_red_car == NULL)
	{
		return 1;
	}

	bitmap_blue_car = bitmap_load(RACINIX_FOLDER_IMAGES "blue_car.bmp");
	if (bitmap_blue_car == NULL)
	{
		return 1;
	}

	bitmap_credits = bitmap_load(RACINIX_FOLDER_IMAGES "credits.bmp");
	if (bitmap_credits == NULL)
	{
		return 1;
	}

	font_impact = font_load(RACINIX_FOLDER_FONTS "impact");
	if (font_impact == NULL)
	{
		return 1;
	}

	vehicle_keys[0].accelerate = KEY_W;
	vehicle_keys[0].brake = KEY_S;
	vehicle_keys[0].turn_left = KEY_A;
	vehicle_keys[0].turn_right = KEY_D;
	vehicle_keys[0].nitrous = KEY_L_CTRL;

	vehicle_keys[1].accelerate = KEY_ARR_UP;
	vehicle_keys[1].brake = KEY_ARR_DOWN;
	vehicle_keys[1].turn_left = KEY_ARR_LEFT;
	vehicle_keys[1].turn_right = KEY_ARR_RIGHT;
	vehicle_keys[1].nitrous = KEY_R_CTRL;

	vehicle_colors[0] = VIDEO_GR_RED;
	vehicle_colors[1] = VIDEO_GR_BLUE;

	vehicle_bitmaps[0] = bitmap_red_car;
	vehicle_bitmaps[1] = bitmap_blue_car;

	if (racinix_dispatcher())
	{
		return 1;
	}
	return 0;
}

int racinix_exit()
{
	bitmap_delete(background);
	bitmap_delete(mouse_cursor);
	bitmap_delete(logo);
	bitmap_delete(bitmap_red_car);
	bitmap_delete(bitmap_blue_car);
	bitmap_delete(bitmap_credits);
	font_delete(font_impact);
	return vg_exit();
}

int racinix_dispatcher()
{
	unsigned mouse_hook_id = MOUSE_HOOK_BIT;
	if (mouse_subscribe_int(&mouse_hook_id) == -1)
	{
		return 1;
	}

	if (mouse_set_stream_mode(MOUSE_NUM_TRIES))
	{
		return 1;
	}

	if (mouse_enable_stream_mode(MOUSE_NUM_TRIES))
	{
		return 1;
	}

	mouse_discard_interrupts(MOUSE_NUM_TRIES, MOUSE_HOOK_BIT);

	if (keyboard_subscribe_int() == -1)
	{
		return 1;
	}

	unsigned char timer_hook_bit;
	if ((timer_hook_bit = timer_subscribe_int()) < 0)
	{
		return 1;
	}

	unsigned serial_hook_id = SERIAL_HOOK_BIT;
	if (serial_subscribe_int(&serial_hook_id, 1, 2) == -1)
	{
		return 1;
	}

	mouse_data_packet_t old_mouse_data_packet, new_mouse_data_packet;
	old_mouse_data_packet.left_button = old_mouse_data_packet.middle_button = old_mouse_data_packet.right_button = false;
	int r, ipc_status;
	unsigned fps_counter;
	message msg;
	key_t key;
	while(1) // Main loop
	{
		// Get a request message.
		if ((r = driver_receive(ANY, &msg, &ipc_status)) != 0) {
			// Driver receive fail
			continue;
		}
		if (is_ipc_notify(ipc_status)) { // received notification
			if (_ENDPOINT_P(msg.m_source) == HARDWARE) // hardware interrupt notification
			{
				if (msg.NOTIFY_ARG & BIT(KEYBOARD_HOOK_BIT)) {

					if ((key = racinix_keyboard_int_handler()) == -1)
					{
						return 1;
					}
					if (racinix_event_handler(RACINIX_EVENT_KEYSTROKE, kbd_keys[key].makecode, kbd_keys[key].pressed) == -1)
					{
						break;
					}
					continue;
				}
				if (msg.NOTIFY_ARG & BIT(timer_hook_bit)) {
					if ((fps_counter = racinix_timer_int_handler()) != -1)
					{
						if (racinix_event_handler(RACINIX_EVENT_NEW_FRAME, fps_counter) == RACINIX_STATE_END)
						{
							break;
						}
					}
				}
				if (msg.NOTIFY_ARG & BIT(MOUSE_HOOK_BIT)) {
					if (racinix_mouse_int_handler(&new_mouse_data_packet) == 0) // Packet ready
					{
						if (racinix_event_handler(RACINIX_EVENT_MOUSE_MOVEMENT, &new_mouse_data_packet) == RACINIX_STATE_END)
						{
							break;
						}
						if (new_mouse_data_packet.left_button != old_mouse_data_packet.left_button)
						{
							if (racinix_event_handler(RACINIX_EVENT_MOUSE_LEFT_BTN, new_mouse_data_packet.left_button) == RACINIX_STATE_END)
							{
								break;
							}
						}
						if (new_mouse_data_packet.right_button != old_mouse_data_packet.right_button)
						{
							if (racinix_event_handler(RACINIX_EVENT_MOUSE_RIGHT_BTN, new_mouse_data_packet.right_button) == RACINIX_STATE_END)
							{
								break;
							}
						}
						old_mouse_data_packet = new_mouse_data_packet;
					}
				}
				if (msg.NOTIFY_ARG & BIT(SERIAL_HOOK_BIT))
				{
					char string[50];
					if (race != NULL && race->vehicles[0] != NULL)
					{
						sprintf(string, "%s", race->vehicles[0]->position.x);
					}
					serial_interrupt_transmit_string(1, string);
					serial_int_handler(1);
				}
			}
		}
	}
	timer_unsubscribe_int();
	keyboard_unsubscribe_int();
	mouse_disable_stream_mode(MOUSE_NUM_TRIES);
	mouse_unsubscribe_int(mouse_hook_id);

	return 0;
}

int racinix_event_handler(int event, ...)
{
	static int state = RACINIX_STATE_MAIN_MENU;
	va_list var_args;
	va_start(var_args, event);
	switch(state)
	{
	case RACINIX_STATE_MAIN_MENU:
	{
		state = racinix_main_menu_event_handler(event, &var_args);
		break;
	}
	case RACINIX_STATE_DESIGN_TRACK:
	{
		state = racinix_track_design_event_handler(event, &var_args);
		break;
	}
	case RACINIX_STATE_RACE:
	{
		state = racinix_race_event_handler(event, &var_args);
		break;
	}
	case RACINIX_STATE_ERROR:
	case RACINIX_STATE_END:
	{
		va_end(var_args);
		return RACINIX_STATE_END;
	}
	break;
	}
	va_end(var_args);
	return 0;
}

int racinix_main_menu_event_handler(int event, va_list *var_args)
{
	static context_menu_t *context_menu;
	static int state = RACINIX_STATE_MAIN_MENU_BASE;
	static size_t button_ID = RACINIX_MAIN_MENU_NUM_BTN;
	char *buttons[RACINIX_MAIN_MENU_NUM_BTN];
	buttons[0] = "1 PLAYER", '\0';
	buttons[1] = "2 PLAYERS IN THE SAME PC", '\0';
	buttons[2] = "2 PLAYERS VIA SERIAL PORT", '\0';
	buttons[3] = "SETTINGS", '\0';
	buttons[4] = "CREDITS", '\0';
	buttons[5] = "EXIT", '\0';
	char *context_menu_track_choice_items[] = { "RANDOM TRACK", "DESIGN TRACK" };
	switch (state)
	{
	case RACINIX_STATE_MAIN_MENU_BASE:
	{
		if (event == RACINIX_EVENT_MOUSE_LEFT_BTN)
		{
			if (va_arg(*var_args, int)) // pressed
			{
				switch (button_ID) // TODO
						{
				case RACINIX_MAIN_MENU_BUTTON_1_PLAYER: // 1 Player
				{
					context_menu = context_menu_create(context_menu_track_choice_items, 2, &vmi, font_impact);
					state = RACINIX_STATE_MAIN_MENU_PICK_TRACK;
					num_players = 1;
					serial_port = false;
					return RACINIX_STATE_MAIN_MENU;
				}
				case RACINIX_MAIN_MENU_BUTTON_2_PLAYERS_SAME_PC: // 2 Players in the same PC
				{
					context_menu = context_menu_create(context_menu_track_choice_items, 2, &vmi, font_impact);
					state = RACINIX_STATE_MAIN_MENU_PICK_TRACK;
					num_players = 2;
					serial_port = false;
					return RACINIX_STATE_MAIN_MENU;
				}
				case RACINIX_MAIN_MENU_BUTTON_2_PLAYERS_SERIAL_PORT: // 2 Players via serial port
					return RACINIX_STATE_MAIN_MENU;
				case RACINIX_MAIN_MENU_BUTTON_SETTINGS: // Settings
					break;
				case RACINIX_MAIN_MENU_BUTTON_CREDITS: // Credits
					//racinix_draw_menu(-1, buttons); // No button being hovered
					//bitmap_draw_alpha(bitmap_credits, (vmi.XResolution - bitmap_credits->bitmap_information_header.width) / 2, (vmi.YResolution - bitmap_credits->bitmap_information_header.height) / 2, VIDEO_GR_64K_TRANSPARENT);
					//bitmap_draw(bitmap_credits, (vmi.XResolution - bitmap_credits->bitmap_information_header.width) / 2, (vmi.YResolution - bitmap_credits->bitmap_information_header.height) / 2);
					bitmap_draw(bitmap_credits, 0, 0);
					vg_swap_buffer();
					vg_swap_mouse_buffer();
					state = RACINIX_STATE_MAIN_MENU_CREDITS;
					return RACINIX_STATE_MAIN_MENU;
				case RACINIX_MAIN_MENU_BUTTON_EXIT: // Exit
					return RACINIX_STATE_END;
				default:
					break; // A button wasn't clicked
						}
			}
		}
		else if (event == RACINIX_EVENT_MOUSE_MOVEMENT)
		{
			racinix_mouse_update(va_arg(*var_args, mouse_data_packet_t *));
			racinix_draw_mouse();

			vector2D_t top_left_corner;
			for (button_ID = 0; button_ID < RACINIX_MAIN_MENU_NUM_BTN; ++button_ID)
			{
				top_left_corner = vectorCreate((vmi.XResolution - font_calculate_string_width(font_impact, buttons[button_ID], RACINIX_MAIN_MENU_CHAR_HEIGHT)) / 2,
						button_ID * (vmi.YResolution / 2) / RACINIX_MAIN_MENU_NUM_BTN + vmi.YResolution / 2);
				if (isPointInAxisAlignedRectangle(
						top_left_corner,
						font_calculate_string_width(font_impact, buttons[button_ID], RACINIX_MAIN_MENU_CHAR_HEIGHT),
						RACINIX_MAIN_MENU_CHAR_HEIGHT,
						mouse_position))
				{
					break;
				}
			}

			return RACINIX_STATE_MAIN_MENU;
		}

		racinix_draw_menu(button_ID, buttons);
		break;
	}
	case RACINIX_STATE_MAIN_MENU_PICK_TRACK:
	{
		if (event == RACINIX_EVENT_MOUSE_LEFT_BTN && va_arg(*var_args, int)) // Left mouse click
		{
			int click = context_menu_click(context_menu, (unsigned)mouse_position.x, (unsigned)mouse_position.y, &vmi);
			switch (click)
			{
			case 0: // Random track
			{
				state = RACINIX_STATE_MAIN_MENU_BASE;
				context_menu_delete(context_menu);
				track_t *track;
				if ((track = track_create(vmi.XResolution, vmi.YResolution)) == NULL)
				{
					return RACINIX_STATE_ERROR;
				}
				if (track_random_generate(track, rand()))
				{
					return RACINIX_STATE_ERROR;
				}
				if ((race = race_create(track, num_players, vehicle_bitmaps, vehicle_keys, vehicle_colors, RACINIX_RACE_FREEZE_TIME, RACINIX_RACE_NUM_LAPS, &vmi, font_impact)) == NULL)
				{
					return RACINIX_STATE_ERROR;
				}
				if (race_start(race))
				{
					return RACINIX_STATE_ERROR;
				}
				// TODO DELETE DA RACE!
				state = RACINIX_STATE_MAIN_MENU_BASE;
				return RACINIX_STATE_RACE;
			}
			case 1:	// Design track
			{
				state = RACINIX_STATE_MAIN_MENU_BASE;
				context_menu_delete(context_menu);

				return RACINIX_STATE_DESIGN_TRACK;
			}
			case CONTEXT_MENU_CLICK_BACKGROUND:
			{
				state = RACINIX_STATE_MAIN_MENU_BASE;
				context_menu_delete(context_menu);
				return RACINIX_STATE_MAIN_MENU;
				break;
			}
			case CONTEXT_MENU_CLICK_NO_BUTTON:
			{
				break;
			}
			}
		}
		else if (event == RACINIX_EVENT_KEYSTROKE && va_arg(*var_args, int) == KEY_ESC && va_arg(*var_args, int)) // Esc pressed
		{
			state = RACINIX_STATE_MAIN_MENU_BASE;
			context_menu_delete(context_menu);
			return RACINIX_STATE_MAIN_MENU;
		}
		else if (event == RACINIX_EVENT_MOUSE_MOVEMENT)
		{
			racinix_mouse_update(va_arg(*var_args, mouse_data_packet_t *));
			context_menu_draw(context_menu, mouse_position, &vmi);
			racinix_draw_mouse();
			return RACINIX_STATE_MAIN_MENU;
		}
		context_menu_draw(context_menu, mouse_position, &vmi);
		break;
	}
	case RACINIX_STATE_MAIN_MENU_CREDITS:
	{
		if ((event == RACINIX_EVENT_KEYSTROKE && va_arg(*var_args, int) == KEY_ESC || event == RACINIX_EVENT_MOUSE_LEFT_BTN) && va_arg(*var_args, int)) // Esc or LMB pressed
		{
			state = RACINIX_STATE_MAIN_MENU_BASE;
			racinix_draw_menu(-1, buttons);
			racinix_draw_mouse();
			return RACINIX_STATE_MAIN_MENU;
		}
		return RACINIX_STATE_MAIN_MENU;
	}
	}
	racinix_draw_mouse();
	return RACINIX_STATE_MAIN_MENU;
}

int racinix_race_event_handler(int event, va_list *var_args)
{
	switch (event)
	{
	case RACINIX_EVENT_NEW_FRAME:
	{
		race_tick(race, RACINIX_DELTA_TIME, va_arg(*var_args, unsigned));
	}
	case RACINIX_EVENT_KEYSTROKE: // int key, bool pressed
	{
		if (va_arg(*var_args, int) == KEY_ESC) // key
		{
			if (va_arg(*var_args, int))
			{
				race_delete(race);
				return RACINIX_STATE_MAIN_MENU;
			}
		}
	}
	}
	return RACINIX_STATE_RACE;
}

int racinix_track_design_event_handler(int event, va_list *var_args)
{
	static int state = RACINIX_STATE_TRACK_DESIGN_NEW;
	static int point_ID;
	static track_t *track;
	switch (state)
	{
	case RACINIX_STATE_TRACK_DESIGN_NEW:
	{
		if ((track = track_create(vmi.XResolution, vmi.YResolution)) == NULL)
		{
			return 1;
		}
		state = RACINIX_STATE_TRACK_DESIGN_DESIGNING;
		break;
	}
	case RACINIX_STATE_TRACK_DESIGN_MOVING:
	{
		if (event == RACINIX_EVENT_MOUSE_LEFT_BTN)
		{
			state = RACINIX_STATE_TRACK_DESIGN_DESIGNING;
			break;
		}
		else if (event == RACINIX_EVENT_MOUSE_RIGHT_BTN)
		{
			state = RACINIX_STATE_TRACK_DESIGN_DESIGNING;
			track_erase_control_point(track, point_ID);
			track_generate_spline(track);
			break;
		}
		else if (event == RACINIX_EVENT_MOUSE_MOVEMENT)
		{
			racinix_mouse_update(va_arg(*var_args, mouse_data_packet_t *));
		}
		track->control_points[point_ID] = mouse_position;
		track_generate_spline(track);
		//track_update_track_points(track);
		break;
	}
	case RACINIX_STATE_TRACK_DESIGN_DESIGNING:
	{
		if (event == RACINIX_EVENT_MOUSE_LEFT_BTN)
		{
			if (va_arg(*var_args, int)) // pressed
			{
				if (track->num_control_points > 0)
				{
					point_ID = track_get_closest_control_point(track, mouse_position);
					if (vectorDistance(mouse_position, track->control_points[point_ID]) < RACINIX_TRACK_DESIGN_SELECT_POINT_RANGE)
					{
						state = RACINIX_STATE_TRACK_DESIGN_MOVING;
						break;
					}
					else
					{
						point_ID = track_spline_to_control_point(track, track_get_closest_spline_point(track, mouse_position));
						track_add_control_point(track, point_ID);
						track->control_points[++point_ID] = mouse_position;
						track_generate_spline(track);
						state = RACINIX_STATE_TRACK_DESIGN_MOVING;
						break;
					}
				}
				if ((track->control_points = realloc(track->control_points, ++(track->num_control_points) * sizeof(vector2D_t))) == NULL)
				{
					return RACINIX_STATE_ERROR;
				}
				track->control_points[track->num_control_points - 1] = mouse_position;

				track_generate_spline(track);
				//track_update_track_points(track);
			}
		}
		else if (event == RACINIX_EVENT_MOUSE_RIGHT_BTN)
		{
			if (va_arg(*var_args, int)) // pressed
			{
				if (track->num_control_points > 0)
				{
					point_ID = track_get_closest_control_point(track, mouse_position);
					if (vectorDistance(mouse_position, track->control_points[point_ID]) < RACINIX_TRACK_DESIGN_SELECT_POINT_RANGE)
					{
						track_erase_control_point(track, point_ID);
						track_generate_spline(track);
					}
				}
			}
		}
		else if (event == RACINIX_EVENT_MOUSE_MOVEMENT)
		{
			racinix_mouse_update(va_arg(*var_args, mouse_data_packet_t *));
		}
		else if (event == RACINIX_EVENT_KEYSTROKE) // int key, bool pressed
		{
			int key = va_arg(*var_args, int);
			if (va_arg(*var_args, int)) // pressed
			{
				if (key == KEY_ESC)
				{
					track_delete(track);
					state = RACINIX_STATE_TRACK_DESIGN_NEW;
					return RACINIX_STATE_MAIN_MENU;
				}
				else if (key == KEY_ENTER)
				{
					if (track_update_track_points(track) == 0)
					{
						if ((race = race_create(track, num_players, vehicle_bitmaps, vehicle_keys, vehicle_colors, RACINIX_RACE_FREEZE_TIME, RACINIX_RACE_NUM_LAPS, &vmi, font_impact)) == NULL)
						{
							return RACINIX_STATE_ERROR;
						}
						if (race_start(race))
						{
							return RACINIX_STATE_ERROR;
						}
						state = RACINIX_STATE_TRACK_DESIGN_NEW;
						return RACINIX_STATE_RACE;
					}
				}
			}
		}
		break;
	}
	}

	vg_fill(RACINIX_COLOR_GRASS);

	track_draw_spline(track);
	if (track->num_control_points >= 3)
	{
		track_draw_finish_line(track);
	}
	track_draw_control_points(track);
	//track_draw(track);

	font_show_string(font_impact, "TRACK DESIGNER", 30, vmi.XResolution / 2, 10, FONT_ALIGNMENT_MIDDLE, VIDEO_GR_WHITE, 2);
	font_show_string(font_impact, "PRESS ENTER TO START THE RACE OR ESC TO EXIT", 15, vmi.XResolution - 11, vmi.YResolution - 25, FONT_ALIGNMENT_RIGHT, VIDEO_GR_WHITE, 2);

	racinix_draw_mouse();
	return RACINIX_STATE_DESIGN_TRACK;
}

int racinix_keyboard_int_handler()
{
	return keyboard_int_handler();
}

int racinix_timer_int_handler()
{
	static double counter = 0;
	static unsigned long timer = 0;
	static unsigned fps_last = 0;
	static unsigned fps_counter = 0;
	static const double reset_number = (double)TIMER_DEFAULT_FREQ / RACINIX_FPS; // For efficiency purposes (avoid calculating it every frame)
	if (time(NULL) > timer)
	{
		fps_last = fps_counter;
		fps_counter = 0;
		timer = time(NULL);
	}
	counter += 1.0;
	if (counter >= reset_number)
	{
		counter -= reset_number;

		++fps_counter;
		return fps_last;
	}
	return -1;
}

int racinix_mouse_int_handler(mouse_data_packet_t *mouse_data_packet)
{
	if(mouse_int_handler(MOUSE_NUM_TRIES))
	{
		return 1;
	}
	if (mouse_get_packet(mouse_data_packet))
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

void racinix_mouse_update(mouse_data_packet_t *mouse_data_packet)
{
	mouse_position = vectorAdd(mouse_position, vectorMultiply(vectorCreate(mouse_data_packet->x_delta, -mouse_data_packet->y_delta), RACINIX_MOUSE_SENSITIVITY));
	if (mouse_position.x < 0)
	{
		mouse_position.x = 0;
	}
	else if (mouse_position.x >= vmi.XResolution)
	{
		mouse_position.x = vmi.XResolution - 1;
	}
	if (mouse_position.y < 0)
	{
		mouse_position.y = 0;
	}
	else if (mouse_position.y >= vmi.YResolution)
	{
		mouse_position.y = vmi.YResolution - 1;
	}
}

void racinix_draw_mouse()
{
	vg_swap_buffer();
	vg_draw_mouse((int)mouse_position.x, (int)mouse_position.y, mouse_cursor);
	vg_swap_mouse_buffer();
}

void racinix_draw_menu(size_t button_ID, char *buttons[])
{
	bitmap_draw(background, 0, 0);
	// Show logo
	bitmap_draw_alpha(logo, (vmi.XResolution - logo->bitmap_information_header.width) / 2, (vmi.YResolution / 2 - logo->bitmap_information_header.height) / 2, VIDEO_GR_64K_TRANSPARENT);

	size_t i;
	for (i = 0; i < RACINIX_MAIN_MENU_NUM_BTN; ++i)
	{
		/*vg_draw_rectangle(
						(vmi.XResolution - font_calculate_string_width(font_impact, buttons[i], RACINIX_MAIN_MENU_CHAR_HEIGHT)) / 2,
						i * (vmi.YResolution / 2) / RACINIX_MAIN_MENU_NUM_BTN + vmi.YResolution / 2,
						font_calculate_string_width(font_impact, buttons[i], RACINIX_MAIN_MENU_CHAR_HEIGHT),
						RACINIX_MAIN_MENU_CHAR_HEIGHT,
						VIDEO_GR_BLUE
				);*/
		if (i == button_ID)
		{
			font_show_string(font_impact, buttons[i], RACINIX_MAIN_MENU_CHAR_HEIGHT, vmi.XResolution / 2 - RACINIX_MAIN_MENU_BTN_TEXT_HOVER_OFFSET, i * (vmi.YResolution / 2) / RACINIX_MAIN_MENU_NUM_BTN + vmi.YResolution / 2 - RACINIX_MAIN_MENU_BTN_TEXT_HOVER_OFFSET, FONT_ALIGNMENT_MIDDLE, RACINIX_COLOR_ORANGE, RACINIX_MAIN_MENU_BTN_TEXT_SHADE + RACINIX_MAIN_MENU_BTN_TEXT_HOVER_OFFSET);
		}
		else
		{
			font_show_string(font_impact, buttons[i], RACINIX_MAIN_MENU_CHAR_HEIGHT, vmi.XResolution / 2, i * (vmi.YResolution / 2) / RACINIX_MAIN_MENU_NUM_BTN + vmi.YResolution / 2, FONT_ALIGNMENT_MIDDLE, VIDEO_GR_WHITE, RACINIX_MAIN_MENU_BTN_TEXT_SHADE);
		}
	}
}
