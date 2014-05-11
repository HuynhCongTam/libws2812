#define DATA_OUT_PORT   GPIOA

#define PERIOD_TIM TIM2
#define WIDTH_DMA_CHANNEL DMA1_Channel2
#define LONG_DMA_CHANNEL DMA1_Channel5
#define SHORT_DMA_CHANNEL DMA1_Channel1

void InitTimer();
void InitOutputPins();
void InitDMA();
void InitIT();
void SetColor(uint8_t string, uint16_t num, uint8_t r, uint8_t g, uint8_t b);
void AddColor(uint8_t string, uint16_t num, uint8_t r, uint8_t g, uint8_t b);
void SubtractColor(uint8_t port_num, uint16_t led_num, uint8_t r, uint8_t g, uint8_t b);
void Start();
void ClearAll();
uint32_t GetColor(uint8_t string, uint16_t num);
void InitLightArray(uint8_t _num_lights);
void StartLights(uint8_t _num_lights);
