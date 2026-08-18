#include "stm32f4xx_hal.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

// PlatformIO STM32Cube projects do not generate stm32f4xx_it.c.
// This handler is required for HAL_GetTick(), HAL_Delay(), FSM timing,
// ultrasonic scheduling and one-shot buzzer timing.
extern "C" void SysTick_Handler(void)
{
    HAL_IncTick();
}

// =====================================================
// Pin assignment
// =====================================================
#define RED_LED_PORT          GPIOC
#define RED_LED_PIN           GPIO_PIN_0

#define YELLOW_LED_PORT       GPIOC
#define YELLOW_LED_PIN        GPIO_PIN_1

#define GREEN_LED_PORT        GPIOC
#define GREEN_LED_PIN         GPIO_PIN_2

#define STATUS_LED_PORT       GPIOA
#define STATUS_LED_PIN        GPIO_PIN_5   // NUCLEO on-board LD2

#define SERVO_PORT            GPIOA
#define SERVO_PIN             GPIO_PIN_0   // TIM2_CH1

#define ULTRASONIC_TRIG_PORT  GPIOB
#define ULTRASONIC_TRIG_PIN   GPIO_PIN_10
#define ULTRASONIC_ECHO_PORT  GPIOA
#define ULTRASONIC_ECHO_PIN   GPIO_PIN_8   // Arduino D7; PB11 is NC on F411RE header

#define BUZZER_PORT           GPIOB
#define BUZZER_PIN            GPIO_PIN_12  // Active-high buzzer module input

UART_HandleTypeDef huart2;
TIM_HandleTypeDef htim2;  // Servo PWM 50 Hz
TIM_HandleTypeDef htim3;  // 1 MHz microsecond counter

// =====================================================
// Serial line buffer
// =====================================================
static char rxBuffer[96];
static uint32_t rxIndex = 0U;
static bool commandReady = false;

// =====================================================
// Gate FSM
// =====================================================
enum class GateState
{
    CLOSED,
    OPENING,
    OPEN,
    CLOSING,
    EMERGENCY
};

static GateState gateState = GateState::CLOSED;
static uint32_t gateStateStartTime = 0U;
static uint8_t gateAngle = 0U;

static const uint32_t GATE_MOVE_TIME_MS = 1000U;
static const uint32_t GATE_OPEN_HOLD_MS = 10000U;

// =====================================================
// Ultrasonic / automatic emergency
// =====================================================
static bool autoDetectEnabled = true;
static uint16_t obstacleThresholdCm = 25U;
static bool autoReleaseEnabled = false;
static uint32_t releaseDelayMs = 5000U;
static bool obstacleClearTracking = false;
static uint32_t obstacleClearStartTime = 0U;
static bool distanceValid = false;
static uint16_t distanceMm = 0U;
static uint8_t consecutiveObstacleHits = 0U;
static uint32_t lastDistanceMeasureTime = 0U;
static uint32_t lastDistanceReportTime = 0U;

static const uint32_t DISTANCE_MEASURE_INTERVAL_MS = 100U;
static const uint32_t DISTANCE_REPORT_INTERVAL_MS = 500U;
static const uint16_t ECHO_TIMEOUT_US = 30000U;

// =====================================================
// One-shot buzzer
// =====================================================
static bool buzzerActive = false;
static bool buzzerEventLatched = false;
static uint32_t buzzerStartTime = 0U;
static const uint32_t BUZZER_ON_TIME_MS = 500U;

// =====================================================
// Runtime heartbeat
// =====================================================
static uint32_t lastStatusBlinkTime = 0U;
static uint32_t lastAliveTime = 0U;

// =====================================================
// Prototypes
// =====================================================
static void SystemClock_Config(void);
static void GPIO_Init(void);
static void TIM2_ServoPWM_Init(void);
static void TIM3_Microseconds_Init(void);
static void USART2_Init(void);
static void Fatal_Error(void);

static void Serial_Send(const char* text);
static void Serial_Poll(void);
static void ProcessCommand(const char* cmd);
static void SendFullStatus(void);

static void SetLeds(bool red, bool yellow, bool green);
static void Servo_SetAngle(uint8_t angle);
static void SendGateAngle(void);

static void EnterClosed(void);
static void EnterOpening(void);
static void EnterOpen(void);
static void EnterClosing(void);
static void EnterEmergency(void);
static void Gate_Update(void);

