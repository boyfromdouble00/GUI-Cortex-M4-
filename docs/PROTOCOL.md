# SERIAL PROTOCOL

- Baud rate: 115200
- Data: 8 bit
- Parity: None
- Stop bit: 1
- Line ending: `\n` 또는 `\r\n`

## PC → MCU

| Command | Function |
|---|---|
| `TRAFFIC` | 신호등 FSM 시작 |
| `MOTOR_ON` | 마지막 PWM 설정으로 모터 ON |
| `MOTOR_OFF` | 모터 출력 0% |
| `MOTOR_PWM:0` ~ `MOTOR_PWM:100` | PWM 듀티 설정 |
| `STATUS?` | 현재 상태 조회 |

## MCU → PC

| Response | Meaning |
|---|---|
| `BOOT:STM32F411RE` | MCU 부팅 완료 |
| `ALIVE` | 메인 루프 및 UART TX 실행 중, 5초 주기 |
| `STATE:NORMAL` | 기본 상태 |
| `STATE:RED` | 빨간불 상태 |
| `STATE:YELLOW` | 노란불 상태 |
| `STATE:GREEN` | 초록불 상태 |
| `COUNT:10` ~ `COUNT:1` | 초록불 남은 시간 |
| `MOTOR:ON` | 실제 PWM 출력 중 |
| `MOTOR:OFF` | 실제 PWM 출력 0% |
| `PWM:0` ~ `PWM:100` | 저장된 PWM 설정값 |
| `BUSY:TRAFFIC` | 신호 FSM 실행 중 |
| `ERR:PWM_RANGE` | PWM 범위 또는 형식 오류 |
| `ERR:UNKNOWN_CMD` | 알 수 없는 명령 |

## FSM

```text
NORMAL
  └─ TRAFFIC → RED 1초 → YELLOW 1초 → GREEN 10초 → NORMAL
```

TRAFFIC 실행 중 실제 PWM은 0%. NORMAL 복귀 시 마지막 PWM과 ON/OFF 설정 복원.
