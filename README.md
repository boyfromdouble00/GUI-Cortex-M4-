# GUI_Cotex_Qt_M4

**Local LLM 기반 스마트 신호등·차단봉 시스템 (STM32 NUCLEO-F411RE + PyQt GUI)**

NUCLEO-F411RE(STM32F411RE, Cortex-M4)가 신호등, SG90 서보 차단봉, HC-SR04 초음파 센서, 부저를 실시간으로 제어하고, Windows PC의 PyQt5 GUI와 로컬 LLM(Ollama)이 상태 설명·자연어 명령 처리·최근 사고 요약을 담당하는 프로젝트입니다.

## 목차

- [주요 기능](#주요-기능)
- [시스템 구성](#시스템-구성)
- [필요한 부품](#필요한-부품)
- [프로젝트 구조](#프로젝트-구조)
- [시작하기](#시작하기)
- [AI 채팅 사용 예](#ai-채팅-사용-예)
- [통신 프로토콜](#통신-프로토콜)
- [안전 우선순위](#안전-우선순위)
- [빠른 점검](#빠른-점검)
- [안전 주의](#안전-주의)

## 주요 기능

- **기본 상태**: 전원 인가 시 빨간불 ON, 차단봉 0°(닫힘)
- **차단봉 개방**: 노란불과 함께 90°까지 이동 후 초록불 ON, 10초 유지 후 자동 닫힘
- **장애물 감지**: HC-SR04로 100ms 간격 2회 연속 감지 시 빨간불·차단봉 0°·부저 0.5초 비상 동작 (오검출 억제)
- **비상 우선순위**: STM32 초음파 FSM이 GUI/AI의 초록불 명령보다 항상 우선
- **AI 채팅 제어**: 로컬 LLM이 자연어 명령을 해석하되, 초록불(개방) 전환만 허용하고 빨간불·주황불·노란불 변경은 차단
- **사고 이력**: 장애물 사고 최근 3건을 `PC_GUI/recent_accidents.json`에 자동 기록
- **로컬 LLM 활용**: 현재 상태 설명, 최근 사고 요약, 로그 분석, 감지 거리/자동 복귀 시간 설정을 자연어로 처리

## 시스템 구성

```
[HC-SR04] --초음파--> [NUCLEO-F411RE] --UART(115200)--> [PyQt5 GUI] <--> [Ollama 로컬 LLM]
[LED x3, 부저, SG90 서보] <-- GPIO/PWM --  (STM32F411RE / Cortex-M4)
```

- **STM32_Firmware**: PlatformIO(STM32Cube 프레임워크) 기반 펌웨어, 상태 FSM과 UART 프로토콜 처리
- **PC_GUI**: PyQt5 GUI, 시리얼 통신, 로컬 LLM(Ollama, `qwen2.5:3b`) 연동
- **bat**: 펌웨어 업로드·GUI 실행·UART 테스트·LLM 설치용 Windows 배치 스크립트
- **docs**: 배선도 및 통신 프로토콜 문서

## 필요한 부품

- NUCLEO-F411RE
- SG90 또는 동급 5V 서보모터 1개
- HC-SR04 초음파 센서 1개
- 빨강·노랑·초록 LED 각 1개
- LED용 220~330Ω 저항 3개
- HC-SR04 ECHO 분압용 1kΩ, 2kΩ 저항 각 1개
- HIGH 신호 입력형 액티브 부저 모듈 1개
- 외부 5V 2A 전원, 브레드보드, 점퍼선
- (선택) 서보 전원 안정화용 470~1000µF 전해콘덴서

> 배선은 반드시 전원을 끈 상태에서 진행하세요. 전체 배선도는 [`docs/WIRING.md`](docs/WIRING.md) 참고.

## 프로젝트 구조

```
GUI_Cotex_Qt_M4/
├── STM32_Firmware/        # PlatformIO 펌웨어 (Cortex-M4)
│   ├── platformio.ini
│   └── src/main.cpp
├── PC_GUI/                 # PyQt5 GUI + 로컬 LLM 연동
│   ├── main.py
│   ├── serial_test.py
│   ├── check_env.py
│   ├── requirements.txt
│   └── recent_accidents.json
├── bat/                     # Windows 실행 스크립트
│   ├── 1_UPLOAD_FIRMWARE.bat
│   ├── 2_RUN_GUI.bat
│   ├── 3_TEST_UART.bat
│   ├── 4_SETUP_LOCAL_LLM.bat
│   └── RUN_ALL.bat
├── docs/
│   ├── PROTOCOL.md          # UART / LLM 명령 프로토콜
│   └── WIRING.md             # 배선도
└── README_FIRST.md
```

## 시작하기

### 1. 펌웨어 업로드

NUCLEO의 ST-LINK USB를 PC에 연결한 뒤 프로젝트 최상위 폴더의 `1_UPLOAD_FIRMWARE.bat`을 더블클릭합니다.
`SUCCESS: FIRMWARE UPLOAD COMPLETE`가 출력되면 완료입니다.
이 스크립트는 일반 `python` 대신 `%USERPROFILE%\.platformio\penv\Scripts\platformio.exe`를 우선 사용해 MSYS2 Python 충돌 문제를 피합니다.

### 2. 로컬 LLM 준비 (선택)

1. [Ollama for Windows](https://ollama.com/download/windows) 설치
2. Windows 시작 메뉴에서 Ollama 한 번 실행
3. `4_SETUP_LOCAL_LLM.bat` 더블클릭
4. `qwen2.5:3b` 모델 다운로드 완료 대기 (약 1.9GB)

> LLM 없이도 STM32 제어와 GUI 기본 기능은 정상 동작합니다.

### 3. GUI 실행

`2_RUN_GUI.bat`을 더블클릭한 뒤, GUI에서 `STMicroelectronics STLink Virtual COM Port`를 선택하고 `연결`을 누릅니다.
정상 연결 시 `STATE:CLOSED`, `GATE:0`, 거리 값 등이 수신됩니다.

수동 설치가 필요하면:

```bash
cd PC_GUI
pip install -r requirements.txt
python main.py
```

## AI 채팅 사용 예

```
지금 왜 빨간불이야?
초록색으로 바꿔줘.
감지 거리를 30cm로 바꿔줘.
장애물이 사라진 뒤 5초 후 열어줘.
최근 사고 3건 요약해줘.
최근 로그에 센서 값이 튄 부분이 있는지 분석해줘.
```

`초록색으로 바꿔줘`는 `GATE_OPEN` 요청으로 변환됩니다. STM32는 현재 초음파 센서 값을 먼저 검사하며, 장애물이 기준 거리 안에 있거나 센서 응답이 없으면 열지 않습니다. 열리는 도중에도 장애물이 2회 연속 감지되면 즉시 비상 닫힘이 실행됩니다.

빨간불·주황불·노란불로 바꾸라는 AI 채팅 요청은 실행되지 않습니다. 차단봉을 직접 닫아야 할 때는 GUI의 `차단봉 닫기 (0°)` 버튼을 사용하세요.

## 통신 프로토콜

USART2 (PA2 TX / PA3 RX, ST-LINK Virtual COM Port), 115200 baud, 8N1, 명령 종료 LF 또는 CR/LF.

주요 PC → STM32 명령: `STATUS?`, `GATE_OPEN`, `GATE_CLOSE`, `AUTO_ON`/`AUTO_OFF`, `THRESHOLD:30`, `EMERGENCY_RESET`, `AUTO_RELEASE_ON`/`OFF`, `RELEASE_DELAY_MS:5000`

주요 STM32 → PC 응답: `STATE:*`, `GATE:0/90`, `DIST_MM:*`, `ALERT:OBSTACLE`, `BUZZER:ON/OFF`, `AUTO:*`, `ALIVE`

전체 명령/응답 표는 [`docs/PROTOCOL.md`](docs/PROTOCOL.md) 참고.

## 안전 우선순위

1. `GATE_OPEN` 수신 시 센서 응답 유무와 현재 거리를 먼저 검사
2. 센서 응답 없음 또는 기준 거리 이내 장애물이면 열기 거부
3. 열림·열리는 중·닫히는 중 2회 연속 장애물 감지 시 `EMERGENCY` 진입
4. 빨간불, 서보 0°, 부저 0.5초 1회 실행
5. 같은 비상 이벤트 중 추가 감지는 부저 재발생 없음
6. 수동 해제 또는 자동 해제 완료 후에만 다음 이벤트의 부저가 다시 허용됨

최종 안전 판단과 제어 권한은 항상 STM32 FSM에 있으며, PC/GUI/LLM은 이를 우회할 수 없습니다.

## 빠른 점검

| 증상 | 확인 사항 |
|---|---|
| LD2가 0.5초마다 깜빡이지 않음 | 펌웨어 메인 루프 정상 여부 |
| 외부 빨간 LED 계속 켜짐 | 기본 닫힘 상태이므로 정상 |
| GUI 거리 "센서 응답 없음" | ECHO가 D7(PA8) 분압 노드에 연결됐는지, 공통 GND 확인 |
| GUI 보드 응답 없음 | COM 포트, 115200 baud, 최신 펌웨어 확인 |
| Ollama 연결 실패 | Windows 트레이에서 Ollama 실행 여부, 모델명 `qwen2.5:3b` 확인 |
| 서보 동작 시 보드 리셋 | 서보 외부 5V 전원 용량과 공통 GND 확인 |

## 안전 주의

이 프로젝트는 **교육·모형용**입니다. 요구사항에 따라 장애물 감지 시 차단봉이 닫히도록 구현되어 있으므로, 실제 사람이나 차량이 통과하는 장치에 그대로 사용하면 위험합니다. 실장 장비에서는 차단봉 아래 별도 안전 센서, 기계식 리미트, 비상 정지 및 인증된 제어기를 추가해야 합니다.

## 라이선스

별도 명시된 라이선스가 없습니다. 사용 전 저장소 소유자에게 문의하세요.