static uint16_t Micros16(void);
static uint16_t ElapsedMicros16(uint16_t start, uint16_t now);
static void DelayMicroseconds(uint16_t microseconds);
static bool Ultrasonic_MeasureMm(uint16_t* measuredMm);
static void Ultrasonic_Update(void);
static void SendDistance(void);

static void Buzzer_StartEmergencyOnce(void);
static void Buzzer_Update(void);
static void Runtime_Heartbeat(void);

// =====================================================
// main
// =====================================================
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    GPIO_Init();
    TIM2_ServoPWM_Init();
    TIM3_Microseconds_Init();
    USART2_Init();

    Servo_SetAngle(0U);
    Serial_Send("BOOT:STM32F411RE_GATE_V9_PA8_FIXED");
    EnterClosed();

    while (1)
    {
        Serial_Poll();

        if (commandReady)
        {
            char command[sizeof(rxBuffer)];
            std::strncpy(command, rxBuffer, sizeof(command));
            command[sizeof(command) - 1U] = '\0';

            rxIndex = 0U;
            commandReady = false;

            ProcessCommand(command);
        }

        Gate_Update();
        Ultrasonic_Update();
        Buzzer_Update();
        Runtime_Heartbeat();
    }
}

// =====================================================
// Clock: HSI 16 MHz
// =====================================================
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {};
    RCC_ClkInitTypeDef clk = {};

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
    {
        while (1) {}
    }

    clk.ClockType =
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    clk.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0) != HAL_OK)
    {
        while (1) {}
    }
}

// =====================================================
// GPIO
// =====================================================
static void GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {};

    gpio.Pin = RED_LED_PIN | YELLOW_LED_PIN | GREEN_LED_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &gpio);

    gpio.Pin = STATUS_LED_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(STATUS_LED_PORT, &gpio);

    gpio.Pin = ULTRASONIC_TRIG_PIN | BUZZER_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(ULTRASONIC_TRIG_PORT, &gpio);

    gpio.Pin = ULTRASONIC_ECHO_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(ULTRASONIC_ECHO_PORT, &gpio);

    HAL_GPIO_WritePin(RED_LED_PORT, RED_LED_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(YELLOW_LED_PORT, YELLOW_LED_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GREEN_LED_PORT, GREEN_LED_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(STATUS_LED_PORT, STATUS_LED_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(
        ULTRASONIC_TRIG_PORT,
        ULTRASONIC_TRIG_PIN | BUZZER_PIN,
        GPIO_PIN_RESET
    );
}

// =====================================================
// TIM2 CH1 Servo PWM
// Timer clock 16 MHz / (15 + 1) = 1 MHz
// ARR 19999 -> 20 ms period -> 50 Hz
// 0 degrees = 1000 us, 90 degrees = 1500 us
// =====================================================
static void TIM2_ServoPWM_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_TIM2_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {};
    gpio.Pin = SERVO_PIN;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF1_TIM2;
    HAL_GPIO_Init(SERVO_PORT, &gpio);

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 15U;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 19999U;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
    {
        Fatal_Error();
    }

    TIM_OC_InitTypeDef channel = {};
    channel.OCMode = TIM_OCMODE_PWM1;
    channel.Pulse = 1000U;
    channel.OCPolarity = TIM_OCPOLARITY_HIGH;
    channel.OCFastMode = TIM_OCFAST_DISABLE;

    if (HAL_TIM_PWM_ConfigChannel(&htim2, &channel, TIM_CHANNEL_1) != HAL_OK)
    {
        Fatal_Error();
    }

    if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1) != HAL_OK)
    {
        Fatal_Error();
    }
}

// =====================================================
// TIM3 free-running 1 MHz microsecond counter
// =====================================================
static void TIM3_Microseconds_Init(void)
{
    __HAL_RCC_TIM3_CLK_ENABLE();

    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 15U;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 0xFFFFU;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
    {
        Fatal_Error();
    }

    if (HAL_TIM_Base_Start(&htim3) != HAL_OK)
    {
        Fatal_Error();
    }
}

// =====================================================
// USART2: PA2 TX / PA3 RX / 115200 8N1
// =====================================================
static void USART2_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {};
    gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &gpio);

    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
        Fatal_Error();
    }
}

static void Fatal_Error(void)
{
    HAL_GPIO_WritePin(RED_LED_PORT, RED_LED_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);

    while (1)
    {
        HAL_GPIO_TogglePin(STATUS_LED_PORT, STATUS_LED_PIN);
        HAL_Delay(150U);
    }
}

