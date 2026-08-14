import sys
import time

from PyQt5.QtCore import Qt, QThread, QTimer, pyqtSignal
from PyQt5.QtWidgets import (
    QApplication,
    QMainWindow,
    QWidget,
    QLabel,
    QPushButton,
    QComboBox,
    QSlider,
    QSpinBox,
    QPlainTextEdit,
    QHBoxLayout,
    QVBoxLayout,
    QGridLayout,
    QGroupBox,
    QMessageBox,
)

import serial
import serial.tools.list_ports


BAUDRATE = 115200


class SerialReader(QThread):
    line_received = pyqtSignal(str)
    error = pyqtSignal(str)

    def __init__(self, ser):
        super().__init__()
        self.ser = ser
        self.running = True

    def run(self):
        buffer = bytearray()

        while self.running:
            try:
                if self.ser and self.ser.is_open and self.ser.in_waiting:
                    data = self.ser.read(self.ser.in_waiting)

                    for byte in data:
                        if byte == 10:  # LF
                            line = buffer.decode(
                                "utf-8",
                                errors="replace"
                            ).strip()

                            buffer.clear()

                            if line:
                                self.line_received.emit(line)

                        elif byte != 13:  # ignore CR
                            buffer.append(byte)

                else:
                    self.msleep(20)

            except Exception as exc:
                self.error.emit(str(exc))
                break

    def stop(self):
        self.running = False
        self.wait(1000)


