## About

rtl_433 is a receiver and decoder for the low-power radio traffic on the 433.92, 868, 315, 345 and
915 MHz ISM bands: weather stations, tyre pressure monitors, door and motion sensors, remote
controls and utility meters, with a decoder for each device protocol it knows. It
reads samples from an RTL-SDR stick directly, or from any other radio with a module for
[soapysdr](#port-soapysdr), and prints what it decodes or sends it on as JSON, MQTT or InfluxDB
data. The example configuration files it ships land in `/etc/rtl_433/`.

## Flags

- `-DENABLE_RTLSDR=ON` — Build the RTL-SDR input on [rtl-sdr](#port-rtl-sdr)'s `librtlsdr`, and
  fail configure if it is not found. Upstream's default.
- `-DENABLE_SOAPYSDR=ON` — Build the SoapySDR input, so every radio with a SoapySDR module can feed
  rtl_433. Upstream's default is `AUTO`, which leaves it out with only a status line when SoapySDR
  is not found; `ON` makes a missing SoapySDR a configure error.
- `-DENABLE_OPENSSL=ON` — Build TLS into the network outputs (for example `influxs://` and an MQTT
  connection with TLS options) with [openssl](#port-openssl). Upstream's `AUTO` again; `ON` makes
  it a configure error when OpenSSL is missing.

## Patches

- `proflame2-bound.patch` — Bounds the frame loop of the Proflame 2 fireplace-remote decoder, `proflame2_mc()`, at seven frames. Its caller hands it a seven-byte buffer, and the loop otherwise keeps decoding while the row holds more valid frames, writing each one to `out[f]` past the end of that buffer. The decoder accepts only a seven-frame result, so stopping at seven changes no decoded message.

## Notes

rtl_433 links [libusb](#port-libusb) itself whenever CMake finds it, to print readable USB error
messages, so libusb is in `depends` in its own right rather than arriving through rtl-sdr.
