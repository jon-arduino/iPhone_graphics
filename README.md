#ESP32 Telemetry Module

A modular GPS telemetry system for RC aircraft, built on the ESP32.  
Features include:

- TinyGPS++ parsing
- Smoothed ground speed and course
- North/East velocity vector
- Climb rate (vertical velocity)
- Clean GPSData struct for logging and BLE telemetry
- Modular architecture for future expansion

## Project Structure

- `src/GPSModule.h` — GPS interface and data model  
- `src/GPSModule.cpp` — UART parsing, smoothing, fix handling  
- `src/main.cpp` — Telemetry output and velocity calculations  

## Current Status

- Stable GPS parsing  
- Averaged speed and course  
- V-north and V-east  
- Climb rate  
- Ready for BLE telemetry integration  

## Branching

- `main` — stable, tested code  
- feature branches — experimental work (e.g., `v-east-climb-rate`)  
**ESP32 ↔ iPhone Graphics Transport**

Architecture & Data Flow Reference

RemoteGraphics Project --- February 2026

**System Overview**

This system streams Adafruit GFX drawing commands from an ESP32 microcontroller to an iPhone over either Bluetooth Low Energy (BLE) or WiFi TCP. The iPhone decodes these commands and renders them to a pixel-accurate framebuffer displayed on screen. The same telemetry data that drives a physical SPI TFT display on the ESP32 is mirrored to the iPhone in real time.

The architecture is deliberately transport-agnostic: a single abstract GraphicsTransport interface separates the GFX command encoding layer from the underlying communication channel. On the ESP32, an ActiveTransport switcher selects between BLE and WiFi at runtime. On the iPhone, a single GFXDisplayView receives bytes from whichever transport is active and renders identically regardless of source.

**Key Design Principles**

-   One graphics pipeline on both sides --- no duplicate render paths

-   Transport switching at runtime --- BLE and WiFi are plug-in implementations

-   Flush-based frame batching --- all GFX commands for one frame travel as a single TCP write

-   Uniform heartbeat (ping/pong) across both transports --- identical diagnostics regardless of channel

-   Separation of concerns --- GFX encoding, transport, connection management, and health monitoring are independent layers

**Section 1 --- ESP32**

**1.1 Graphics Pipeline**

The ESP32 graphics pipeline converts Adafruit GFX API calls into a compact binary command stream and delivers it to the iPhone. The pipeline is:

> Adafruit_iPhoneTFT (Adafruit GFX subclass --- public drawing API)
>
> ↓
>
> Graphics (encodes GFX calls to binary frames)
>
> ↓
>
> ActiveTransport (single-slot switcher --- selects BLE or WiFi)
>
> ↓
>
> BleGraphicsTransport OR WiFiTransport
>
> ↓
>
> BLEManager OR WiFiManager
>
> ↓
>
> BLE notifications OR TCP socket → iPhone

**Adafruit_iPhoneTFT**

Subclass of Adafruit_GFX. Overrides every drawing method (drawPixel, fillRect, drawLine, print, etc.) to call Graphics::sendCommand() instead of writing to SPI. Width and height are passed at construction and represent the virtual screen dimensions sent to the iPhone in the init frame.

-   File: **Adafruit_iPhone/Adafruit_iPhoneTFT.h/.cpp**

-   Key method: **flush()** --- sends GFX_CMD_FLUSH opcode then calls \_transport.flush() to drain the accumulation buffer

**Graphics**

Encodes each GFX API call into a length-prefixed binary frame and passes it to GraphicsTransport::send(). Frames follow the format: \[opcode:1\]\[payload length:2\]\[payload\...\]. Opcodes match the GFXOp enum defined on both sides.

-   File: **graphics/Graphics.h/.cpp**

-   Critical: **Graphics::flush()** must call both sendCommand(GFX_CMD_FLUSH) AND \_transport.flush() --- the opcode tells the iPhone to render, the transport flush drains the TCP buffer

**GraphicsTransport (abstract base)**

Pure virtual interface that all transport implementations conform to. Defines send(), flush(), canSend(), and begin(). This is the only type Graphics knows about --- it has no knowledge of BLE or WiFi specifics.

