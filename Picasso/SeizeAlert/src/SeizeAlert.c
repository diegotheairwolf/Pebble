/*
* SeizeAlert - A Seizure Notification and Detection System.
*
* Copyright © 2014 Pablo S. Campos.
*
* No part of this application may be reproduced without Pablo S. Campos's express consent.
*
* For more contact Pablo S. Campos at pablo.campos@utexas.edu
* 
*/




#include <pebble.h>
#include <pebble_fonts.h>
#include <SeizeAlert.h>

#define ALERT_WINDOW 10

#define STEP_ONE_LOWER_BOUND 800
#define STEP_ONE_HIGHER_BOUND 1000
#define STEP_ONE_SAMPLES 4

#define STEP_TWO_LOWER_BOUND 500
#define STEP_TWO_SAMPLES 25

#define STEP_THREE_HIGHER_BOUND 100
#define STEP_THREE_SAMPLES 50

//////////////////////////////////////////  Globals  ///////////////////////////////////////////////

static const uint32_t SEIZURE_LOG_TAGS[3] = { 0x5, 0xd, 0xe }; // fall, seizure, alert

// Watch layers
TextLayer *text_date_layer;
TextLayer *text_time_layer;
Layer *line_layer;

// Battery and Bluetooth layers
static Layer *battery_layer;
static BitmapLayer *bluetooth_layer;

static GBitmap *icon_battery;
static GBitmap *icon_battery_charge;
static GBitmap *bluetooth_bitmap = NULL;

// SeizeAlert layers
static Window *window;
static TextLayer *seizealert_layer;
static TextLayer *text_layer;
static TextLayer *text_layer_up;
static GBitmap *seizealert_logo = NULL;
static BitmapLayer *logo_layer;

// SeizeAlert logic globals
static int cntdown_ctr = 0;

static uint8_t battery_level;
static bool battery_plugged;

char text_buffer[250];
int timer_frequency = 40;		// Time setup for timer function in milliseconds
int countdown_frequency = 1000;		// Time setup for countdown function in milliseconds
static AppTimer *timer;

bool false_positive = true;		// State of false positive
bool event_fall = false;


// SeizeAlert's FSM variables
int current_state = 0;
int step_1_counter = 0;
int step_2_counter = 0;
bool step_2_flag = false;
int step_3_counter = 0;
bool step_3_flag = false;
int step_4_counter = 0;
bool step_4_flag = false;

// Data logging struct
typedef struct {
  uint32_t tag;
  TextLayer *text_layer;
  char text[20];
  DataLoggingSessionRef logging_session;
  int count;
  GBitmap *bitmap;
} SeizureData;

static SeizureData s_seizure_datas[3]; // 0 = fall, 1 = seizure, 2 = start of countdown








/////////////////////////////////////////// SeizeAlert Logic /////////////////////////////////////////////

/*
	Custom square root function. Math library
	is not completely included with Pebble SDK. 
*/
  float my_sqrt(const float num) {
  const uint MAX_STEPS = 40;
  const float MAX_ERROR = 0.001;
  
  float answer = num;
  float ans_sqr = answer * answer;
  uint step = 0;
  while((ans_sqr - num > MAX_ERROR) && (step++ < MAX_STEPS)) {
    answer = (answer + (num / answer)) / 2;
    ans_sqr = answer * answer;
  }
  return answer;
}



/*
	This functions sets the time to next call at 
	timer_frequency in milliseconds.
*/
static void set_timer() {
  timer = app_timer_register(timer_frequency, timer_callback, NULL);
}



static void set_countdown() {
  timer = app_timer_register(countdown_frequency, countdown_callback, NULL);
}



