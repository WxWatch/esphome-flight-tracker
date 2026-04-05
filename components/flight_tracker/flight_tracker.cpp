#include "flight_tracker.h"
#include "string_utils.h"

#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/watchdog/watchdog.h"
#include "esphome/components/network/util.h"

namespace esphome
{
  namespace flight_tracker
  {

    static const char *TAG = "flight_tracker.component";

    void FlightTracker::setup()
    {
      this->connect_tcp_();

      this->set_interval("check_stale_aircraft", 10000, [this]()
                         {
    if (this->tcp_client_.connected() && !this->schedule_state_.aircraft.empty()) {
      bool has_stale_aircraft = false;

      this->schedule_state_.mutex.lock();

      auto now = this->rtc_->now();
      if (now.is_valid()) {
        for (auto &aircraft : this->schedule_state_.aircraft) {
          if (now.timestamp - aircraft.last_seen > 60) {
            has_stale_aircraft = true;
            break;
          }
        }
      }

      this->schedule_state_.mutex.unlock();

      if (has_stale_aircraft) {
        ESP_LOGD(TAG, "Stale aircraft detected, reconnecting");
        ESP_LOGD(TAG, "  Current RTC time: %d", now.timestamp);
        ESP_LOGD(TAG, "  Last heartbeat: %d", this->last_heartbeat_);
        this->reconnect();
      }
    } });
    }

    void FlightTracker::loop()
    {
      if (this->tcp_client_.connected())
      {
        while (this->tcp_client_.available())
        {
          String line = this->tcp_client_.readStringUntil('\n');
          this->on_tcp_data_(line.c_str());
        }
      }

      if (this->last_heartbeat_ != 0 && millis() - this->last_heartbeat_ > 60000)
      {
        ESP_LOGW(TAG, "Heartbeat timeout, reconnecting");
        this->reconnect();
        return;
      }
    }

    void FlightTracker::dump_config()
    {
      ESP_LOGCONFIG(TAG, "Flight Tracker:");
      ESP_LOGCONFIG(TAG, "  Host: %s", this->host_.c_str());
      ESP_LOGCONFIG(TAG, "  Port: %d", this->port_);
      ESP_LOGCONFIG(TAG, "  Limit: %d", this->limit_);
    }

    void FlightTracker::reconnect()
    {
      this->close();
      this->connect_tcp_();
    }

    void FlightTracker::close(bool fully)
    {
      this->tcp_client_.stop();
      if (fully)
      {
        this->fully_closed_ = true;
      }
    }

    void FlightTracker::on_shutdown()
    {
      this->cancel_interval("check_stale_trips");
      this->close(true);
    }

    void FlightTracker::on_tcp_data_(const std::string &data)
    {
      ESP_LOGV(TAG, "Received data: %s", data.c_str());

      if (data.find("MSG,") != 0) {
        return; // Not a MSG line
      }

      std::vector<std::string> fields = split_string(data, ',');
      if (fields.size() < 15) {
        return;
      }

      std::string icao = fields[4];
      std::string callsign = fields[10];
      int altitude = atoi(fields[11].c_str());
      int speed = atoi(fields[12].c_str());
      int heading = atoi(fields[13].c_str());
      float lat = atof(fields[14].c_str());
      float lon = atof(fields[15].c_str());

      if (icao.empty() || callsign.empty()) {
        return;
      }

      this->schedule_state_.mutex.lock();

      // Find existing aircraft or add new
      auto it = std::find_if(this->schedule_state_.aircraft.begin(), this->schedule_state_.aircraft.end(),
                             [&icao](const Aircraft &a) { return a.icao == icao; });

      if (it != this->schedule_state_.aircraft.end()) {
        it->callsign = callsign;
        it->altitude = altitude;
        it->speed = speed;
        it->heading = heading;
        it->lat = lat;
        it->lon = lon;
        it->last_seen = this->rtc_->now().timestamp;
        it->is_realtime = true;
      } else {
        Aircraft aircraft;
        aircraft.icao = icao;
        aircraft.callsign = callsign;
        aircraft.altitude = altitude;
        aircraft.speed = speed;
        aircraft.heading = heading;
        aircraft.lat = lat;
        aircraft.lon = lon;
        aircraft.last_seen = this->rtc_->now().timestamp;
        aircraft.is_realtime = true;
        this->schedule_state_.aircraft.push_back(aircraft);
      }

      this->schedule_state_.mutex.unlock();

      this->last_heartbeat_ = millis();
    }

