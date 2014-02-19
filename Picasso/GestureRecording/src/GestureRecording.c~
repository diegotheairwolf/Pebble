#include <pebble.h>
#include <GestureRecording.h>

#define MAX_ACCEL 4000
#define HISTORY_MAX 100

/////////////////////  Globals  //////////////////////////
static Window *window;
static TextLayer *text_layer;

char text_buffer[250];
int timer_frequency = 100;		// Time setup for timer function in milliseconds
static AppTimer *timer;
static int last_x = 0;

bool record_gesture = false;		// State of false positive

static AccelData history[HISTORY_MAX];	// Array of accelerometer data

//////////////////////////////////////////////////////////



/*
	This function sets the time to next call at 
	timer_frequency in milliseconds.
*/

static void set_timer() {
  if (record_gesture == true)timer = app_timer_register(timer_frequency, timer_callback, NULL);
}



/*
	This function peeks the accelerometer,
	and check if the "select" button has been
	pressed (false positive). If not, prints
	values to screen and sets timer to come
	back here again, using set_timer().
*/

static void timer_callback() {
  int secs = (10 - (last_x/10));

  // Get last value from accelerometer
  AccelData accel;
  accel.x = 0;
  accel.y = 0;
  accel.z = 0;
  accel_service_peek(&accel);

  // Save to history
  history[last_x].x = accel.x;
  history[last_x].y = accel.y;
  history[last_x].z = accel.z;
  last_x++;

  // Check if last record
  if (last_x < HISTORY_MAX){
    set_timer();			// Reset timer function
    snprintf(text_buffer, 124, "Reconding for\n%d seconds...", secs);
    text_layer_set_text(text_layer, text_buffer);
  } 
  if (last_x >= HISTORY_MAX){
    record_gesture = false;		// End recording
    last_x = 0;
    text_layer_set_text(text_layer, "Gesture Reconded\nsuccesfully");
  }
}



/*
	Process tap on ACCEL_AXIS_X, ACCEL_AXIS_Y or ACCEL_AXIS_Z
*/

void accel_tap_handler(AccelAxisType axis, int32_t direction) {
}



static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (record_gesture == false){
    vibes_short_pulse();
    text_layer_set_text(text_layer, "Reconding for\n10 seconds...");
    record_gesture = true;
    set_timer();
  }
}



static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
}



static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (record_gesture == false){
    text_layer_set_text(text_layer, "Logging data\nto console...");

    for (int i=0 ; i < HISTORY_MAX ; i++){
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Value: %d, X=%d, Y=%d, Z=%d", i, history[i].x, history[i].y, history[i].z);      
    }
    text_layer_set_text(text_layer, "Data logging\nsuccessful");
  }
}



static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}



static void window_load(Window *window) {
  // init tap service
  accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);
  accel_tap_service_subscribe(&accel_tap_handler);

  // initialize window
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  text_layer = text_layer_create((GRect) { .origin = { 0, 50 }, .size = { bounds.size.w, 100 } });
  text_layer_set_text(text_layer, "Press Select\nto record a\ngesture. Press\ndown to log data.");
  
  text_layer_set_overflow_mode(text_layer, GTextOverflowModeWordWrap);

  text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(text_layer));
}



static void window_unload(Window *window) {
  text_layer_destroy(text_layer);
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
}



static void deinit(void) {
  // deinit accel tap
  accel_tap_service_unsubscribe();  
  window_destroy(window);
}



int main(void) {
  init();  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);
  app_event_loop();
  deinit();
}
