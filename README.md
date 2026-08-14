# TrafficMotor FINAL v7 SYSTICK FIX - NUCLEO-F411RE

https://loving-spruce-d3c.notion.site/3bc930b4d58d806a9041daa730388d5a?pvs=73

NUCLEO-F411RE와 PyQt5 GUI를 USB Virtual COM Port로 연결하는 신호등·모터 PWM 프로젝트.

## 주요 기능

- 빨간색 1초 → 노란색 1초 → 초록색 10초 → 기본 상태 복귀
- 신호 동작 중 모터 자동 정지, 종료 후 이전 PWM 설정 복원
- GUI에서 모터 ON/OFF 및 PWM 듀티 0~100% 조절
- PA0의 TIM2_CH1에서 20 kHz PWM 출력
- USART2 115200 bps 명령·상태 통신

## 가장 간단한 실행 순서

1. 압축 해제
2. NUCLEO-F411RE의 ST-LINK USB 포트를 PC에 연결
3. 모터를 분리한 상태에서 LED부터 배선 및 테스트
4. 프로젝트 최상위의 `1_UPLOAD_FIRMWARE.bat` 더블클릭
5. `SUCCESS` 확인 후 `2_RUN_GUI.bat` 더블클릭
6. GUI에서 `포트 새로고침` → `STLink Virtual COM Port (COMx)` 선택 → `연결`

처음부터 업로드와 GUI 실행을 연속으로 하려면 `RUN_ALL.bat`을 더블클릭.

처음 실행할 때 PlatformIO, PyQt5, pyserial이 없으면 배치파일이 자동으로 설치함. Python은 3.11 또는 3.12 권장.

이 PC에서는 `python` 명령이 `C:\msys64\ucrt64\bin\python.exe`를 가리키고 `py` 명령도 없으므로 두 명령을 사용하지 않음. 펌웨어는 `%USERPROFILE%\.platformio\penv\Scripts\platformio.exe`, GUI는 `%USERPROFILE%\.platformio\penv\Scripts\python.exe`를 직접 사용함.

## GUI 사용법

- `MOTOR ON`: 마지막 PWM 값으로 모터 출력 시작
- `MOTOR OFF`: PWM 출력 0%로 정지
- PWM 슬라이더 또는 숫자 입력: 0~100% 선택
- `PWM 적용`: 선택한 PWM을 보드에 전송
- 슬라이더는 손을 놓는 순간에도 자동 적용
- `신호 변경 시작`: RED 1초 → YELLOW 1초 → GREEN 10초 실행
- `상태 조회`: 현재 신호, 모터 상태, 설정 PWM 재조회

## 수동 실행 방법

PowerShell에서 압축을 푼 프로젝트 폴더로 이동한 뒤 실행.

### 1. 펌웨어 빌드·업로드

```powershell
cd .\STM32_Firmware
"%USERPROFILE%\.platformio\penv\Scripts\platformio.exe" run
"%USERPROFILE%\.platformio\penv\Scripts\platformio.exe" run -t upload
```

PlatformIO 실행 파일의 실제 경로를 사용하므로 `pio`, `python`, `py`의 PATH 설정과 무관하게 실행 가능.

정상 완료 문구:

```text
SUCCESS
```

### 2. GUI 실행

```powershell
cd ..\PC_GUI
"%USERPROFILE%\.platformio\penv\Scripts\python.exe" -m pip install -r requirements.txt
"%USERPROFILE%\.platformio\penv\Scripts\python.exe" main.py
```

GUI도 PlatformIO에 포함된 Windows Python을 사용하므로 별도의 `py` 명령이 필요 없음.

## 배선 요약

```text
PC0 ── 330Ω ── RED LED ───── GND
PC1 ── 330Ω ── YELLOW LED ── GND
PC2 ── 330Ω ── GREEN LED ─── GND

PA0 (TIM2_CH1 PWM) ── 모터 드라이버/트랜지스터 입력
GND ────────────────── 모터 전원 GND와 공통 연결
```

모터를 PA0에 직접 연결하지 말 것. 트랜지스터 또는 모터 드라이버와 플라이백 다이오드 사용. 자세한 내용은 `docs/WIRING.md` 참고.