    void FlightTracker::connect_tcp_()
    {
      if (this->host_.empty())
      {
        ESP_LOGW(TAG, "No host set, not connecting");
        return;
      }

      if (this->fully_closed_)
      {
        ESP_LOGW(TAG, "Connection fully closed, not reconnecting");
        return;
      }

      if (this->tcp_client_.connected())
      {
        ESP_LOGV(TAG, "Not reconnecting, already connected");
        return;
      }

      ESP_LOGD(TAG, "Connecting to TCP server (attempt %d): %s:%d", this->connection_attempts_, this->host_.c_str(), this->port_);

      bool connection_success = this->tcp_client_.connect(this->host_.c_str(), this->port_);

      if (connection_success)
      {
        ESP_LOGI(TAG, "TCP connection established");
        this->connection_attempts_ = 0;
        this->has_ever_connected_ = true;
        this->last_heartbeat_ = millis();
      }
      else
      {
        ESP_LOGW(TAG, "TCP connection failed");
        this->connection_attempts_++;
        if (this->connection_attempts_ < 5)
        {
          this->defer([this]()
                      { this->connect_tcp_(); }, 5000 * this->connection_attempts_);
        }
        else
        {
          ESP_LOGE(TAG, "Failed to connect after 5 attempts");
          this->status_set_error(LOG_STR("Failed to connect to dump1090"));
        }
      }
    }

      this->last_heartbeat_ = 0;

      ESP_LOGD(TAG, "Connecting to WebSocket server (attempt %d): %s", this->connection_attempts_, this->base_url_.c_str());

      bool connection_success = false;
      if (esphome::network::is_connected())
      {
        connection_success = this->ws_client_.connect(this->base_url_.c_str());
      }
      else
      {
        ESP_LOGW(TAG, "Not connected to network; skipping connection attempt");
      }

      if (!connection_success)
      {
        this->connection_attempts_++;

        if (this->connection_attempts_ >= 3)
        {
          this->status_set_error(LOG_STR("Failed to connect to WebSocket server"));
        }

        if (this->connection_attempts_ >= 15)
        {
          ESP_LOGE(TAG, "Could not connect to WebSocket server within 15 attempts.");
          ESP_LOGE(TAG, "It's likely that the network is not truly connected; rebooting the device to try to recover.");
          App.reboot();
        }

        auto timeout = std::min(15000, this->connection_attempts_ * 5000);
        ESP_LOGW(TAG, "Failed to connect, retrying in %ds", timeout / 1000);

        this->set_timeout("reconnect", timeout, [this]()
                          { this->connect_ws_(); });
      }
      else
      {
        this->has_ever_connected_ = true;
        this->connection_attempts_ = 0;
        this->status_clear_error();
      }
    }

    void FlightTracker::set_abbreviations_from_text(const std::string &text)
    {
      this->abbreviations_.clear();
      for (const auto &line : split(text, '\n'))
      {
        auto parts = split(line, ';');

        if (parts.size() == 1)
        {
          // If only one part is provided, treat it as a removal (replace with empty string)
          this->add_abbreviation(parts[0], "");
          continue;
        }

        if (parts.size() != 2)
        {
          ESP_LOGW(TAG, "Invalid abbreviation line: %s", line.c_str());
          continue;
        }

        this->add_abbreviation(parts[0], parts[1]);
      }
    }

    void FlightTracker::set_route_styles_from_text(const std::string &text)
    {
      this->route_styles_.clear();
      for (const auto &line : split(text, '\n'))
      {
        auto parts = split(line, ';');
        if (parts.size() != 3)
        {
          ESP_LOGW(TAG, "Invalid route style line: %s", line.c_str());
          continue;
        }
        uint32_t color = std::stoul(parts[2], nullptr, 16);
        this->add_route_style(parts[0], parts[1], Color(color));
      }
    }

