/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "lcd.h"
#include "ds18b20.h"
#include "impedance.h"
#include "gradient_boosting_model.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define TCS_S0_PORT GPIOB
#define TCS_S0_PIN  GPIO_PIN_0

#define TCS_S1_PORT GPIOB
#define TCS_S1_PIN  GPIO_PIN_1

#define TCS_S2_PORT GPIOB
#define TCS_S2_PIN  GPIO_PIN_2

#define TCS_S3_PORT GPIOB
#define TCS_S3_PIN  GPIO_PIN_10

#define DHT11_PORT GPIOB
#define DHT11_PIN  GPIO_PIN_12

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/

/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

CRC_HandleTypeDef hcrc;

DAC_HandleTypeDef hdac;
DMA_HandleTypeDef hdma_dac1;

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim6;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */


float dht11_humidity = 0.0f;
volatile uint32_t tcs_capture1 = 0;
volatile uint32_t tcs_capture2 = 0;
volatile uint32_t tcs_period = 0;

volatile uint32_t tcs_edge_count = 0;

volatile uint8_t tcs_capture_done = 0;
volatile uint8_t tcs_capture_state = 0;


uint32_t tcs_red = 0;
uint32_t tcs_green = 0;
uint32_t tcs_blue = 0;


/* Impedance */
ImpedanceResult_t impedance_result;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_CRC_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_DAC_Init(void);
static void MX_TIM6_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static void Debug_Print(const char *text)
{
    HAL_UART_Transmit(
        &huart2,
        (uint8_t *)text,
        strlen(text),
        HAL_MAX_DELAY
    );
}

static void Debug_Printf(const char *format, ...)
{
    char buffer[256];
    va_list args;

    va_start(args, format);

    vsnprintf(
        buffer,
        sizeof(buffer),
        format,
        args
    );

    va_end(args);

    HAL_UART_Transmit(
        &huart2,
        (uint8_t *)buffer,
        strlen(buffer),
        HAL_MAX_DELAY
    );
}

/* =========================================================
 * DHT11 FUNCTIONS
 * ========================================================= */

static void DHT11_SetOutput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}


static void DHT11_SetInput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}


/*
 * Microsecond delay using DWT cycle counter.
 */
static void DHT11_Delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;

    uint32_t cycles =
        us * (HAL_RCC_GetHCLKFreq() / 1000000U);

    while ((DWT->CYCCNT - start) < cycles)
    {
    }
}


/*
 * Initialize DWT cycle counter.
 */
static void DHT11_DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    DWT->CYCCNT = 0;

    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}


/*
 * Wait until DHT11 data line reaches requested level.
 */
static uint8_t DHT11_WaitForLevel(
    GPIO_PinState level,
    uint32_t timeout_us)
{
    uint32_t start = DWT->CYCCNT;

    uint32_t timeout_cycles =
        timeout_us *
        (HAL_RCC_GetHCLKFreq() / 1000000U);

    while (HAL_GPIO_ReadPin(
               DHT11_PORT,
               DHT11_PIN) != level)
    {
        if ((DWT->CYCCNT - start) > timeout_cycles)
        {
            return 0;
        }
    }

    return 1;
}


/*
 * Read DHT11 humidity.
 *
 * DHT11 sends 40 bits:
 *
 * Byte 0 = Humidity integer
 * Byte 1 = Humidity decimal
 * Byte 2 = Temperature integer
 * Byte 3 = Temperature decimal
 * Byte 4 = Checksum
 *
 * For DHT11:
 *
 * Humidity = data[0]
 * Temperature = data[2]
 */
static HAL_StatusTypeDef DHT11_Read(float *humidity)
{
    uint8_t data[5] = {0};

    if (humidity == NULL)
    {
        return HAL_ERROR;
    }

    /* DHT11 requires a start pulse of at least 18 ms. */
    DHT11_SetOutput();
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET);
    HAL_Delay(20);
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
    DHT11_Delay_us(30);
    DHT11_SetInput();

    /* Sensor response: LOW (~80 us), then HIGH (~80 us). */
    if (!DHT11_WaitForLevel(GPIO_PIN_RESET, 100U) ||
        !DHT11_WaitForLevel(GPIO_PIN_SET, 100U) ||
        !DHT11_WaitForLevel(GPIO_PIN_RESET, 100U))
    {
        return HAL_TIMEOUT;
    }

    /* Read 40 bits. A longer HIGH pulse represents a one. */
    for (uint8_t bit = 0; bit < 40U; bit++)
    {
        if (!DHT11_WaitForLevel(GPIO_PIN_SET, 100U))
        {
            return HAL_TIMEOUT;
        }

        DHT11_Delay_us(40U);
        data[bit / 8U] <<= 1U;
        if (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET)
        {
            data[bit / 8U] |= 1U;
        }

        if (!DHT11_WaitForLevel(GPIO_PIN_RESET, 100U))
        {
            return HAL_TIMEOUT;
        }
    }

    /* DHT11 checksum is the low eight bits of the first four bytes' sum. */
    if ((uint8_t)(data[0] + data[1] + data[2] + data[3]) != data[4])
    {
        return HAL_ERROR;
    }

    /* DHT11 reports integer humidity in data[0]. */
    *humidity = (float)data[0] + ((float)data[1] / 10.0f);
    return HAL_OK;
}