-   File: **graphics/GraphicsTransport.h**

-   Virtual methods: send(data, len), flush(), canSend(), begin()

**ActiveTransport**

Single-slot transport switcher. Holds a pointer to whichever transport is currently active. All GraphicsTransport calls are forwarded to the active transport. WiFi takes priority over BLE when both are connected. Transport switching happens in updateActiveTransport() in main.cpp every loop iteration.

-   File: **graphics/ActiveTransport.h**

-   Key methods: set(GraphicsTransport\*), get(), canSend()

-   Header-only --- no .cpp needed

**BleGraphicsTransport**

Concrete GraphicsTransport for BLE. Calls BLEManager::sendBytes() on send(). flush() is a no-op --- BLE batching is handled naturally by the TX ring buffer draining via pump_BLE_txQ() in loop(). canSend() delegates to BLEManager::canSend().

-   File: **ble/BleGraphicsTransport.h/.cpp**

**WiFiTransport**

Concrete GraphicsTransport for WiFi. Accumulates all send() calls into a std::vector (\_txBuf) and drains the entire buffer as a single TCP write on flush(). This is the core of flush-based frame batching --- 100+ individual GFX draw calls per frame become one TCP write, eliminating lwIP fragmentation and buffer pressure.

-   File: **WiFi/WiFiTransport.h/.cpp**

-   Key methods: send() (accumulates), flush() (drains), tick() (auto-flush after idle timeout), reset() (clears buffer on disconnect)

-   Constant: AUTO_FLUSH_MS = 50 --- if no explicit flush() arrives within 50ms, accumulated bytes are sent automatically. Set via setAutoFlushMs() from main.cpp

-   Critical: reset() must be called on WiFi disconnect via wifiManager.onDisconnected() --- prevents stale partial-frame bytes from prepending to the next connection\'s init sequence

**1.2 BLE Transport --- BLEManager**

BLEManager implements a Nordic UART Service (NUS) profile using NimBLE. It exposes a TX characteristic (notify) for ESP32-to-iPhone data and an RX characteristic (write) for iPhone-to-ESP32 data. All GFX bytes flow through the TX characteristic as BLE notifications.

-   File: **ble/BLEManager.h/.cpp**

-   Library: **NimBLE-Arduino**

**TX Ring Buffer**

GFX bytes are queued into an 8192-byte ring buffer (\_txQ) rather than sent immediately. This decouples the GFX encoder from BLE notification timing. pump_BLE_txQ() drains the ring buffer in loop(), sending one MTU-sized chunk per call. Chunk size adapts to negotiated MTU (up to 120 bytes).

-   pump_BLE_txQ() --- call every loop() iteration to drain pending bytes

-   MIN_GAP_US = 500µs --- minimum gap between notifications to avoid overrunning the BLE stack

**BLE Heartbeat (Ping/Pong)**

BLEManager::tick() sends a framed ping every PING_INTERVAL_MS and logs lateness thresholds if pong is delayed. Unlike WiFi, BLE does not drop the connection on pong timeout --- the BLE stack manages disconnection through its own link supervision timeout.

-   Ping frame: \[0xA5\]\[0x01\]\[0x00\]\[0xF0\] (4 bytes, framed back-channel format)

-   Pong frame: \[0xA5\]\[0x01\]\[0x00\]\[0xF1\] --- received via RX characteristic write, parsed by processBackChannel()

-   Lateness thresholds logged once per episode: 500ms (loop slow), 1500ms (significant delay), 6000ms (critical)

**Back-Channel Parser**

processBackChannel() parses framed messages arriving on the RX characteristic. Frame format: \[0xA5 magic\]\[lenLow\]\[lenHigh\]\[cmd\]\[payload\...\]. Currently handles GFX_CMD_PONG (0xF1). The same framing is used by WiFiManager\'s back-channel parser --- both sides use identical protocol.

**1.3 WiFi Transport --- WiFiManager**

WiFiManager handles WiFi connection, mDNS service advertisement, TCP server, and the WiFi heartbeat. It runs an AsyncTCP server on port 9000 and accepts one client at a time. GFX bytes are sent to the connected client via AsyncTCP\'s non-blocking write API.

