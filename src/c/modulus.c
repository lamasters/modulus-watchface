#include <pebble.h>

static Window *s_window;
static TextLayer *s_time_layer;
static TextLayer *s_day_layer;
static TextLayer *s_date_layer;
static TextLayer *s_loc_layer;
static TextLayer *s_temperature_layer;
static TextLayer *s_high_layer;
static TextLayer *s_low_layer;
static Layer *s_temp_arc_layer;
static Layer *s_health_layer;
static Layer *s_battery_layer;
static GFont rounded_font;
static GPath *s_bolt_path = NULL;
static const GPathInfo BOLT_PATH_INFO = {
  .num_points = 6,
  .points = (GPoint []) {{12, 0}, {11, 10}, {16, 10}, {8, 24}, {9, 14}, {4, 14}}
};

static GColor background_color;
static GColor text_color;
static GColor accent_color;

static int32_t step_count = 0;
static int32_t step_goal = 5000;
static int32_t move_minutes = 0;
static int32_t move_goal = 30;
static int32_t active_calories = 0;
static int32_t active_goal = 300;
static int32_t battery_level = 100;
static int32_t temperature = 20;
static int32_t low_temp = 15;
static int32_t high_temp = 25;
static char temp_buffer[8];
static char low_buffer[8];
static char high_buffer[8];
static char location[64] = "My Location";
static int32_t weather_update_interval = 30;

static const int PADDING = 4;

static void request_weather() {
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_uint8(iter, 0, 0);
  app_message_outbox_send();
}

static void inbox_recv_callback(DictionaryIterator *iterator, void *context) {
  Tuple *temp_tuple = dict_find(iterator, MESSAGE_KEY_CUR_TEMP);
  Tuple *low_tuple = dict_find(iterator, MESSAGE_KEY_LOW_TEMP);
  Tuple *high_tuple = dict_find(iterator, MESSAGE_KEY_HIGH_TEMP);

  if (temp_tuple && low_tuple && high_tuple) {
    temperature = temp_tuple->value->int32;
    low_temp = low_tuple->value->int32;
    high_temp = high_tuple->value->int32;
    snprintf(temp_buffer, sizeof(temp_buffer), "%d", (int)temperature);
    snprintf(low_buffer, sizeof(low_buffer), "%d", (int)low_temp);
    snprintf(high_buffer, sizeof(high_buffer), "%d", (int)high_temp);
    APP_LOG(APP_LOG_LEVEL_INFO, "Temperature: %d", (int)temperature);
    APP_LOG(APP_LOG_LEVEL_INFO, "Low: %d", (int)low_temp);
    APP_LOG(APP_LOG_LEVEL_INFO, "High: %d", (int)high_temp);
    text_layer_set_text(s_temperature_layer, temp_buffer);
    text_layer_set_text(s_low_layer, low_buffer);
    text_layer_set_text(s_high_layer, high_buffer);
  }

  Tuple *location_tuple = dict_find(iterator, MESSAGE_KEY_LOCATION);
  if (location_tuple) {
    snprintf(location, sizeof(location), "%s", location_tuple->value->cstring);
    text_layer_set_text(s_loc_layer, location);
  }

  Tuple *update_interval_tuple = dict_find(iterator, MESSAGE_KEY_UPDATE_INTERVAL);
  if (update_interval_tuple) {
    weather_update_interval = update_interval_tuple->value->int32;
    persist_write_int(MESSAGE_KEY_UPDATE_INTERVAL, weather_update_interval);
  }

  Tuple *api_key = dict_find(iterator, MESSAGE_KEY_OWM_API_KEY);
  if (api_key) {
    request_weather();
  }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context)
{
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context)
{
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context)
{
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

static void update_health_metrics() {
  step_count = health_service_sum_today(HealthMetricStepCount);
  move_minutes = health_service_sum_today(HealthMetricActiveSeconds) / 60;
  active_calories = health_service_sum_today(HealthMetricActiveKCalories);
}

static void update_time() {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  static char time_buffer[] = "00:00";

  strftime(time_buffer, sizeof(time_buffer), clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);
  text_layer_set_text(s_time_layer, time_buffer);

  static char day_buffer[] = "Mon";
  strftime(day_buffer, sizeof(day_buffer), "%a", tick_time);
  text_layer_set_text(s_day_layer, day_buffer);

  static char date_buffer[] = "01";
  strftime(date_buffer, sizeof(date_buffer), "%d", tick_time);
  text_layer_set_text(s_date_layer, date_buffer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  if (tick_time->tm_min % weather_update_interval == 0) {
    request_weather();
  }
  update_time();
  if (tick_time->tm_min % 5 == 0) {
    update_health_metrics();
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "Steps: %d", (int)step_count);
  APP_LOG(APP_LOG_LEVEL_INFO, "Move: %d", (int)move_minutes);
  APP_LOG(APP_LOG_LEVEL_INFO, "Active: %d", (int)active_calories);
  layer_mark_dirty(s_health_layer);
}

static void temperature_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_stroke_width(ctx, 4);
  graphics_context_set_stroke_color(ctx, accent_color);
  graphics_draw_circle(ctx, GPoint(20, 20), 18);
  graphics_context_set_fill_color(ctx, background_color);
  graphics_fill_radial(ctx, GRect(0, 0, 41, 41), GOvalScaleModeFitCircle, 6, DEG_TO_TRIGANGLE(127), DEG_TO_TRIGANGLE(233));
}

static void health_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, accent_color);
  int step_angle = (int)((float)step_count / (float)step_goal * 360.0);
  graphics_fill_radial(ctx, GRect(0, 0, 41, 41), GOvalScaleModeFitCircle, 5, 0, DEG_TO_TRIGANGLE(step_angle));
  int move_angle = (int)((float)move_minutes / (float)move_goal * 360.0);
  graphics_fill_radial(ctx, GRect(7, 7, 27, 27), GOvalScaleModeFitCircle, 5, 0, DEG_TO_TRIGANGLE(move_angle));
  int active_angle = (int)((float)active_calories / (float)active_goal * 360.0);
  graphics_fill_radial(ctx, GRect(14, 14, 13, 13), GOvalScaleModeFitCircle, 5, 0, DEG_TO_TRIGANGLE(active_angle));
}