/* =========================================================
 * TCS3200 FUNCTIONS
 * ========================================================= */

static void TCS3200_SetFilter(uint8_t filter)
{
    switch (filter)
    {
        case 0:     // RED
            HAL_GPIO_WritePin(TCS_S2_PORT, TCS_S2_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(TCS_S3_PORT, TCS_S3_PIN, GPIO_PIN_RESET);
            break;

        case 1:     // GREEN
            HAL_GPIO_WritePin(TCS_S2_PORT, TCS_S2_PIN, GPIO_PIN_SET);
            HAL_GPIO_WritePin(TCS_S3_PORT, TCS_S3_PIN, GPIO_PIN_SET);
            break;

        case 2:     // BLUE
            HAL_GPIO_WritePin(TCS_S2_PORT, TCS_S2_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(TCS_S3_PORT, TCS_S3_PIN, GPIO_PIN_SET);
            break;

        case 3:     // CLEAR
            HAL_GPIO_WritePin(TCS_S2_PORT, TCS_S2_PIN, GPIO_PIN_SET);
            HAL_GPIO_WritePin(TCS_S3_PORT, TCS_S3_PIN, GPIO_PIN_RESET);
            break;
    }
}


static void TCS3200_Init(void)
{
    Debug_Print("[TCS] Initializing...\r\n");

    /*
     * S0 = HIGH
     * S1 = LOW
     *
     * Frequency scaling = 20%
     */
    HAL_GPIO_WritePin(
        TCS_S0_PORT,
        TCS_S0_PIN,
        GPIO_PIN_SET
    );

    HAL_GPIO_WritePin(
        TCS_S1_PORT,
        TCS_S1_PIN,
        GPIO_PIN_RESET
    );

    /*
     * Start with RED filter.
     */
    TCS3200_SetFilter(0);

    /*
     * Reset debug counters.
     */
    tcs_edge_count = 0;
    tcs_capture_done = 0;
    tcs_capture_state = 0;
    tcs_period = 0;

    /*
     * Start TIM3 CH1 input capture.
     *
     * PA6 = TIM3_CH1 = TCS OUT
     */
    if (HAL_TIM_IC_Start_IT(
            &htim3,
            TIM_CHANNEL_1
        ) != HAL_OK)
    {
        Debug_Print("[TCS][ERROR] TIM3 CH1 start FAILED\r\n");
        return;
    }

    Debug_Print("[TCS][OK] TIM3 CH1 capture started\r\n");

    Debug_Printf(
        "[TCS] S0=%d S1=%d S2=%d S3=%d\r\n",
        HAL_GPIO_ReadPin(TCS_S0_PORT, TCS_S0_PIN),
        HAL_GPIO_ReadPin(TCS_S1_PORT, TCS_S1_PIN),
        HAL_GPIO_ReadPin(TCS_S2_PORT, TCS_S2_PIN),
        HAL_GPIO_ReadPin(TCS_S3_PORT, TCS_S3_PIN)
    );

    Debug_Print("[TCS] OUT = PA6 / TIM3_CH1\r\n");
}

static uint32_t TCS3200_ReadFrequency(void)
{
    uint32_t timeout;

    tcs_capture_done = 0;
    tcs_capture_state = 0;
    tcs_period = 0;

    uint32_t edges_before = tcs_edge_count;

    timeout = HAL_GetTick();

    while (!tcs_capture_done)
    {
        if ((HAL_GetTick() - timeout) > 100)
        {
            Debug_Printf(
                "[TCS][DEBUG] TIMEOUT | edges=%lu | period=%lu\r\n",
                tcs_edge_count - edges_before,
                tcs_period
            );

            return 0;
        }
    }

    Debug_Printf(
        "[TCS][DEBUG] edges=%lu period=%lu\r\n",
        tcs_edge_count - edges_before,
        tcs_period
    );

    if (tcs_period == 0)
    {
        Debug_Print("[TCS][ERROR] Period = 0\r\n");
        return 0;
    }

    uint32_t frequency =
        1000000UL / tcs_period;

    Debug_Printf(
        "[TCS][OK] Frequency = %lu Hz\r\n",
        frequency
    );

    return frequency;
}

static uint32_t TCS3200_ReadColor(uint8_t filter)
{
    TCS3200_SetFilter(filter);

    /*
     * Allow the sensor output to settle after
     * changing the color filter.
     */
    HAL_Delay(50);

    return TCS3200_ReadFrequency();
}


static void TCS3200_ReadRGB(
    uint32_t *red,
    uint32_t *green,
    uint32_t *blue)
{
    *red = TCS3200_ReadColor(0);
    *green = TCS3200_ReadColor(1);
    *blue = TCS3200_ReadColor(2);
}

/*
 * TCS3200-only oil appearance classification.
 * These thresholds are provisional and must be calibrated against measured
 * fresh/used-oil samples under controlled illumination and fixed geometry.
 * This classification is independent of the ML remaining-cycle prediction.
 * Return 1 for FRESH and 0 for USED.
 */
static uint8_t TCS_ClassifyOil(uint32_t red, uint32_t green, uint32_t blue)
{
    uint8_t fresh_score = 0U;

    if (red >= 393U)   fresh_score++;
    if (green >= 397U) fresh_score++;
    if (blue >= 493U)  fresh_score++;

    return (fresh_score >= 2U) ? 1U : 0U;
}

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */


     float temperature = 0.0f;
    HAL_StatusTypeDef temperature_status = HAL_ERROR;
    HAL_StatusTypeDef humidity_status = HAL_ERROR;
    HAL_StatusTypeDef impedance_status = HAL_ERROR;
    char lcd_line1[17];
    char lcd_line2[17];
    uint8_t lcd_page = 0U;
    uint8_t oil_is_fresh = 0U;
    uint8_t tcs_readings_valid = 0U;
    uint8_t model_inputs_valid = 0U;
    int32_t remaining_cycles = -1;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_CRC_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_TIM3_Init();
  MX_USART1_UART_Init();
  MX_TIM2_Init();
  MX_USART2_UART_Init();
  MX_DAC_Init();
  MX_TIM6_Init();
  /* USER CODE BEGIN 2 */

  DHT11_DWT_Init();

  Debug_Print("\r\n\r\n");

  Debug_Print("Oil Quality Analyzer - live sensor acquisition\r\n");

  Debug_Print("\r\n");


  /* =========================================================
   * LCD
   * ========================================================= */

  Debug_Print("[INIT] Initializing LCD...\r\n");

  LCD_Init(&hi2c1);

  LCD_Clear();

  LCD_SetCursor(0, 0);
  LCD_Print("Oil Quality");

  LCD_SetCursor(1, 0);
  LCD_Print("Initializing...");

  HAL_Delay(1500);

  Debug_Print("[OK] LCD initialized\r\n");


  /* =========================================================
   * DS18B20
   * ========================================================= */

  Debug_Print("[INIT] Checking DS18B20...\r\n");

  if (DS18B20_Init() == HAL_OK)
  {
      Debug_Print("[OK] DS18B20 detected\r\n");
  }
  else
  {
      Debug_Print("[WARN] DS18B20 NOT detected\r\n");
  }


  /* =========================================================
   * IMPEDANCE
   * ========================================================= */

  Debug_Print("[INIT] Initializing impedance measurement...\r\n");

  Impedance_Init();

  Debug_Print("[OK] Impedance initialized\r\n");

  /* =========================================================
   * TCS3200
   * ========================================================= */

  Debug_Print("[INIT] Initializing TCS3200...\r\n");

  TCS3200_Init();

  Debug_Print("[OK] TCS3200 initialized\r\n");


  /* =========================================================
   * SYSTEM READY
   * ========================================================= */

  Debug_Print("\r\n");

  Debug_Print("========================================\r\n");
  Debug_Print("       SYSTEM READY\r\n");
  Debug_Print("========================================\r\n");

  Debug_Print("\r\n");


  LCD_Clear();

  LCD_SetCursor(0, 0);
  LCD_Print("Live Readings");

  LCD_SetCursor(1, 0);
  LCD_Print("Sensors starting");

  HAL_Delay(500);

  /* USER CODE END 2 */

  /* Initialize leds */
  BSP_LED_Init(LED2);

  /* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      /* Acquire actual temperature from the DS18B20. */
      temperature_status = DS18B20_ReadTemperature(&temperature);

      /* Acquire actual relative humidity from the DHT11. */
      humidity_status = DHT11_Read(&dht11_humidity);

      /* Read the TCS3200 output frequencies for each colour filter. */
      TCS3200_ReadRGB(&tcs_red, &tcs_green, &tcs_blue);

      /* Measure the analog response of the impedance circuit. */
      impedance_status = Impedance_Measure(&impedance_result);

      /* Classify oil from TCS3200 readings only (provisional thresholds). */
      tcs_readings_valid = (tcs_red > 0U && tcs_green > 0U && tcs_blue > 0U);
      if (tcs_readings_valid)
      {
          oil_is_fresh = TCS_ClassifyOil(tcs_red, tcs_green, tcs_blue);
      }

      /*
       * Run the ML model only when every required input is a successful,
       * non-zero live acquisition. The model output is remaining cycles only;
       * it does not decide whether the oil is fresh or used.
       */
      model_inputs_valid = (temperature_status == HAL_OK) &&
                           (humidity_status == HAL_OK) &&
                           (impedance_status == HAL_OK) &&
                           tcs_readings_valid;
      remaining_cycles = -1;
      if (model_inputs_valid)
      {
          remaining_cycles = oil_quality_predict(
              temperature,
              dht11_humidity,
              (float)tcs_red,
              (float)tcs_green,
              (float)tcs_blue,
              impedance_result.ac_rms_voltage
          );

          if (remaining_cycles < 0 || remaining_cycles > 4)
          {
              remaining_cycles = -1;
          }
      }

      /* Print raw acquired values, sensor-derived oil class, and ML estimate. */
      Debug_Print("\r\n========== LIVE SENSOR READINGS ==========\r\n");

      if (temperature_status == HAL_OK)
          Debug_Printf("DS18B20 temperature: %.2f C\r\n", temperature);
      else
          Debug_Printf("DS18B20 temperature: ERROR (status=%d)\r\n", temperature_status);

      if (humidity_status == HAL_OK)
          Debug_Printf("DHT11 humidity: %.1f %%RH\r\n", dht11_humidity);
      else
          Debug_Printf("DHT11 humidity: ERROR (status=%d)\r\n", humidity_status);

      Debug_Printf("TCS3200 red frequency: %lu Hz\r\n", tcs_red);
      Debug_Printf("TCS3200 green frequency: %lu Hz\r\n", tcs_green);
      Debug_Printf("TCS3200 blue frequency: %lu Hz\r\n", tcs_blue);

      if (impedance_status == HAL_OK)
      {
          Debug_Printf("Impedance circuit DC voltage: %.4f V\r\n", impedance_result.dc_voltage);
          Debug_Printf("Impedance circuit RMS voltage: %.4f V\r\n", impedance_result.rms_voltage);
          Debug_Printf("Impedance circuit AC RMS voltage: %.4f V\r\n", impedance_result.ac_rms_voltage);
          Debug_Printf("Excitation frequency: %lu Hz\r\n", impedance_result.frequency_hz);
      }
      else
      {
          Debug_Printf("Impedance circuit measurement: ERROR (status=%d)\r\n", impedance_status);
      }

      if (tcs_readings_valid)
          Debug_Printf("TCS-based oil classification: %s\r\n", oil_is_fresh ? "FRESH" : "USED");
      else
          Debug_Print("TCS-based oil classification: UNAVAILABLE (sensor timeout)\r\n");

      if (remaining_cycles >= 0)
          Debug_Printf("ML predicted remaining frying cycles: %ld\r\n", (long)remaining_cycles);
      else
          Debug_Print("ML remaining-cycle prediction: UNAVAILABLE (invalid sensor input)\r\n");

      Debug_Print("==========================================\r\n");

      /* Rotate the 16x2 LCD through four pages of measurements and results. */
      LCD_Clear();
      switch (lcd_page)
      {
          case 0U:
              if (temperature_status == HAL_OK)
                  snprintf(lcd_line1, sizeof(lcd_line1), "Temp: %.1f C", temperature);
              else
                  snprintf(lcd_line1, sizeof(lcd_line1), "Temp: SENSOR ERR");

              if (humidity_status == HAL_OK)
                  snprintf(lcd_line2, sizeof(lcd_line2), "Humidity: %.1f%%", dht11_humidity);
              else
                  snprintf(lcd_line2, sizeof(lcd_line2), "Humidity: ERROR");
              break;

          case 1U:
              snprintf(lcd_line1, sizeof(lcd_line1), "R:%lu G:%lu", tcs_red, tcs_green);
              snprintf(lcd_line2, sizeof(lcd_line2), "B:%lu Hz", tcs_blue);
              break;

          case 2U:
              if (impedance_status == HAL_OK)
              {
                  snprintf(lcd_line1, sizeof(lcd_line1), "AC RMS: %.3fV", impedance_result.ac_rms_voltage);
                  snprintf(lcd_line2, sizeof(lcd_line2), "DC: %.3fV", impedance_result.dc_voltage);
              }
              else
              {
                  snprintf(lcd_line1, sizeof(lcd_line1), "Impedance ERROR");
                  snprintf(lcd_line2, sizeof(lcd_line2), "Check circuit");
              }
              break;

          default:
              if (tcs_readings_valid)
                  snprintf(lcd_line1, sizeof(lcd_line1), "Oil: %s", oil_is_fresh ? "FRESH" : "USED");
              else
                  snprintf(lcd_line1, sizeof(lcd_line1), "Oil: TCS ERROR");

              if (remaining_cycles >= 0)
                  snprintf(lcd_line2, sizeof(lcd_line2), "Cycles left: %ld", (long)remaining_cycles);
              else
                  snprintf(lcd_line2, sizeof(lcd_line2), "Cycles: ML N/A");
              break;
      }

      LCD_SetCursor(0, 0);
      LCD_Print(lcd_line1);
      LCD_SetCursor(1, 0);
      LCD_Print(lcd_line2);

      lcd_page = (uint8_t)((lcd_page + 1U) % 4U);
      HAL_Delay(2000);
  }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 180;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief CRC Initialization Function
  * @param None
  * @retval None
  */
static void MX_CRC_Init(void)
{

  /* USER CODE BEGIN CRC_Init 0 */

  /* USER CODE END CRC_Init 0 */

  /* USER CODE BEGIN CRC_Init 1 */

  /* USER CODE END CRC_Init 1 */
  hcrc.Instance = CRC;
  if (HAL_CRC_Init(&hcrc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CRC_Init 2 */

  /* USER CODE END CRC_Init 2 */

}

/**
  * @brief DAC Initialization Function
  * @param None
  * @retval None
  */
static void MX_DAC_Init(void)
{

  /* USER CODE BEGIN DAC_Init 0 */

  /* USER CODE END DAC_Init 0 */

  DAC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN DAC_Init 1 */

  /* USER CODE END DAC_Init 1 */

  /** DAC Initialization
  */
  hdac.Instance = DAC;
  if (HAL_DAC_Init(&hdac) != HAL_OK)
  {
    Error_Handler();
  }

  /** DAC channel OUT1 config
  */
  sConfig.DAC_Trigger = DAC_TRIGGER_T6_TRGO;
  sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  if (HAL_DAC_ConfigChannel(&hdac, &sConfig, DAC_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DAC_Init 2 */

  /* USER CODE END DAC_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_IC_InitTypeDef sConfigIC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 89;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4294967295;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_IC_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
  sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
  sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
  sConfigIC.ICFilter = 0;
  if (HAL_TIM_IC_ConfigChannel(&htim2, &sConfigIC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_IC_InitTypeDef sConfigIC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 89;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_IC_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
  sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
  sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
  sConfigIC.ICFilter = 0;
  if (HAL_TIM_IC_ConfigChannel(&htim3, &sConfigIC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 0;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 624;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
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
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2|GPIO_PIN_6|GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB,
      GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_10
      |GPIO_PIN_4|GPIO_PIN_5,
      GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PC0 PC8 PC9 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_8|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PC1 */
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PC2 PC6 PC7 */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_6|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PA5 */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB1 PB2 PB10
                           PB12 PB4 PB5 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_10
                        |GPIO_PIN_4|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* DHT11 DATA - PB12 */
  GPIO_InitStruct.Pin = GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */




void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3 &&
        htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1)
    {
        uint32_t capture;

        tcs_edge_count++;

        capture = HAL_TIM_ReadCapturedValue(
            htim,
            TIM_CHANNEL_1
        );

        if (tcs_capture_state == 0)
        {
            tcs_capture1 = capture;
            tcs_capture_state = 1;
        }
        else
        {
            tcs_capture2 = capture;

            if (tcs_capture2 >= tcs_capture1)
            {
                tcs_period =
                    tcs_capture2 - tcs_capture1;
            }
            else
            {
                /*
                 * TIM3 is a 16-bit timer.
                 */
                tcs_period =
                    (0xFFFFUL - tcs_capture1)
                    + tcs_capture2
                    + 1UL;
            }

            tcs_capture_done = 1;
            tcs_capture_state = 0;
        }
    }
}


/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
