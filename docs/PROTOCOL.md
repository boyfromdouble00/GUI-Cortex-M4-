# UART 및 Local LLM 프로토콜

## UART 설정

- USART2: PA2 TX / PA3 RX
- ST-LINK Virtual COM Port
- 115200 baud, 8 data bits, no parity, 1 stop bit
- 명령 끝: LF 또는 CR/LF

## PC → STM32 명령

| 명령 | 의미 |
|---|---|
| `STATUS?` | 전체 상태 조회 |
| `GATE_OPEN` | 초음파 안전 조건 확인 후 90° 열기·초록불 |
| `GATE_CLOSE` | 0° 닫기 |
| `TRAFFIC` | `GATE_OPEN` 호환 별칭 |
| `AUTO_ON` | 초음파 자동 비상 기능 사용 |
| `AUTO_OFF` | 초음파 자동 비상 기능 중지 |
| `THRESHOLD:30` | 감지 기준을 30cm로 설정(5~200cm) |
| `EMERGENCY_RESET` | 장애물이 제거된 경우 비상 상태 수동 해제 |
| `AUTO_RELEASE_ON` | 장애물 제거 후 자동 열림 사용 |
| `AUTO_RELEASE_OFF` | 장애물 제거 후 자동 열림 중지(기본값) |
| `RELEASE_DELAY_MS:5000` | 자동 열림 대기 5초 설정(1000~60000ms) |

## STM32 → PC 응답

| 응답 | 의미 |
|---|---|
| `BOOT:STM32F411RE_GATE_V9_PA8_FIXED` | 펌웨어 부팅 |
| `STATE:CLOSED` | 빨간불, 차단봉 0° |
| `STATE:OPENING` | 노란불, 90° 이동 중 |
| `STATE:OPEN` | 초록불, 차단봉 90° |
| `STATE:CLOSING` | 노란불, 0° 이동 중 |
| `STATE:EMERGENCY` | 장애물 비상 닫힘 |
| `GATE:0`, `GATE:90` | 서보 목표 각도 |
| `DIST_MM:184` | 거리 184mm(18.4cm) |
| `DIST:TIMEOUT` | HC-SR04 응답 없음 |
| `ALERT:OBSTACLE` | 장애물 사고 발생 |
| `BUZZER:ON`, `BUZZER:OFF` | 부저 상태 |
| `AUTO:ON/OFF` | 초음파 자동 비상 상태 |
| `AUTO_RELEASE:ON/OFF` | 자동 열림 상태 |
| `RELEASE_DELAY_MS:5000` | 자동 열림 대기 시간 |
| `EMERGENCY:CLEAR_TIMER_STARTED` | 장애물 제거 후 대기 시작 |
| `EMERGENCY:AUTO_CLEARED` | 자동 비상 해제 및 열기 시작 |
| `ALIVE` | 5초 주기 펌웨어 생존 신호 |

## 안전 우선순위

1. `GATE_OPEN` 수신 시 센서 응답 유무와 현재 거리를 먼저 검사
2. 센서 응답 없음 또는 기준 거리 이내 장애물이면 열기 거부
3. 열림·열리는 중·닫히는 중에 2회 연속 장애물 감지 시 `EMERGENCY`
4. 빨간불, 서보 0°, 부저 0.5초 1회 실행
5. 같은 비상 이벤트 중 추가 감지는 부저를 다시 울리지 않음
6. 수동 해제 또는 선택적 자동 해제가 완료돼야 다음 이벤트의 부저 1회가 허용됨

## AI 명령 제한

PyQt GUI의 로컬 LLM 응답은 허용 목록에 있는 명령만 UART로 변환됩니다. AI 채팅에서 초록불 전환은 `GATE_OPEN`으로 변환하지만 빨간불·주황불·노란불 변경 요청은 실행하지 않습니다. 직접적인 안전 판단과 최종 제어 권한은 항상 STM32 FSM에 있습니다.