-   File: **WiFi/WiFiManager.h/.cpp**

-   Library: **ESPAsyncTCP (AsyncTCP for ESP32)**

-   Discovery: **mDNS** --- advertises \_uart.\_tcp service as esp32-gps.local on port 9000. iPhone discovers via NWBrowser.

**Connection Lifecycle**

> WiFi connects → MDNS starts → TCP server starts
>
> iPhone connects → onClientConnected() → callbacks registered
>
> GFX data flows via send() → AsyncClient::write()
>
> iPhone disconnects → onError(-14) → onDisconnect → \_onDisconnected callback
>
> ↓
>
> wifiTransport.reset() clears stale buffer

**AsyncTCP Callback Safety**

AsyncTCP fires onError before onDisconnect on abrupt close (iOS error -14). The \_onDisconnected callback is fired from onError (not onDisconnect) because \_client is nulled in onError --- by the time onDisconnect fires, the \_client == c check is false and the callback would be missed. delete c is only called in onDisconnect, never in onError, to prevent double-free heap corruption.

**send() Flow Control**

send() loops calling \_client-\>space() and writing chunks until all bytes are sent or a 2000ms timeout expires. This blocking approach is safe because frames are small (\< 1KB) and the TCP buffer (5744 bytes) rarely fills under normal operation. The timeout prevents infinite blocking if the iPhone becomes unresponsive.

**TCP Buffer Readiness on Reconnect**

After a new connection is accepted, AsyncTCP\'s lwIP send buffer may briefly show space() == 0 even though connected() is true. main.cpp gates initPhoneUI() on wifiManager.clientSpace() \> 0 to ensure the buffer is allocated before sending the init sequence. Sending before this check passes results in add() failing silently and the iPhone receiving a truncated init sequence.

-   clientSpace() --- inline method returning \_client-\>space() or 0 if no client

**WiFi Heartbeat**

WiFiManager::tick() sends a framed ping every PING_INTERVAL_MS. If no pong arrives within PONG_TIMEOUT_MS (9 seconds, 3x interval), the client is dropped and the TCP socket closed. Lateness thresholds are logged identically to BLE. The timeout-based drop is unique to WiFi --- BLE relies on the BLE stack\'s own link supervision.

-   Callbacks: onDisconnected() fires when client disconnects (error or clean), onFirstPong() fires once after first pong following each new connection

**1.4 main.cpp --- System Orchestration**

main.cpp instantiates all subsystems, wires callbacks, and runs the main loop. All timing constants are defined here as constexpr values and passed to subsystems --- nothing is hardcoded in headers.

**Tuning Constants**

  ----------------------- ----------------------------------------------------------
  **PING_INTERVAL_MS**    3000 --- how often ESP32 sends a ping to iPhone

  **PONG_TIMEOUT_MS**     9000 --- WiFi drops connection if no pong in this time

  **AUTO_FLUSH_MS**       50 --- WiFiTransport auto-flushes after this idle period

  **DISP_W / DISP_H**     128 x 160 --- virtual screen dimensions

  **ROT_PHONE**           1 --- screen rotation sent in init frame
  ----------------------- ----------------------------------------------------------

**Transport Selection (updateActiveTransport)**

Called every loop() iteration. WiFi takes priority --- if wifiTransport.canSend() is true, it becomes active regardless of BLE state. If WiFi is unavailable, falls back to BLE. When the active transport changes, initPhoneUI() is scheduled (gated on clientSpace() \> 0 for WiFi).

**loop() Call Order**

> ble.pump_BLE_txQ() // drain BLE TX ring buffer
>
> wifiManager.tick(PING, PONG) // WiFi heartbeat + connection management
>
> wifiTransport.tick() // WiFi auto-flush if idle
>
> ble.tick(PING, PONG) // BLE heartbeat
>
> updateActiveTransport() // switch BLE/WiFi if needed
>
> clientSpace() \> 0 → initPhoneUI() // deferred init gate
>
> pollConsole() // serial commands
>
> gps.update() → renderTelemetry() // GPS data → GFX commands → transport

**Section 2 --- iPhone**

**2.1 Overview**

