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
static BitmapLayer *s_condition_layer;
static GBitmap *s_condition_bitmap;
static GFont quicksand_45;
static GFont quicksand_15;
static GPath *s_bolt_path = NULL;
static const GPathInfo BOLT_PATH_INFO = {
    .num_points = 6,
    .points = (GPoint[]){{12, 0}, {11, 10}, {16, 10}, {8, 24}, {9, 14}, {4, 14}}};

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
static int weather_index = 0;

static const int PADDING = 4;
static const int32_t WEATHER_ICONS[6] = {
    RESOURCE_ID_CLEAR,
    RESOURCE_ID_CLOUD,
    RESOURCE_ID_FOG,
    RESOURCE_ID_RAIN,
    RESOURCE_ID_SNOW,
    RESOURCE_ID_STORM,
};

static void request_weather()
{
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_uint8(iter, 0, 0);
  app_message_outbox_send();
}

static void inbox_recv_callback(DictionaryIterator *iterator, void *context)
{
  Tuple *temp_tuple = dict_find(iterator, MESSAGE_KEY_CUR_TEMP);
  Tuple *low_tuple = dict_find(iterator, MESSAGE_KEY_LOW_TEMP);
  Tuple *high_tuple = dict_find(iterator, MESSAGE_KEY_HIGH_TEMP);

  if (temp_tuple && low_tuple && high_tuple)
  {
    temperature = temp_tuple->value->int32;
    low_temp = low_tuple->value->int32;
    high_temp = high_tuple->value->int32;
    persist_write_int(MESSAGE_KEY_CUR_TEMP, temperature);
    persist_write_int(MESSAGE_KEY_LOW_TEMP, low_temp);
    persist_write_int(MESSAGE_KEY_HIGH_TEMP, high_temp);
    snprintf(temp_buffer, sizeof(temp_buffer), "%d", (int)temperature);
    snprintf(low_buffer, sizeof(low_buffer), "%d", (int)low_temp);
    snprintf(high_buffer, sizeof(high_buffer), "%d", (int)high_temp);
    text_layer_set_text(s_temperature_layer, temp_buffer);
    text_layer_set_text(s_low_layer, low_buffer);
    text_layer_set_text(s_high_layer, high_buffer);
  }

  Tuple *conditions_tuple = dict_find(iterator, MESSAGE_KEY_CONDITIONS);
  if (conditions_tuple)
  {
    weather_index = conditions_tuple->value->int32;
    persist_write_int(MESSAGE_KEY_CONDITIONS, weather_index);
    gbitmap_destroy(s_condition_bitmap);
    s_condition_bitmap = gbitmap_create_with_resource(WEATHER_ICONS[weather_index]);
    bitmap_layer_set_bitmap(s_condition_layer, s_condition_bitmap);
    layer_mark_dirty(bitmap_layer_get_layer(s_condition_layer));
  }

  Tuple *location_tuple = dict_find(iterator, MESSAGE_KEY_LOCATION);
  if (location_tuple)
  {
    snprintf(location, sizeof(location), "%s", location_tuple->value->cstring);
    persist_write_string(MESSAGE_KEY_LOCATION_NAME, location_tuple->value->cstring);
    text_layer_set_text(s_loc_layer, location);
  }

  Tuple *update_interval_tuple = dict_find(iterator, MESSAGE_KEY_UPDATE_INTERVAL);
  if (update_interval_tuple)
  {
    weather_update_interval = update_interval_tuple->value->int32;
    persist_write_int(MESSAGE_KEY_UPDATE_INTERVAL, weather_update_interval);
  }

  Tuple *api_key = dict_find(iterator, MESSAGE_KEY_OWM_API_KEY);
  if (api_key)
  {
    request_weather();
  }

  Tuple *bg_colour_tuple = dict_find(iterator, MESSAGE_KEY_BACKGROUND_COLOUR);
  if (bg_colour_tuple)
  {
    background_color = GColorFromHEX(bg_colour_tuple->value->int32);
    persist_write_int(MESSAGE_KEY_BACKGROUND_COLOUR, bg_colour_tuple->value->int32);
    text_color = gcolor_legible_over(background_color);
    window_set_background_color(s_window, background_color);
    text_layer_set_text_color(s_time_layer, text_color);
    text_layer_set_text_color(s_date_layer, text_color);
    text_layer_set_text_color(s_loc_layer, text_color);
    text_layer_set_text_color(s_temperature_layer, text_color);
  }

  Tuple *accent_colour_tuple = dict_find(iterator, MESSAGE_KEY_ACCENT_COLOUR);
  if (accent_colour_tuple)
  {
    accent_color = GColorFromHEX(accent_colour_tuple->value->int32);
    persist_write_int(MESSAGE_KEY_ACCENT_COLOUR, accent_colour_tuple->value->int32);
    text_layer_set_text_color(s_low_layer, accent_color);
    text_layer_set_text_color(s_high_layer, accent_color);
    text_layer_set_text_color(s_day_layer, accent_color);
    layer_mark_dirty(s_temp_arc_layer);
    layer_mark_dirty(s_health_layer);
    layer_mark_dirty(s_battery_layer);
  }

  Tuple *step_goal_tuple = dict_find(iterator, MESSAGE_KEY_STEP_GOAL);
  if (step_goal_tuple)
  {
    step_goal = step_goal_tuple->value->int32;
    persist_write_int(MESSAGE_KEY_STEP_GOAL, step_goal);
  }

  Tuple *move_goal_tuple = dict_find(iterator, MESSAGE_KEY_MOVE_GOAL);
  if (move_goal_tuple)
  {
    move_goal = move_goal_tuple->value->int32;
    persist_write_int(MESSAGE_KEY_MOVE_GOAL, move_goal);
  }

  Tuple *active_goal_tuple = dict_find(iterator, MESSAGE_KEY_CAL_GOAL);
  if (active_goal_tuple)
  {
    active_goal = active_goal_tuple->value->int32;
    persist_write_int(MESSAGE_KEY_CAL_GOAL, active_goal);
  }

  if (step_goal_tuple || move_goal_tuple || active_goal_tuple)
  {
    layer_mark_dirty(s_health_layer);
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

static void update_health_metrics()
{
  step_count = health_service_sum_today(HealthMetricStepCount);
  move_minutes = health_service_sum_today(HealthMetricActiveSeconds) / 60;
  active_calories = health_service_sum_today(HealthMetricActiveKCalories);
}

static void update_time()
{
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

static void tick_handler(struct tm *tick_time, TimeUnits units_changed)
{
  if (tick_time->tm_min % weather_update_interval == 0)
  {
    request_weather();
  }
  update_time();
  if (tick_time->tm_min % 5 == 0)
  {
    update_health_metrics();
  }
  layer_mark_dirty(s_health_layer);
}

static void temperature_update_proc(Layer *layer, GContext *ctx)
{
  graphics_context_set_stroke_width(ctx, 4);
  graphics_context_set_stroke_color(ctx, accent_color);
  graphics_draw_circle(ctx, GPoint(20, 20), 18);
  graphics_context_set_fill_color(ctx, background_color);
  graphics_fill_radial(ctx, GRect(0, 0, 41, 41), GOvalScaleModeFitCircle, 6, DEG_TO_TRIGANGLE(127), DEG_TO_TRIGANGLE(233));
  int cur_temp = temperature;
  if (cur_temp < low_temp)
  {
    cur_temp = low_temp;
  }
  if (cur_temp > high_temp)
  {
    cur_temp = high_temp;
  }
  float relative_temp = (float)(cur_temp - low_temp) / (float)(high_temp - low_temp);
  float angle = 235 + (485 - 235) * relative_temp;
  int x = (int)(20 + 18.0 * sin_lookup(DEG_TO_TRIGANGLE(angle)) / TRIG_MAX_RATIO);
  int y = (int)(20 - 18.0 * cos_lookup(DEG_TO_TRIGANGLE(angle)) / TRIG_MAX_RATIO);
  graphics_context_set_fill_color(ctx, background_color);
  graphics_fill_circle(ctx, GPoint(x, y), 4);
  graphics_context_set_fill_color(ctx, text_color);
  graphics_fill_circle(ctx, GPoint(x, y), 2);
}

static void health_update_proc(Layer *layer, GContext *ctx)
{
  graphics_context_set_fill_color(ctx, GColorDarkGray);
  graphics_fill_radial(ctx, GRect(0, 0, 41, 41), GOvalScaleModeFitCircle, 5, 0, DEG_TO_TRIGANGLE(360));
  graphics_fill_radial(ctx, GRect(7, 7, 27, 27), GOvalScaleModeFitCircle, 5, 0, DEG_TO_TRIGANGLE(360));
  graphics_fill_radial(ctx, GRect(14, 14, 13, 13), GOvalScaleModeFitCircle, 5, 0, DEG_TO_TRIGANGLE(360));
  graphics_context_set_fill_color(ctx, accent_color);
  int step_angle = (int)((float)step_count / (float)step_goal * 360.0);
  graphics_fill_radial(ctx, GRect(0, 0, 41, 41), GOvalScaleModeFitCircle, 5, 0, DEG_TO_TRIGANGLE(step_angle));
  int move_angle = (int)((float)move_minutes / (float)move_goal * 360.0);
  graphics_fill_radial(ctx, GRect(7, 7, 27, 27), GOvalScaleModeFitCircle, 5, 0, DEG_TO_TRIGANGLE(move_angle));
  int active_angle = (int)((float)active_calories / (float)active_goal * 360.0);
  graphics_fill_radial(ctx, GRect(14, 14, 13, 13), GOvalScaleModeFitCircle, 5, 0, DEG_TO_TRIGANGLE(active_angle));
}

static void battery_update_proc(Layer *layer, GContext *ctx)
{
  if (battery_level > 20)
  {
    graphics_context_set_fill_color(ctx, accent_color);
  }
  else
  {
    graphics_context_set_fill_color(ctx, GColorSunsetOrange);
  }
  int angle = (int)((float)(100 - battery_level) / 100.0 * 360.0);
  graphics_fill_radial(ctx, GRect(0, 0, 41, 41), GOvalScaleModeFitCircle, 5, DEG_TO_TRIGANGLE(angle), DEG_TO_TRIGANGLE(360));
  gpath_draw_filled(ctx, s_bolt_path);
}

static void battery_callback(BatteryChargeState state)
{
  battery_level = state.charge_percent;
  layer_mark_dirty(s_battery_layer);
}

static void main_window_load(Window *window)
{
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_time_layer = text_layer_create(GRect(PADDING, 20, bounds.size.w - PADDING * 2, 52));
  text_layer_set_text(s_time_layer, "24");
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentRight);
  text_layer_set_font(s_time_layer, quicksand_45);
  text_layer_set_text_color(s_time_layer, text_color);
  text_layer_set_background_color(s_time_layer, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));

  s_day_layer = text_layer_create(GRect(bounds.size.w - text_layer_get_content_size(s_time_layer).w - PADDING, 0, 37, 21));
  text_layer_set_text(s_day_layer, "");
  text_layer_set_text_alignment(s_day_layer, GTextAlignmentRight);
  text_layer_set_font(s_day_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_color(s_day_layer, accent_color);
  text_layer_set_background_color(s_day_layer, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(s_day_layer));

  s_date_layer = text_layer_create(GRect(PADDING, 0, bounds.size.w - PADDING * 2, 21));
  text_layer_set_text(s_date_layer, "");
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentRight);
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_color(s_date_layer, text_color);
  text_layer_set_background_color(s_date_layer, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));

  s_condition_layer = bitmap_layer_create(GRect(PADDING + 5, 85, 21, 21));
  s_condition_bitmap = gbitmap_create_with_resource(WEATHER_ICONS[weather_index]);
  bitmap_layer_set_bitmap(s_condition_layer, s_condition_bitmap);
  bitmap_layer_set_alignment(s_condition_layer, GAlignCenter);
  bitmap_layer_set_compositing_mode(s_condition_layer, GCompOpSet);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_condition_layer));

  s_loc_layer = text_layer_create(GRect(PADDING + 30, 85, bounds.size.w - PADDING - 30, 21));
  text_layer_set_text(s_loc_layer, location);
  text_layer_set_text_alignment(s_loc_layer, GTextAlignmentLeft);
  text_layer_set_font(s_loc_layer, quicksand_15);
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

