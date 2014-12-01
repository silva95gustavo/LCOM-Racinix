#include "proj.h"

#define MOUSE_NUM_TRIES		10
#define MOUSE_HOOK_BIT	12
#define PI 					3.14159265358979323846

#define BIT(n) (0x01<<(n))

static vector2D_t mouse_position;
static vbe_mode_info_t vmi;
static track_t *track;
static vehicle_keys_t vehicle_keys[2];
static uint16_t vehicle_colors[2];

// Bitmaps
static bitmap_t *background;
static bitmap_t *mouse_cursor;
static bitmap_t *logo;
static bitmap_t *car;

int main(int argc, char **argv) {

	/* Initialize service */
	printf("N");
	sef_startup();
	printf("p");

	if (racinix_start())
	{
		racinix_exit();
		printf("Racinix: An error occurred and the program was stopped.\n");
	}
	printf("o");

	racinix_exit();

	return 0;
}

int racinix_start()
{
	printf("N");
	srand(time(NULL));
	//vg_exit(); // Fix darker colors first time the mode is changed.
	vg_init(RACINIX_VIDEO_MODE);
	vbe_get_mode_info(RACINIX_VIDEO_MODE, &vmi);

	mouse_position = vectorCreate(vmi.XResolution / 2, vmi.YResolution / 2);

	background = bitmap_load("/home/lcom/proj/images/background.bmp");
	if (background == NULL)
	{
		return 1;
	}
	printf("p");

	mouse_cursor = bitmap_load("/home/lcom/proj/images/cursor.bmp");
	if (mouse_cursor == NULL)
	{
		return 1;
	}

	logo = bitmap_load("/home/lcom/proj/images/logo.bmp");
	if (logo == NULL)
	{
		return 1;
	}

	car = bitmap_load("/home/lcom/proj/images/car.bmp");
	if (logo == NULL)
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

	vehicle_colors[0] = rgb(0, 0, 255);
	vehicle_colors[1] = rgb(255, 0, 0);

	if (racinix_dispatcher() != 0)
	{
		return 1;
	}
	return 0;
}

int racinix_exit()
{
	bitmap_delete(background);
	bitmap_delete(mouse_cursor);
	return vg_exit();
}

int racinix_dispatcher()
{
	printf("N");

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
	printf("N");

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

	mouse_data_packet_t old_mouse_data_packet, new_mouse_data_packet;
	old_mouse_data_packet.left_button = old_mouse_data_packet.middle_button = old_mouse_data_packet.right_button = false;
	int r, ipc_status;
	message msg;
	key_t key;
	printf("N");

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
					if (racinix_timer_int_handler() == 0)
					{
						if (racinix_event_handler(RACINIX_EVENT_NEW_FRAME) == -1)
						{
							break;
						}
					}
				}
				if (msg.NOTIFY_ARG & BIT(MOUSE_HOOK_BIT)) {
					if (racinix_mouse_int_handler(&new_mouse_data_packet) == 0) // Packet ready
					{
						if (racinix_event_handler(RACINIX_EVENT_MOUSE_MOVEMENT, &new_mouse_data_packet) == -1)
						{
							break;
						}
						if (new_mouse_data_packet.left_button != old_mouse_data_packet.left_button)
						{
							if (racinix_event_handler(RACINIX_EVENT_MOUSE_LEFT_BTN, new_mouse_data_packet.left_button) == -1)
							{
								break;
							}
						}
						old_mouse_data_packet = new_mouse_data_packet;
					}
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
		if (event == RACINIX_EVENT_NEW_RACE)
		{
			state = racinix_race_event_handler(event, &var_args);
		}
		else
		{
			state = racinix_main_menu_event_handler(event, &var_args);
		}
		break;
	}
	case RACINIX_STATE_PICK_TRACK:
	case RACINIX_STATE_DESIGN_TRACK:
		break;
	case RACINIX_STATE_RACE:
	{
		state = racinix_race_event_handler(event, &var_args);
		break;
	}
	case RACINIX_STATE_END:
	{
		va_end(var_args);
		return -1;
	}
	break;
	}
	va_end(var_args);
	return 0;
}