The iPhone app receives binary GFX command frames from the ESP32, decodes them, and renders them to a UIView-based framebuffer displayed in SwiftUI. The architecture mirrors the ESP32 --- a single display buffer receives bytes from whichever transport is active (BLE or WiFi), and a unified TransportMonitor tracks connection health across both.

> BLEManager / WiFiManager
>
> ↓ raw bytes
>
> GFXDisplayView.pushBytes()
>
> ↓ decoded frames
>
> GFXCommandStream → iPhoneGFXRenderer → CGImage framebuffer
>
> ↓ GFX_CMD_FLUSH
>
> setNeedsDisplay() → UIView draw() → screen

**2.2 GFX Decoding Layer --- RGB565.swift**

All GFX decoding lives in RGB565.swift. This file defines the binary protocol, the command stream parser, the renderer, and the display view.

-   File: **RGB565.swift**

**GFXOp Enum**

Maps opcode bytes to Swift cases. Every opcode the ESP32 can send must be listed here. Unknown opcodes are skipped by the stream parser.

  ------------------------ --------------------------------------------------------------
  **0x00 initDisplay**     Sets virtual screen width/height, allocates framebuffer

  **0x01 setRotation**     Logical rotation (0-3)

  **0x02 fillScreen**      Fill entire framebuffer with RGB565 color

  **0x03 flush**           Frame boundary --- apply pending frames and render to screen

  **0x10 drawPixel**       Single pixel at (x,y) with color

  **0x11 fillRect**        Filled rectangle

  **0x12 drawLine**        Line between two points

  **0x13 drawFastHLine**   Horizontal line (optimized)

  **0x14 drawFastVLine**   Vertical line (optimized)

  **0x15 drawRect**        Unfilled rectangle outline

  **0x20 setTextColor**    Foreground (and optional background) text color

  **0x21 setTextSize**     Text scaling factor

  **0x22 setCursor**       Text cursor position

  **0x23 print**           UTF-8 string rendered using bitmap font

  **0x24 setTextWrap**     Enable/disable text wrapping

  **0x30 drawBitmap**      Raw RGB565 pixel block

  **0xF0 ping**            Heartbeat from ESP32 --- must respond with pong

  **0xF1 pong**            Response from iPhone --- sent via active transport
  ------------------------ --------------------------------------------------------------

**GFXCommandStream**

Stateful byte stream parser. Accumulates raw bytes pushed from BLE notifications or WiFi TCP reads. Parses length-prefixed frames: \[opcode:1\]\[length:2 little-endian\]\[payload:length\]. Handles fragmentation --- a frame may arrive split across multiple pushBytes() calls. Returns complete (op, payload) tuples when frames are fully assembled.

-   reset() --- call on reconnect to discard any partially-assembled frame from the previous session

**iPhoneGFXRenderer**

Executes decoded GFX commands against an in-memory RGB565 framebuffer. Maintains text cursor, color, and size state. Implements the same drawing primitives as Adafruit_GFX including the bitmap font renderer. makeImage() converts the RGB565 buffer to a CGImage for display.

-   apply(op:payload:) --- dispatch a decoded frame to the appropriate drawing function

-   makeImage() --- converts RGB565 framebuffer to CGImage using CGBitmapContext

**GFXDisplayView**

UIView subclass that owns GFXCommandStream and iPhoneGFXRenderer. All bytes from both BLE and WiFi are pushed here. Implements flush-based buffering --- frames are accumulated in pendingFrames\[\] until a GFX_CMD_FLUSH frame arrives, then all are applied to the renderer at once and setNeedsDisplay() is called. This matches the ESP32\'s flush-based send pattern exactly.

-   pushBytes() --- entry point for all incoming bytes, regardless of transport

-   onPingReceived --- callback closure, called immediately when a ping frame arrives (bypasses flush buffering). Wire to TransportMonitor.shared.pingArrived() in LiveGraphicsView

-   onFramebufferSizeChanged --- fires when initDisplay changes screen dimensions, used to update GFXFramebufferModel for SwiftUI layout

-   resetStream() --- call on reconnect to discard partial frames and cancel pending auto-flush timer

