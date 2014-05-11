#include "stm32f0xx.h"
#include "stm32f0xx_dma.h"
#include "libws2812.h"
#include <stdio.h>
#include <stdlib.h>


uint16_t *source;
uint16_t destination;
uint8_t update_pending = 0;
uint16_t ones = 0xff;
uint16_t zeros = 0x00;

uint8_t num_lights;
uint16_t array_size;

void StartLights(uint8_t _num_lights){
	InitLightArray(_num_lights);
	InitOutputPins();
	InitTimer();
	InitDMA();
	InitIT();
}

void InitLightArray(uint8_t _num_lights){
	num_lights = _num_lights;
	array_size = 8 * 3 * num_lights;
	source = malloc(sizeof(uint16_t) * array_size);
}

uint32_t GetColor(uint8_t prot_num, uint16_t led_num){
	uint8_t mask = 0x01 << prot_num;
	uint32_t retval = 0;
	led_num = led_num * 24;
	for(uint8_t i = 0; i < 24; i++){
		if(source[led_num + i] & mask){
			retval |= 0x01 << (23 - i);
		}
	}
	return ((retval & 0xff0000) >> 8) | ((retval  & 0x00ff00) << 8) | (retval  & 0x0000ff);
}

void SetColor(uint8_t port_num, uint16_t led_num, uint8_t r, uint8_t g, uint8_t b){
	uint8_t bit;
	uint32_t offset, color, mask;

	bit = (0x01 << port_num);
	offset = led_num * 24;

	// rearrange the color inputs to make iteration a more simple operation
	// when packing the bits in to the source array
	color = (g << 16) | (r << 8) | (b << 0);

	for(mask = (0x01 << 23); mask; mask >>= 1){
		if(color & mask){
			source[offset] |= bit;
		}else{
			source[offset] &= ~bit;
		}
		offset++;
	}
}

// Instead of just setting and overriding a particular position this adds each of
// colors in the given position.
void AddColor(uint8_t port_num, uint16_t led_num, uint8_t r, uint8_t g, uint8_t b){

	uint32_t color = GetColor(port_num, led_num);

	if(r + ((color & 0xff0000) >> 16) > 255){
		r = 255;
	}else{
		r = r + ((color & 0xff0000) >> 16);
	}

	if(g + ((color & 0x00ff00) >> 8) > 255){
		g = 255;
	}else{
		g = g + ((color & 0x00ff00) >> 8);
	}

	if(b + (color & 0x0000ff) > 255){
		b = 255;
	}else{
		b = b + (color & 0x0000ff);
	}
	SetColor(port_num, led_num, r, g, b);
}

void SubtractColor(uint8_t port_num, uint16_t led_num, uint8_t r, uint8_t g, uint8_t b){

	uint32_t color = GetColor(port_num, led_num);

	if(r >= ((color & 0xff0000) >> 16)){
		r = 0;
	}else{
		r = ((color & 0xff0000) >> 16) - r;
	}

	if(g >= ((color & 0x00ff00) >> 8)){
		g = 0;
	}else{
		g = ((color & 0x00ff00) >> 8) - g;
	}

	if(b >= (color & 0x0000ff)){
		b = 0;
	}else{
		b = (color & 0x0000ff) - b;
	}
	SetColor(port_num, led_num, r, g, b);
}

void ClearAll(){
	for(int i = 0; i < array_size; i++){
		source[i] = 0;
	}
}

void DMA1_Channel4_5_IRQHandler(void){
	if(DMA_GetITStatus(DMA1_IT_TC5)){
		TIM_Cmd(PERIOD_TIM, DISABLE);
		DMA_Cmd(LONG_DMA_CHANNEL, DISABLE);
		DMA_Cmd(SHORT_DMA_CHANNEL, DISABLE);
		DMA_Cmd(WIDTH_DMA_CHANNEL, DISABLE);
		DMA_ClearITPendingBit(DMA1_IT_GL5);
		DATA_OUT_PORT->BSRR = 0xffff0000;
		update_pending = 0;
	}
}

