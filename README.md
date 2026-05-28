# DMX-Interface

Art-Net interface for controlling DMX devices via WiFi or Ethernet.

> [!NOTE]
> This project is currently in a rewrite phase, we switched from Arduino to ESP-IDF and are reworking the codebase. The current state is not stable, but you can check out the [legacy/arduino](https://github.com/HendrikRauh/dmx-interface/tree/legacy/arduino) branch.
> Feel free to help us by contributing to the project!

______________________________________________________________________

## 🛒 Parts

| Count | Part |
| ----- | --------------------- |
| 1x | ESP32 (Lolin S2 Mini) |
| 2x | RS485 |
| 1x | W5500-ETH |
| 1x | LED-Button |
| 1x | ♂️-DMX-socket |
| 1x | ♀️-DMX-socket |

Additionally, you need:

- some wires
- soldering equipment
- 3D-printer
- small screws (see [case](#-case))
- heat shrink tubing
- hot glue gun

______________________________________________________________________

## 🔌 Wiring

> [!IMPORTANT]
> You have to short-circuit `R0` on the RS485 boards to enable the termination resistor required for DMX for the first and last devices in the chain.

![Circuit diagram](/assets/circuit/diagram.svg)

| GPIO | Usage |
| ------- | -------------- |
| GND | GND to others |
| 3,3V | VIN on RS485 |
| 5V/VBUS | VIN on W5500 |
| 0 | Onboard Button |
| 5 | Ext. Button |
| 7 | Ext. LED |
| 15 | Onboard LED |
| 17 | U1TXD |
| 18 | U1RXD |
| 21 | U0TXD |
| 33 | U0RXD |
| 34 | SPI CS |
| 35 | SPI MOS |
| 36 | SPI SCK |
| 37 | SPI MISO |

______________________________________________________________________

## 🚀 Installation (ESP-IDF)

This project uses `ESP-IDF` as the build system.

1. Install `ESP-IDF` (`idf.py`) and `invoke`, or use the provided Nix shell:

   ```bash
   nix develop
   ```

2. Connect your board.

3. Build and flash the firmware:

   ```bash
   inv flash
   ```

4. Open the serial monitor (optional):

   ```bash
   inv monitor --port /dev/ttyUSB0
   ```

5. 🏁 Done 🎉

______________________________________________________________________

## 🧑‍💻 Development

### Required tools

- `ESP-IDF` (includes `idf.py`)

- `invoke` (for project tasks)

- Optional but recommended for development:

  - `pre-commit` (for code quality hooks)
  - `clang-format` (C/C++)
  - `prettier` (JavaScript/CSS/HTML/YAML)
  - `svgo` (SVG optimization)
  - `nixfmt` (Nix formatting)

### Environment setup

This repository includes a `flake.nix` with a ready-to-use development shell.

```bash
nix develop
```

Alternatively, you can use `direnv` to automatically enter the development shell when you `cd` into the project directory.

If you are not using Nix, manually install the packages listed in the `buildInputs` section in `flake.nix`.

This project uses [invoke](https://www.pyinvoke.org/) to simplify running common commands.
Run `inv --list` to see all available tasks.

Examples:

```bash
inv flash
inv reset
inv config
inv docs
inv docs -o
```

### Pre-commit hooks

This project uses [git-hooks.nix](https://github.com/cachix/git-hooks.nix) to run code quality, formatting, and consistency checks.

**Setup:**

```bash
# Enter the development shell and install the Git hooks via shellHook
nix develop

# Run all configured checks
nix flake check

# Or run only the pre-commit-style checks
nix build .#checks.x86_64-linux.pre-commit-check
```

______________________________________________________________________

## 📦 Case

All print files (STL, STEP, X_T) can be found in [assets/case](/assets/case/). Alternatively you can view the project on [OnShape](https://cad.onshape.com/documents/7363818fd18bf0cbf094790e/w/52455282b39e47fbde5d0e53/e/9bec98aa83a813dc9a4d6ab2) where you can export the files in a format you like.

![Prusa Slicer with case loaded](/assets/case/Screenshot.png)

| Part | Screw | Count |
| ----------- | ------- | ----- |
| Case lid | M2x5 | 4x |
| ESP32 | M2x5 | 2x |
| W5500 | M2,5x5 | 2x |
| XLR sockets | M3+Nuts | 4x |

______________________________________________________________________

## 💡 Status LED

| LED | Description |
| --------------------------------- | ------------------------- |
| ![off](/assets/led/off.svg) | no power; LED deactivated |
| ![static](/assets/led/static.svg) | powered on; normal |
| ![slow](/assets/led/slow.svg) | startup; warning |
| ![fast](/assets/led/fast.svg) | resetting; error |

______________________________________________________________________

## ⚙️ Default config

To reset the settings, hold down the button and connect the ESP to the power supply, the LED will flash rapidly. After 3 seconds the LED will turn solid and the settings are reset. If you release the button early, you will abort the reset and the LED flashes slowly.

| Setting | Value |
| -------------- | -------------------------------------- |
| TYPE | WiFi AP |
| SSID | ChaosDMX-□□□□ |
| PASSWORD | mbgmbgmbg <!-- cspell:disable-line --> |
| IP-Address | 192.168.4.1 |
| DMX1 (Left) | OUTPUT; Universe 1 |
| DMX2 (Right) | INPUT; Universe 2 |
| LED Brightness | 10 % |

______________________________________________________________________

## 🤝 Contributing

Contributions, issues, and feature requests are welcome!<br />Feel free to check the [issues page](https://github.com/HendrikRauh/dmx-interface/issues).

Special thanks to all the contributors:

<a href="https://github.com/HendrikRauh/dmx-interface/graphs/contributors">
    <img src="https://contrib.rocks/image?repo=HendrikRauh/dmx-interface" alt="Contributors listed with their avatars" />
</a>
