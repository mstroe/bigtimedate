/*

   Big Time watch

   A digital watch with large digits.


   A few things complicate the implementation of this watch:

   a) The largest size of the Nevis font which the Pebble handles
      seems to be ~47 units (points or pixels?). But the size of
      characters we want is ~100 points.

      This requires us to generate and use images instead of
      fonts--which complicates things greatly.

   b) When I started it wasn't possible to load all the images into
      RAM at once--this means we have to load/unload each image when
      we need it. The images are slightly smaller now than they were
      but I figured it would still be pushing it to load them all at
      once, even if they just fit, so I've stuck with the load/unload
      approach.

 */

#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"

#include "resource_ids.auto.h"

	
#define MY_UUID {0x12, 0xAB, 0xF1, 0xD8, 0xC4, 0x74, 0x47, 0x96, 0x81, 0x83, 0x77, 0xE2, 0x7E, 0x0A, 0xCB, 0xA5}
PBL_APP_INFO(MY_UUID, "Big Time Date", "Pebble Technology", 0x5, 0x0, RESOURCE_ID_IMAGE_MENU_ICON, APP_INFO_WATCH_FACE);

Window window;
TextLayer date_layer;

GFont font_date;        // Font for date

//
// There's only enough memory to load about 6 of 10 required images
// so we have to swap them in & out...
//
// We have one "slot" per digit location on screen.
//
// Because layers can only have one parent we load a digit for each
// slot--even if the digit image is already in another slot.
//
// Slot on-screen layout:
//     0 1
//     2 3
//
#define TOTAL_IMAGE_SLOTS 4

#define NUMBER_OF_IMAGES 10
	
#define DATE_FRAME      (GRect(1, 128, 144, 168-126))

// These images are 72 x 84 pixels (i.e. a quarter of the display),
// black and white with the digit character centered in the image.
// (As generated by the `fonttools/font2png.py` script.)
const int IMAGE_RESOURCE_IDS[NUMBER_OF_IMAGES] = {
  RESOURCE_ID_IMAGE_NUM_0, RESOURCE_ID_IMAGE_NUM_1, RESOURCE_ID_IMAGE_NUM_2,
  RESOURCE_ID_IMAGE_NUM_3, RESOURCE_ID_IMAGE_NUM_4, RESOURCE_ID_IMAGE_NUM_5,
  RESOURCE_ID_IMAGE_NUM_6, RESOURCE_ID_IMAGE_NUM_7, RESOURCE_ID_IMAGE_NUM_8,
  RESOURCE_ID_IMAGE_NUM_9
};

BmpContainer image_containers[TOTAL_IMAGE_SLOTS];

#define EMPTY_SLOT -1

// The state is either "empty" or the digit of the image currently in
// the slot--which was going to be used to assist with de-duplication
// but we're not doing that due to the one parent-per-layer
// restriction mentioned above.
int image_slot_state[TOTAL_IMAGE_SLOTS] = {EMPTY_SLOT, EMPTY_SLOT, EMPTY_SLOT, EMPTY_SLOT};


void load_digit_image_into_slot(int slot_number, int digit_value) {
  /*

     Loads the digit image from the application's resources and
     displays it on-screen in the correct location.

     Each slot is a quarter of the screen.

   */

  // TODO: Signal these error(s)?

  if ((slot_number < 0) || (slot_number >= TOTAL_IMAGE_SLOTS)) {
    return;
  }

  if ((digit_value < 0) || (digit_value > 9)) {
    return;
  }

  if (image_slot_state[slot_number] != EMPTY_SLOT) {
    return;
  }

  image_slot_state[slot_number] = digit_value;
  bmp_init_container(IMAGE_RESOURCE_IDS[digit_value], &image_containers[slot_number]);
  image_containers[slot_number].layer.layer.frame.origin.x = (slot_number % 2) * 72;
  image_containers[slot_number].layer.layer.frame.origin.y = (slot_number / 2) * 64;
  layer_add_child(&window.layer, &image_containers[slot_number].layer.layer);

}


void unload_digit_image_from_slot(int slot_number) {
  /*

     Removes the digit from the display and unloads the image resource
     to free up RAM.

     Can handle being called on an already empty slot.

   */

  if (image_slot_state[slot_number] != EMPTY_SLOT) {
    layer_remove_from_parent(&image_containers[slot_number].layer.layer);
    bmp_deinit_container(&image_containers[slot_number]);
    image_slot_state[slot_number] = EMPTY_SLOT;
  }

}


void display_value(unsigned short value, unsigned short row_number, bool show_first_leading_zero) {
  /*

     Displays a numeric value between 0 and 99 on screen.

     Rows are ordered on screen as:

       Row 0
       Row 1

     Includes optional blanking of first leading zero,
     i.e. displays ' 0' rather than '00'.

   */
  value = value % 100; // Maximum of two digits per row.

  // Column order is: | Column 0 | Column 1 |
  // (We process the columns in reverse order because that makes
  // extracting the digits from the value easier.)
  for (int column_number = 1; column_number >= 0; column_number--) {
    int slot_number = (row_number * 2) + column_number;
    unload_digit_image_from_slot(slot_number);
    if (!((value == 0) && (column_number == 0) && !show_first_leading_zero)) {
      load_digit_image_into_slot(slot_number, value % 10);
    }
    value = value / 10;
  }
}


