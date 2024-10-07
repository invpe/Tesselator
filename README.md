# Tesselator

![image](https://github.com/user-attachments/assets/ebf88d73-133c-4bd6-9492-cb8744253d4f)


Tesselator is a lightweight and efficient distributed computing solution designed even for home use, utilizing tiny ESP32 devices and a simple Python tool. 
The system operates without the need for a central server, as nodes communicate over UDP to advertise their availability and retrieve tasks, payloads, and arguments through HTTP endpoints, all managed by a straightforward Python script.

The project leverages dynamic binary loading and execution using an ELF loader, allowing the ESP32 to run precompiled tasks with maximum efficiency. Intermediate files, including binaries, input payloads, and output payloads, are stored on the device using SPIFFS, eliminating the need for additional hardware beyond the ESP32 node itself.






