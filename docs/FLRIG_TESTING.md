# Rig Control Guide

ContestLogX supports three rig control backends, selectable in **Rig → Rig Connection**.

## Option A - flrig (recommended)

[flrig](https://www.w1hkj.org/) provides full rig control: frequency, mode, CW keying via cwio, PTT, power, and bandwidth. This is the recommended backend for CW operators.

### Setup

1. Start flrig and connect it to your radio.
2. In flrig: **Config → Server** → enable XML-RPC server (default port: `12345`).
3. In ContestLogX: **Rig → Rig Connection** → select **flrig (XML-RPC)**.
4. Enter `localhost` / `12345`, click **Connect**.

### Testing flrig directly

```bash
curl -X POST http://localhost:12345/RPC2 \
  -H "Content-Type: text/xml" \
  -d '<?xml version="1.0"?><methodCall><methodName>rig.get_vfo</methodName></methodCall>'
```

## Option B - Hamlib (rigctld)

[Hamlib](https://hamlib.github.io/) supports 400+ radio models via the `rigctld` network daemon. It provides frequency and mode control. CW keying and some other features depend on individual rig capabilities and are unavailable for most radios.

### 1. Find your rig model number

```bash
rigctl -l | grep -i alinco
# 17001  Alinco     DX-77        0.6     Stable      RS-232
```

### 2. Start rigctld

```bash
rigctld -m 17001 \
  -r /dev/serial/by-id/usb-FTDI_FT232R_USB_UART_FTHJCZ32-if00-port0 \
  -s 9600 \
  --set-conf=post_write_delay=200,timeout=800
```

Common options:

| Flag | Description |
|------|-------------|
| `-m` | Rig model number (from `rigctl -l`) |
| `-r` | Serial device path |
| `-s` | Serial baud rate |
| `-t` | TCP listening port (default: `4532`) |
| `--set-conf` | Rig-specific tuning (timeouts, write delays, etc.) |

### 3. Connect ContestLogX

1. **Rig → Rig Connection** → select **Hamlib (rigctld)**.
2. Enter `localhost` and the rigctld port (default: `4532`).
3. Click **Connect**.

### Testing rigctld directly

```bash
# Get frequency
echo "f" | nc localhost 4532

# Get mode
echo "m" | nc localhost 4532

# Set frequency to 14.250 MHz
echo "F 14250000" | nc localhost 4532
```

### CW keying via Hamlib

Most rigs do not support CW keying through Hamlib. You can check yours:

```bash
rigctl -m 17001 -r /dev/ttyUSB0 -s 9600 b "TEST"
```

If the response is `RPRT -11` (Feature not available) or `RPRT -4` (Feature not implemented), CW keying is not supported for your rig. Use flrig for CW keying instead.

## Option C - Mocked (testing)

The mocked backend simulates a radio without any real hardware. It is useful for:

- **SO2R practice** - configure Radio R as mocked when you only have one physical radio
- **UI testing** - verify QSO entry, CW memories, and freq/mode changes without a rig
- **Demo/training** - show the application workflow without station equipment

### Setup

1. **Rig → Rig Connection** → select **Mocked (testing)**.
2. Click **Connect** - the simulated radio connects instantly.
3. Default state: 14.200 MHz USB, 25 WPM, 100W.

All commands (set frequency, set mode, CW keying, PTT, power, bandwidth) are accepted and the mocked radio returns the last-set values. The rig name shows as "Mocked Rig" in the status display.

## Debugging

Run ContestLogX in debug mode and enable rig debug logging:

```bash
./clx --debug
```

Then in the app: **Debug → Rig Debug Logging** (checkbox). This logs all command/response traffic for both flrig and Hamlib backends.

## Backend Comparison

| Feature | flrig | Hamlib (rigctld) | Mocked |
|---------|-------|------------------|--------|
| Frequency control | Yes | Yes | Yes (simulated) |
| Mode control | Yes | Yes | Yes (simulated) |
| CW keying | Yes (cwio) | Depends on rig | Yes (simulated) |
| PTT control | Yes | Yes | Yes (simulated) |
| Power control | Yes | Depends on rig | Yes (simulated) |
| Bandwidth control | Yes | Depends on rig | Yes (simulated) |
| Supported radios | [flrig list](https://www.w1hkj.org/) | [400+ models](https://hamlib.github.io/) | None (virtual) |
