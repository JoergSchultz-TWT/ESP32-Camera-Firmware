# ESP32 Camera API

**Status:** Draft  
**Version:** 0.1  
**Purpose:** Shared interface contract between the ESP32 camera firmware and smartphone clients.

---

## 1. Scope

This document defines the network-facing API of the ESP32 camera device.

The camera is intended to be a reusable standalone component. It must not depend on any specific application such as PawPlate. Applications may use the camera for different purposes, but they should interact with it through the same API.

The initial system consists of:

- **Camera device:** Seeed Studio XIAO ESP32-S3 Sense
- **Camera firmware:** based on `ESP32-CAM_MJPEG2SD`
- **Client:** React Native / Expo smartphone application
- **Transport:** Wi-Fi
- **Recording model:** local recording to microSD, followed by post-session transfer to the phone
- **Preview model:** low-resolution MJPEG preview before recording
- **Important constraint:** preview and recording do not need to run simultaneously

This is an intentionally small first version. The API should grow only when an actual use case requires it.

---

## 2. Design Principles

1. **The camera is application-agnostic.**  
   The firmware should not know why a recording is being made.

2. **The phone owns the user interaction.**  
   The ESP32 exposes device capabilities and state; the smartphone decides how these are presented.

3. **Record locally first.**  
   Video is written to the camera's microSD card during a session.

4. **Transfer after recording.**  
   The completed recording can then be downloaded by the smartphone.

5. **Keep the protocol simple.**  
   Prefer plain HTTP and JSON unless a more complex mechanism is clearly justified.

6. **Make state explicit.**  
   The phone should always be able to ask what the camera is currently doing.

7. **Design for reuse.**  
   The same API should be usable from the standalone camera app, PawPlate, Dog AI Gym, or other future applications.

---

## 3. API Versioning

All application API endpoints should be placed below:

```text
/api/v1/
```

Example:

```text
GET /api/v1/status
```

The preview stream may remain outside this namespace if required by the underlying `ESP32-CAM_MJPEG2SD` implementation.

Breaking changes require a new API version.

---

## 4. Device States

The camera should expose one primary state.

Suggested initial states:

```text
idle
preview
recording
processing
transferring
error
```

### `idle`

The camera is ready and no preview or recording is active.

### `preview`

The camera is providing a live preview.

### `recording`

The camera is recording JPEG/MJPEG data to microSD.

### `processing`

Recording has stopped, but the file is still being finalized.

### `transferring`

A recording is currently being downloaded.

### `error`

The camera cannot perform normal operation until the error is resolved.

The implementation may later distinguish additional internal states, but clients should not need to understand firmware internals.

---

## 5. General Response Format

Successful JSON responses should use HTTP 2xx status codes.

Example:

```json
{
  "success": true
}
```

Errors should use an appropriate HTTP status code and return a machine-readable code plus a human-readable message.

Example:

```json
{
  "success": false,
  "error": {
    "code": "CAMERA_BUSY",
    "message": "Cannot start preview while recording."
  }
}
```

Suggested error codes include:

```text
CAMERA_BUSY
NO_SD_CARD
SD_CARD_FULL
RECORDING_NOT_ACTIVE
PREVIEW_NOT_ACTIVE
RECORDING_NOT_FOUND
INVALID_REQUEST
INTERNAL_ERROR
```

---

## 6. Device Information

### `GET /api/v1/info`

Returns relatively static information about the camera.

Example response:

```json
{
  "apiVersion": "1",
  "firmwareVersion": "0.1.0",
  "deviceId": "camera-001",
  "cameraModel": "OV2640",
  "board": "XIAO_ESP32S3_SENSE",
  "storage": {
    "present": true,
    "totalBytes": 31914983424,
    "freeBytes": 28735111168
  }
}
```

### Notes

- `deviceId` must be stable for a particular camera.
- `cameraModel` should be determined from the detected sensor PID rather than hard-coded.
- Expected values initially include `OV2640` and `OV3660`.

---

## 7. Camera Status

### `GET /api/v1/status`

Returns the current runtime state.

Example response:

```json
{
  "state": "idle",
  "previewActive": false,
  "recordingActive": false,
  "recording": null
}
```

While recording:

```json
{
  "state": "recording",
  "previewActive": false,
  "recordingActive": true,
  "recording": {
    "id": "20260929_061530",
    "startedAt": "2026-09-29T06:15:30Z",
    "durationMs": 18542,
    "bytesWritten": 18723456
  }
}
```

The exact timestamp strategy can be revised if the ESP32 does not yet have reliable wall-clock time. A recording ID must not depend on correct wall-clock time unless time synchronization is guaranteed.

