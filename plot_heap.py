import glob
import re
import sys

import matplotlib.pyplot as plt
import serial


def list_serial_ports():
    # Linux: /dev/ttyUSB*, /dev/ttyACM*
    ports = glob.glob('/dev/ttyUSB*') + glob.glob('/dev/ttyACM*')
    return ports

def select_port():
    ports = list_serial_ports()
    if not ports:
        print("Keine seriellen Ports gefunden.")
        sys.exit(1)
    print("Verfügbare serielle Ports:")
    for i, port in enumerate(ports):
        print(f"  [{i}] {port}")
    while True:
        try:
            idx = int(input(f"Port wählen [0-{len(ports)-1}]: "))
            if 0 <= idx < len(ports):
                return ports[idx]
        except Exception:
            pass
        print("Ungültige Eingabe.")

def main():
    port = select_port()
    try:
        with serial.Serial(port, 9600, timeout=1) as ser:
            heap_values = []
            times = []

            plt.ion()
            fig, ax = plt.subplots()
            line, = ax.plot(times, heap_values)
            ax.set_xlabel('Messung')
            ax.set_ylabel('Free Heap (Bytes)')
            ax.set_title(f'Serial: {port}')

            print("Drücke Strg+C oder schließe das Plot-Fenster zum Beenden.")
            try:
                while True:
                    # Check if the plot window is still open
                    if not plt.fignum_exists(fig.number):
                        print("Plot-Fenster geschlossen. Beende.")
                        break
                    line_raw = ser.readline().decode(errors='ignore')
                    m = re.search(r'\[MEM_DEBUG\] Free heap: (\d+)', line_raw)
                    if m:
                        heap = int(m.group(1))
                        heap_values.append(heap)
                        times.append(len(times))
                        line.set_xdata(times)
                        line.set_ydata(heap_values)
                        ax.relim()
                        ax.autoscale_view()
                        plt.draw()
                        plt.pause(0.01)
                    else:
                        plt.pause(0.01)
            except KeyboardInterrupt:
                print("\nBeendet.")
                plt.close(fig)
            finally:
                plt.ioff()
                plt.show()
    except Exception as e:
        print(f"Fehler beim Öffnen von {port}: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()