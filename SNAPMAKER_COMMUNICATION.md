# Snapmaker Communication Protocol Documentation

This document describes the communication protocols used by SnapmakerOrca for printer communication, based on analysis of the bambu_networking library integration.

## Overview

SnapmakerOrca uses a hybrid communication approach:
- **REST API** (HTTP) for commands and file transfers
- **MQTT** for real-time status updates and events

## REST API

### Base URL
```
http://<printer_ip>:8080/api/v1/
```

### Endpoints

#### Status
```
GET /api/v1/status
```
Returns current printer status including:
- Print state (idle, printing, paused, error)
- Current temperatures (nozzle, bed)
- Print progress
- Current file name

#### Connect
```
POST /api/v1/connect
Content-Type: application/json

{
  "version": "1.0"
}
```
Establishes connection with the printer.

#### Print File
```
POST /api/v1/print_file
Content-Type: multipart/form-data

file: <gcode or 3mf file>
task_ams_mapping: <JSON array of filament mappings>
```

### Filament Mapping Format

The `task_ams_mapping` field uses the following JSON structure:

```json
[
  {
    "ams": 0,
    "targetColor": "#FF0000",
    "filamentId": "GFL00",
    "filamentType": "PLA",
    "nozzleId": 0
  },
  {
    "ams": 1,
    "targetColor": "#0000FF",
    "filamentId": "GFL01",
    "filamentType": "PETG",
    "nozzleId": 1
  }
]
```

Fields:
- `ams`: Target tray/slot ID (0-based)
- `targetColor`: Hex color of the target filament
- `filamentId`: Filament preset ID
- `filamentType`: Material type string
- `nozzleId`: Which nozzle/extruder to use (for multi-nozzle systems)

## MQTT Protocol

### Connection
- Default port: 1883
- Uses TLS when available
- Topics are prefixed with device serial number

### Topics

#### Status Updates
```
device/<serial>/report
```
Publishes JSON status updates:
```json
{
  "print": {
    "command": "push_status",
    "msg": 0,
    "sequence_id": "123",
    "gcode_state": "RUNNING",
    "mc_percent": 45,
    "mc_remaining_time": 3600
  }
}
```

#### Commands
```
device/<serial>/request
```
Accepts JSON commands:
```json
{
  "print": {
    "command": "pause",
    "sequence_id": "124"
  }
}
```

### Common Commands
- `pause` - Pause current print
- `resume` - Resume paused print
- `stop` - Cancel current print
- `push_status` - Request status update

## DeviceManager Integration

OrcaSlicer's `DeviceManager` class (`src/slic3r/GUI/DeviceManager.hpp`) provides the abstraction layer:

```cpp
class MachineObject {
    // Connection state
    bool is_connected();
    std::string get_dev_id();

    // AMS/Filament info
    std::map<std::string, Ams*> amsList;
    int ams_exist_bits;
    int tray_exist_bits;

    // Print control
    int command_start_print(const std::string& filename);
    int command_pause();
    int command_resume();
};
```

## Slot Count Configuration

The Snapmaker U1 supports 8 material slots (vs 4 for standard Bambu AMS).

Configure in printer profile JSON:
```json
{
  "material_slot_count": "8",
  "supports_filament_mapping": "1"
}
```

## Related Files

- `src/slic3r/GUI/DeviceManager.cpp` - Device communication logic
- `src/slic3r/GUI/SelectMachine.cpp` - Print dialog and AMS mapping
- `src/slic3r/GUI/FilamentMappingDialog.cpp` - Generic filament mapping UI
- `src/libslic3r/ProjectTask.hpp` - FilamentInfo structure definition

## See Also

- BambuStudio networking library documentation
- Klipper printer communication docs (for G-code format)