---

## 8. Preview

The preview is intended only for framing before recording.

The initial design explicitly does **not** require preview and recording to operate simultaneously.

### `POST /api/v1/preview/start`

Starts preview mode.

Example response:

```json
{
  "success": true,
  "state": "preview",
  "streamUrl": "/stream"
}
```

Possible failure:

```json
{
  "success": false,
  "error": {
    "code": "CAMERA_BUSY",
    "message": "Preview cannot be started while recording."
  }
}
```

---

### `POST /api/v1/preview/stop`

Stops preview mode.

Example response:

```json
{
  "success": true,
  "state": "idle"
}
```

---

### `GET /stream`

Returns the MJPEG preview stream.

Suggested content type:

```text
multipart/x-mixed-replace
```

The implementation may initially use the streaming endpoint already provided by `ESP32-CAM_MJPEG2SD`.

Preview settings should favor responsiveness over image quality.

Initial target range:

```text
640x480 or 800x600
approximately 10-15 fps
```

These values are targets, not protocol guarantees.

---

## 9. Recording

### `POST /api/v1/recordings/start`

Starts a new local recording.

If preview is currently running, the firmware may automatically stop preview before starting recording.

Initial request body:

```json
{}
```

Possible future request:

```json
{
  "label": "optional-client-label"
}
```

Example response:

```json
{
  "success": true,
  "recording": {
    "id": "rec_000012",
    "startedAt": "2026-09-29T06:15:30Z"
  }
}
```

The recording identifier must uniquely identify the resulting recording on the device.

---

### `POST /api/v1/recordings/stop`

Stops the active recording and finalizes the recording file.

Example response:

```json
{
  "success": true,
  "recording": {
    "id": "rec_000012",
    "durationMs": 342106,
    "sizeBytes": 281994321,
    "ready": true
  }
}
```

If file finalization takes noticeable time, the response may instead return:

```json
{
  "success": true,
  "recording": {
    "id": "rec_000012",
    "ready": false
  }
}
```

The client can then poll the recording metadata endpoint until `ready` becomes `true`.

---

## 10. Recordings

### `GET /api/v1/recordings`

Lists recordings currently stored on the camera.

Example response:

```json
{
  "recordings": [
    {
      "id": "rec_000012",
      "filename": "rec_000012.avi",
      "sizeBytes": 281994321,
      "durationMs": 342106,
      "createdAt": "2026-09-29T06:15:30Z",
      "ready": true
    },
    {
      "id": "rec_000011",
      "filename": "rec_000011.avi",
      "sizeBytes": 92177411,
      "durationMs": 110430,
      "createdAt": "2026-09-29T05:52:01Z",
      "ready": true
    }
  ]
}
```

Fields may initially be limited to information already available from `ESP32-CAM_MJPEG2SD`.

---

### `GET /api/v1/recordings/{id}`

Returns metadata for one recording.

Example:

```json
{
  "id": "rec_000012",
  "filename": "rec_000012.avi",
  "sizeBytes": 281994321,
  "durationMs": 342106,
  "createdAt": "2026-09-29T06:15:30Z",
  "ready": true
}
```

---

### `GET /api/v1/recordings/{id}/file`

Downloads the recording.

Expected response:

```text
HTTP 200
Content-Type: application/octet-stream
Content-Length: ...
```

If a stable media type is established later, it should replace `application/octet-stream`.

The initial implementation should favor reliable transfer over sophisticated streaming.

Support for HTTP range requests would be useful later, but is not required for the first implementation.

---

### `DELETE /api/v1/recordings/{id}`

Deletes a recording from the camera's microSD card.

Example response:

```json
{
  "success": true
}
```

Deletion must fail while the specified recording is still active or being finalized.

---

## 11. Storage Status

Storage information may already be included in `/api/v1/info`, but a runtime endpoint may be useful.

### `GET /api/v1/storage`

Example response:

```json
{
  "present": true,
  "totalBytes": 31914983424,
  "freeBytes": 28735111168,
  "usedBytes": 3179872256
}
```

This endpoint is optional for the first implementation if equivalent information is already available elsewhere.

---

## 12. Configuration

Configuration should be deliberately limited in version 1.

Possible future endpoint:

```text
GET  /api/v1/config
PUT  /api/v1/config
```

Potential settings include:

```json
{
  "preview": {
    "width": 640,
    "height": 480,
    "quality": 20
  },
  "recording": {
    "width": 800,
    "height": 600,
    "fps": 15,
    "quality": 12
  }
}
```