// =====================================================
// Serial
// =====================================================
static void Serial_Send(const char* text)
{
    const uint16_t length = static_cast<uint16_t>(std::strlen(text));
    static uint8_t lineEnd[] = {'\r', '\n'};

    (void)HAL_UART_Transmit(
        &huart2,
        reinterpret_cast<uint8_t*>(const_cast<char*>(text)),
        length,
        100U
    );

    (void)HAL_UART_Transmit(
        &huart2,
        lineEnd,
        sizeof(lineEnd),
        100U
    );
}

static void Serial_Poll(void)
{
    uint8_t byte = 0U;

    while (HAL_UART_Receive(&huart2, &byte, 1U, 0U) == HAL_OK)
    {
        const char c = static_cast<char>(byte);

        if (c == '\r' || c == '\n')
        {
            if (rxIndex > 0U && !commandReady)
            {
                rxBuffer[rxIndex] = '\0';
                commandReady = true;
            }
        }
        else if (!commandReady)
        {
            if (rxIndex < sizeof(rxBuffer) - 1U)
            {
                rxBuffer[rxIndex++] = c;
            }
            else
            {
                rxIndex = 0U;
            }
        }
    }
}

static void ProcessCommand(const char* cmd)
{
    if (std::strcmp(cmd, "GATE_OPEN") == 0 || std::strcmp(cmd, "TRAFFIC") == 0)
    {
        if (gateState == GateState::EMERGENCY)
        {
            Serial_Send("ERR:EMERGENCY_RESET_REQUIRED");
            return;
        }

        if (autoDetectEnabled)
        {
            if (!distanceValid)
            {
                Serial_Send("ERR:SENSOR_TIMEOUT");
                return;
            }

            if (distanceMm <= obstacleThresholdCm * 10U)
            {
                Serial_Send("ERR:OBSTACLE");
                return;
            }
        }

        EnterOpening();
        return;
    }

    if (std::strcmp(cmd, "GATE_CLOSE") == 0)
    {
        if (gateState == GateState::EMERGENCY)
        {
            Serial_Send("STATE:EMERGENCY");
            return;
        }

        EnterClosing();
        return;
    }

    if (std::strcmp(cmd, "AUTO_ON") == 0)
    {
        autoDetectEnabled = true;
        consecutiveObstacleHits = 0U;
        Serial_Send("AUTO:ON");
        return;
    }

    if (std::strcmp(cmd, "AUTO_OFF") == 0)
    {
        autoDetectEnabled = false;
        consecutiveObstacleHits = 0U;
        Serial_Send("AUTO:OFF");
        return;
    }

    if (std::strcmp(cmd, "AUTO_RELEASE_ON") == 0)
    {
        autoReleaseEnabled = true;
        obstacleClearTracking = false;
        Serial_Send("AUTO_RELEASE:ON");
        return;
    }

    if (std::strcmp(cmd, "AUTO_RELEASE_OFF") == 0)
    {
        autoReleaseEnabled = false;
        obstacleClearTracking = false;
        Serial_Send("AUTO_RELEASE:OFF");
        return;
    }

    if (std::strcmp(cmd, "EMERGENCY_RESET") == 0)
    {
        if (gateState != GateState::EMERGENCY)
        {
            Serial_Send("ERR:NOT_EMERGENCY");
            return;
        }

        const uint16_t clearDistanceCm = obstacleThresholdCm + 10U;
        if (
            autoDetectEnabled &&
            (!distanceValid || distanceMm <= clearDistanceCm * 10U)
        )
        {
            Serial_Send("ERR:OBSTACLE_NOT_CLEARED");
            return;
        }

        EnterClosed();
        Serial_Send("EMERGENCY:CLEARED");
        return;
    }

    static const char thresholdPrefix[] = "THRESHOLD:";
    if (std::strncmp(cmd, thresholdPrefix, sizeof(thresholdPrefix) - 1U) == 0)
    {
        const char* valueText = cmd + sizeof(thresholdPrefix) - 1U;
        char* end = nullptr;
        const long value = std::strtol(valueText, &end, 10);

        if (valueText == end || *end != '\0' || value < 5L || value > 200L)
        {
            Serial_Send("ERR:THRESHOLD_RANGE");
            return;
        }

        obstacleThresholdCm = static_cast<uint16_t>(value);

        char message[32];
        std::snprintf(
            message,
            sizeof(message),
            "THRESHOLD:%u",
            static_cast<unsigned int>(obstacleThresholdCm)
        );
        Serial_Send(message);
        return;
    }

    static const char releaseDelayPrefix[] = "RELEASE_DELAY_MS:";
    if (
        std::strncmp(
            cmd,
            releaseDelayPrefix,
            sizeof(releaseDelayPrefix) - 1U
        ) == 0
    )
    {
        const char* valueText = cmd + sizeof(releaseDelayPrefix) - 1U;
        char* end = nullptr;
        const long value = std::strtol(valueText, &end, 10);

        if (
            valueText == end || *end != '\0' ||
            value < 1000L || value > 60000L
        )
        {
            Serial_Send("ERR:RELEASE_DELAY_RANGE");
            return;
        }

        releaseDelayMs = static_cast<uint32_t>(value);

        char message[40];
        std::snprintf(
            message,
            sizeof(message),
            "RELEASE_DELAY_MS:%lu",
            static_cast<unsigned long>(releaseDelayMs)
        );
        Serial_Send(message);
        return;
    }

    if (std::strcmp(cmd, "STATUS?") == 0)
    {
        SendFullStatus();
        return;
    }

    Serial_Send("ERR:UNKNOWN_CMD");
}

