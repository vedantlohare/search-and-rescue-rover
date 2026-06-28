Firmware Architecture

The rover firmware is divided into three independent nodes.

Node 1

ESP32-CAM

Streams live MJPEG video.

---------------------

Node 2

ESP32 WROOM

Receives UDP commands

Reads sensors

Controls DC motors

Performs localization

---------------------

Node 3

Arduino UNO

Controls robotic arm servos

Executes manipulation commands

Communicates with Node 2