int racinix_main_menu_event_handler(int event, va_list *var_args)
{
	char *buttons[RACINIX_MAIN_MENU_NUM_BTN];
	buttons[0] = "1 Player", '\0';
	buttons[1] = "2 Players in the same PC", '\0';
	buttons[2] = "2 Players via serial port", '\0';
	buttons[3] = "Settings", '\0';
	buttons[4] = "Credits", '\0';
	buttons[5] = "Exit", '\0';
	if (event == RACINIX_EVENT_MOUSE_LEFT_BTN)
	{
		if (va_arg(*var_args, int)) // pressed
		{
			vector2D_t top_left_corner;
			size_t i;
			for (i = 0; i < RACINIX_MAIN_MENU_NUM_BTN; ++i)
			{
				top_left_corner = vectorCreate((vmi.XResolution - RACINIX_MAIN_MENU_CHAR_WIDTH * strlen(buttons[i])) / 2,
						i * (vmi.YResolution / 2) / RACINIX_MAIN_MENU_NUM_BTN + vmi.YResolution / 2);
				if (isPointInAxisAlignedRectangle(
						top_left_corner,
						RACINIX_MAIN_MENU_CHAR_WIDTH * strlen(buttons[i]),
						RACINIX_MAIN_MENU_CHAR_HEIGHT,
						mouse_position))
				{
					break;
				}
			}
			switch (i) // TODO
			{
			case RACINIX_MAIN_MENU_BUTTON_1_PLAYER: // 1 Player
				racinix_event_handler(RACINIX_EVENT_NEW_RACE, 1, false);
				return RACINIX_STATE_RACE;
			case RACINIX_MAIN_MENU_BUTTON_2_PLAYERS_SAME_PC: // 2 Players in the same PC
				racinix_event_handler(RACINIX_EVENT_NEW_RACE, 2, false);
				return RACINIX_STATE_RACE;
			case RACINIX_MAIN_MENU_BUTTON_2_PLAYERS_SERIAL_PORT: // 2 Players via serial port
				return RACINIX_STATE_MAIN_MENU;
			case RACINIX_MAIN_MENU_BUTTON_SETTINGS: // Settings
				break;
			case RACINIX_MAIN_MENU_BUTTON_CREDITS: // Credits
				break;
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
		return RACINIX_STATE_MAIN_MENU;
	}

	// Show menu
	bitmap_draw(background, 0, 0);

	// Show logo
	bitmap_draw_alpha(logo, (vmi.XResolution - logo->bitmap_information_header.width) / 2, (vmi.YResolution / 2 - logo->bitmap_information_header.height) / 2);

	size_t i;
	for (i = 0; i < RACINIX_MAIN_MENU_NUM_BTN; ++i)
	{
		// TODO Write text
		vg_draw_rectangle(
				(vmi.XResolution - RACINIX_MAIN_MENU_CHAR_WIDTH * strlen(buttons[i])) / 2,
				i * (vmi.YResolution / 2) / RACINIX_MAIN_MENU_NUM_BTN + vmi.YResolution / 2,
				RACINIX_MAIN_MENU_CHAR_WIDTH * strlen(buttons[i]),
				RACINIX_MAIN_MENU_CHAR_HEIGHT,
				VIDEO_GR_BLUE
		);
	}
	racinix_draw_mouse();
	return RACINIX_STATE_MAIN_MENU;
}

int racinix_race_event_handler(int event, va_list *var_args)
{
	static int state;
	static unsigned num_vehicles;
	static vehicle_t **vehicles;
	switch (event)
	{
	case RACINIX_EVENT_NEW_RACE: // unsigned num_players, bool serial_port
	{
		if (va_arg(*var_args, unsigned) == 1) // num_players
		{
			if ((vehicles = malloc(1 * sizeof(vehicle_t *))) == NULL)
			{
				return 1;
			}
			state = RACINIX_STATE_RACE_RACING_1_PLAYER;
			num_vehicles = 1;
		}
		else
		{
			if (va_arg(*var_args, int)) // serial_port
			{
				// TODO implement serial port mode... don't forget to malloc *vehicles properly
				return RACINIX_STATE_MAIN_MENU;
			}
			else
			{
				if ((vehicles = malloc(2 * sizeof(vehicle_t *))) == NULL)
				{
					return 1;
				}
				state = RACINIX_STATE_RACE_RACING_2_PLAYERS_SAME_PC;
			}
			num_vehicles = 2;
		}
		track = track_generate(vmi.XResolution, vmi.YResolution, rand());

		vector2D_t starting_position_increment = vectorDivide(vectorSubtract(track->outside_spline[0], track->inside_spline[0]), num_vehicles + 1);
		vector2D_t starting_position_offset, temp_vector;
		vector2D_t starting_position;
		double heading = atan2(track->spline[1].y - track->spline[0].y, track->spline[1].x - track->spline[0].x);

		size_t i;
		for (i = 0; i < num_vehicles; ++i)
		{
			//temp_vector = vectorRotate(starting_position_increment, PI / 2);
			//normalize(&temp_vector);
			//starting_position_offset = vectorMultiply(temp_vector, vehicles[i]->length / 2);
			//starting_position = vectorAdd(vectorAdd(track->inside_spline[0], vectorMultiply(starting_position_increment, i + 1)), starting_position_offset);
			starting_position = vectorAdd(track->inside_spline[0], vectorMultiply(starting_position_increment, i + 1));
			vehicles[i] = vehicle_create(20, 40, &starting_position, heading, car, vehicle_keys[i], vehicle_colors[i]);
		}

	}
	case RACINIX_EVENT_NEW_FRAME:
	{
		static vehicle_keys_t vehicle_keys;
		vg_swap_mouse_buffer();
		vg_fill(RACINIX_COLOR_GRASS);
		track_draw(track, vmi.XResolution, vmi.YResolution);
		size_t i;
		for (i = 0; i < num_vehicles; ++i)
		{
			vg_draw_circle(track->control_points[vehicles[i]->current_checkpoint].x, track->control_points[vehicles[i]->current_checkpoint].y, 5, vehicles[i]->checkpoint_color);
		}
		for (i = 0; i < num_vehicles; ++i)
		{
			racinix_update_vehicle(vehicles[i]);
		}

		// Vehicle-vehicle collision
		unsigned wheel_ID;
		size_t j;
		for (i = 0; i < num_vehicles; ++i)
		{
			for (j = 0; j < num_vehicles; ++j)
			{
				if (i != j)
				{
					wheel_ID = vehicle_check_vehicle_collision(vehicles[i], vehicles[j]);
					if (wheel_ID != -1)
					{
						vehicle_vehicle_collision_handler(vehicles[i], wheel_ID, vehicles[j]);
					}
				}
			}
		}
		vg_swap_buffer();
	}
	case RACINIX_EVENT_KEYSTROKE: // int key, bool pressed
	{
		if (va_arg(*var_args, int) == KEY_ESC) // key
		{
			if (va_arg(*var_args, int))
			{
				return RACINIX_STATE_MAIN_MENU;
			}
		}
	}
	}
	return RACINIX_STATE_RACE;
}

void racinix_update_vehicle(vehicle_t *vehicle)
{
	// Vehicle 1
	double drag = VEHICLE_DRAG;
	size_t i;
	for(i = 0; i < VEHICLE_NUM_WHEELS; ++i)
	{
		drag += track_get_point_drag(track, (int)vehicle->wheels[i].x, (int)vehicle->wheels[i].y, vmi.XResolution, vmi.YResolution);
	}
	vehicle_tick(vehicle, track, &vmi, 1.0 / FPS, drag);
}

int racinix_keyboard_int_handler()
{
	return keyboard_int_handler();
}

int racinix_timer_int_handler()
{
	static unsigned counter = 0;
	if (counter >= TIMER_DEFAULT_FREQ / FPS)
	{
		counter = 1;
		return 0;
	}
	return ++counter;
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
