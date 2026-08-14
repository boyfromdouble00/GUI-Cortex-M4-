import sys
import time

import serial
import serial.tools.list_ports


def choose_port():
    if len(sys.argv) >= 2:
        return sys.argv[1]

    ports = list(serial.tools.list_ports.comports())

    for port in ports:
        identity = f"{port.description} {port.hwid}".lower()
        if "stlink" in identity or "st-link" in identity:
            return port.device

    if len(ports) == 1:
        return ports[0].device

    return None


def main():
    port = choose_port()

    if not port:
        print("ERROR: ST-LINK Virtual COM Port를 찾지 못했습니다.")
        return 1

    print(f"PORT: {port}")
    print("OPEN: 115200 8N1")

    try:
        with serial.Serial(
            port=port,
            baudrate=115200,
            timeout=0.1,
            write_timeout=1.0,
            rtscts=False,
            dsrdtr=False,
        ) as ser:
            ser.dtr = True
            ser.rts = True
            print(f"DTR: {ser.dtr}, RTS: {ser.rts}")
            time.sleep(0.4)
            ser.reset_input_buffer()
            ser.write(b"STATUS?\n")
            ser.flush()
            print("TX: STATUS?")

            deadline = time.monotonic() + 3.0
            received = bytearray()

            while time.monotonic() < deadline:
                waiting = ser.in_waiting
                if waiting:
                    received.extend(ser.read(waiting))
                time.sleep(0.02)

            if received:
                print("RX:", repr(bytes(received)))
                print("RESULT: UART OK")
                return 0

            print("RX: b''")
            print("RESULT: UART NO RESPONSE")
            print("PA2/PA3와 ST-LINK VCP 연결 또는 펌웨어 UART를 확인하세요.")
            return 2

    except Exception as exc:
        print("ERROR:", exc)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
