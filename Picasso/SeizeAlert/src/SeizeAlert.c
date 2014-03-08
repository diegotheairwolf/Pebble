#include <pebble.h>
#include <pebble_fonts.h>
#include <SeizeAlert.h>

#define HISTORY_MAX 20			// Check buffer every 2 seconds (20 accel data at 10Hz)
#define FALL_THRESHSOLD 1200
#define SEIZURE_THRESHSOLD 1000
#define SEIZURE_SHAKING 3
#define ALERT_WINDOW 10

//////////////////////////////////////////  Globals  ///////////////////////////////////////////////

static const uint32_t SEIZURE_LOG_TAGS[3] = { 0x5, 0xd, 0xe }; // fall, seizure

// Watch layers
TextLayer *text_date_layer;
TextLayer *text_time_layer;
Layer *line_layer;

// SeizeAlert layers
static Window *window;
static TextLayer *text_layer;
static TextLayer *text_layer_up;

// SeizeAlert logic globals
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
      if (event_seizure) {
        report_seizure();
        text_layer_set_text(text_layer_up, "SeizeAlert!!!");
        text_layer_set_text(text_layer, "A seizure has been\nreported to\nyour phone.");
      } else {
        report_fall();
        text_layer_set_text(text_layer_up, "SeizeAlert!!!");
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



/*
	Report that countdown has started!!!
*/
static void report_countdown(void) {
  SeizureData *seizure_data = &s_seizure_datas[2];
  time_t now = time(NULL);
  data_logging_log(seizure_data->logging_session, (uint8_t *)&now, 1);
  data_logging_finish(seizure_data->logging_session);

  seizure_data->logging_session = data_logging_create(SEIZURE_LOG_TAGS[2], DATA_LOGGING_UINT, 4, false);
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
    text_layer_set_font(text_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_BOLD_SUBSET_49)));
    display_countdown(10);
    report_countdown();
    cntdown_ctr++;
    if (event_seizure){
      text_layer_set_text(text_layer_up, "Seizure?");
    } else {
      text_layer_set_text(text_layer_up, "Fall?");
    }
    set_countdown();
  }
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
  false_positive = true;
  event_seizure = false;
  event_fall = false;
  shake_counter = 0;
  cntdown_ctr = 0;
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





/////////////////////////////////////////// Standard Logic (Init/Deinit) /////////////////////////////////////////////

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}



static void window_load(Window *window) {
  // init tap service
  accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);
  accel_tap_service_subscribe(&accel_tap_handler);

  // WatchFace Layers
  window_set_background_color(window, GColorBlack);

  Layer *window_layer = window_get_root_layer(window);

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
  GRect bounds = layer_get_bounds(window_layer);

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

  // init SeizeAlert data
  init_seizure_datas();
}



static void window_unload(Window *window) {
  text_layer_destroy(text_layer);
  text_layer_destroy(text_layer_up);
  text_layer_destroy(text_date_layer);
  text_layer_destroy(text_time_layer);

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