    void FlightTracker::draw_text_centered_(const char *text, Color color)
    {
      int display_center_x = this->display_->get_width() / 2;
      int display_center_y = this->display_->get_height() / 2;
      this->display_->print(display_center_x, display_center_y, this->font_, color, display::TextAlign::CENTER, text);
    }

    void FlightTracker::set_realtime_color(const Color &color)
    {
      this->realtime_color_ = color;
      this->realtime_color_dark_ = Color(
          (color.r * 0.5),
          (color.g * 0.5),
          (color.b * 0.5));
    }

    const uint8_t realtime_icon[6][6] = {
        {0, 0, 0, 3, 3, 3},
        {0, 0, 3, 0, 0, 0},
        {0, 3, 0, 0, 2, 2},
        {3, 0, 0, 2, 0, 0},
        {3, 0, 2, 0, 0, 1},
        {3, 0, 2, 0, 1, 1}};

    void HOT FlightTracker::draw_realtime_icon_(int bottom_right_x, int bottom_right_y, unsigned long uptime)
    {
      const int num_frames = 6;
      const int idle_frame_duration = 3000;
      const int anim_frame_duration = 200;
      const int cycle_duration = idle_frame_duration + (num_frames - 1) * anim_frame_duration;

      unsigned long cycle_time = uptime % cycle_duration;

      int frame;
      if (cycle_time < idle_frame_duration)
      {
        frame = 0;
      }
      else
      {
        frame = 1 + (cycle_time - idle_frame_duration) / anim_frame_duration;
      }

      auto is_segment_lit = [frame](uint8_t segment)
      {
        switch (segment)
        {
        case 1:
          return frame >= 1 && frame <= 3;
        case 2:
          return frame >= 2 && frame <= 4;
        case 3:
          return frame >= 3 && frame <= 5;
        default:
          return false;
        }
      };

      for (uint8_t i = 0; i < 6; ++i)
      {
        for (uint8_t j = 0; j < 6; ++j)
        {
          uint8_t segment_number = realtime_icon[i][j];
          if (segment_number == 0)
          {
            continue;
          }

          Color icon_color = is_segment_lit(segment_number) ? this->realtime_color_ : this->realtime_color_dark_;
          this->display_->draw_pixel_at(bottom_right_x - (5 - j), bottom_right_y - (5 - i), icon_color);
        }
      }
    }

    void FlightTracker::draw_aircraft(
        const Aircraft &aircraft, int y_offset, int font_height, unsigned long uptime, uint rtc_now,
        bool no_draw, int *callsign_overflow_out, int scroll_cycle_duration)
    {
      if (!no_draw)
      {
        this->display_->print(0, y_offset, this->font_, Color(0xFFFFFF), display::TextAlign::TOP_LEFT, aircraft.callsign.c_str());
      }

      int callsign_width, _;
      this->font_->measure(aircraft.callsign.c_str(), &callsign_width, &_, &_, &_);

      std::string altitude_display = std::to_string(aircraft.altitude) + "ft";

      int altitude_width;
      this->font_->measure(altitude_display.c_str(), &altitude_width, &_, &_, &_);

      int callsign_clipping_start = 0;
      int callsign_clipping_end = this->display_->get_width() - altitude_width - 2;

      if (!no_draw)
      {
        Color altitude_color = aircraft.is_realtime ? this->realtime_color_ : Color(0xa7a7a7);
        this->display_->print(this->display_->get_width() + 1, y_offset, this->font_, altitude_color, display::TextAlign::TOP_RIGHT, altitude_display.c_str());
      }

      if (aircraft.is_realtime)
      {
        callsign_clipping_end -= 8;

        if (!no_draw)
        {
          int icon_bottom_right_x = this->display_->get_width() - altitude_width - 2;
          int icon_bottom_right_y = y_offset + font_height - 6;

          this->draw_realtime_icon_(icon_bottom_right_x, icon_bottom_right_y, uptime);
        }
      }

      int callsign_max_width = callsign_clipping_end - callsign_clipping_start;

      int callsign_actual_width;
      this->font_->measure(aircraft.callsign.c_str(), &callsign_actual_width, &_, &_, &_);

      int callsign_overflow = callsign_actual_width - callsign_max_width;
      if (callsign_overflow_out)
      {
        *callsign_overflow_out = callsign_overflow;
      }

      if (no_draw)
      {
        return;
      }

      int scroll_offset = 0;
      if (callsign_overflow > 0 && scroll_cycle_duration > 0)
      {
        /// Note: The scroll may jump if callsign_clipping_end changes (e.g. due to the width of the altitude changing).
        /// This is probably not a big deal, since the display makes sudden changes anyway (e.g. when aircraft are updated)
        /// and this happens relatively infrequently.

        int scroll_time = callsign_overflow * 1000 / scroll_speed;
        int scroll_cycle_time = uptime % scroll_cycle_duration;

        // Scroll idle (left side - default)
        if (scroll_cycle_time < idle_time_left)
        {
          // scroll_offset = 0; do nothing
        }
        else if (scroll_cycle_time < idle_time_left + scroll_time)
        {
          // Scrolling left
          int time_since_scroll_start = scroll_cycle_time - idle_time_left;
          scroll_offset = time_since_scroll_start * scroll_speed / 1000;
        }
        else if (scroll_cycle_time < idle_time_left + scroll_time + idle_time_right)
        {
          // Scroll idle (right side)
          scroll_offset = callsign_overflow;
        }
        else
        {
          // Scrolling right
          int time_since_scroll_end = scroll_cycle_time - (idle_time_left + scroll_time + idle_time_right);
          scroll_offset = callsign_overflow - (time_since_scroll_end * scroll_speed / 1000);
        }
      }

      this->display_->start_clipping(callsign_clipping_start, callsign_clipping_end);
      this->display_->print(-scroll_offset, y_offset, this->font_, Color(0xFFFFFF), display::TextAlign::TOP_LEFT, aircraft.callsign.c_str());
      this->display_->end_clipping();
    }

