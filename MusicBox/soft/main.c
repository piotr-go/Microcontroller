/*
  Copyright (C) 2018 Piotr Gozdur <piotr_go>.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/


#include <stdint.h>
#include "inc/stm32f10x.h"
#include "sample.h"
#include "music.h"


uint16_t sBuff[256];
volatile uint8_t sbPtr = 0;

void TIM2_IRQHandler(void){
	TIM2->SR = 0; // clear interrupt flag, koniecznie na początku
	TIM2->CCR1 = sBuff[sbPtr++];
}

int main(void){
	RCC->CR |= ((uint32_t)RCC_CR_HSEON);
	while((RCC->CR & RCC_CR_HSERDY) == 0);
	/* Enable Prefetch Buffer */
	FLASH->ACR |= FLASH_ACR_PRFTBE;
	/* Flash 2 wait state */
	FLASH->ACR &= (uint32_t)((uint32_t)~FLASH_ACR_LATENCY);
	FLASH->ACR |= (uint32_t)FLASH_ACR_LATENCY_2;
	/* HCLK = SYSCLK */
	RCC->CFGR |= (uint32_t)RCC_CFGR_HPRE_DIV1;
	/* PCLK1 = HCLK */
	RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE1_DIV2;
	/* PCLK2 = HCLK */
	RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE2_DIV1;
	/*  PLL configuration: PLLCLK = HSE * 9 = 72 MHz */
	RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL));
	RCC->CFGR |= (uint32_t)(RCC_CFGR_PLLSRC_HSE | RCC_CFGR_PLLMULL9);
	/* Enable PLL */
	RCC->CR |= RCC_CR_PLLON;
	/* Wait till PLL is ready */
	while((RCC->CR & RCC_CR_PLLRDY) == 0){}
	/* Select PLL as system clock source */
	RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_SW));
	RCC->CFGR |= (uint32_t)RCC_CFGR_SW_PLL;
	/* Wait till PLL is used as system clock source */
	while ((RCC->CFGR & (uint32_t)RCC_CFGR_SWS) != (uint32_t)0x08){}


	RCC->APB2ENR |= RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPDEN | RCC_APB2ENR_IOPCEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPAEN;
	AFIO->MAPR = (2<<24);	//JTAG-DP Disabled and SW-DP Enabled


	uint16_t cnt1 = 0;
	uint16_t data;
	uint32_t time = 0;
	uint16_t tone[8] = {0,0,0,0,0,0,0,0};
	uint16_t tCnt[8] = {0,0,0,0,0,0,0,0};
	uint8_t ltCnt[8] = {0,0,0,0,0,0,0,0};
	int8_t sData[8] = {0,0,0,0,0,0,0,0};
	uint16_t sPtr[8] = {0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff};
	uint8_t i, j;
	uint16_t max;
	uint8_t wbPtr = 0;


	// Configure pins
	GPIOA->CRL = 0x4444444A;
	GPIOA->ODR = 0x0001;

	//================

	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;	// enable TIM2 clock
	TIM2->CR1 = 0;

	TIM2->CCMR1 = 0x68;
	TIM2->CCER = 0x03;
	TIM2->CCR1 = 0x0080;

	TIM2->PSC = 1-1;
	TIM2->ARR = 1152-1;
	TIM2->DIER = TIM_DIER_UIE;	// enable update interrupt
	TIM2->CR1 = TIM_CR1_CEN;	// counter enabled
	NVIC_EnableIRQ(TIM2_IRQn);	// Turn on interrupt


	while(1){
		while(1){
			data = music[cnt1];
			if(++cnt1 >= sizeof(music)/2) cnt1 = 0;

			if(data>=0x8000){
				time = data-0x8000;
				time *= 16;
				break;
			}
			else{
				j = 0;
				max = 0;
				for(i=0; i<8; i++){
					if(max < sPtr[i]){
						max = sPtr[i];
						j = i;
					}
				}

				tone[j] = data;
				tCnt[j] = 0;
				ltCnt[j] = 0xff;
				sPtr[j] = 0;
			}
		}

		while(time){
			while(wbPtr == sbPtr){}

			for(i=0; i<8; i++){
				if(sPtr[i] < sizeof(sample)){
					if(ltCnt[i] != (tCnt[i]+tone[i])>>12){
						sData[i] = (int8_t)sample[sPtr[i]]/2;
						sPtr[i]++;
					}
					tCnt[i] += tone[i];
					ltCnt[i] = tCnt[i]>>12;
				}
			}

			sBuff[wbPtr++] = 512 + sData[0] + sData[1] + sData[2] + sData[3] + sData[4] + sData[5] + sData[6] + sData[7];
			time--;
		}
	}
}