static void countdown_callback() {
  if (!false_positive){
    if (cntdown_ctr == 10) {
      cntdown_ctr = 0;
      text_layer_set_font(text_layer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
      if (event_fall) {
        report_fall();
        text_layer_set_text(text_layer_up, "SeizeAlert!!!");
        text_layer_set_text(text_layer, "A fall has been\nreported to\nyour phone.");
      } 
      false_positive = true;
      event_fall = false;
    } else {
      display_countdown(ALERT_WINDOW - cntdown_ctr);
      cntdown_ctr++;
      set_countdown();
    }
  }
}



/*
	This function peeks the accelerometer,
	and check if the "select" button has been
	pressed (false positive). If not, prints
	values to screen and sets timer to come
	back here again, using set_timer().
*/
static void timer_callback() {
  int test, x, y, z;
  AccelData accel;

  // Get last value from accelerometer
  accel_service_peek(&accel);

  // Calculate accelerometer values
  x = accel.x;
  y = accel.y;
  z = accel.z;
  x = x * x;
  y = y * y;
  z = z * z;
  
  test = (int)(abs(my_sqrt(x + y + z)-1000));		// int(abs(sqrt(x^2 + y^2 + z^2)-1000))

  switch( current_state ){

    case 0:	// Step 0: Normal mode
      if ((test >= STEP_ONE_LOWER_BOUND) && (test <= STEP_ONE_HIGHER_BOUND) && (false_positive)){
        current_state++;
      } else {
        current_state = 0;
        break;
      }

    case 1:	// Step 1: Consecutive values between 800 and 1000 (4 or more)
      if ((test >= STEP_ONE_LOWER_BOUND) && (test <= STEP_ONE_HIGHER_BOUND)){
        step_1_counter++;
        break;
      } else if (step_1_counter >= STEP_ONE_SAMPLES){
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "Step 1 has passed: last value is = %d...counter = %d\n", test, step_1_counter);
        step_1_counter = 0;
        current_state++;
      } else {
        step_1_counter = 0;
        current_state = 0;
        break;
      }

    case 2:	// Step 2: Is there any value greater than 500 in the next second?
      if (test >= STEP_TWO_LOWER_BOUND){
        step_2_flag = true;
      }
      step_2_counter++;
      if (step_2_counter >= STEP_TWO_SAMPLES){
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "Step 2 has passed: Checked 1 second (x>500): last value is = %d...counter = %d\n", test, step_2_counter);
        step_2_counter = 0;
        if (step_2_flag){
          step_2_flag = false;
          current_state++;
          break;
        } else {
          current_state = 0;
          break;
        }
      } else {
        break;
      }

    case 3:	// Step 3: Check inactivity 2 seconds (all 50 values less than 100?)
      if (test >= STEP_THREE_HIGHER_BOUND){
        step_3_flag = true;
      }
      step_3_counter++;
      if (step_3_counter >= STEP_THREE_SAMPLES){
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "Step 3 has passed: Checked 2 seconds (x<100?): last value is = %d...counter = %d\n", test, step_3_counter);
        step_3_counter = 0;
        if (step_3_flag){
          step_3_flag = false;
          current_state++;
          break;
        } else {
          current_state += 2;
        }
      } else {
        break;
      }

    case 4:	// Step 4: Recheck inactivity 2 seconds (all 50 values less than 100?)
      if (test >= STEP_THREE_HIGHER_BOUND){
        step_4_flag = true;
      }
      step_4_counter++;
      if (step_4_counter >= STEP_THREE_SAMPLES){
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "Step 4 has passed: Re-checked 2 seconds (x<100?): last value is = %d...counter = %d\n", test, step_4_counter);
        step_4_counter = 0;
        if (step_4_flag){
          step_4_flag = false;
          current_state = 0;
          break;
        } else {
          current_state++;
        }
      } else {
        break;
      }

    case 5:	// Step 5: Start Countdown
      //APP_LOG(APP_LOG_LEVEL_DEBUG, "Step 5 has passed: Countdown has started\n");
      false_positive = false;
      event_fall = true;
      text_layer_set_font(text_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_BOLD_SUBSET_49)));
      display_countdown(10);
      report_countdown();
      cntdown_ctr++;
      text_layer_set_text(text_layer_up, "Fall?");
      set_countdown();
      current_state = 0;	// Reset current_state to zero
  }

  set_timer();			// Reset timer function
}



void accel_data_handler(AccelData *data, uint32_t num_samples) {
  // Do nothing!
}



/*
	Report that a fall has happened!!!
*/
static void report_fall(void) {
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "SeizeAlert is datalogging a fall\n");
  event_fall = false;
  SeizureData *seizure_data = &s_seizure_datas[0];
  time_t now = time(NULL);
  data_logging_log(seizure_data->logging_session, (uint8_t *)&now, 1);
  data_logging_finish(seizure_data->logging_session);

  seizure_data->logging_session = data_logging_create(SEIZURE_LOG_TAGS[0], DATA_LOGGING_UINT, 4, false);
}


/*
	Report that countdown has started!!!
*/
static void report_countdown(void) {
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "SeizeAlert is datalogging a countdown\n");
  SeizureData *seizure_data = &s_seizure_datas[2];
  time_t now = time(NULL);
  data_logging_log(seizure_data->logging_session, (uint8_t *)&now, 1);
  data_logging_finish(seizure_data->logging_session);

  seizure_data->logging_session = data_logging_create(SEIZURE_LOG_TAGS[2], DATA_LOGGING_UINT, 4, false);
}



void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  // Do nothing
}



void display_countdown(int count){
  set_seizealert_screen();
  snprintf(text_buffer, 124, "%d", count);
  text_layer_set_text(text_layer, text_buffer);
}



