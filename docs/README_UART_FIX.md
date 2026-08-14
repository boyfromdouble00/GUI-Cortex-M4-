# UART v2 수정 이력

최종 실행 방법은 루트의 `README_FIRST.md` 참고.

증상:
- Reset 후 `STATE:NORMA`까지만 수신
- 기존 UART interrupt/HAL 송수신 구조에서 응답 불완전

v2 변경:
- USART2 RX interrupt 제거
- USART2 RX polling 사용
- TX는 짧은 ASCII 메시지에 대해 USART2 TXE/TC flag 기반 직접 전송
- Serial protocol과 PyQt GUI 명령은 기존과 동일

## 다시 올리기

PowerShell:

```powershell
cd .\STM32_Firmware
py -3.11 -m platformio run
py -3.11 -m platformio run -t upload
```

Upload SUCCESS 후, 모터는 우선 분리한 상태에서 테스트:

```powershell
python -c "import serial,time; s=serial.Serial('COM5',115200,timeout=3); time.sleep(.2); s.write(b'STATUS?\n'); time.sleep(.2); print(repr(s.read(200))); s.close()"
```

정상 예:

```text
b'STATE:NORMAL\r\nMOTOR:ON\r\nPWM:100\r\n'
```

정상 확인 후 PC_GUI/main.py 실행.

주의:
모터 연결 시 보드가 반복 reset되면 소프트웨어 문제가 아님.
그 경우 GPIO 직결 부하가 보드에 영향을 주는 것이므로 모터 구동 회로를 분리해야 함.