static void main_window_unload(Window *window)
{
  bitmap_layer_destroy(s_condition_layer);
  gbitmap_destroy(s_condition_bitmap);
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
  fonts_unload_custom_font(quicksand_45);
  fonts_unload_custom_font(quicksand_15);
}

static void init(void)
{
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
                                           .load = main_window_load,
                                           .unload = main_window_unload,
                                       });

  quicksand_45 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_QUICKSAND_45));
  quicksand_15 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_QUICKSAND_15));

  if (persist_exists(MESSAGE_KEY_UPDATE_INTERVAL))
  {
    weather_update_interval = persist_read_int(MESSAGE_KEY_UPDATE_INTERVAL);
  }
  if (persist_exists(MESSAGE_KEY_BACKGROUND_COLOUR))
  {
    background_color = GColorFromHEX(persist_read_int(MESSAGE_KEY_BACKGROUND_COLOUR));
    text_color = gcolor_legible_over(background_color);
  }
  else
  {
    background_color = GColorBlack;
    text_color = GColorWhite;
  }
  if (persist_exists(MESSAGE_KEY_ACCENT_COLOUR))
  {
    accent_color = GColorFromHEX(persist_read_int(MESSAGE_KEY_ACCENT_COLOUR));
  }
  else
  {
    accent_color = GColorMediumAquamarine;
  }
  if (persist_exists(MESSAGE_KEY_STEP_GOAL))
  {
    step_goal = persist_read_int(MESSAGE_KEY_STEP_GOAL);
  }
  if (persist_exists(MESSAGE_KEY_MOVE_GOAL))
  {
    move_goal = persist_read_int(MESSAGE_KEY_MOVE_GOAL);
  }
  if (persist_exists(MESSAGE_KEY_CAL_GOAL))
  {
    active_goal = persist_read_int(MESSAGE_KEY_CAL_GOAL);
  }
  if (persist_exists(MESSAGE_KEY_CONDITIONS))
  {
    weather_index = persist_read_int(MESSAGE_KEY_CONDITIONS);
  }
  if (persist_exists(MESSAGE_KEY_LOCATION_NAME))
  {
    persist_read_string(MESSAGE_KEY_LOCATION_NAME, location, sizeof(location));
  }
  if (persist_exists(MESSAGE_KEY_CUR_TEMP))
  {
    temperature = persist_read_int(MESSAGE_KEY_CUR_TEMP);
  }
  if (persist_exists(MESSAGE_KEY_LOW_TEMP))
  {
    low_temp = persist_read_int(MESSAGE_KEY_LOW_TEMP);
  }
  if (persist_exists(MESSAGE_KEY_HIGH_TEMP))
  {
    high_temp = persist_read_int(MESSAGE_KEY_HIGH_TEMP);
  }

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

  update_time();
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_callback);
  battery_level = battery_state_service_peek().charge_percent;
  update_health_metrics();
}

static void deinit(void)
{
  window_destroy(s_window);
}

int main(void)
{
  init();
  app_event_loop();
  deinit();
}