void set_seizealert_screen(void){
  text_layer_set_background_color(text_layer, GColorBlack);
  text_layer_set_background_color(text_layer_up, GColorBlack);
}



void set_watchface_screen(void){
  text_layer_set_background_color(text_layer, GColorClear);
  text_layer_set_background_color(text_layer_up, GColorClear);
}



/*
	This is the handler function for the false
	positive button (SELECT button on Pebble).
*/
static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  set_false_alarm_event();
}


/*
	This function makes sure seizealert is running 
	in normal state.
*/
void set_false_alarm_event(void){
  false_positive = true;
  event_fall = false;
  cntdown_ctr = 0;

  current_state = 0;
  step_1_counter = 0;
  step_2_counter = 0;
  step_2_flag = false;
  step_3_counter = 0;
  step_3_flag = false;
  step_4_counter = 0;
  step_4_flag = false;

  text_layer_set_font(text_layer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
  set_watchface_screen();
  text_layer_set_text(text_layer, "");
  text_layer_set_text(text_layer_up, "");
}



static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
}



static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
}






/////////////////////////////////////////// WatchFace Logic /////////////////////////////////////////////


void line_layer_update_callback(Layer *layer, GContext* ctx) {
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}

void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
  // Need to be static because they're used by the system later.
  static char time_text[] = "00:00";
  static char date_text[] = "Xxxxxxxxx 00";

  char *time_format;


  // TODO: Only update the date when it's changed.
  strftime(date_text, sizeof(date_text), "%B %e", tick_time);
  text_layer_set_text(text_date_layer, date_text);


  if (clock_is_24h_style()) {
    time_format = "%R";
  } else {
    time_format = "%I:%M";
  }

  strftime(time_text, sizeof(time_text), time_format, tick_time);

  // Kludge to handle lack of non-padded hour format string
  // for twelve hour clock.
  if (!clock_is_24h_style() && (time_text[0] == '0')) {
    memmove(time_text, &time_text[1], sizeof(time_text) - 1);
  }

  text_layer_set_text(text_time_layer, time_text);
}



/*
* Battery icon callback handler
*/
static void battery_layer_update_callback(Layer *layer, GContext *ctx) {

	graphics_context_set_compositing_mode(ctx, GCompOpAssign);
	if (!battery_plugged) {
		graphics_draw_bitmap_in_rect(ctx, icon_battery, GRect(0, 0, 24, 12));
		graphics_context_set_stroke_color(ctx, GColorBlack);
		graphics_context_set_fill_color(ctx, GColorWhite);
		graphics_fill_rect(ctx, GRect(7, 4, (uint8_t)((battery_level / 100.0) * 11.0), 4), 0, GCornerNone);
	} else {
		graphics_draw_bitmap_in_rect(ctx, icon_battery_charge, GRect(0, 0, 24, 12));
	}
}


/*
* Battery state change
*/
static void battery_state_handler(BatteryChargeState charge) {
	battery_level = charge.charge_percent;
	battery_plugged = charge.is_plugged;
	layer_mark_dirty(battery_layer);
}


/*
* Bluetooth connection status
*/
static void bluetooth_state_handler(bool connected) {
	layer_set_hidden(bitmap_layer_get_layer(bluetooth_layer), !connected);
}


/////////////////////////////////////////// Standard Logic (Init/Deinit) /////////////////////////////////////////////

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}