-   Auto-flush timer: GFXAutoFlushIntervalSeconds = 0.05 (50ms) --- if no explicit flush arrives within 50ms of the last frame, pending frames are rendered automatically. Mirrors AUTO_FLUSH_MS on ESP32.

**2.3 BLE Transport --- BLEManager (iPhone)**

iPhone-side BLE Central. Scans for and connects to the ESP32\'s Nordic UART Service. Receives GFX bytes via TX characteristic notifications and pushes them to GFXDisplayView. Sends pong frames back via RX characteristic writes.

-   File: **BLEManager.swift**

-   Framework: **CoreBluetooth**

-   Threading: \@MainActor --- all callbacks and state on main thread

**Key Properties**

  ------------------------- -----------------------------------------------------------------------------
  **displayView**           GFXDisplayView --- shared with WiFi, single framebuffer for both transports

  **fbModel**               GFXFramebufferModel --- \@Published dimensions for SwiftUI aspect ratio

  **connectedPeripheral**   \@Published CBPeripheral? --- nil when disconnected

  **rxChar**                CBCharacteristic for RX (write to ESP32)

  **txChar**                CBCharacteristic for TX (notifications from ESP32)
  ------------------------- -----------------------------------------------------------------------------

**Data Flow**

didUpdateValueFor characteristic fires on every BLE notification. Data is passed directly to displayView.pushBytes(). The displayView accumulates frames until flush arrives --- BLE notifications may carry partial frames, and GFXCommandStream handles reassembly.

**Sending Pong**

TransportMonitor calls BLEManager.shared.writeToESP32(pongFrame, withResponse: false) to send the pong. withResponse: false uses Write Without Response for lower latency. The pong frame is 4 bytes: \[0xA5\]\[0x01\]\[0x00\]\[0xF1\].

**BLE Connect/Disconnect Detection**

BLEManager has no connect/disconnect callbacks --- LiveGraphicsView detects state changes via .onChange(of: ble.connectedPeripheral). On connect (peripheral != nil) and WiFi not active, TransportMonitor.setActiveTransport(.ble) is called. On disconnect, setActiveTransport(.none) is called if BLE was the active transport.

**2.4 WiFi Transport --- WiFiManager (iPhone)**

iPhone-side WiFi client. Discovers ESP32 via mDNS (NWBrowser), connects via NWConnection TCP, receives GFX bytes, and sends pong frames back. Notifies TransportMonitor of connection state changes.

-   File: **WiFiManager.swift**

-   Framework: **Network (NWBrowser, NWConnection)**

-   Threading: Background DispatchQueue for network I/O, main thread for state updates

**Discovery**

NWBrowser scans for \_uart.\_tcp Bonjour services. Each discovered device appears as a WiFiDevice in discoveredDevices\[\]. The user selects a device from the scan list UI to initiate connection.

**Connection State**

  ---------------------- ----------------------------------------------------------
  **.idle**              Not connected, not scanning

  **.connecting**        TCP connection in progress

  **.connected**         TCP ready --- data flowing

  **.failed(String)**    Connection failed or dropped --- reconnect timer running
  ---------------------- ----------------------------------------------------------

**TransportMonitor Notifications**

WiFiManager calls TransportMonitor at key lifecycle points:

-   TCP .ready: setActiveTransport(.wifi) + notifyTCPReady(true)

-   TCP .failed / .waiting: notifyTCPReady(false)

-   disconnect(): setActiveTransport(.none)

-   handleUnexpectedDisconnect(): notifyTCPReady(false) + setActiveTransport(.none)

**Reconnect Logic**

On unexpected disconnect, scheduleReconnect() starts an exponential backoff timer (2s initial, doubling up to 30s). On retry, resolveAndReconnect() re-queries discoveredDevices for a fresh endpoint in case the ESP32\'s IP changed, falling back to the last known endpoint.

**Sending Pong**

TransportMonitor calls WiFiManager.shared.send(pongFrame). send() uses NWConnection.send() with .contentProcessed completion. WiFiManager.swift has no knowledge of the ping/pong protocol --- it just sends raw Data.

**2.5 TransportMonitor --- Unified Health Monitoring**

