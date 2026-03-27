#include <pebble.h>

#define DIAL_COLOR GColorBlack
#define BEZEL_COLOR GColorFromRGB(210, 210, 210)
#define BEZEL_SHADOW_COLOR GColorFromRGB(92, 92, 92)
#define MARKER_COLOR GColorFromRGB(210, 210, 210)
#define TEXT_COLOR GColorFromRGB(184, 184, 184)
#define ACCENT_COLOR GColorFromRGB(204, 36, 36)
#define DATE_BOX_COLOR GColorFromRGB(196, 196, 196)
#define DATE_FILL_COLOR GColorBlack
#define SECOND_HAND_COLOR GColorFromRGB(216, 216, 216)
#define HAND_BASE_COLOR GColorFromRGB(190, 190, 190)
#define HAND_HIGHLIGHT_COLOR GColorFromRGB(228, 228, 228)

static Window *s_window;
static Layer *s_face_layer;

static GFont s_subtitle_font;
static GFont s_footer_font;
static GFont s_date_font;

static struct tm s_current_time;

static GPoint prv_point_on_circle(GPoint center, int32_t angle, int16_t radius) {
  return GPoint(
      center.x + (int16_t)(sin_lookup(angle) * (int32_t)radius / TRIG_MAX_RATIO),
      center.y - (int16_t)(cos_lookup(angle) * (int32_t)radius / TRIG_MAX_RATIO));
}

static GPoint prv_offset_point(GPoint point, int32_t angle, int16_t distance) {
  return GPoint(
      point.x + (int16_t)(sin_lookup(angle) * (int32_t)distance / TRIG_MAX_RATIO),
      point.y - (int16_t)(cos_lookup(angle) * (int32_t)distance / TRIG_MAX_RATIO));
}

static void prv_draw_marker_line(GContext *ctx, GPoint center, int32_t angle,
                                 int16_t outer_radius, int16_t inner_radius,
                                 GColor color, uint8_t width) {
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, width);
  graphics_draw_line(ctx,
                     prv_point_on_circle(center, angle, outer_radius),
                     prv_point_on_circle(center, angle, inner_radius));
}

static void prv_draw_dial(GContext *ctx, GRect bounds) {
  const GPoint center = grect_center_point(&bounds);
  const int16_t outer_radius = bounds.size.w / 2;
  const int16_t dial_radius = outer_radius;
  const int16_t marker_outer = dial_radius;
  const int16_t tick_inner = marker_outer - 34;
  const int16_t minute_tick_length = 6;
  const int16_t minute_tick_outer = marker_outer - minute_tick_length;
  const int16_t minute_tick_inner = minute_tick_outer - minute_tick_length;

  graphics_context_set_fill_color(ctx, DIAL_COLOR);
  graphics_fill_circle(ctx, center, dial_radius);

  for (int i = 0; i < 60; ++i) {
    if (i % 5 == 0) {
      continue;
    }
    const int32_t angle = TRIG_MAX_ANGLE * i / 60;
    prv_draw_marker_line(ctx, center, angle, minute_tick_outer, minute_tick_inner, MARKER_COLOR, 1);
  }

  for (int hour = 1; hour < 12; ++hour) {
    const int32_t angle = TRIG_MAX_ANGLE * hour / 12;
    prv_draw_marker_line(ctx, center, angle, marker_outer, tick_inner, MARKER_COLOR, 1);
  }

  prv_draw_marker_line(ctx, center, 0, marker_outer, tick_inner, MARKER_COLOR, 1);
}

