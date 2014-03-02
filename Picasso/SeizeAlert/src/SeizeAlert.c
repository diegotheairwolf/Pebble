#include <pebble.h>
#include <pebble_fonts.h>
#include <SeizeAlert.h>

#define HISTORY_MAX 20			// Check buffer every 2 seconds (20 accel data at 10Hz)
#define FALL_THRESHSOLD 1000
#define SEIZURE_THRESHSOLD 800
#define SEIZURE_SHAKING 3
#define ALERT_WINDOW 10

/////////////////////  Globals  //////////////////////////

static const uint32_t SEIZURE_LOG_TAGS[2] = { 0x5, 0xd }; // fall, seizure

static Window *window;
static TextLayer *text_layer;

static int cntdown_ctr = 0;

char text_buffer[250];
int timer_frequency = 100;		// Time setup for timer function in milliseconds
int countdown_frequency = 1000;		// Time setup for countdown function in milliseconds
static AppTimer *timer;
static int last_x = 0;

bool false_positive = true;		// State of false positive
bool event_seizure = false;
bool event_fall = false;
int shake_counter = 0;

static AccelData history[HISTORY_MAX];	// Array of accelerometer data

typedef struct {
  uint32_t tag;
  TextLayer *text_layer;
  char text[20];
  DataLoggingSessionRef logging_session;
  int count;
  GBitmap *bitmap;
} SeizureData;

static SeizureData s_seizure_datas[2]; // 0 = fall, 1 = seizure

//////////////////////////////////////////////////////////



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
      if (event_seizure) {
        report_seizure();
        text_layer_set_text(text_layer, "A seizure has been\nreported to\nyour phone.");
      } else {
        report_fall();
        text_layer_set_text(text_layer, "A fall has been\nreported to\nyour phone.");
      }
      false_positive = true;
      event_seizure = false;
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
  // Get last value from accelerometer
  AccelData accel;
  accel.x = 0;
  accel.y = 0;
  accel.z = 0;
  accel_service_peek(&accel);

  // Save to history buffer and increase counter
  history[last_x].x = accel.x;
  history[last_x].y = accel.y;
  history[last_x].z = accel.z;
  last_x++;

  // Check if last value on buffer
  if (last_x >= HISTORY_MAX) {
    last_x = 0;
    test_buffer_vals(); 	// Test all buffer values
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
  event_seizure = false;
  event_fall = false;
  SeizureData *seizure_data = &s_seizure_datas[0];
  time_t now = time(NULL);
  data_logging_log(seizure_data->logging_session, (uint8_t *)&now, 1);
  data_logging_finish(seizure_data->logging_session);

  seizure_data->logging_session = data_logging_create(SEIZURE_LOG_TAGS[0], DATA_LOGGING_UINT, 4, false);
}



/*
	Report that a seizure has happened!!!
*/
static void report_seizure(void) {
  event_seizure = false;
  event_fall = false;
  SeizureData *seizure_data = &s_seizure_datas[1];
  time_t now = time(NULL);
  data_logging_log(seizure_data->logging_session, (uint8_t *)&now, 1);
  data_logging_finish(seizure_data->logging_session);

  seizure_data->logging_session = data_logging_create(SEIZURE_LOG_TAGS[1], DATA_LOGGING_UINT, 4, false);
}



void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  // Do nothing
}



/*
	This functions checks all values on the buffer and
	tests for a certain treshold (constant FALL_THRESHSOLD).
*/
void test_buffer_vals(void){
  int test, x, y, z;
  for (int i=0 ; i < HISTORY_MAX ; i++){
    x = (history[i].x) * (history[i].x);
    y = (history[i].y) * (history[i].y);
    z = (history[i].z) * (history[i].z);
    test = (int)(abs(my_sqrt(x + y + z)-1000));		// int(abs(sqrt(x^2 + y^2 + z^2)-1000))

    if ((test > FALL_THRESHSOLD)  && false_positive) {		// Trigger countdown of fall event
      event_fall = true;
    }

    if ((test > SEIZURE_THRESHSOLD)  && false_positive) {	// Trigger countdown of fall event
      shake_counter++;
      if (shake_counter >= SEIZURE_SHAKING) event_seizure = true;
    }
  }
  
  if ((event_fall || event_seizure) && false_positive) {
    false_positive = false;
    shake_counter = 0;
    text_layer_set_font(text_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
    display_countdown(10);
    cntdown_ctr++;
    set_countdown();
  }
}



void display_countdown(int count){
  snprintf(text_buffer, 124, "%d", count);
  text_layer_set_text(text_layer, text_buffer);
}



/*
	This is the handler function for the false
	positive button (SELECT button on Pebble).
*/
static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  false_positive = true;
  event_seizure = false;
  event_fall = false;
  shake_counter = 0;
  cntdown_ctr = 0;
  text_layer_set_font(text_layer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
  text_layer_set_text(text_layer, "False Positive \n shake it \n again");
}



static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
}



static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
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

  text_layer = text_layer_create((GRect) { .origin = { 0, 40 }, .size = { bounds.size.w, 100 } });
  text_layer_set_font(text_layer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
  text_layer_set_text(text_layer, "Program running\nat normal state.");
  
  text_layer_set_overflow_mode(text_layer, GTextOverflowModeWordWrap);

  text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(text_layer));

  // init SeizeAlert data
  init_seizure_datas();
}



static void window_unload(Window *window) {
  text_layer_destroy(text_layer);
  deinit_seizure_datas();
}



static void init_seizure_datas(void) {
  for (int i = 0; i < 2; i++) {
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
  
  // Initialize Buffer at 20Hz
  set_timer();
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
