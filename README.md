# DMX-Interface

> _Art-Net interface for controlling DMX devices via WiFi or Ethernet._

---

## 🛒 Parts

| Count | Part          |
| ----- | ------------- |
| 1x    | ESP32         |
| 2x    | RS485         |
| 1x    | W5500-ETH     |
| 1x    | LED-Button    |
| 1x    | ♂️-DMX-socket |
| 1x    | ♀️-DMX-socket |

> Additionally you need: `some wires`, `soldering equipment`, `3D-printer`, `small screws`, `shrink tubing`, `hot glue gun`

---

## 📱 Implemented microcontrollers

-   [x] Lolin S2 mini
-   [ ] ESP 32 WROOM
-   [ ] ESP 32 C3

> For other microcontrollers you may need to adjust the `platformio.ini`

---

## 🔌 Wiring

You have to short-circuit `R0` on the RS485 boards to enable the termination resistor required for DMX before the first and after the last device in line.

![Circuit diagram](/assets/circuit/diagram.svg)

| GPIO | Usage          |
| ---- | -------------- |
| GND  | GND to others  |
| 3,5V | VIN on RS485   |
| 5V   | VIN on W5500   |
| 0    | Onboard Button |
| 5    | Ext. Button    |
| 7    | Ext. LED       |
| 15   | Onboard LED    |
| 17   | U1TXD          |
| 18   | U1RXD          |
| 21   | U0TXD          |
| 33   | U0RXD          |
| 34   | SPI CS         |
| 35   | SPI MOS        |
| 36   | SPI SCK        |
| 37   | SPI MISO       |

---

## 🚀 Installation

1. make sure you have [PlatformIO](https://platformio.org/) installed
2. open the project folder in PlatformIO
3. click `Upload Filesystem Image`
4. click `Upload and Monitor`
5. 🏁 you are done 🎉

---

## 📦 Case

All print files (STL, STEP, X_T) can be found in [assets/case](/assets/case/). Alternatively you can view the project on [OnShape](https://cad.onshape.com/documents/7363818fd18bf0cbf094790e/w/52455282b39e47fbde5d0e53/e/9bec98aa83a813dc9a4d6ab2) where you can export the files in a format you like.

![Prusa Slicer with case loaded](/assets/case/Screenshot.png)

---

## 💡 Status LED

| LED                               | Description               |
| --------------------------------- | ------------------------- |
| ![off](/assets/led/off.gif)       | no power; LED deactivated |
| ![static](/assets/led/static.gif) | powered on; normal        |
| ![slow](/assets/led/slow.gif)     | startup; warning          |
| ![fast](/assets/led/fast.gif)     | resetting; error          |

---

## ⚙️ Default config

To reset the settings, hold down the button and connect the ESP to the power supply. As soon as the LED flashes quickly, the settings are reset.

| Setting        | Value              |
| -------------- | ------------------ |
| TYPE           | WiFi AP            |
| SSID           | ChaosDMX-□□□□      |
| PASSWORD       | mbgmbgmbg          |
| IP-Address     | 192.168.4.1        |
| DMX1 (Left)    | OUTPUT; Universe 1 |
| DMX2 (Right)   | INPUT; Universe 2  |
| LED Brightness | 10 %               |

---

## 🤝 Contributing

Contributions, issues and feature requests are welcome!<br />Feel free to check the [issues page](https://github.com/HendrikRauh/dmx-interface/issues).

---

## 👥 Authors

### [@HendrikRauh](https://github.com/HendrikRauh)

### [@RaffaelW](https://github.com/RaffaelW)

### [@psxde](https://github.com/psxde)

[Chaostreff Backnang](https://chaostreff-backnang.de/)