These fields should not become part of the public API until the firmware can reliably support them.

For the first implementation, sensible defaults in firmware are preferable.

---

## 13. Client Workflow

The expected first-version client workflow is:

```text
Connect to camera
      |
      v
GET /api/v1/info
      |
      v
GET /api/v1/status
      |
      v
POST /api/v1/preview/start
      |
      v
Display /stream
      |
      v
User presses Record
      |
      v
POST /api/v1/preview/stop
      |
      v
POST /api/v1/recordings/start
      |
      v
Local recording to microSD
      |
      v
User presses Stop
      |
      v
POST /api/v1/recordings/stop
      |
      v
Wait until recording is ready
      |
      v
GET /api/v1/recordings/{id}/file
      |
      v
Store recording on smartphone
      |
      v
DELETE recording from camera
      |
      v
Return to idle / preview
```

The final delete step should happen only after the smartphone has verified a successful transfer.

---

## 14. Connection and Discovery

The exact Wi-Fi architecture is not yet fixed.

Possible options include:

1. ESP32 camera provides its own Wi-Fi access point.
2. ESP32 camera joins an existing WLAN.
3. Camera supports both modes.
4. A provisioning mechanism selects the desired network.

The API defined in this document should remain the same regardless of how the phone reaches the camera.

Device discovery is therefore intentionally left open in version 0.1.

Potential future options include:

```text
mDNS
fixed access-point address
QR-code provisioning
BLE-assisted provisioning
manual IP entry
```

---

## 15. Events / WebSocket

A WebSocket or server-sent event channel may become useful for asynchronous status updates.

Possible events:

```text
recording_started
recording_stopped
recording_ready
storage_low
transfer_started
transfer_completed
error
```

However, version 1 should not require WebSockets unless polling proves insufficient.

A simple polling model via:

```text
GET /api/v1/status
```

is acceptable initially.

---

## 16. File Format

The initial firmware is based on `ESP32-CAM_MJPEG2SD`.

The actual recording format should therefore follow what the firmware can produce reliably, rather than forcing the ESP32 to encode H.264.

The smartphone application may later:

- play the recording directly;
- repackage the MJPEG/JPEG recording;
- convert it into a more conventional container;
- upload it to another application or backend.

The public API should not assume H.264.

---

## 17. Security

Version 1 is intended for local development and direct local Wi-Fi use.

The initial implementation may therefore operate without authentication.

Before use in a released product, the following must be considered:

- device pairing;
- camera authentication;
- Wi-Fi credentials;
- unauthorized download of recordings;
- unauthorized start/stop commands;
- firmware updates;
- unique device identity.

Security mechanisms should be added without changing the fundamental camera operations defined here.

---

## 18. Firmware Update

OTA firmware updating is a desirable future capability, but is outside the first API version.

Potential future endpoints:

```text
GET  /api/v1/firmware
POST /api/v1/firmware/update
```

The initial development workflow will use USB flashing.

---

## 19. Minimum Version 1 API

The smallest useful implementation is:

```text
GET    /api/v1/info
GET    /api/v1/status

POST   /api/v1/preview/start
POST   /api/v1/preview/stop
GET    /stream

POST   /api/v1/recordings/start
POST   /api/v1/recordings/stop

GET    /api/v1/recordings
GET    /api/v1/recordings/{id}
GET    /api/v1/recordings/{id}/file
DELETE /api/v1/recordings/{id}
```

Everything else in this document should be considered optional until required.

---

## 20. Open Questions

The following decisions are deliberately not fixed yet:

- OV2640 versus OV3660 on the actual hardware batch
- final preview resolution and frame rate
- final recording resolution and frame rate
- exact recording file/container format produced by `ESP32-CAM_MJPEG2SD`
- Wi-Fi topology
- device discovery
- timestamp synchronization
- recording ID format
- whether preview must be explicitly stopped or is automatically stopped by `recordings/start`
- whether polling is sufficient or a WebSocket/event channel is useful
- transfer speed and whether resumable/range downloads are needed
- when recordings should be automatically removed from the SD card
- whether the smartphone should automatically convert recordings after transfer

These should be resolved through implementation and measurement rather than prematurely fixed in the API.

---

## 21. Compatibility Rule

Both the firmware repository and smartphone repository should keep a copy of this document.

One repository should eventually be designated the canonical source.

Changes that affect both firmware and clients should update:

1. this API document;
2. the firmware implementation;
3. the client implementation;
4. the API version if the change is breaking.

The goal is that any application capable of speaking this API can use the ESP32 camera without knowing how the camera firmware is implemented internally.