static void battery_update_proc(Layer *layer, GContext *ctx) {
  if (battery_level > 20) {
    graphics_context_set_fill_color(ctx, accent_color);
  } else {
    graphics_context_set_fill_color(ctx, GColorSunsetOrange);
  }
  int angle = (int)((float)(100 - battery_level) / 100.0 * 360.0);
  graphics_fill_radial(ctx, GRect(0, 0, 41, 41), GOvalScaleModeFitCircle, 5, DEG_TO_TRIGANGLE(angle), DEG_TO_TRIGANGLE(360));
  gpath_draw_filled(ctx, s_bolt_path);
}

static void battery_callback(BatteryChargeState state) {
  battery_level = state.charge_percent;
  layer_mark_dirty(s_battery_layer);
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_time_layer = text_layer_create(GRect(PADDING, 20, bounds.size.w - PADDING * 2, 52));
  text_layer_set_text(s_time_layer, "");
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentRight);
  text_layer_set_font(s_time_layer, rounded_font);
  text_layer_set_text_color(s_time_layer, text_color);
  text_layer_set_background_color(s_time_layer, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));

  s_day_layer = text_layer_create(GRect(bounds.size.w - 65, 0, 37, 21));
  text_layer_set_text(s_day_layer, "");
  text_layer_set_text_alignment(s_day_layer, GTextAlignmentLeft);
  text_layer_set_font(s_day_layer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
  text_layer_set_text_color(s_day_layer, accent_color);
  text_layer_set_background_color(s_day_layer, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(s_day_layer));

  s_date_layer = text_layer_create(GRect(PADDING, 0, bounds.size.w - PADDING * 2, 21));
  text_layer_set_text(s_date_layer, "");
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentRight);
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
  text_layer_set_text_color(s_date_layer, text_color);
  text_layer_set_background_color(s_date_layer, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));

  s_loc_layer = text_layer_create(GRect(PADDING + 26, 85, bounds.size.w - PADDING * 2, 21));
  text_layer_set_text(s_loc_layer, location);
  text_layer_set_text_alignment(s_loc_layer, GTextAlignmentLeft);
  text_layer_set_font(s_loc_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_color(s_loc_layer, text_color);
  text_layer_set_background_color(s_loc_layer, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(s_loc_layer));

  s_temp_arc_layer = layer_create(GRect(PADDING + 1, 115, 41, 41));
  layer_set_update_proc(s_temp_arc_layer, temperature_update_proc);
  layer_add_child(window_layer, s_temp_arc_layer);

  s_temperature_layer = text_layer_create(GRect(PADDING + 2, 122, 41, 41));
  text_layer_set_text(s_temperature_layer, "22");
  text_layer_set_text_alignment(s_temperature_layer, GTextAlignmentCenter);
  text_layer_set_font(s_temperature_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_color(s_temperature_layer, text_color);
  text_layer_set_background_color(s_temperature_layer, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(s_temperature_layer));

  s_low_layer = text_layer_create(GRect(PADDING + 8, 145, 20, 15));
  text_layer_set_text(s_low_layer, "15");
  text_layer_set_text_alignment(s_low_layer, GTextAlignmentLeft);
  text_layer_set_font(s_low_layer, fonts_get_system_font(FONT_KEY_GOTHIC_09));
  text_layer_set_text_color(s_low_layer, accent_color);
  text_layer_set_background_color(s_low_layer, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(s_low_layer));
  
  s_high_layer = text_layer_create(GRect(PADDING + 15, 145, 20, 15));
  text_layer_set_text(s_high_layer, "25");
  text_layer_set_text_alignment(s_high_layer, GTextAlignmentRight);
  text_layer_set_font(s_high_layer, fonts_get_system_font(FONT_KEY_GOTHIC_09));
  text_layer_set_text_color(s_high_layer, accent_color);
  text_layer_set_background_color(s_high_layer, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(s_high_layer));

  s_health_layer = layer_create(GRect(PADDING * 2 + 41 + 3, 115, 41, 41));
  layer_set_update_proc(s_health_layer, health_update_proc);
  layer_add_child(window_layer, s_health_layer);

  s_bolt_path = gpath_create(&BOLT_PATH_INFO);
  gpath_move_to(s_bolt_path, GPoint(10, 8));
  s_battery_layer = layer_create(GRect(bounds.size.w - PADDING - 41, 115, 41, 41));
  layer_set_update_proc(s_battery_layer, battery_update_proc);
  layer_add_child(window_layer, s_battery_layer);
}

static void main_window_unload(Window *window) {
  layer_destroy(s_temp_arc_layer);
  text_layer_destroy(s_temperature_layer);
  text_layer_destroy(s_low_layer);
  text_layer_destroy(s_high_layer);
  layer_destroy(s_health_layer);
  layer_destroy(s_battery_layer);
  gpath_destroy(s_bolt_path);
  text_layer_destroy(s_loc_layer);
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_day_layer);
  text_layer_destroy(s_date_layer);
}

static void init(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });

  background_color = GColorBlack;
  text_color = GColorWhite;
  accent_color = GColorMediumAquamarine;

  rounded_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_QUICKSAND_51));

  const bool animated = true;
  window_stack_push(s_window, animated);
  window_set_background_color(s_window, background_color);

  app_message_register_inbox_received(inbox_recv_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);

  const int inbox_size = 256;
  const int outbox_size = 128;
  app_message_open(inbox_size, outbox_size);

  if (persist_exists(MESSAGE_KEY_UPDATE_INTERVAL)) {
    weather_update_interval = persist_read_int(MESSAGE_KEY_UPDATE_INTERVAL);
  }

  update_time();
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_callback);
  battery_level = battery_state_service_peek().charge_percent;
  update_health_metrics();
}

static void deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
