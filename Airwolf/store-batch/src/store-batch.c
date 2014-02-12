#include <pebble.h>

static Window *window;
static TextLayer *text_layer;

/* 
   added this because turns out they're not defined.
   maybe you can just copy and paste the enum
   or maybe declare or include the enum
*/ 
#define DATA_LOG_SUCCESS 0
#define DATA_LOG_BUSY    1
#define DATA_LOG_FULL    2
#define DATA_LOG_NOT_FOUND 3
#define DATA_LOG_CLOSED    4
#define DATA_LOG_INVALID_PARAMS 5 

/*
typedef enum {
  DATA_LOG_SUCCESS = 0,
  DATA_LOG_BUSY, //! Someone else is writing to this log
  DATA_LOG_FULL, //! No more space to save data
  DATA_LOG_NOT_FOUND, //! The log does not exist
  DATA_LOG_CLOSED, //! The log was made inactive
  DATA_LOG_INVALID_PARAMS
} DataLoggingResult;
*/

void checkDataLoggingResult(DataLoggingResult r) {

  /*
  change the text_layers to something more useful
  than printing whatever out
  */

  if ( r == DATA_LOG_SUCCESS ) {
    text_layer_set_text(text_layer, "DATA_LOG_SUCCESS");
  } else if ( r == DATA_LOG_BUSY) {
    text_layer_set_text(text_layer, "DATA_LOG_BUSY");
  } else if ( r == DATA_LOG_FULL) {
    text_layer_set_text(text_layer, "DATA_LOG_FULL");
  } else if ( r == DATA_LOG_NOT_FOUND) {
    text_layer_set_text(text_layer, "DATA_LOG_NOT_FOUND");
  } else if ( r ==  DATA_LOG_CLOSED) {
    text_layer_set_text(text_layer, "DATA_LOG_CLOSED");
  } else if ( r == DATA_LOG_INVALID_PARAMS ) {
    text_layer_set_text(text_layer, "DATA_LOG_CLOSED");
  } else {
    text_layer_set_text(text_layer, "WTF");
  }
}

int flag = 0;

void log_data(uint16_t *sample) {
    
  /*
    data is flushed at end of each session
    try to data_loggin_create in init, do multiple 
    data_logging_finish, and see what happens
  */ 

  if (flag) {
    return;
  }
  flag = 1;
  DataLoggingSessionRef logging_session = data_logging_create(0xbeef, DATA_LOGGING_BYTE_ARRAY, sizeof(int16_t), false);
  DataLoggingResult r = data_logging_log(logging_session, sample, 30);
  checkDataLoggingResult(r);
  data_logging_finish(logging_session);
  
}

void accel_data_handler(AccelData *data, uint32_t num_samples) {

  if ( data[0].z == 0) {
    text_layer_set_text(text_layer, "Zero :|");
  } else if (  data[0].z > 0 ) {
    text_layer_set_text(text_layer, "Positive");
  } else if ( data[0].z < 0 ) {
    text_layer_set_text(text_layer, "Negative br");
  }

  uint16_t sample[30];

  uint16_t i = 0;
  uint16_t j = 0;
  for( ; i < num_samples; i += 1) {
    sample[j] = data[i].x;
    sample[j+1] = data[i].y;
    sample[j+2] = data[i].z;
    j += 3;
  }
  
  // **** lol, make sure to delete this line
  sample[0] += sample[3];

  // log dat brah
  log_data((uint16_t*)sample);
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(text_layer, "Select");
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(text_layer, "Arriba");
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(text_layer, "Down");
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  text_layer = text_layer_create((GRect) { .origin = { 0, 72 }, .size = { bounds.size.w, 20 } });
  text_layer_set_text(text_layer, "Press a button");
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

  // init accel batches service
  accel_data_service_subscribe(10, &accel_data_handler);
  accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);
  //datalogging service stuff
}

static void deinit(void) {
  // deinit accel batches service
  accel_data_service_unsubscribe();

  window_destroy(window);
}

int main(void) {
  init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

  app_event_loop();
  deinit();
}
