# WIRING - NUCLEO-F411RE

## LED

```text
PC0 ── 330Ω ── RED LED ───── GND
PC1 ── 330Ω ── YELLOW LED ── GND
PC2 ── 330Ω ── GREEN LED ─── GND
```

- LED 긴 다리(Anode): 저항·GPIO 쪽
- LED 짧은 다리(Cathode): GND 쪽

## Motor PWM

PA0은 `TIM2_CH1`, 20 kHz PWM 출력 핀.

권장 연결:

```text
NUCLEO PA0 ── 1kΩ ── MOSFET Gate 또는 모터 드라이버 PWM 입력
NUCLEO GND ────────── 모터 전원 GND와 공통 연결

외부 모터전원 (+) ── Motor ── MOSFET Drain
MOSFET Source ───────────────── GND
Motor 양단 ── 플라이백 다이오드
```

주의:

- 일반 DC 모터를 PA0과 GND 사이에 직접 연결하지 말 것
- GPIO 허용 전류 초과 및 모터 역기전력으로 NUCLEO가 손상될 수 있음
- 모터에는 외부 전원, MOSFET/트랜지스터 또는 모터 드라이버, 플라이백 다이오드 사용
- 외부 모터 전원과 NUCLEO의 GND는 반드시 공통 연결

## On-board status LED

- PA5 = NUCLEO 내장 사용자 LED `LD2`
- 펌웨어가 GPIO 초기화까지 실행되면 0.5초 간격으로 깜빡임
- PA5 LD2는 외부 신호등의 초록불이 아니라 펌웨어 실행 확인용

## UART / Serial

USART2 핀:

- PA2 = USART2_TX
- PA3 = USART2_RX
- 115200 baud, 8N1

NUCLEO-F411RE의 기본 ST-LINK Virtual COM Port를 사용하므로 별도 TX/RX 점퍼선은 필요 없음.

```text
PC ── USB ── ST-LINK ── Virtual COM Port ── USART2
```