static void window_load(Window *window) {

  // init tap service
  accel_service_set_sampling_rate(ACCEL_SAMPLING_25HZ);
  accel_tap_service_subscribe(&accel_tap_handler);
  window_set_background_color(window, GColorBlack);
  Layer *window_layer = window_get_root_layer(window);

  
  // Battery Layer
  icon_battery = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_ICON);
  icon_battery_charge = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_CHARGE);

  BatteryChargeState initial = battery_state_service_peek();
  battery_level = initial.charge_percent;
  battery_plugged = initial.is_plugged;
  battery_layer = layer_create(GRect(0,3,24,12)); //24*12
  layer_set_update_proc(battery_layer, &battery_layer_update_callback);
  layer_add_child(window_layer, battery_layer);

  // Bluetooth Layer
  bluetooth_layer = bitmap_layer_create(GRect(25, 3, 9, 12));
  layer_add_child(window_layer, bitmap_layer_get_layer(bluetooth_layer));
  bluetooth_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BLUETOOTH_ICON);
  bitmap_layer_set_bitmap(bluetooth_layer, bluetooth_bitmap);
  layer_set_hidden(bitmap_layer_get_layer(bluetooth_layer), !bluetooth_connection_service_peek());

  // Welcome User
  GRect bounds = layer_get_bounds(window_layer);
  seizealert_layer = text_layer_create((GRect) { .origin = { 0, 13 }, .size = { (bounds.size.w - 40), (bounds.size.h - 40) } });
  text_layer_set_text_color(seizealert_layer, GColorWhite);
  text_layer_set_background_color(seizealert_layer, GColorClear);
  text_layer_set_font(seizealert_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  
  text_layer_set_overflow_mode(seizealert_layer, GTextOverflowModeWordWrap);

  text_layer_set_text_alignment(seizealert_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(seizealert_layer));
  //text_layer_set_text(seizealert_layer, "SeizeAlert v2.0\nSeizeAlert Status:\n-Running");
  
  seizealert_logo = gbitmap_create_with_resource(RESOURCE_ID_PEBBLE_LOGO);		// SeizeAlert logo
  logo_layer = bitmap_layer_create(GRect(7,13,(130),(60))); 	// Layer size (width, height)
  layer_add_child(window_layer, bitmap_layer_get_layer(logo_layer));
  bitmap_layer_set_bitmap(logo_layer, seizealert_logo);

  // WatchFace Layers
  text_date_layer = text_layer_create(GRect(8, 68, 144-8, 168-68));
  text_layer_set_text_color(text_date_layer, GColorWhite);
  text_layer_set_background_color(text_date_layer, GColorClear);
  text_layer_set_font(text_date_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_21)));
  layer_add_child(window_layer, text_layer_get_layer(text_date_layer));

  text_time_layer = text_layer_create(GRect(7, 92, 144-7, 168-92));
  text_layer_set_text_color(text_time_layer, GColorWhite);
  text_layer_set_background_color(text_time_layer, GColorClear);
  text_layer_set_font(text_time_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_BOLD_SUBSET_49)));
  layer_add_child(window_layer, text_layer_get_layer(text_time_layer));

  GRect line_frame = GRect(8, 97, 139, 2);
  line_layer = layer_create(line_frame);
  layer_set_update_proc(line_layer, line_layer_update_callback);
  layer_add_child(window_layer, line_layer);

  // initialize SeizeAlert window Lower layer
  text_layer = text_layer_create((GRect) { .origin = { 0, 40 }, .size = { bounds.size.w, (bounds.size.h - 40) } });
  text_layer_set_text_color(text_layer, GColorWhite);
  text_layer_set_background_color(text_layer, GColorClear);
  text_layer_set_font(text_layer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
  
  text_layer_set_overflow_mode(text_layer, GTextOverflowModeWordWrap);

  text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(text_layer));

  // initialize SeizeAlert window Upper layer
  text_layer_up = text_layer_create((GRect) { .origin = { 0, 0 }, .size = { bounds.size.w, 40 } });
  text_layer_set_text_color(text_layer_up, GColorWhite);
  text_layer_set_background_color(text_layer_up, GColorClear);
  text_layer_set_font(text_layer_up, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  
  text_layer_set_overflow_mode(text_layer_up, GTextOverflowModeWordWrap);

  text_layer_set_text_alignment(text_layer_up, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(text_layer_up));

  // Init SeizeAlert data
  init_seizure_datas();

  // Subscribe Battery and Bluetooth handlers
  battery_state_service_subscribe(&battery_state_handler);
  bluetooth_connection_service_subscribe(bluetooth_state_handler);
}



static void window_unload(Window *window) {
  text_layer_destroy(text_layer);
  text_layer_destroy(text_layer_up);
  text_layer_destroy(text_date_layer);
  text_layer_destroy(text_time_layer);

  bitmap_layer_destroy(bluetooth_layer);
  layer_destroy(battery_layer);
  gbitmap_destroy(icon_battery);
  gbitmap_destroy(icon_battery_charge);
  gbitmap_destroy(bluetooth_bitmap);

  deinit_seizure_datas();
}



static void init_seizure_datas(void) {
  for (int i = 0; i < 3; i++) {
    SeizureData *seizure_data = &s_seizure_datas[i];
    seizure_data->logging_session = data_logging_create(SEIZURE_LOG_TAGS[i], DATA_LOGGING_UINT, 4, false);
  }
}



static void deinit_seizure_datas(void) {
  for (int i = 0; i < 2; i++) {
    SeizureData *seizure_data = &s_seizure_datas[i];
    data_logging_finish(seizure_data->logging_session);
  }
}



static void init(void) {
  window = window_create();
  window_set_click_config_provider(window, click_config_provider);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  const bool animated = true;
  window_stack_push(window, animated);

  tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
  
  // Initialize Buffer at 20Hz
  set_timer();
}



static void deinit(void) {
  // deinit accel tap
  accel_tap_service_unsubscribe();
  tick_timer_service_unsubscribe();
  window_destroy(window);
}



int main(void) {
  init();  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "SeizeAlert Initializing...");
  app_event_loop();
  deinit();
}