class Lamp(QLabel):
    def __init__(self, name, on_color):
        super().__init__(name)

        self.on_color = on_color
        self.setAlignment(Qt.AlignCenter)
        self.setFixedSize(115, 115)

        self.set_on(False)

    def set_on(self, on):
        # Qt stylesheet warning 방지를 위해 border-radius 제거.
        # 원형 대신 둥근 사각형 상태 표시로 사용.
        if on:
            style = (
                "QLabel {"
                f"background-color: {self.on_color};"
                "border: 4px solid #202020;"
                "color: #111111;"
                "font-weight: bold;"
                "font-size: 15px;"
                "padding: 8px;"
                "}"
            )
        else:
            style = (
                "QLabel {"
                "background-color: #333333;"
                "border: 4px solid #222222;"
                "color: #aaaaaa;"
                "font-weight: bold;"
                "font-size: 15px;"
                "padding: 8px;"
                "}"
            )

        self.setStyleSheet(style)


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()

        self.setWindowTitle(
            "NUCLEO-F411RE Traffic + Motor Controller"
        )

        self.resize(850, 700)

        self.ser = None
        self.reader = None
        self.awaiting_status = False

        # ---------------- Serial UI ----------------
        self.port_combo = QComboBox()
        self.refresh_btn = QPushButton("포트 새로고침")
        self.connect_btn = QPushButton("연결")

        # ---------------- Lamps ----------------
        self.red = Lamp("RED", "#ff3b30")
        self.yellow = Lamp("YELLOW", "#ffd60a")
        self.green = Lamp("GREEN", "#34c759")

        self.state_label = QLabel("상태: 연결 안 됨")

        self.count_label = QLabel(
            "GREEN 남은 시간: --"
        )

        self.count_label.setStyleSheet(
            "font-size: 22px; font-weight: bold;"
        )

        self.traffic_btn = QPushButton(
            "신호 변경 시작"
        )

        self.traffic_btn.setMinimumHeight(55)

        # ---------------- Motor ----------------
        self.motor_state = QLabel("Motor: --")
        self.motor_state.setStyleSheet(
            "font-size: 20px; font-weight: bold;"
        )

        self.motor_on_btn = QPushButton(
            "MOTOR ON"
        )

        self.motor_off_btn = QPushButton(
            "MOTOR OFF"
        )

        self.pwm_slider = QSlider(Qt.Horizontal)
        self.pwm_slider.setRange(0, 100)
        self.pwm_slider.setValue(100)
        self.pwm_slider.setTickInterval(10)
        self.pwm_slider.setTickPosition(QSlider.TicksBelow)

        self.pwm_spin = QSpinBox()
        self.pwm_spin.setRange(0, 100)
        self.pwm_spin.setValue(100)
        self.pwm_spin.setSuffix(" %")

        self.pwm_apply_btn = QPushButton(
            "PWM 적용"
        )

        self.status_btn = QPushButton(
            "상태 조회"
        )

        # ---------------- Log ----------------
        self.log = QPlainTextEdit()
        self.log.setReadOnly(True)

        # ---------------- Signals ----------------
        self.refresh_btn.clicked.connect(
            self.refresh_ports
        )

        self.connect_btn.clicked.connect(
            self.toggle_connection
        )

        self.traffic_btn.clicked.connect(
            self.start_traffic
        )

        self.motor_on_btn.clicked.connect(
            lambda: self.send_cmd("MOTOR_ON")
        )

        self.motor_off_btn.clicked.connect(
            lambda: self.send_cmd("MOTOR_OFF")
        )

        self.pwm_slider.valueChanged.connect(
            self.pwm_spin.setValue
        )

        self.pwm_spin.valueChanged.connect(
            self.pwm_slider.setValue
        )

        self.pwm_slider.sliderReleased.connect(
            self.apply_pwm
        )

        self.pwm_apply_btn.clicked.connect(
            self.apply_pwm
        )

        self.status_btn.clicked.connect(
            self.request_status
        )

        # ---------------- Layout ----------------
        serial_box = QGroupBox("Serial")
        serial_layout = QHBoxLayout(serial_box)

        serial_layout.addWidget(
            QLabel("COM Port")
        )

        serial_layout.addWidget(
            self.port_combo,
            1
        )

        serial_layout.addWidget(
            self.refresh_btn
        )

        serial_layout.addWidget(
            self.connect_btn
        )

        lamp_box = QGroupBox(
            "Traffic Signal"
        )

        lamp_layout = QVBoxLayout(
            lamp_box
        )

        lamps_layout = QHBoxLayout()
        lamps_layout.addStretch()
        lamps_layout.addWidget(self.red)
        lamps_layout.addWidget(self.yellow)
        lamps_layout.addWidget(self.green)
        lamps_layout.addStretch()

        lamp_layout.addLayout(
            lamps_layout
        )

        lamp_layout.addWidget(
            self.state_label
        )

        lamp_layout.addWidget(
            self.count_label
        )

        lamp_layout.addWidget(
            self.traffic_btn
        )

        motor_box = QGroupBox(
            "Motor PWM Control"
        )

        motor_layout = QGridLayout(
            motor_box
        )

        motor_layout.addWidget(
            self.motor_state,
            0,
            0,
            1,
            2
        )

        motor_layout.addWidget(
            self.motor_on_btn,
            1,
            0
        )

        motor_layout.addWidget(
            self.motor_off_btn,
            1,
            1
        )

        motor_layout.addWidget(
            QLabel("PWM 듀티 (0~100%)"),
            2,
            0,
            1,
            2
        )

        motor_layout.addWidget(
            self.pwm_slider,
            3,
            0
        )

        motor_layout.addWidget(
            self.pwm_spin,
            3,
            1
        )

        motor_layout.addWidget(
            self.pwm_apply_btn,
            4,
            0,
            1,
            2
        )

        motor_layout.addWidget(
            self.status_btn,
            5,
            0,
            1,
            2
        )

        central = QWidget()

        layout = QVBoxLayout(
            central
        )

        layout.addWidget(serial_box)
        layout.addWidget(lamp_box)
        layout.addWidget(motor_box)
        layout.addWidget(
            QLabel("통신 로그")
        )
        layout.addWidget(
            self.log,
            1
        )

        self.setCentralWidget(
            central
        )

        self.refresh_ports()
        self.set_lamps("NORMAL")

    def add_log(self, text):
        stamp = time.strftime(
            "%H:%M:%S"
        )

        self.log.appendPlainText(
            f"[{stamp}] {text}"
        )

    def refresh_ports(self):
        self.port_combo.clear()

        ports = sorted(
            serial.tools.list_ports.comports(),
            key=lambda port: port.device
        )

        preferred_index = -1

        for index, port in enumerate(ports):
            description = (
                port.description or ""
            )

            self.port_combo.addItem(
                f"{port.device} | {description}",
                port.device
            )

            identity = (
                description + " " + (port.hwid or "")
            ).lower()

            if preferred_index < 0 and (
                "stlink" in identity
                or "st-link" in identity
                or "stmicroelectronics" in identity
            ):
                preferred_index = index

        if preferred_index >= 0:
            self.port_combo.setCurrentIndex(
                preferred_index
            )

        self.add_log(
            f"COM 포트 {len(ports)}개 검색"
        )

    def toggle_connection(self):
        if self.ser and self.ser.is_open:
            self.disconnect_serial()
            return

        if self.port_combo.count() == 0:
            QMessageBox.warning(
                self,
                "COM 포트 없음",
                "보드 USB 연결 후 포트 새로고침을 누르세요."
            )

            return

        port = self.port_combo.currentData()

        try:
            self.ser = serial.Serial(
                port=port,
                baudrate=BAUDRATE,
                timeout=0.05,
                write_timeout=1.0,
                rtscts=False,
                dsrdtr=False
            )
            self.ser.dtr = True
            self.ser.rts = True
            self.ser.reset_input_buffer()

            self.reader = SerialReader(
                self.ser
            )

            self.reader.line_received.connect(
                self.handle_line
            )

            self.reader.error.connect(
                self.serial_error
            )

            self.reader.start()

            self.connect_btn.setText(
                "연결 해제"
            )

            self.state_label.setText(
                f"상태: {port} 연결됨"
            )

            self.add_log(
                f"CONNECTED {port} @ {BAUDRATE}"
            )

            QTimer.singleShot(
                150,
                self.request_status
            )

        except Exception as exc:
            self.ser = None

            QMessageBox.critical(
                self,
                "Serial 연결 실패",
                str(exc)
            )

    def disconnect_serial(self):
        if self.reader:
            self.reader.stop()
            self.reader = None

        if self.ser:
            try:
                self.ser.close()
            except Exception:
                pass

        self.ser = None
        self.awaiting_status = False

        self.connect_btn.setText(
            "연결"
        )

        self.state_label.setText(
            "상태: 연결 안 됨"
        )

        self.motor_state.setText(
            "Motor: --"
        )

        self.add_log(
            "DISCONNECTED"
        )

    def serial_error(self, message):
        self.add_log(
            "SERIAL ERROR: " + message
        )

        self.disconnect_serial()

    def send_cmd(self, cmd):
        if not self.ser or not self.ser.is_open:
            QMessageBox.warning(
                self,
                "Serial",
                "먼저 COM 포트를 연결하세요."
            )

            return False

        try:
            self.ser.write(
                (cmd + "\n").encode("ascii")
            )
            self.ser.flush()

            self.add_log(
                "TX > " + cmd
            )

            return True

        except Exception as exc:
            self.serial_error(
                str(exc)
            )

            return False

    def handle_line(self, line):
        self.awaiting_status = False

        self.add_log(
            "RX < " + line
        )

        if line.startswith("STATE:"):
            state = (
                line
                .split(":", 1)[1]
                .strip()
                .upper()
            )

            self.set_lamps(
                state
            )

            self.state_label.setText(
                "상태: " + state
            )

            if state == "NORMAL":
                self.traffic_btn.setEnabled(
                    True
                )

        elif line.startswith("COUNT:"):
            try:
                seconds = int(
                    line.split(":", 1)[1]
                )

                self.count_label.setText(
                    f"GREEN 남은 시간: {seconds}초"
                )

            except ValueError:
                pass

        elif line.startswith("MOTOR:"):
            value = (
                line
                .split(":", 1)[1]
                .strip()
                .upper()
            )

            self.motor_state.setText(
                "Motor: " + value
            )

        elif line.startswith("PWM:"):
            try:
                duty = int(
                    line.split(":", 1)[1]
                )

                if 0 <= duty <= 100:
                    self.pwm_slider.setValue(duty)
                    self.pwm_spin.setValue(duty)

            except ValueError:
                pass

    def set_lamps(self, state):
        state = state.upper()

        self.red.set_on(
            state in ("RED", "NORMAL")
        )

        self.yellow.set_on(
            state == "YELLOW"
        )

        self.green.set_on(
            state == "GREEN"
        )

        if state != "GREEN":
            self.count_label.setText(
                "GREEN 남은 시간: --"
            )

    def start_traffic(self):
        if self.send_cmd("TRAFFIC"):
            self.traffic_btn.setEnabled(
                False
            )

    def apply_pwm(self):
        duty = self.pwm_spin.value()
        self.send_cmd(f"MOTOR_PWM:{duty}")

    def request_status(self):
        if self.ser and self.ser.is_open:
            self.awaiting_status = self.send_cmd("STATUS?")

            if self.awaiting_status:
                QTimer.singleShot(
                    1500,
                    self.check_status_response
                )

    def check_status_response(self):
        if (
            self.awaiting_status
            and self.ser
            and self.ser.is_open
        ):
            self.state_label.setText(
                "상태: 보드 응답 없음"
            )
            self.add_log(
                "응답 없음: 펌웨어 업로드, 115200 baud, "
                "STLink Virtual COM Port를 확인하세요."
            )

    def closeEvent(self, event):
        if self.ser or self.reader:
            self.disconnect_serial()

        event.accept()


if __name__ == "__main__":
    app = QApplication(sys.argv)

    window = MainWindow()
    window.show()

    sys.exit(app.exec_())