static void SendFullStatus(void)
{
    switch (gateState)
    {
        case GateState::CLOSED:
            Serial_Send("STATE:CLOSED");
            break;
        case GateState::OPENING:
            Serial_Send("STATE:OPENING");
            break;
        case GateState::OPEN:
            Serial_Send("STATE:OPEN");
            break;
        case GateState::CLOSING:
            Serial_Send("STATE:CLOSING");
            break;
        case GateState::EMERGENCY:
            Serial_Send("STATE:EMERGENCY");
            break;
    }

    SendGateAngle();
    Serial_Send(autoDetectEnabled ? "AUTO:ON" : "AUTO:OFF");
    Serial_Send(autoReleaseEnabled ? "AUTO_RELEASE:ON" : "AUTO_RELEASE:OFF");

    char message[32];
    std::snprintf(
        message,
        sizeof(message),
        "THRESHOLD:%u",
        static_cast<unsigned int>(obstacleThresholdCm)
    );
    Serial_Send(message);

    std::snprintf(
        message,
        sizeof(message),
        "RELEASE_DELAY_MS:%lu",
        static_cast<unsigned long>(releaseDelayMs)
    );
    Serial_Send(message);

    SendDistance();
    Serial_Send(buzzerActive ? "BUZZER:ON" : "BUZZER:OFF");
}

// =====================================================
// LEDs / Servo
// =====================================================
static void SetLeds(bool red, bool yellow, bool green)
{
    HAL_GPIO_WritePin(
        RED_LED_PORT,
        RED_LED_PIN,
        red ? GPIO_PIN_SET : GPIO_PIN_RESET
    );
    HAL_GPIO_WritePin(
        YELLOW_LED_PORT,
        YELLOW_LED_PIN,
        yellow ? GPIO_PIN_SET : GPIO_PIN_RESET
    );
    HAL_GPIO_WritePin(
        GREEN_LED_PORT,
        GREEN_LED_PIN,
        green ? GPIO_PIN_SET : GPIO_PIN_RESET
    );
}

static void Servo_SetAngle(uint8_t angle)
{
    if (angle > 90U)
    {
        angle = 90U;
    }

    const uint32_t pulseUs =
        1000U + (static_cast<uint32_t>(angle) * 500U) / 90U;

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pulseUs);
    gateAngle = angle;
}

static void SendGateAngle(void)
{
    char message[24];
    std::snprintf(
        message,
        sizeof(message),
        "GATE:%u",
        static_cast<unsigned int>(gateAngle)
    );
    Serial_Send(message);
}

// =====================================================
// Gate FSM
// =====================================================
static void EnterClosed(void)
{
    gateState = GateState::CLOSED;
    gateStateStartTime = HAL_GetTick();
    consecutiveObstacleHits = 0U;
    obstacleClearTracking = false;
    buzzerEventLatched = false;
    buzzerActive = false;

    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
    Servo_SetAngle(0U);
    SetLeds(true, false, false);

    Serial_Send("STATE:CLOSED");
    SendGateAngle();
    Serial_Send("BUZZER:OFF");
}

