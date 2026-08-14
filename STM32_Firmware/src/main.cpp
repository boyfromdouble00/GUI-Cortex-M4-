#include "stm32f4xx_hal.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

// PlatformIO STM32Cube projects do not generate stm32f4xx_it.c for us.
// Without this handler the first 1 ms SysTick interrupt enters the weak
// Default_Handler and freezes the CPU permanently.
extern "C" void SysTick_Handler(void)
{
    HAL_IncTick();
}

// =====================================================
// Pin assignment
// =====================================================
#define RED_LED_PORT       GPIOC
#define RED_LED_PIN        GPIO_PIN_0

#define YELLOW_LED_PORT    GPIOC
#define YELLOW_LED_PIN     GPIO_PIN_1

#define GREEN_LED_PORT     GPIOC
#define GREEN_LED_PIN      GPIO_PIN_2

#define STATUS_LED_PORT    GPIOA
#define STATUS_LED_PIN     GPIO_PIN_5   // NUCLEO on-board LD2

#define MOTOR_PORT         GPIOA
#define MOTOR_PIN          GPIO_PIN_0

UART_HandleTypeDef huart2;
TIM_HandleTypeDef htim2;

// =====================================================
// Serial line buffer
// =====================================================
static char rxBuffer[64];
static uint32_t rxIndex = 0;
static bool commandReady = false;

// =====================================================
// Traffic FSM
// =====================================================
enum class TrafficState
{
    NORMAL,
    RED,
    YELLOW,
    GREEN
};

static TrafficState trafficState = TrafficState::NORMAL;
static uint32_t stateStartTime = 0;
static uint32_t lastCountTime = 0;
static uint32_t lastStatusBlinkTime = 0;
static uint32_t lastAliveTime = 0;
static bool motorEnabled = true;
static uint8_t motorDutyWanted = 100;
static uint8_t motorDutyOutput = 0;

// =====================================================
// Prototypes
// =====================================================
static void SystemClock_Config(void);
static void GPIO_Init(void);
static void TIM2_PWM_Init(void);
static void USART2_Init(void);

static void Serial_Send(const char* text);
static void Serial_Poll(void);
static void Runtime_Heartbeat(void);

static void SetLeds(bool red, bool yellow, bool green);
static void Motor_ApplyDuty(uint8_t duty);
static void Motor_RefreshOutput(void);
static void SendMotorStatus(void);

static void EnterNormal(void);
static void StartTraffic(void);
static void ProcessCommand(const char* cmd);
static void Traffic_Update(void);

// =====================================================
// main
// =====================================================
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    GPIO_Init();
    TIM2_PWM_Init();
    USART2_Init();

    motorEnabled = true;
    motorDutyWanted = 100;

    // 부팅 표시를 먼저 송신
    Serial_Send("BOOT:STM32F411RE");

    // 초기 상태
    EnterNormal();

    while (1)
    {
        // UART interrupt 대신 단순 polling
        Serial_Poll();

        if (commandReady)
        {
            char command[64];

            std::strncpy(command, rxBuffer, sizeof(command));
            command[sizeof(command) - 1] = '\0';

            rxIndex = 0;
            commandReady = false;

            ProcessCommand(command);
        }

        Traffic_Update();
        Runtime_Heartbeat();

        // No HAL_Delay here: USART2 @115200 must be polled continuously.
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
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {};

    // LEDs PC0/PC1/PC2
    gpio.Pin =
        RED_LED_PIN |
        YELLOW_LED_PIN |
        GREEN_LED_PIN;

    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOC, &gpio);

    // NUCLEO on-board LD2 (PA5): firmware alive indicator
    gpio.Pin = STATUS_LED_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(STATUS_LED_PORT, &gpio);

    // Turn the normal-state RED LED on immediately. This happens before
    // PWM/UART initialization so wiring and initialization faults can be
    // distinguished on the real board.
    HAL_GPIO_WritePin(
        GPIOC,
        YELLOW_LED_PIN | GREEN_LED_PIN,
        GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        RED_LED_PORT,
        RED_LED_PIN,
        GPIO_PIN_SET
    );

    HAL_GPIO_WritePin(
        STATUS_LED_PORT,
        STATUS_LED_PIN,
        GPIO_PIN_SET
    );
}

