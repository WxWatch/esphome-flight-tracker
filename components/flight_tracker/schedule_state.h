#pragma once

#include <vector>
#include <mutex>

#include "esphome/components/display/display.h"

namespace esphome
{
  namespace flight_tracker
  {

    class Aircraft
    {
    public:
      std::string icao;
      std::string callsign;
      int altitude; // feet
      int speed;    // knots
      float lat;
      float lon;
      int heading; // degrees
      time_t last_seen;
      bool is_realtime;
    };

    class ScheduleState
    {
    public:
      std::mutex mutex;
      std::vector<Aircraft> aircraft;
    };

  } // namespace flight_tracker
} // namespace esphome