static void EnterOpening(void)
{
    if (gateState == GateState::OPEN || gateState == GateState::OPENING)
    {
        Serial_Send("BUSY:GATE_OPEN");
        return;
    }

    gateState = GateState::OPENING;
    gateStateStartTime = HAL_GetTick();
    obstacleClearTracking = false;
    consecutiveObstacleHits = 0U;

    SetLeds(false, true, false);
    Servo_SetAngle(90U);

    Serial_Send("STATE:OPENING");
    SendGateAngle();
}

static void EnterOpen(void)
{
    gateState = GateState::OPEN;
    gateStateStartTime = HAL_GetTick();

    Servo_SetAngle(90U);
    SetLeds(false, false, true);

    Serial_Send("STATE:OPEN");
    SendGateAngle();
}

static void EnterClosing(void)
{
    if (gateState == GateState::CLOSED || gateState == GateState::CLOSING)
    {
        Serial_Send("BUSY:GATE_CLOSED");
        return;
    }

    gateState = GateState::CLOSING;
    gateStateStartTime = HAL_GetTick();

    SetLeds(false, true, false);
    Servo_SetAngle(0U);

    Serial_Send("STATE:CLOSING");
    SendGateAngle();
}

static void EnterEmergency(void)
{
    if (gateState == GateState::EMERGENCY)
    {
        return;
    }

    gateState = GateState::EMERGENCY;
    gateStateStartTime = HAL_GetTick();
    obstacleClearTracking = false;

    SetLeds(true, false, false);
    Servo_SetAngle(0U);
    Buzzer_StartEmergencyOnce();

    SendDistance();
    Serial_Send("ALERT:OBSTACLE");
    Serial_Send("STATE:EMERGENCY");
    SendGateAngle();
}

static void Gate_Update(void)
{
    const uint32_t now = HAL_GetTick();
    const uint32_t elapsed = now - gateStateStartTime;

    switch (gateState)
    {
        case GateState::CLOSED:
            break;

        case GateState::OPENING:
            if (elapsed >= GATE_MOVE_TIME_MS)
            {
                EnterOpen();
            }
            break;

        case GateState::OPEN:
            if (elapsed >= GATE_OPEN_HOLD_MS)
            {
                EnterClosing();
            }
            break;

        case GateState::CLOSING:
            if (elapsed >= GATE_MOVE_TIME_MS)
            {
                EnterClosed();
            }
            break;

        case GateState::EMERGENCY:
            break;
    }
}

// =====================================================
// Ultrasonic HC-SR04
// =====================================================
static uint16_t Micros16(void)
{
    return static_cast<uint16_t>(__HAL_TIM_GET_COUNTER(&htim3));
}

static uint16_t ElapsedMicros16(uint16_t start, uint16_t now)
{
    return static_cast<uint16_t>(now - start);
}

static void DelayMicroseconds(uint16_t microseconds)
{
    const uint16_t start = Micros16();

    while (ElapsedMicros16(start, Micros16()) < microseconds)
    {
    }
}

static bool Ultrasonic_MeasureMm(uint16_t* measuredMm)
{
    if (measuredMm == nullptr)
    {
        return false;
    }

    uint16_t waitStart = Micros16();
    while (
        HAL_GPIO_ReadPin(ULTRASONIC_ECHO_PORT, ULTRASONIC_ECHO_PIN) == GPIO_PIN_SET
    )
    {
        if (ElapsedMicros16(waitStart, Micros16()) >= ECHO_TIMEOUT_US)
        {
            return false;
        }
    }

    HAL_GPIO_WritePin(
        ULTRASONIC_TRIG_PORT,
        ULTRASONIC_TRIG_PIN,
        GPIO_PIN_RESET
    );
    DelayMicroseconds(2U);

    HAL_GPIO_WritePin(
        ULTRASONIC_TRIG_PORT,
        ULTRASONIC_TRIG_PIN,
        GPIO_PIN_SET
    );
    DelayMicroseconds(10U);

    HAL_GPIO_WritePin(
        ULTRASONIC_TRIG_PORT,
        ULTRASONIC_TRIG_PIN,
        GPIO_PIN_RESET
    );

    waitStart = Micros16();
    while (
        HAL_GPIO_ReadPin(ULTRASONIC_ECHO_PORT, ULTRASONIC_ECHO_PIN) == GPIO_PIN_RESET
    )
    {
        if (ElapsedMicros16(waitStart, Micros16()) >= ECHO_TIMEOUT_US)
        {
            return false;
        }
    }

    const uint16_t pulseStart = Micros16();
    while (
        HAL_GPIO_ReadPin(ULTRASONIC_ECHO_PORT, ULTRASONIC_ECHO_PIN) == GPIO_PIN_SET
    )
    {
        if (ElapsedMicros16(pulseStart, Micros16()) >= ECHO_TIMEOUT_US)
        {
            return false;
        }
    }

    const uint16_t pulseWidthUs =
        ElapsedMicros16(pulseStart, Micros16());

    if (pulseWidthUs < 100U || pulseWidthUs >= ECHO_TIMEOUT_US)
    {
        return false;
    }

    const uint32_t calculatedMm =
        (static_cast<uint32_t>(pulseWidthUs) * 10U) / 58U;

    if (calculatedMm == 0U || calculatedMm > 5000U)
    {
        return false;
    }

    *measuredMm = static_cast<uint16_t>(calculatedMm);
    return true;
}

