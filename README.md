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
