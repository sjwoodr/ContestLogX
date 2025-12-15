# flrig Testing Guide

## Current Status

**Issues to diagnose:**
1. Test button shows "Freq: 0Hz, Mode: (blank)"
2. Frequency/mode not updating in main window

## Debug Output Added

The application now has extensive debug output to diagnose flrig communication issues.

### Running with Debug Output

```bash
cd build
./ContestLogX 2>&1 | tee flrig-test.log
```
Be sure to enable Flrig debug messages from the Debug menu.
This will show all debug messages and save to `flrig-test.log`.

### What to Look For

When you click **Test** in the rig dialog, you should see:

```
Sending frequency request: <?xml version="1.0"?>
<methodCall>
  <methodName>rig.get_vfo</methodName>
</methodCall>

Sending HTTP request: POST /RPC2 HTTP/1.1
Host: localhost:12345
User-Agent: .clx/12.0
Content-Type: text/xml
Content-Length: XX
Connection: keep-alive

<?xml version="1.0"?>...

Request sent, bytes written: XXX
Received data: [HTTP response]
Complete response received
Response buffer: [full response]
Parsed frequency: 14250000
```

### Expected flrig Response Format

flrig should respond with XML like:

```xml
HTTP/1.1 200 OK
Content-Type: text/xml
Content-Length: XXX

<?xml version="1.0"?>
<methodResponse>
  <params>
    <param>
      <value><double>14250000</double></value>
    </param>
  </params>
</methodResponse>
```

## Common Issues

### 1. flrig Not Running
**Symptom:** "Connection refused" error
**Fix:** Start flrig first, enable XML-RPC server

### 2. Wrong Port
**Symptom:** Connection timeout
**Fix:** Check flrig port (Config → Server → XML-RPC port, default 12345)

### 3. No Response
**Symptom:** "Frequency request timeout" in log
**Possible causes:**
- flrig not responding to XML-RPC
- Firewall blocking connection
- Wrong hostname/port

### 4. Empty Response
**Symptom:** Parsed frequency is 0, mode is blank
**Possible causes:**
- XML parsing error
- flrig returning fault/error
- Response format unexpected

## Testing Steps

1. **Start flrig:**
   ```bash
   flrig
   ```

2. **Enable XML-RPC in flrig:**
   - Config → Server
   - Check "Enable XML-RPC"
   - Note the port number
   - Click "Save"

3. **Start .clx Qt:**
   ```bash
   cd .clxQt/build
   ./.clxQt 2>&1 | tee flrig-test.log
   ```

4. **Connect to rig:**
   - Menu: Rig → flrig Connection
   - Enter host: localhost
   - Enter port: 12345 (or your port)
   - Click "Connect"
   - Check for "Connected to flrig" message

5. **Test communication:**
   - Click "Test" button
   - Look at console output
   - Should show frequency and mode

6. **Check main window:**
   - Frequency should appear in entry field
   - Mode should be selected in combo box
   - Bottom left button should show "14250.0 USB" (or current freq/mode)

## Debugging Checklist

- [ ] flrig is running
- [ ] XML-RPC enabled in flrig
- [ ] Correct port number
- [ ] ContestLogX shows "Connected"
- [ ] Console shows XML request being sent
- [ ] Console shows HTTP response received
- [ ] Response contains valid XML
- [ ] Frequency parsed as non-zero number
- [ ] Mode parsed as non-empty string

## If Still Not Working

1. **Save the log file:**
   ```bash
   ./ContestLogX 2>&1 > flrig-debug.log
   ```

2. **Test flrig directly with curl:**
   ```bash
   curl -X POST http://localhost:12345/RPC2 \
     -H "Content-Type: text/xml" \
     -d '<?xml version="1.0"?>
<methodCall>
  <methodName>rig.get_vfo</methodName>
</methodCall>'
   ```

3. **Check response:**
   - Should show XML with frequency value
   - If not, flrig XML-RPC server may not be working

## Known Working Configuration

- **flrig version:** 1.4.x or later
- **XML-RPC:** Enabled
- **Port:** 12345
- **Host:** localhost
- **Rig:** Any supported by flrig

## Freq/Mode Button

The bottom-left button shows current frequency and mode:
- **Format:** "14250.0 USB"
- **Location:** Status bar, left side
- **Function:** Click to open rig dialog
- **Updates:** Every 500ms when connected

If button shows "14250.0 USB" but never changes:
1. Check debug output for polling messages
2. Verify rig is responding to get_vfo/get_mode
3. Check poll timer is running (should see messages every 500ms)

## Contact

If issues persist after following this guide, provide:
1. flrig version
2. .clx Qt log file
3. flrig XML-RPC test results (curl command above)
4. Radio model