// =====================================================
// TIM2 PWM
// PA0 = TIM2_CH1 / 20 kHz / duty 0..100%
// Timer clock: 16 MHz, prescaler 15 -> 1 MHz, ARR 49 -> 20 kHz
// =====================================================
static void TIM2_PWM_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_TIM2_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {};
    gpio.Pin = MOTOR_PIN;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF1_TIM2;
    HAL_GPIO_Init(MOTOR_PORT, &gpio);

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 15U;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 49U;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
    {
        while (1) {}
    }

    TIM_OC_InitTypeDef channel = {};
    channel.OCMode = TIM_OCMODE_PWM1;
    channel.Pulse = 0U;
    channel.OCPolarity = TIM_OCPOLARITY_HIGH;
    channel.OCFastMode = TIM_OCFAST_DISABLE;

    if (HAL_TIM_PWM_ConfigChannel(&htim2, &channel, TIM_CHANNEL_1) != HAL_OK)
    {
        while (1) {}
    }

    if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1) != HAL_OK)
    {
        while (1) {}
    }
}

// =====================================================
// USART2
// PA2 TX / PA3 RX / 115200 8N1
// UART RX interrupt 대신 polling 사용
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
        while (1) {}
    }
}

// =====================================================
// UART TX - STM32 HAL blocking transmit with a finite timeout.
// Messages are short, so this is deterministic and avoids direct-register
// state handling differences between STM32Cube package versions.
// =====================================================
static void Serial_Send(const char* text)
{
    const uint16_t length = static_cast<uint16_t>(std::strlen(text));
    static const uint8_t lineEnd[] = {'\r', '\n'};

    (void)HAL_UART_Transmit(
        &huart2,
        reinterpret_cast<uint8_t*>(const_cast<char*>(text)),
        length,
        100U
    );

    (void)HAL_UART_Transmit(
        &huart2,
        const_cast<uint8_t*>(lineEnd),
        sizeof(lineEnd),
        100U
    );
}

// =====================================================
// UART RX polling
// =====================================================
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

// =====================================================
// Runtime heartbeat
// - PA5/LD2 toggles every 500 ms: main loop is alive.
// - ALIVE is sent every 5 s: UART TX path is alive.
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

// =====================================================
// LED
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

// =====================================================
// Motor PWM
// =====================================================
static void Motor_ApplyDuty(uint8_t duty)
{
    if (duty > 100U)
    {
        duty = 100U;
    }

    const uint32_t timerCounts = htim2.Init.Period + 1U;
    const uint32_t pulse =
        (timerCounts * static_cast<uint32_t>(duty)) / 100U;

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pulse);
    motorDutyOutput = duty;
}

static void Motor_RefreshOutput(void)
{
    const bool trafficAllowsMotor = (trafficState == TrafficState::NORMAL);
    const uint8_t outputDuty =
        (motorEnabled && trafficAllowsMotor) ? motorDutyWanted : 0U;

    Motor_ApplyDuty(outputDuty);
}

static void SendMotorStatus(void)
{
    Serial_Send(motorDutyOutput > 0U ? "MOTOR:ON" : "MOTOR:OFF");

    char message[32];
    std::snprintf(
        message,
        sizeof(message),
        "PWM:%u",
        static_cast<unsigned int>(motorDutyWanted)
    );
    Serial_Send(message);
}

// =====================================================
// NORMAL
// =====================================================
static void EnterNormal(void)
{
    trafficState = TrafficState::NORMAL;

    SetLeds(true, false, false);
    Motor_RefreshOutput();

    Serial_Send("STATE:NORMAL");
    SendMotorStatus();
}

