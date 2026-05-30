# ChaosDMX

> [!WARNING]
> This project is currently in a rewrite phase, we are currently switching the Framework from [Arduino](https://www.arduino.cc/) to [ESP-IDF](https://developer.espressif.com/tags/esp-idf/) and are reworking the codebase.
> The current state is not stable, but you can check out the [legacy/arduino](https://github.com/HendrikRauh/dmx-interface/tree/legacy/arduino) branch.
> Feel free to help us by [contributing](#-contributing) to the project!

ChaosDMX is an open-source, multi-protocol DIY [DMX](https://en.wikipedia.org/wiki/DMX512) interface that acts as a versatile bridge between lighting
control software (e.g., [QLC+](https://www.qlcplus.org/)) and physical stage equipment like fixtures, spotlights, moving heads, and fog machines.
It features two fully configurable ports that can be individually set up as either DMX-Input or DMX-Output.

The interface supports [Art-Net](https://art-net.org.uk/) and [sACN / E1.31](https://entertainment.sundrax.com/blog/ultimate-guide-sacn-control-lighting-over-network). It is able to operate over an Ethernet connection or over Wi-Fi.

______________________________________________________________________

## 🛒 Parts

| Count | Part |
| --- | --- |
| 1x | ESP32 (Lolin S2 Mini) |
| 2x | RS485 |
| 1x | W5500-ETH |
| 1x | LED-Button |
| 1x | ♂️-XLR-socket |
| 1x | ♀️-XLR-socket |

Additionally, you need:

- some wires
- soldering equipment
- 3D-printer
- some screws (see [case](#-case))
- heat shrink tubing

______________________________________________________________________

## 🔌 Wiring

> [!IMPORTANT]
>  You need to enable the termination resistor required for DMX for the first and last devices in the chain on your board. On the board we used (RS485 boards) we had to short-circuit `R0` to do so.
> 
Have a look at the following diagram for how to wire the components together, the table below shows the pinout of the ESP32 and how to connect them to it.

<img src="assets/circuit/diagram.svg" onerror="this.onerror=null; this.src='diagram.svg';" alt="Circuit diagram">

| GPIO | Usage |
| --- | --- |
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

## 🚀 Installation

1. Install [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html) (`idf.py`) on your system
2. Connect the ESP32 to your computer via USB
3. Flash the firmware by running `idf.py flash` or if you have [invoke](https://www.pyinvoke.org/) installed: `inv flash` in the project folder
4. (optional) Monitor the serial output using `idf.py monitor` or `inv monitor`

> [!TIP]
> If the ESP32 does not show up as a serial device, you might need to place it in bootloader mode.

______________________________________________________________________

## 📦 Case

All print files can be found in the folder `assets/case`.
Alternatively, you can view the current case on [OnShape](https://cad.onshape.com/documents/7363818fd18bf0cbf094790e/w/52455282b39e47fbde5d0e53/e/9bec98aa83a813dc9a4d6ab2) where you can export the files in a format of your choice.
Feel free to design your own case (and maybe address the [issues with the current one](https://github.com/HendrikRauh/dmx-interface/issues/92)) and share it with us!

![Prusa Slicer with case loaded](/assets/case/Screenshot.png)

In addition to the print you will need:

| Count | Part | Location |
| --- | --- | --- |
| 6x | M2x5 screw | Case lid, ESP32 |
| 2x | M2,5x5 screw | W5500 |
| 4x | M3 screw | XLR sockets |
| 4x | M3 nut | XLR sockets |

______________________________________________________________________

## 💡 Status LED

We have a status LED-Button that shows the current state of the device.

| LED | Description |
| --- | --- |
| ![off](/assets/led/off.svg) | no power; LED deactivated |
| ![static](/assets/led/static.svg) | powered on; normal |
| ![slow](/assets/led/slow.svg) | startup; warning |
| ![fast](/assets/led/fast.svg) | resetting; error |

______________________________________________________________________

## ⚙️ Config

You can configure the device by connecting to the WiFi network and accessing the web interface via the default (listed below) or configured IP address.

### Default config

To reset the settings, hold down the button and connect the ESP to the power supply, the LED will flash rapidly. After 3 seconds the LED will turn solid and the settings are reset. If you release the button early, you will abort the reset and the LED will flash slowly.

| Setting | Value |
| --- | --- |
| Type | WiFi AP |
| SSID | ChaosDMX-□□□□ |
| Password | ChaosDMX |
| IP-Address | 192.168.4.1 |
| DMX0 | OUTPUT; Universe 1 |
| DMX1 | INPUT; Universe 2 |
| LED Brightness | 10 % |

______________________________________________________________________

## 🧑‍💻 Development

We provide a ready-to-use development environment using Nix, but you can also set up the environment manually if you prefer to do so. If you do not want to use Nix make sure to install the required tools listed in the `buildInputs` section in `flake.nix`.
If you are on Windows we recommend using [WSL](https://learn.microsoft.com/en-us/windows/wsl/) and following the instructions below inside the WSL session.

> [!WARNING]
> Under WSL, further steps are required, needs research! [USB usage](https://learn.microsoft.com/en-us/windows/wsl/connect-usb) etc (usbipd-win)

### Setup NIX

> [!WARNING]
> There is a nix installer that enables flakes by default, which we recommend

For usage of the development environment, having the nix package manager installed on your system is required. In addition, you need to enable flakes support by adding the following lines to your `~/.config/nix/nix.conf`:

```conf
experimental-features = nix-command flakes
```

You can do this by running the following command that installs nix and enables flakes support:

```bash
curl -L https://nixos.org/nix/install | sh
```

### Setup direnv (optional)

Optionally, you are able to use [direnv](https://direnv.net/) to automatically enter the development environment whenever you navigate to the project folder. Further installation instructions can be found on [their website](https://direnv.net/docs/installation.html).

When using it for the first time, you need to allow the `.envrc` file by running `direnv allow` in the project folder.
Afterwards, you can simply navigate to the project folder and you will automatically enter the development environment.

### Commands

This project uses [invoke](https://www.pyinvoke.org/) to simplify running common commands.
Run `inv --list` to see all available tasks or have a look at the `tasks.py` file.

Here is a small selection of the most common tasks:

| Command | Description |
| --- | --- |
| `inv build` | Build the firmware using ESP-IDF |
| `inv flash` | Flash the firmware to the ESP32 |
| `inv monitor` | Monitor the serial output |
| `inv docs -o` | Generate the documentation using Doxygen and open it in your browser |

### Pre-commit hooks

This project uses [git-hooks.nix](https://github.com/cachix/git-hooks.nix) to run code quality, formatting, and consistency checks. When using the dev-shell, these run before your commit and format the code etc.
If you are not using the dev-shell, the action runner will do the check on the repository again and check it for you.

### Documentation

Further documentation including data structures and code can be found on [hendrikrauh.github.io/dmx-interface](https://hendrikrauh.github.io/dmx-interface/).
[Doxygen](https://www.doxygen.nl/) is used to generate the documentation from the source code, you can also generate it locally by running `inv docs` or `inv docs -o` to open it in your browser after generation.
Functions, variables, and data structures should be documented using Doxygen comments, look at the [Doxygen manual](https://www.doxygen.nl/manual/docblocks.html) for more information on how to write these comments.

The documentation for your branch will be automatically generated and published under `https://hendrikrauh.github.io/dmx-interface/branch/<your-branch-name>` when you push your changes.

______________________________________________________________________

## 🤝 Contributing

Contributions, issues, and feature requests are welcome!

Feel free to check the [issues page](https://github.com/HendrikRauh/dmx-interface/issues) and the [pull requests](https://github.com/HendrikRauh/dmx-interface/pulls) on GitHub.
Thanks to all contributors who have helped make this project better!

<a href="https://github.com/HendrikRauh/dmx-interface/graphs/contributors">
    <img src="https://contrib.rocks/image?repo=HendrikRauh/dmx-interface" alt="Contributors listed with their avatars" />
</a>
