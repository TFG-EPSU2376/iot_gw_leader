# Gateway OpenThread Leader to LoRa

This project is an implementation of an OpenThread Leader to LoRa gateway using the ESP32 platform. It acts as the Leader of an OpenThread network, gathers data from the network, and transmits the data over LoRa. This setup can be used to extend the reach of IoT devices connected via OpenThread by using LoRa to communicate with remote servers or other LoRa nodes.

## Features

- Acts as an OpenThread Leader, managing an OpenThread mesh network
- Gathers data from connected OpenThread devices
- Transmits data over LoRa to other LoRa receivers or gateways
- Configurable LoRa and OpenThread settings

## Hardware Requirements

- ESP32 Development Board with OpenThread support
- LoRa Module (SX1276/SX1278)
- OpenThread-capable devices (e.g., nRF52840)
- WiFi network for configuration and monitoring (optional)

## Software Requirements

- ESP-IDF Framework
- OpenThread Stack
- Configured LoRa settings

## Installation

1. Clone the repository:

   ```sh
   git clone https://github.com/TFG-EPSU2376/iot_gw_lora2mqtt
   cd iot_gw_lora2mqtt
   ```

2. Set up the ESP-IDF environment:
   Follow the instructions on the [ESP-IDF documentation](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/) to set up your ESP32 development environment.

3. Configure the project, especially the `sdkconfig` file if needed.

4. Build and flash the project:

   ```sh
   idf.py build
   idf.py flash
   idf.py monitor
   ```

5. Configuration
   Update the OpenThread and LoRa configuration as needed in main.c:

#define OPENTHREAD_CHANNEL 11
#define OPENTHREAD_PANID 0x1234
#define LORA_FREQUENCY 915E6
#define LORA_SPREADING_FACTOR 7
#define LORA_BANDWIDTH 125E3
#define LORA_SYNC_WORD 0x12
Usage
Once flashed, the ESP32 will act as the Leader of the OpenThread network. Connected OpenThread devices will send data to the Leader, which will then be transmitted over LoRa. Monitor the ESP32's serial output to view data transmission logs.

## Troubleshooting

Ensure your OpenThread devices are correctly configured to join the network.
Verify that the LoRa settings match those of your LoRa receiver.
Use the idf.py monitor command to debug and view real-time logs.
Contributing
Contributions are welcome! Please submit a pull request or open an issue to discuss potential changes.

License
This project is licensed under the MIT License - see the LICENSE file for details.