Single source of truth for ping health across both transports. Owns the watchdog timer, threshold crossing logs, pong routing, and NWPathMonitor. All UI health state comes from here regardless of which transport is active.

-   File: **TransportMonitor.swift**

-   Threading: \@MainActor --- all state on main thread, Timer on main RunLoop

-   Pattern: ObservableObject singleton --- TransportMonitor.shared

**State Machine**

  -----------------------------------------------------------------------------------------------------
  **State**       **Trigger**                 **Effect**
  --------------- --------------------------- ---------------------------------------------------------
  .none           setActiveTransport(.none)   Stop watchdog, stop NWPathMonitor, pingHealth = .normal

  .wifi           setActiveTransport(.wifi)   Start NWPathMonitor, start watchdog, reset thresholds

  .ble            setActiveTransport(.ble)    Stop NWPathMonitor, start watchdog, reset thresholds
  -----------------------------------------------------------------------------------------------------

**pingArrived()**

Called by GFXDisplayView.onPingReceived (wired in LiveGraphicsView). Resets lastPingReceivedAt, clears loggedThresholds, sets pingHealth = .normal, and routes the pong to the correct transport based on activeTransport.

-   BLE pong: BLEManager.shared.writeToESP32(pongFrame)

-   WiFi pong: WiFiManager.shared.send(pongFrame)

**Ping Health Diagnosis**

checkPingHealth() fires every second via a main-thread Timer. When elapsed time since last ping exceeds 6 seconds (2x ping interval), a PingHealthState is published. WiFi uses three-way diagnosis; BLE uses two-way.

  ---------------------------------------------------------------------------------
  **State**             **Transport**   **Condition**
  --------------------- --------------- -------------------------------------------
  .iPhoneLostWiFi       WiFi            NWPathMonitor reports network unsatisfied

  .esp32Unreachable     WiFi            Network ok, TCP not in .ready state

  .esp32NotResponding   WiFi            Network ok, TCP ready, no pings arriving

  .bleNotResponding     BLE             BLE stack connected, no pings arriving
  ---------------------------------------------------------------------------------

**NWPathMonitor**

Started only when WiFi transport is active. Detects when the iPhone itself loses network connectivity --- distinguishing \'iPhone lost WiFi\' from \'ESP32 out of range\'. Runs on a background queue, posts networkSatisfied updates to main thread.

**Threshold Crossing Log**

Logs are printed once per delay episode (reset when pingArrived() is called). Three thresholds: 500ms (loop may be slow), 1500ms (WARNING: significantly delayed), 6000ms (CRITICAL: connection likely dead). Same thresholds and logic on both ESP32 and iPhone sides.

**2.6 LiveGraphicsView --- SwiftUI Integration**

Main SwiftUI view for the graphics display. Shows GFXDisplayView wrapped in a UIViewRepresentable, overlays the PingHealthBanner, and wires all callbacks between BLEManager, WiFiManager, TransportMonitor, and GFXDisplayView.

-   File: **LiveGraphicsView.swift**

**Callback Wiring (onAppear)**

-   wifi.onDataReceived → BLEManager.shared.displayView.pushBytes(data)

-   wifi.onConnected → displayView.resetStream()

-   displayView.onPingReceived → TransportMonitor.shared.pingArrived()

-   .onChange(ble.connectedPeripheral) → TransportMonitor.setActiveTransport(.ble / .none)

**PingHealthBanner**

Shown when TransportMonitor.pingHealth != .normal. Slides in from top with animation. Shows transport-appropriate icon, live second count, and explanation text. Disconnect button routes to wifi.disconnect() or ble.disconnect() based on activeTransport. Disappears automatically when pings resume.

**Section 3 --- Binary Wire Protocol**

**3.1 GFX Command Frames (ESP32 → iPhone)**

All GFX data travels as length-prefixed binary frames in a continuous byte stream. There is no message boundary marker --- the length field provides framing.

> \[ opcode : 1 byte \]\[ payload_length : 2 bytes little-endian \]\[ payload : N bytes \]

The iPhone\'s GFXCommandStream accumulates bytes until a complete frame is assembled, handling TCP/BLE fragmentation transparently. Frames arrive as a continuous stream with no gaps between them.

