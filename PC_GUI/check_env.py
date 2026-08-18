import sys

print("Python:", sys.version)

try:
    import PyQt5
    print("PyQt5: OK")
except Exception as exc:
    print("PyQt5: FAIL", exc)

try:
    import serial
    print(
        "pyserial: OK",
        getattr(serial, "__version__", "unknown")
    )
except Exception as exc:
    print("pyserial: FAIL", exc)
