#pragma once

#include <map>
#include <WiFiClient.h>

#include "esphome/core/component.h"
#include "esphome/components/display/display.h"
#include "esphome/components/font/font.h"
#include "esphome/components/time/real_time_clock.h"

#include "schedule_state.h"

namespace esphome
{
  namespace flight_tracker
  {

    class FlightTracker : public Component
    {
    public:
      void setup() override;
      void loop() override;
      void dump_config() override;
      void on_shutdown() override;

      float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

      void reconnect();
      void close(bool fully = false);

      void draw_schedule();

      Localization *get_localization() { return &this->localization_; }

      void set_display(display::Display *display) { display_ = display; }
      void set_font(font::Font *font) { font_ = font; }
      void set_rtc(time::RealTimeClock *rtc) { rtc_ = rtc; }

      void set_host(const std::string &host) { host_ = host; }
      void set_port(int port) { port_ = port; }
      void set_limit(int limit) { limit_ = limit; }
      void set_scroll_headsigns(bool scroll_headsigns) { scroll_headsigns_ = scroll_headsigns; }

      void set_realtime_color(const Color &color);

    protected:
      static constexpr int scroll_speed = 10; // pixels/second
      static constexpr int idle_time_left = 5000;
      static constexpr int idle_time_right = 1000;

      std::string from_now_(time_t unix_timestamp, uint rtc_now) const;
      void draw_text_centered_(const char *text, Color color);
      void draw_realtime_icon_(int bottom_right_x, int bottom_right_y, unsigned long now);

      void draw_aircraft(
          const Aircraft &aircraft, int y_offset, int font_height, unsigned long uptime, uint rtc_now,
          bool no_draw = false, int *callsign_overflow_out = nullptr, int scroll_cycle_duration = 0);

      Localization localization_{};
      ScheduleState schedule_state_;

      display::Display *display_;
      font::Font *font_;
      time::RealTimeClock *rtc_;

      WiFiClient tcp_client_{};

      void on_tcp_data_(const std::string &data);
      void connect_tcp_();
      int connection_attempts_ = 0;
      unsigned long last_heartbeat_ = 0;
      bool has_ever_connected_ = false;
      bool fully_closed_ = false;

      std::string host_;
      int port_ = 30003;
      int limit_;

      bool scroll_headsigns_ = false;

      Color realtime_color_ = Color(0x20FF00);
      Color realtime_color_dark_ = Color(0x00A700);
    };

  } // namespace flight_tracker
} // namespace esphome
