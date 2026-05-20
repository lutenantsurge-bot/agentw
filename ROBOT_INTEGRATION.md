# Robot + IP Webcam Integration for llama-box-agent

This document describes the robot control and IP Webcam integration added to llama-box-agent.

## Overview

The `-sbot` flag enables two new modules:

1. **SerialRobot** - USB serial communication with Arduino/ESP32 for motor control
2. **IPCam** - JPEG frame capture from Android IP Webcam app

## Building with Robot Mode

```bash
cd ~/Desktop/llama-box-agentw
./build.sh -sbot
```

## Command Line Options

```
-sbot             Enable robot + camera mode
-sbot-port <p>    Serial port (default: /dev/ttyUSB0)
-sbot-baud <n>    Baud rate (default: 115200)
-sbot-cam <ip>    IP Webcam IP address (default: 192.168.1.100)
-sbot-cam-port <n> IP Webcam port (default: 8080)
```

## Serial Protocol (Arduino/ESP32)

The robot communicates via text commands over serial (newline-terminated):

| Command | Meaning |
|---------|---------|
| `F<speed>` | Forward, speed 0-255 |
| `B<speed>` | Backward |
| `L<speed>` | Turn left (differential) |
| `R<speed>` | Turn right |
| `S` | Stop motors |
| `X` | Emergency stop |
| `A<n>:<deg>` | Servo arm n to degree 0-180 |

## Arduino Example (L298N Motor Driver)

```cpp
// RC Car L298N Arduino Sketch
#define ENA 9
#define IN1 8
#define IN2 7
#define ENB 10
#define IN3 6
#define IN4 5

int currentSpeed = 160;

void setup() {
  pinMode(ENA, OUTPUT); pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  Serial.begin(115200);
  stopMotors();
  Serial.println("RC Car L298N Ready");
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    handleCommand(cmd);
  }
}

void handleCommand(String cmd) {
  if (cmd.startsWith("F")) forward(cmd.substring(1).toInt());
  else if (cmd.startsWith("B")) backward(cmd.substring(1).toInt());
  else if (cmd.startsWith("L")) turnLeft(cmd.substring(1).toInt());
  else if (cmd.startsWith("R")) turnRight(cmd.substring(1).toInt());
  else if (cmd == "S") stopMotors();
  else if (cmd == "X") { currentSpeed = 0; stopMotors(); }
}

void forward(int s) {
  int spd = (s==-1)?currentSpeed:s;
  analogWrite(ENA, spd); digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  analogWrite(ENB, spd); digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void backward(int s) {
  int spd = (s==-1)?currentSpeed:s;
  analogWrite(ENA, spd); digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  analogWrite(ENB, spd); digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
}

void turnLeft(int s) {
  int spd = (s==-1)?currentSpeed:s;
  analogWrite(ENA, spd); digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  analogWrite(ENB, spd); digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void turnRight(int s) {
  int spd = (s==-1)?currentSpeed:s;
  analogWrite(ENA, spd); digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  analogWrite(ENB, spd); digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
}

void stopMotors() {
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}
```

## IP Webcam Setup

1. Install "IP Webcam" app on Android phone
2. Start the server (port 8080 by default)
3. Note the phone's IP address
4. Connect to the system with: `-sbot -sbot-cam <phone-ip>`

## Tool Calls

The LLM can now call these tools:

### rc_control
```json
{
  "name": "rc_control",
  "arguments": {
    "action": "forward",
    "speed": 160
  }
}
```

Actions: forward, backward, left, right, stop, emergency_stop

### webcam_capture
```json
{
  "name": "webcam_capture",
  "arguments": {
    "save_as": "frame.jpg"
  }
}
```

## Usage Examples

### Basic robot control
```bash
./build.sh -sbot
./build/bin/llama-box-agent
```

### Custom serial settings
```bash
./build/bin/llama-box-agent -sbot -sbot-port /dev/ttyACM0 -sbot-baud 9600
```

### With IP Webcam
```bash
./build/bin/llama-box-agent -sbot -sbot-cam 192.168.1.50 -sbot-cam-port 8080
```

### Combined with voice
```bash
WHISPER_ENABLED=1 ./build/bin/llama-box-agent -sbot
```

## Files Modified/Created

- `serial_robot.h/cpp` - Serial communication module
- `ipcam.h/cpp` - IP Webcam capture module  
- `robot_tools.h/cpp` - Robot control tools
- `main.cpp` - Updated with -sbot flag and robot integration
- `CMakeLists.txt` - Added WITH_ROBOT and WITH_IPCAM options
- `build.sh` - Added -sbot build flag

## Next Steps

- Add vision-to-robot bridge (clip.cpp for obstacle detection)
- Implement autonomous heartbeat loop
- Add dataset logging for fine-tuning
- Integrate with llama-box multimodal pipeline
