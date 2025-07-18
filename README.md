# RF Security Protocol Analyzer

## Overview

RF Security Protocol Analyzer is a desktop application designed for real-time analysis and monitoring of wireless radio frequency (RF) protocols. Leveraging a software-defined radio (SDR), it scans, detects, and classifies signals across common ISM bands (e.g., 433 MHz, 868 MHz, 915 MHz), Zigbee, LoRa, TPMS, garage door remotes, and other wireless devices. The application provides a graphical user interface (GUI) for interactive control and visualization, and a protocol analyzer for security monitoring and threat detection.

## Features
- **Real-Time RF Scanning:** Continuously monitors and scans RF spectrum using SDR hardware.
- **Protocol Detection:** Identifies and classifies a wide range of wireless protocols and devices.
- **Device Tracking:** Tracks detected devices, flags unauthorized or unknown devices, and maintains a device database.
- **Security Alerts:** Detects suspicious activity and highlights potential security risks.
- **Interactive GUI:** User-friendly interface for frequency tuning, gain adjustment, protocol scanning, and visualization.

## Usage
1. **Connect your SDR hardware** (compatible with the application).
2. **Build and run the application** using the provided makefile.
3. **Use the GUI** to start scanning, tune frequencies, and monitor detected devices and protocols.

### Controls (from within the app)
- Arrow keys: Frequency tuning
- +/-: Gain adjustment
- S: Start/stop protocol scan
- P: Pause scan
- M: Manual control
- Q/ESC: Quit

## Requirements
- C++17 or later
- Compatible SDR hardware (e.g., RTL-SDR)
- SDL2 for GUI rendering

## License
This project is licensed under the MIT License.

---

For more information or contributions, please open an issue or submit a pull request.