USART2는 NUCLEO의 ST-LINK Virtual COM Port에 기본 연결되므로 PA2/PA3 점퍼선은 필요 없음.

## 정상 동작 확인

펌웨어 업로드 직후:

```text
외부 PC0 RED ON
YELLOW OFF
GREEN OFF
보드 내장 PA5 LD2 0.5초 간격 점멸 (메인 루프 실행 표시)
PWM 100%
```

외부 빨간 LED와 보드 내장 LD2는 서로 다른 LED임. 외부 PC0 LED가 꺼져 있고 PA5 LD2만 깜빡이면 펌웨어는 실행 중이며 외부 LED 배선·극성을 확인해야 함.

GUI에서 `연결`을 누른 뒤 로그에 다음과 비슷하게 표시되면 정상:

```text
TX > STATUS?
RX < STATE:NORMAL
RX < MOTOR:ON
RX < PWM:100
```

## UART 단독 진단

GUI를 완전히 닫고 프로젝트 최상위의 `3_TEST_UART.bat`을 실행. COM5에 `STATUS?`를 직접 전송하여 GUI와 무관하게 UART 상태 확인.

정상 예:

```text
TX: STATUS?
RX: b'STATE:NORMAL\r\nMOTOR:ON\r\nPWM:100\r\n'
RESULT: UART OK
```

v7 핵심 수정:

- `SysTick_Handler()` 추가 및 `HAL_IncTick()` 호출
- STM32가 부팅 1ms 후 `Default_Handler`에서 정지하던 문제 해결
- `HAL_GetTick()` 기반 신호등 FSM, 카운트다운, LD2 점멸 정상화

UART 관련 수정:

- USART2 송수신을 레지스터 직접 처리에서 `HAL_UART_Transmit`, `HAL_UART_Receive` 방식으로 교체
- GUI의 DTR/RTS 강제 OFF 설정 제거 후 ON으로 설정
- PA5 LD2 0.5초 주기 점멸 추가
- UART TX 생존 확인용 `ALIVE` 메시지 5초 주기 전송

## 문제 해결

### `pio`가 명령으로 인식되지 않음

`pio run` 대신 다음 명령 사용:

```powershell
"%USERPROFILE%\.platformio\penv\Scripts\platformio.exe" run
"%USERPROFILE%\.platformio\penv\Scripts\platformio.exe" run -t upload
```

또는 `1_UPLOAD_FIRMWARE.bat` 더블클릭.

### GUI에서 COM 포트가 보이지 않음

- ST-LINK 쪽 USB 포트에 연결했는지 확인
- 충전 전용이 아닌 데이터 USB 케이블 사용
- Windows 장치 관리자 → `포트(COM & LPT)` 확인
- ST-LINK 드라이버 설치 후 GUI의 `포트 새로고침` 클릭

### 연결은 되지만 `STATUS?` 응답이 없음

- 먼저 GUI와 시리얼 모니터를 모두 종료
- `1_UPLOAD_FIRMWARE.bat`으로 펌웨어 다시 업로드
- 업로드가 끝난 뒤 보드 RESET 버튼 1회 누르기
- GUI에서 STLink Virtual COM Port를 다시 연결
- Baud rate는 115200 고정

### 업로드 실패

- GUI, PuTTY, Arduino Serial Monitor 등 COM 포트를 사용하는 프로그램 종료
- ST-LINK USB 재연결
- NUCLEO 보드 모델이 F411RE인지 확인
- VS Code로 실행할 때는 `STM32_Firmware` 폴더를 PlatformIO 프로젝트로 열기

## 폴더 구조

```text
TrafficMotor_FINAL_v7_SYSTICK_FIXED_NUCLEO_F411RE
├─ 1_UPLOAD_FIRMWARE.bat
├─ 2_RUN_GUI.bat
├─ 3_TEST_UART.bat
├─ RUN_ALL.bat
├─ STM32_Firmware
│  ├─ platformio.ini
│  └─ src\main.cpp
├─ PC_GUI
│  ├─ main.py
│  ├─ requirements.txt
│  └─ run_gui.bat
└─ docs
   ├─ WIRING.md
   └─ PROTOCOL.md
```