static void Ultrasonic_Update(void)
{
    const uint32_t now = HAL_GetTick();

    if (now - lastDistanceMeasureTime < DISTANCE_MEASURE_INTERVAL_MS)
    {
        return;
    }

    lastDistanceMeasureTime = now;

    uint16_t measuredMm = 0U;
    distanceValid = Ultrasonic_MeasureMm(&measuredMm);

    if (distanceValid)
    {
        distanceMm = measuredMm;
    }

    const bool gateCanTriggerEmergency =
        gateState == GateState::OPENING ||
        gateState == GateState::OPEN ||
        gateState == GateState::CLOSING;

    const bool obstacleDetected =
        distanceValid &&
        distanceMm <= obstacleThresholdCm * 10U;

    if (autoDetectEnabled && gateCanTriggerEmergency && obstacleDetected)
    {
        if (consecutiveObstacleHits < 3U)
        {
            ++consecutiveObstacleHits;
        }

        if (consecutiveObstacleHits >= 2U)
        {
            EnterEmergency();
        }
    }
    else
    {
        consecutiveObstacleHits = 0U;
    }

    if (gateState == GateState::EMERGENCY && autoReleaseEnabled)
    {
        const uint16_t clearDistanceCm = obstacleThresholdCm + 10U;
        const bool obstacleIsClear =
            distanceValid && distanceMm > clearDistanceCm * 10U;

        if (obstacleIsClear)
        {
            if (!obstacleClearTracking)
            {
                obstacleClearTracking = true;
                obstacleClearStartTime = now;
                Serial_Send("EMERGENCY:CLEAR_TIMER_STARTED");
            }
            else if (now - obstacleClearStartTime >= releaseDelayMs)
            {
                obstacleClearTracking = false;
                buzzerEventLatched = false;
                buzzerActive = false;
                HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
                Serial_Send("EMERGENCY:AUTO_CLEARED");
                EnterOpening();
            }
        }
        else
        {
            obstacleClearTracking = false;
        }
    }

    if (now - lastDistanceReportTime >= DISTANCE_REPORT_INTERVAL_MS)
    {
        lastDistanceReportTime = now;
        SendDistance();
    }
}

static void SendDistance(void)
{
    if (!distanceValid)
    {
        Serial_Send("DIST:TIMEOUT");
        return;
    }

    char message[32];
    std::snprintf(
        message,
        sizeof(message),
        "DIST_MM:%u",
        static_cast<unsigned int>(distanceMm)
    );
    Serial_Send(message);
}

// =====================================================
// Buzzer: one 500 ms beep per emergency event
// =====================================================
static void Buzzer_StartEmergencyOnce(void)
{
    if (buzzerEventLatched)
    {
        return;
    }

    buzzerEventLatched = true;
    buzzerActive = true;
    buzzerStartTime = HAL_GetTick();

    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
    Serial_Send("BUZZER:ON");
}

static void Buzzer_Update(void)
{
    if (!buzzerActive)
    {
        return;
    }

    if (HAL_GetTick() - buzzerStartTime >= BUZZER_ON_TIME_MS)
    {
        buzzerActive = false;
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
        Serial_Send("BUZZER:OFF");
    }
}

// =====================================================
// Heartbeat
// =====================================================
static void Runtime_Heartbeat(void)
{
    const uint32_t now = HAL_GetTick();

    if (now - lastStatusBlinkTime >= 500U)
    {
        lastStatusBlinkTime = now;
        HAL_GPIO_TogglePin(STATUS_LED_PORT, STATUS_LED_PIN);
    }

    if (now - lastAliveTime >= 5000U)
    {
        lastAliveTime = now;
        Serial_Send("ALIVE");
    }
}
