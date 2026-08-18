# NUCLEO-F411RE 배선도

## 핀 연결표

| 기능 | 부품 단자 | NUCLEO 핀 | 연결 방법 |
|---|---|---|---|
| 빨간 신호 | LED 애노드(+) | PC0 | PC0 → 220~330Ω → LED + |
| 노란 신호 | LED 애노드(+) | PC1 | PC1 → 220~330Ω → LED + |
| 초록 신호 | LED 애노드(+) | PC2 | PC2 → 220~330Ω → LED + |
| LED 공통 | LED 캐소드(-) | GND | 각 LED - → GND |
| 서보 PWM | SG90 Signal | PA0 (A0) | PA0 → 서보 주황/노랑 신호선 |
| 서보 전원 | SG90 VCC | 외부 5V | NUCLEO 3.3V 사용 금지 |
| 서보 접지 | SG90 GND | 공통 GND | 외부 전원 GND와 NUCLEO GND 연결 |
| 초음파 Trigger | HC-SR04 TRIG | PB10 (D6) | D6(PB10) → TRIG 직접 연결 |
| 초음파 Echo | HC-SR04 ECHO | PA8 (D7) | 반드시 1kΩ/2kΩ 분압 후 D7(PA8) |
| 초음파 전원 | HC-SR04 VCC | 외부 5V | 5V 전원 연결 |
| 초음파 접지 | HC-SR04 GND | 공통 GND | 공통 접지 |
| 부저 제어 | 액티브 부저 S/IN | PB12 | HIGH 입력형 모듈의 S/IN 연결 |
| 부저 전원 | 액티브 부저 VCC | 외부 5V | 모듈 정격 확인 |
| 부저 접지 | 액티브 부저 GND | 공통 GND | 공통 접지 |
| PC 통신 | ST-LINK VCP | PA2/PA3 | 보드 내부 연결, 점퍼선 불필요 |
| 상태 표시 | 내장 LD2 | PA5 | 펌웨어 생존 표시, 추가 배선 없음 |

PC0, PC1, PC2, PB12는 보드의 Morpho 헤더 실크 인쇄를 보고 연결하십시오. PA0은 Arduino A0, PB10은 D6, PA8은 D7 위치입니다. NUCLEO-F411RE에서 PB11 헤더는 연결되지 않으므로 사용하지 않습니다.

## 전체 연결 개념도

```text
                         NUCLEO-F411RE
                    ┌────────────────────┐
 PC0 ── 330Ω ──▶ RED LED ───────────────┤
 PC1 ── 330Ω ──▶ YELLOW LED ────────────┤
 PC2 ── 330Ω ──▶ GREEN LED ─────────────┤
                    │                    │
 PA0 ───────────────┼────▶ SG90 SIGNAL   │
 D6/PB10 ───────────┼────▶ HC-SR04 TRIG  │
 D7/PA8 ◀─ 분압 노드┼───── HC-SR04 ECHO  │
 PB12 ──────────────┼────▶ BUZZER IN      │
 GND  ──────────────┴───── 공통 GND       │
                    └────────────────────┘

 외부 5V 2A (+) ─────┬────▶ SG90 VCC
                     ├────▶ HC-SR04 VCC
                     └────▶ BUZZER VCC

 외부 5V GND ────────┬────▶ SG90 GND
                     ├────▶ HC-SR04 GND
                     ├────▶ BUZZER GND
                     └────▶ NUCLEO GND
```

## HC-SR04 ECHO 5V → 3.3V 분압

HC-SR04 ECHO는 약 5V이므로 STM32의 PA8(D7)에 직접 연결하면 안 됩니다.

```text
HC-SR04 ECHO ── 1kΩ ──┬────▶ PA8 (D7)
                      │
                     2kΩ
                      │
                     GND
```

이 구성은 5V ECHO를 약 3.3V로 낮춥니다. 저항 위치를 반대로 연결하지 마십시오.

## 부저 선택

코드는 PB12가 HIGH일 때 울리는 액티브 부저 기준입니다.

- 3핀 HIGH-trigger 액티브 부저 모듈: `S/IN → PB12`, `VCC → 외부 5V`, `GND → 공통 GND`
- 2핀 부저: PB12에 직접 연결하지 말고 NPN 트랜지스터 사용

```text
PB12 ── 1kΩ ──▶ NPN Base
NPN Emitter ──▶ GND
NPN Collector ──▶ Buzzer (-)
Buzzer (+) ──▶ 외부 5V
```

자기식 부저라면 부저 양단에 역기전력 방지 다이오드를 추가합니다. 다이오드 캐소드는 +5V 방향입니다.

## 전원 주의

- 서보를 NUCLEO 3.3V 핀에서 구동하지 마십시오.
- 외부 5V 전원의 GND와 NUCLEO GND는 반드시 공통 연결합니다.
- NUCLEO를 USB로 켠 상태에서 외부 5V의 +선을 NUCLEO 5V 핀에 함께 연결하지 마십시오. 이 프로젝트는 외부 5V의 +를 서보·센서·부저에만 공급합니다.
- 서보 가까이에 470~1000µF 전해콘덴서를 추가하면 순간 전류에 의한 리셋을 줄일 수 있습니다. 극성을 확인하십시오.
