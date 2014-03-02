#include <pebble.h>
#include <SeizeAlert.h>

#define MAX_ACCEL 4000
#define HISTORY_MAX 100

/////////////////////  Globals  //////////////////////////

static const uint32_t SEIZURE_LOG_TAGS[2] = { 0x5, 0xd }; // fall, seizure

static Window *window;
static TextLayer *text_layer;

char text_buffer[250];
int timer_frequency = 100;		// Time setup for timer function in milliseconds
static AppTimer *timer;
static int last_x = 0;

static int highest_x = 0;		// Variables used to get highest overall acceleration at any point
static int highest_y = 0;
static int highest_z = 0;
static int overall = 0;
static int temp_overall = 0;

bool false_positive = true;		// State of false positive

static int debugging_counter = 0;

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
	This function sets the time to next call at 
	timer_frequency in milliseconds.
*/

static void set_timer() {
  timer = app_timer_register(timer_frequency, timer_callback, NULL);
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

  // Save to history and increase counter
  history[last_x].x = accel.x;
  history[last_x].y = accel.y;
  history[last_x].z = accel.z;
  last_x++;
  if (last_x >= HISTORY_MAX) last_x = 0;

  // Log values to smartphone every 10 seconds
  if (last_x == (HISTORY_MAX-1)){
    					// TODO: Add logic after data logging has been enabled 
  }

  // Check for false positive, print accelerometer 
  // values to screen, and set timer again
  if (!false_positive){
    temp_overall = abs(accel.x) + (accel.y) + abs(accel.z);

    snprintf(text_buffer, 124, "Time: %d\n X:%d,Y:%d,Z:%d", last_x, accel.x, accel.y, accel.z);

    APP_LOG(APP_LOG_LEVEL_DEBUG, text_buffer);

    if (temp_overall > overall){
      overall = temp_overall;
      highest_x = accel.x;
      highest_y = accel.y;
      highest_z = accel.z;
    } else {
      overall = overall;
    }
    overall = ((temp_overall > overall) ? temp_overall : overall);

    //vibes_short_pulse();
    snprintf(text_buffer, 125, "X:%d, Y:%d, Z:%d \n Overall:%d \n X:%d, Y:%d, Z:%d", accel.x, accel.y, accel.z, overall, highest_x, highest_y, highest_z);

    text_layer_set_text(text_layer, text_buffer);
    set_timer();	// Reset timer function
  }
}



void accel_data_handler(AccelData *data, uint32_t num_samples) {
  // Process 10 events - every 1 second
  
}



static void report_fall(void) {
  // A fall has happened!
  SeizureData *seizure_data = &s_seizure_datas[0];
  time_t now = time(NULL);
  data_logging_log(seizure_data->logging_session, (uint8_t *)&now, 1);
  data_logging_finish(seizure_data->logging_session);

  seizure_data->logging_session = data_logging_create(SEIZURE_LOG_TAGS[0], DATA_LOGGING_UINT, 4, false);
}



/*
	Process tap on ACCEL_AXIS_X, ACCEL_AXIS_Y or ACCEL_AXIS_Z
	Direction is 1 or -1.
*/

void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  // Initialize timer routine
  if (false_positive == true){
    set_timer();
    report_fall();
    false_positive = false;
  }
  //text_layer_set_text(text_layer, "Shaked");
}



static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  // False positive!!!
  false_positive = true;
  text_layer_set_text(text_layer, "False Positive \n shake it \n again");

  overall = 0;
  highest_x = 0;
  highest_y = 0;
  highest_z = 0;
}



static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
//  text_layer_set_text(text_layer, "Up");
}



static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
//  text_layer_set_text(text_layer, "Down");
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

  //accel_data_service_subscribe(0, &accel_data_handler);

  // initialize window
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  text_layer = text_layer_create((GRect) { .origin = { 0, 50 }, .size = { bounds.size.w, 100 } });
  text_layer_set_text(text_layer, "Try to \n shake it \n hard");
  
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
}



static void deinit(void) {
  // deinit accel batches
//  accel_data_service_unsubscribe();

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