**3.2 Back-Channel Frames (iPhone → ESP32)**

Back-channel messages (pong, future UI events) use a separate framing with a magic byte sync marker, allowing the ESP32\'s back-channel parser to resync if bytes are lost.

> \[ 0xA5 magic : 1 byte \]\[ payload_length : 2 bytes little-endian \]\[ cmd : 1 byte \]\[ payload : N bytes \]
>
> Pong frame: 0xA5 0x01 0x00 0xF1 (4 bytes total)
>
> Ping frame: 0xA5 0x01 0x00 0xF0 (4 bytes total, ESP32 → iPhone via GFX stream)

**3.3 Flush-Based Frame Batching**

Each logical display frame (one renderTelemetry() call) is bounded by explicit flush() calls in the application code. The ESP32 accumulates all GFX commands for a frame in WiFiTransport.\_txBuf and sends them as a single TCP write on flush(). The iPhone accumulates decoded frames in GFXDisplayView.pendingFrames\[\] until GFX_CMD_FLUSH arrives, then renders all at once.

> ESP32 renderTelemetry():
>
> drawLabel() → WiFiTransport.send() → \_txBuf.append() } accumulated
>
> fillRect() → WiFiTransport.send() → \_txBuf.append() } in buffer
>
> print() → WiFiTransport.send() → \_txBuf.append() } \~870 bytes
>
> → iphone_tft.flush():
>
> sendCommand(GFX_CMD_FLUSH) → \_txBuf.append(flush opcode)
>
> \_transport.flush() → WiFiTransport drains \_txBuf as ONE write
>
> iPhone GFXDisplayView.pushBytes():
>
> frame(drawLabel) → pendingFrames.append() } buffered
>
> frame(fillRect) → pendingFrames.append() } until flush
>
> frame(print) → pendingFrames.append() }
>
> frame(flush) → applyPendingFrames() → iPhoneGFXRenderer → setNeedsDisplay()

**Section 4 --- File Index**

**ESP32 Source Files**

  --------------------------------------------------------------------------------------------------------
  **File**                      **Location**           **Purpose**
  ----------------------------- ---------------------- ---------------------------------------------------
  main.cpp                      src/                   System orchestration, tuning constants, loop()

  ActiveTransport.h             src/graphics/          Runtime transport switcher (header only)

  GraphicsTransport.h           src/graphics/          Abstract base class for all transports

  Graphics.h/.cpp               src/graphics/          GFX command encoder

  BleGraphicsTransport.h/.cpp   src/ble/               BLE concrete transport (delegates to BLEManager)

  BLEManager.h/.cpp             src/ble/               NimBLE UART server, TX ring buffer, BLE heartbeat

  WiFiTransport.h/.cpp          src/WiFi/              WiFi concrete transport (flush-based batching)

  WiFiManager.h/.cpp            src/WiFi/              AsyncTCP server, mDNS, WiFi heartbeat

  Adafruit_iPhoneTFT.h/.cpp     src/Adafruit_iPhone/   Adafruit_GFX subclass, public drawing API

  DisplayModule.h/.cpp          src/display/           High-level telemetry renderer (calls GFX API)
  --------------------------------------------------------------------------------------------------------

**iPhone Source Files**

  --------------------------------------------------------------------------------------------------------------
  **File**                  **Type**           **Purpose**
  ------------------------- ------------------ -----------------------------------------------------------------
  RGB565.swift              Core --- edit      GFXOp enum, GFXCommandStream, iPhoneGFXRenderer, GFXDisplayView

  TransportMonitor.swift    New file           Unified ping health, watchdog, pong routing, NWPathMonitor

  WiFiManager.swift         Replace existing   NWBrowser, NWConnection, TCP client, notifies TransportMonitor

  BLEManager.swift          Minimal edits      CoreBluetooth Central, pushes bytes to displayView

  LiveGraphicsView.swift    Replace existing   SwiftUI display view, callback wiring, PingHealthBanner

  WiFiScanView.swift        Unchanged          WiFi device discovery UI

  BluetoothScanView.swift   Unchanged          BLE device scan UI
  --------------------------------------------------------------------------------------------------------------