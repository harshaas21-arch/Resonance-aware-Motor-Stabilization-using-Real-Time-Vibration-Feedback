# Resonance-aware-Motor-Stabilization-using-Real-Time-Vibration-Feedback
Project
Mechanical systems that incorporate rotating components are fundamental to a wide range of engineering 
applications, spanning from industrial machinery and automotive drivetrains to household appliances and 
precision instruments. A critical challenge inherent to all such systems is the management of vibration — 
particularly the phenomenon known as resonance, which occurs when the operating frequency of the 
rotating element coincides with a natural frequency of the mechanical structure. 

Resonance produces dramatically amplified oscillations that can compromise structural integrity, degrade 
performance, increase bearing wear, and in severe cases cause catastrophic mechanical failure. Traditional 
approaches to mitigating resonance typically rely on passive design strategies: adding damping materials, 
redesigning mechanical structures, or incorporating vibration isolation mounts. While effective, these 
methods are inherently static and cannot adapt to changing operational conditions or unpredictable 
excitation frequencies. 

The rapid advancement of low-cost microcontrollers, MEMS-based inertial sensors, and power 
electronics has opened the door to an entirely different class of solution: active, real-time feedback 
control. Rather than designing around vibration, an active system continuously monitors the vibration 
state of the machine and adjusts its operating parameters on the fly to avoid or exit resonant conditions. 
This project presents the design and implementation of a Resonance-Aware Motor Stabilization System 
built around the ESP32 microcontroller and the ADXL345 three-axis digital accelerometer. The system 
drives a DC motor through an L293D motor driver IC, with motor speed commanded either by a physical 
potentiometer or via a browser-based wireless dashboard. Simultaneously, the ADXL345 measures 
vibration along the Z-axis at a high repetition rate, and an exponential moving average filter extracts a 
smoothed vibration intensity metric. When this metric exceeds a configurable threshold, the firmware 
automatically reduces the PWM duty cycle applied to the motor, lowering its speed and moving the 
operating point away from the resonant region. 

The project goes beyond simple threshold switching by incorporating a wireless web dashboard accessible 
over the local WiFi network. The dashboard — named MOTOCTRL — provides live charts of vibration 
and PWM output, directional control, speed override capability, and runtime adjustment of the vibration 
threshold and damping parameters, all through a zero-dependency single-page web application served 
directly from the ESP32's flash memory. WebSocket communication ensures sub-200 ms latency between 
the embedded controller and the browser. 
 
The result is a compact, fully self-contained embedded system that demonstrates the practical integration 
of inertial sensing, digital signal conditioning, closed-loop feedback control, and wireless human-machine 
interface design — all running on a single microcontroller board.

Working Principle:

At startup the ESP32 initialises the I2C bus on GPIO 21 (SDA) and GPIO 22 (SCL) and configures the 
ADXL345. It then connects to the specified WiFi network, starts an HTTP server on port 80 to serve the 
MOTOCTRL dashboard page, and opens a WebSocket server on port 81 for bidirectional binary-latency 
communication. The main control loop executes every 10 ms and performs the following sequence: 
1. Read the 12-bit ADC value from the potentiometer connected to GPIO 34. Map this value 
linearly to a desired PWM duty cycle in the range 0–255. 
2. Poll the ADXL345 for a new acceleration sample. Compute the absolute deviation of the Z-axis 
reading from the baseline gravitational acceleration (9.8 m/s2) to isolate dynamic vibration. 
3. Apply an EMA filter with coefficient 0.3 to the raw vibration sample: filteredZ = 0.7 × filteredZ 
+ 0.3 × rawZ. 
4. Compare filteredZ against the vibration threshold. If the threshold is exceeded, subtract the 
damping amount from the desired PWM to obtain the final PWM. Clamp the result to the range 
[0, 255], with a minimum non-zero value of 120 to prevent motor stall. 
5. Write the final PWM to the EN pin of the L293D using the ESP32 ledc peripheral at 5 kHz 
switching frequency, 8-bit resolution. 
6. Every 100 ms, broadcast a JSON telemetry packet over WebSocket to all connected browser 
clients. 