static void prv_draw_branding(GContext *ctx, GRect bounds) {
  const GPoint center = grect_center_point(&bounds);

  graphics_context_set_text_color(ctx, TEXT_COLOR);
  graphics_draw_text(ctx, "AUTOMATIC", s_subtitle_font,
                     GRect(center.x - 50, center.y - 70, 100, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, "MADE IN AUSTRIA", s_footer_font,
                     GRect(center.x - 55, center.y + 72, 110, 12),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void prv_draw_date_window(GContext *ctx, GRect bounds) {
  const GPoint center = grect_center_point(&bounds);
  char date_buffer[3];
  snprintf(date_buffer, sizeof(date_buffer), "%d", s_current_time.tm_mday);

  const GRect frame = GRect(center.x + 64, center.y - 10, 30, 20);
  const GRect text_frame = GRect(frame.origin.x, frame.origin.y - 3, frame.size.w, frame.size.h);

  graphics_context_set_fill_color(ctx, DATE_FILL_COLOR);
  graphics_fill_rect(ctx, frame, 0, GCornerNone);

  graphics_context_set_stroke_color(ctx, DATE_BOX_COLOR);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_rect(ctx, frame);

  graphics_context_set_text_color(ctx, ACCENT_COLOR);
  graphics_draw_text(ctx, date_buffer, s_date_font, text_frame,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void prv_draw_hand(GContext *ctx, GPoint center, int32_t angle,
                          int16_t length, int16_t back_length,
                          uint8_t base_width, uint8_t highlight_width,
                          uint8_t accent_width, int16_t accent_offset) {
  const GPoint tip = prv_point_on_circle(center, angle, length);
  const GPoint tail = prv_point_on_circle(center, angle + TRIG_MAX_ANGLE / 2, back_length);
  const int32_t perpendicular = angle + TRIG_MAX_ANGLE / 4;

  graphics_context_set_stroke_color(ctx, HAND_BASE_COLOR);
  graphics_context_set_stroke_width(ctx, base_width);
  graphics_draw_line(ctx, tail, tip);

  graphics_context_set_stroke_color(ctx, HAND_HIGHLIGHT_COLOR);
  graphics_context_set_stroke_width(ctx, highlight_width);
  graphics_draw_line(ctx, tail, tip);

  graphics_context_set_stroke_color(ctx, ACCENT_COLOR);
  graphics_context_set_stroke_width(ctx, accent_width);
  graphics_draw_line(ctx,
                     prv_offset_point(center, perpendicular, accent_offset),
                     prv_offset_point(tip, perpendicular, accent_offset));
}

static void prv_draw_hands(GContext *ctx, GRect bounds) {
  const GPoint center = grect_center_point(&bounds);
  const int32_t second_angle = TRIG_MAX_ANGLE * s_current_time.tm_sec / 60;
  const int32_t minute_angle =
      TRIG_MAX_ANGLE * (s_current_time.tm_min * 60 + s_current_time.tm_sec) / 3600;
  const int32_t hour_angle =
      TRIG_MAX_ANGLE *
      (((s_current_time.tm_hour % 12) * 3600) + (s_current_time.tm_min * 60) + s_current_time.tm_sec) /
      (12 * 3600);

  prv_draw_hand(ctx, center, hour_angle, 60, 8, 8, 4, 2, 1);
  prv_draw_hand(ctx, center, minute_angle, 88, 10, 6, 3, 1, 1);

  graphics_context_set_stroke_color(ctx, SECOND_HAND_COLOR);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, center, prv_point_on_circle(center, second_angle, 100));

  graphics_context_set_fill_color(ctx, HAND_BASE_COLOR);
  graphics_fill_circle(ctx, center, 7);
  graphics_context_set_fill_color(ctx, DIAL_COLOR);
  graphics_fill_circle(ctx, center, 4);
  graphics_context_set_stroke_color(ctx, BEZEL_COLOR);
  graphics_draw_circle(ctx, center, 7);
}

static void prv_face_layer_update_proc(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);

  graphics_context_set_antialiased(ctx, true);

  prv_draw_dial(ctx, bounds);
  prv_draw_branding(ctx, bounds);
  prv_draw_date_window(ctx, bounds);
  prv_draw_hands(ctx, bounds);
}

static void prv_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  s_current_time = *tick_time;
  layer_mark_dirty(s_face_layer);
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  const GRect bounds = layer_get_bounds(window_layer);

  s_face_layer = layer_create(bounds);
  layer_set_update_proc(s_face_layer, prv_face_layer_update_proc);
  layer_add_child(window_layer, s_face_layer);
}

static void prv_window_unload(Window *window) {
  layer_destroy(s_face_layer);
}

static void prv_init(void) {
  time_t now = time(NULL);
  s_current_time = *localtime(&now);

  s_subtitle_font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  s_footer_font = fonts_get_system_font(FONT_KEY_GOTHIC_09);
  s_date_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  s_window = window_create();
  window_set_background_color(s_window, DIAL_COLOR);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });

  tick_timer_service_subscribe(SECOND_UNIT, prv_tick_handler);
  window_stack_push(s_window, true);
}

static void prv_deinit(void) {
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