// =====================================================
// Start traffic
// =====================================================
static void StartTraffic(void)
{
    if (trafficState != TrafficState::NORMAL)
    {
        Serial_Send("BUSY:TRAFFIC");
        return;
    }

    trafficState = TrafficState::RED;
    stateStartTime = HAL_GetTick();

    Motor_RefreshOutput();
    Serial_Send("MOTOR:OFF");

    SetLeds(true, false, false);
    Serial_Send("STATE:RED");
}

// =====================================================
// Command parser
// =====================================================
static void ProcessCommand(const char* cmd)
{
    if (std::strcmp(cmd, "TRAFFIC") == 0)
    {
        StartTraffic();
        return;
    }

    if (std::strcmp(cmd, "MOTOR_ON") == 0)
    {
        motorEnabled = true;

        if (motorDutyWanted == 0U)
        {
            motorDutyWanted = 100U;
        }

        Motor_RefreshOutput();
        SendMotorStatus();
        return;
    }

    if (std::strcmp(cmd, "MOTOR_OFF") == 0)
    {
        motorEnabled = false;
        Motor_RefreshOutput();

        Serial_Send("MOTOR:OFF");
        return;
    }

    static const char pwmPrefix[] = "MOTOR_PWM:";
    if (std::strncmp(cmd, pwmPrefix, sizeof(pwmPrefix) - 1U) == 0)
    {
        const char* valueText = cmd + sizeof(pwmPrefix) - 1U;
        char* end = nullptr;
        const long duty = std::strtol(valueText, &end, 10);

        if (valueText == end || *end != '\0' || duty < 0L || duty > 100L)
        {
            Serial_Send("ERR:PWM_RANGE");
            return;
        }

        motorDutyWanted = static_cast<uint8_t>(duty);
        motorEnabled = (motorDutyWanted > 0U);
        Motor_RefreshOutput();
        SendMotorStatus();
        return;
    }

    if (std::strcmp(cmd, "STATUS?") == 0)
    {
        switch (trafficState)
        {
            case TrafficState::NORMAL:
                Serial_Send("STATE:NORMAL");
                break;

            case TrafficState::RED:
                Serial_Send("STATE:RED");
                break;

            case TrafficState::YELLOW:
                Serial_Send("STATE:YELLOW");
                break;

            case TrafficState::GREEN:
                Serial_Send("STATE:GREEN");
                break;
        }

        SendMotorStatus();

        return;
    }

    Serial_Send("ERR:UNKNOWN_CMD");
}

// =====================================================
// FSM
// =====================================================
static void Traffic_Update(void)
{
    uint32_t now = HAL_GetTick();

    switch (trafficState)
    {
        case TrafficState::NORMAL:
            break;

        case TrafficState::RED:
            if (now - stateStartTime >= 1000U)
            {
                trafficState = TrafficState::YELLOW;
                stateStartTime = now;

                SetLeds(false, true, false);
                Serial_Send("STATE:YELLOW");
            }
            break;

        case TrafficState::YELLOW:
            if (now - stateStartTime >= 1000U)
            {
                trafficState = TrafficState::GREEN;
                stateStartTime = now;
                lastCountTime = now;

                SetLeds(false, false, true);
                Serial_Send("STATE:GREEN");
                Serial_Send("COUNT:10");
            }
            break;

        case TrafficState::GREEN:
        {
            uint32_t elapsed = now - stateStartTime;

            if (elapsed >= 10000U)
            {
                EnterNormal();
                break;
            }

            if (now - lastCountTime >= 1000U)
            {
                lastCountTime += 1000U;

                uint32_t secondsPassed = elapsed / 1000U;
                uint32_t remain = 10U - secondsPassed;

                char message[32];

                std::snprintf(
                    message,
                    sizeof(message),
                    "COUNT:%lu",
                    (unsigned long)remain
                );

                Serial_Send(message);
            }

            break;
        }
    }
}
