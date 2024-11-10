# DMX-Interface

Art-Net-Interface

## Case

All print files (STL, STEP, X_T) can be found in [assets/case](/assets/case/). Alternatively you can view the project on [OnShape](https://cad.onshape.com/documents/7363818fd18bf0cbf094790e/w/52455282b39e47fbde5d0e53/e/9bec98aa83a813dc9a4d6ab2) where you can export the files in a format you like.

![Prusa Slicer with case loaded](/assets/case/Screenshot.png)

## Wiring

![Handwritten diagram](/assets/circuit/handwritten/circuit%20diagram.jpeg)

## Pin usage

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