unsigned short get_display_hour(unsigned short hour) {

  if (clock_is_24h_style()) {
    return hour;
  }

  unsigned short display_hour = hour % 12;

  // Converts "0" to "12"
  return display_hour ? display_hour : 12;

}


void display_time(PblTm *tick_time) {

  // TODO: Use `units_changed` and more intelligence to reduce
  //       redundant digit unload/load?

  display_value(get_display_hour(tick_time->tm_hour), 0, false);
  display_value(tick_time->tm_min, 1, true);
}

void display_date(PebbleTickEvent *t) {
	static char date_text[] = "XXX 00";
	
	if (t->units_changed & DAY_UNIT)
	    {		
		    string_format_time(date_text,
	                           sizeof(date_text),
	                           "%a %d",
	                           t->tick_time);

			// Triggered if day of month < 10
			if (date_text[4] == '0')
			{
			    // Hack to get rid of the leading zero of the day of month
	            memmove(&date_text[4], &date_text[5], sizeof(date_text) - 1);
			}
			
			/*** LOCALIZATION CODE BEGIN ***
			
			// Primitive hack to translate the day of week to another language
			// Needs to be exactly 3 characters, e.g. "Mon" or "Mo "
			// Supported characters: A-Z, a-z, 0-9
			
			if (date_text[0] == 'M')
			{
				memcpy(&date_text, "XXX", 3); // Monday
			}
			
			if (date_text[0] == 'T' && date_text[1] == 'u')
			{
				memcpy(&date_text, "XXX", 3); // Tuesday
			}
			
			if (date_text[0] == 'W')
			{
				memcpy(&date_text, "XXX", 3); // Wednesday
			}
			
			if (date_text[0] == 'T' && date_text[1] == 'h')
			{
				memcpy(&date_text, "XXX", 3); // Thursday
			}
			
			if (date_text[0] == 'F')
			{
				memcpy(&date_text, "XXX", 3); // Friday
			}
			
			if (date_text[0] == 'S' && date_text[1] == 'a')
			{
				memcpy(&date_text, "XXX", 3); // Saturday
			}
			
			if (date_text[0] == 'S' && date_text[1] == 'u')
			{
				memcpy(&date_text, "XXX", 3); // Sunday
			}
			
			// Uncomment the line below if your labels consist of 2 characters and 1 space, e.g. "Mo "
			//memmove(&date_text[3], &date_text[4], sizeof(date_text) - 1);
			
			*** LOCALIZATION CODE END ***/
			
	        text_layer_set_text(&date_layer, date_text);
	    }

}


void handle_minute_tick(AppContextRef ctx, PebbleTickEvent *t) {
  (void)t;
  (void)ctx;

  display_time(t->tick_time);	
  display_date(t);
}


void handle_init(AppContextRef ctx) {
  (void)ctx;

  window_init(&window, "Big Time watch");
  window_stack_push(&window, true);
  window_set_background_color(&window, GColorBlack);

  resource_init_current_app(&APP_RESOURCES);

  ResHandle res_d;	

	res_d = resource_get_handle(RESOURCE_ID_FUTURA_28); // Date font
    font_date = fonts_load_custom_font(res_d);
	text_layer_init(&date_layer, window.layer.frame);
    text_layer_set_text_color(&date_layer, GColorWhite);
    text_layer_set_background_color(&date_layer, GColorClear);
    text_layer_set_font(&date_layer, font_date);
    text_layer_set_text_alignment(&date_layer, GTextAlignmentCenter);
    layer_set_frame(&date_layer.layer, DATE_FRAME);
    layer_add_child(&window.layer, &date_layer.layer);
	
  // Avoids a blank screen on watch start.
  PblTm tick_time;
  PebbleTickEvent t;
  t.tick_time = &tick_time;
  t.units_changed = SECOND_UNIT | MINUTE_UNIT | HOUR_UNIT | DAY_UNIT;

  get_time(&tick_time);
  display_time(&tick_time);
  display_date(&t);
}


void handle_deinit(AppContextRef ctx) {
  (void)ctx;

  for (int i = 0; i < TOTAL_IMAGE_SLOTS; i++) {
    unload_digit_image_from_slot(i);
  }
	
  fonts_unload_custom_font(font_date);

}


void pbl_main(void *params) {
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,
    .deinit_handler = &handle_deinit,

    .tick_info = {
      .tick_handler = &handle_minute_tick,
      .tick_units = MINUTE_UNIT
    }

  };
  app_event_loop(params, &handlers);
}
