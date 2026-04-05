# ESPHome Flight Tracker Component

This is an external component for [ESPHome](https://esphome.io/) that fetches and displays live aircraft data from dump1090 in SBS1 format.

## Note

Though this can technically work with any display supported by ESPHome, it is optimized for and tested with a 128x64 LED matrix display.

## Usage

Check out the `examples` directory for a full example configuration.

You can use this component in your ESPHome configuration by importing it with `external_components`:

```yaml
external_components:
  - source: github://wxwatch/esphome-flight-tracker
    components: [flight_tracker]
```

You will need these components in your configuration:

- [Display](https://esphome.io/components/display/)
- [Font](https://esphome.io/components/font/)
- [Time](https://esphome.io/components/time/)

Then you can define an instance of the component in your YAML configuration:

```yaml
flight_tracker:
  id: tracker

  # Host of the dump1090 server
  host: "192.168.1.100"

  # Port of the dump1090 SBS1 output (default 30003)
  port: 30003

  # Maximum number of aircraft to show
  limit: 3

  # Whether to scroll long callsigns
  scroll_headsigns: false

  # Color for realtime indicator
  realtime_color: 0x00FF00
```

Then, finally, in your display's draw lambda:

```yaml
display:
  - platform: # ...
    id: # ...
    lambda: |-
      id(tracker).draw_schedule();
```

## License

```
MIT License

Copyright (c) 2025 TJ Horner

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
