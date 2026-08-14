# v4 PWM + UART FIX

핵심 수정:
- main loop의 `HAL_Delay(1)` 제거
- USART2 RX polling을 계속 수행
- ORE / NE / FE / PE 오류 처리 추가
- Baud rate 및 GUI protocol은 기존과 동일: 115200 8N1
- PA0 TIM2_CH1 20 kHz PWM 추가
- GUI PWM 0~100% 조절 추가

## 업로드
`STM32_Firmware` 폴더에서:

```powershell
py -3.11 -m platformio run
py -3.11 -m platformio run -t upload
```

## 1차 테스트
모터는 우선 PA0에서 분리한 상태 권장.

```powershell
python -c "import serial,time; s=serial.Serial('COM5',115200,timeout=2); s.write(b'STATUS?\n'); time.sleep(.2); print(repr(s.read(200))); s.close()"
```

정상 예:
`b'STATE:NORMAL\r\nMOTOR:ON\r\nPWM:100\r\n'`

정상 확인 후 `PC_GUI/main.py` 실행.