    void HOT FlightTracker::draw_schedule()
    {
      if (this->display_ == nullptr)
      {
        ESP_LOGW(TAG, "No display attached, cannot draw schedule");
        return;
      }

      if (!esphome::network::is_connected())
      {
        this->draw_text_centered_("Waiting for network", Color(0x252627));
        return;
      }

      if (!this->rtc_->now().is_valid())
      {
        this->draw_text_centered_("Waiting for time sync", Color(0x252627));
        return;
      }

      if (this->host_.empty())
      {
        this->draw_text_centered_("No host set", Color(0x252627));
        return;
      }

      if (this->status_has_error())
      {
        this->draw_text_centered_("Error connecting to dump1090", Color(0xFE4C5C));
        return;
      }

      if (!this->has_ever_connected_)
      {
        this->draw_text_centered_("Connecting...", Color(0x252627));
        return;
      }

      if (this->schedule_state_.aircraft.empty())
      {
        this->draw_text_centered_("No aircraft detected", Color(0x252627));
        return;
      }

      this->schedule_state_.mutex.lock();

      int nominal_font_height = this->font_->get_ascender() + this->font_->get_descender();
      unsigned long uptime = millis();
      uint rtc_now = this->rtc_->now().timestamp;

      int scroll_cycle_duration = 0;
      if (this->scroll_headsigns_)
      {
        int largest_callsign_overflow = 0;
        for (const Aircraft &aircraft : this->schedule_state_.aircraft)
        {
          int callsign_overflow;
          this->draw_aircraft(aircraft, 0, nominal_font_height, uptime, rtc_now, true, &callsign_overflow);
          largest_callsign_overflow = max(largest_callsign_overflow, callsign_overflow);
        }

        if (largest_callsign_overflow > 0)
        {
          int longest_scroll_time = largest_callsign_overflow * 1000 / scroll_speed;
          scroll_cycle_duration = idle_time_left + idle_time_right + 2 * longest_scroll_time;
        }
      }

      int max_aircraft_height = (this->limit_ * this->font_->get_ascender()) + ((this->limit_ - 1) * this->font_->get_descender());
      int y_offset = (this->display_->get_height() % max_aircraft_height) / 2;

      for (const Aircraft &aircraft : this->schedule_state_.aircraft)
      {
        this->draw_aircraft(aircraft, y_offset, nominal_font_height, uptime, rtc_now, false, nullptr, scroll_cycle_duration);
        y_offset += nominal_font_height;
      }

      this->schedule_state_.mutex.unlock();
    }

  } // namespace flight_tracker
} // namespace esphome