void Start(){
	// this will block until the next update can start,
	// but once it does you are free to do what ever else you need to do while
	// DMA takes care of the data stream
	// to avoid this one could check the update_pending manually before invoking this.
	// any time new data is clocked in the low 50us is automatically loaded in so that
	// the new data is presented on the LEDs
	while(update_pending){}

	update_pending = 1;

	// The Timer must be disabled before the CNT register can be updated.
	TIM_Cmd(PERIOD_TIM, DISABLE);
	// The DMA channels must be disabled before the CNDTRs can be updated.
	DMA_Cmd(LONG_DMA_CHANNEL, DISABLE);
	DMA_Cmd(SHORT_DMA_CHANNEL, DISABLE);
	DMA_Cmd(WIDTH_DMA_CHANNEL, DISABLE);

	SHORT_DMA_CHANNEL->CNDTR = array_size;
	// this gives ~50us of low to latch the new data
	LONG_DMA_CHANNEL->CNDTR = array_size + 30;
	WIDTH_DMA_CHANNEL->CNDTR = array_size;

	// This just enables the channel nothing will happen until the timer is started.
	DMA_Cmd(LONG_DMA_CHANNEL, ENABLE);
	DMA_Cmd(SHORT_DMA_CHANNEL, ENABLE);
	DMA_Cmd(WIDTH_DMA_CHANNEL, ENABLE);

	// so that the Update DMA request is generated immediately
	PERIOD_TIM->CNT = 59;
	DATA_OUT_PORT->BSRR = 0xffff0000;

	// Start the timer, DMA transfers will begin immediately on the first update
	TIM_Cmd(PERIOD_TIM, ENABLE);
}

void InitDMA(){
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

	DMA_InitTypeDef DMA_InitStructure;
	// Taking over DATA_OUT_PORT in this case it's just GPIOA
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&DATA_OUT_PORT->ODR;
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Disable;
	// HalfWord == 16 bit We are taking over the whole DATA_OUT_PORT
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
	// Normal mode that when the CNDTR reaches zero the DMA transfer stops
	DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
	DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
	DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;

	DMA_Cmd(WIDTH_DMA_CHANNEL, DISABLE);
	DMA_DeInit(WIDTH_DMA_CHANNEL);
	DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)&ones;
	DMA_Init(WIDTH_DMA_CHANNEL, &DMA_InitStructure);

	DMA_Cmd(LONG_DMA_CHANNEL, DISABLE);
	DMA_DeInit(LONG_DMA_CHANNEL);
	DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)&zeros;
	DMA_Init(LONG_DMA_CHANNEL, &DMA_InitStructure);

	// Actual data DMA writer
	DMA_Cmd(SHORT_DMA_CHANNEL, DISABLE);
	DMA_DeInit(SHORT_DMA_CHANNEL);
	DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)source;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_Init(SHORT_DMA_CHANNEL, &DMA_InitStructure);
}

void InitIT(){
	DMA_ITConfig(LONG_DMA_CHANNEL, DMA_IT_TC, ENABLE);

	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel4_5_IRQn;
	NVIC_Init(&NVIC_InitStructure);
}

void InitOutputPins(){
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);

	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;
	GPIO_Init(DATA_OUT_PORT, &GPIO_InitStructure);
}

// Set timer to 800khz frequency pulse on update
// 16,000,000Mhz / 800,000hz = 60 (Period)
// All time keeping is a fraction of that 60 tick clock.
// Empirical evidence shows that these numbers can be shortened and the 800khz frequency can be increased
// but in an effort to insure compatibility with other batches of WS2812s, I've timed everything as close
// to the data sheet as possible.
void InitTimer(){
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

	// 1.25us(1s / 800,000) The Period Timer Update Event signals begin and end of period on DMA1_Channel5
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	TIM_TimeBaseStructure.TIM_Period = 59; // Calculated Period is N - 1 because it is indexed from 0
	TIM_TimeBaseStructure.TIM_Prescaler = 0;
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;

	TIM_TimeBaseInit(PERIOD_TIM, &TIM_TimeBaseStructure);

	TIM_OCInitTypeDef  TIM_OCInitStructure;

	// We are only using the DMA events so Timing mode is used to avoid changing any pin state.
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_Timing;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	// Without interaction from OC3 this will give us a steady state of outputting only 1s on the output pins
	// Every 38/60 of the time period the output pins are brought low to signal a 1
	TIM_OCInitStructure.TIM_Pulse = 38;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OC1Init(PERIOD_TIM, &TIM_OCInitStructure);
	TIM_OC1PreloadConfig(PERIOD_TIM, TIM_OCPreload_Disable);

	// A 0 is indicated by a shorter high period.
	// During a 0 the data line is brought low (18/60) of the time period otherwise via OC1 the dataline is brought
	// low later in the period to signal a 1. Essentially this is the actual data source being clocked in.
	TIM_OCInitStructure.TIM_Pulse = 18;
	TIM_OC3Init(PERIOD_TIM, &TIM_OCInitStructure);
	TIM_OC3PreloadConfig(PERIOD_TIM, TIM_OCPreload_Disable);

	// Enable the DMA events on CC1, CC3 and update to trigger updates on DATA_OUT_PORT
	TIM_DMACmd(PERIOD_TIM, TIM_DMA_Update, ENABLE); // WIDTH_DMA_CHANNEL
	TIM_DMACmd(PERIOD_TIM, TIM_DMA_CC1, ENABLE);    // LONG_DMA_CHANNEL
	TIM_DMACmd(PERIOD_TIM, TIM_DMA_CC3, ENABLE);    // SHORT_DMA_CHANNEL
}
