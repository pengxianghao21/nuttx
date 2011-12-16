/****************************************************************************
 * arch/arm/src/stm32/stm32_adc.c
 *
 *   Copyright (C) 2011 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *           Diego Sanchez <dsanchez@nx-engineering.com>
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/
 
/****************************************************************************
 * Included Files
 ****************************************************************************/
 
#include <nuttx/config.h>

#include <stdio.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <semaphore.h>
#include <errno.h>
#include <assert.h>
#include <debug.h>

#include <arch/board/board.h>
#include <nuttx/arch.h>
#include <nuttx/analog/adc.h>

#include "up_internal.h"
#include "up_arch.h"

#include "chip.h"
#include "stm32_internal.h"
#include "stm32_adc.h"

#ifdef CONFIG_ADC

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
/* Configuration ************************************************************/
/* Up to 3 ADC interfaces are supported */

#if STM32_NADC < 3
#  undef CONFIG_STM32_ADC3
#endif

#if STM32_NADC < 2
#  undef CONFIG_STM32_ADC2
#endif

#if STM32_NADC < 1
#  undef CONFIG_STM32_ADC1
#endif

#if defined(CONFIG_STM32_ADC1) || defined(CONFIG_STM32_ADC2) || defined(CONFIG_STM32_ADC3)

/* Timer configuration:  If a timer trigger is specified, then get information
 * about the timer.
 */

#if defined(CONFIG_STM32_TIM1_ADC1)
#    define ADC1_HAVE_TIMER           1
#    define ADC1_EXTSEL_VALUE         ADC_CR2_EXTSEL_T1CC1
#    define ADC1_TIMER_BASE           STM32_TIM1_BASE
#    define ADC1_TIMER_PCLK_FREQUENCY STM32_PCLK2_FREQUENCY
#elif defined(CONFIG_STM32_TIM2_ADC1)
#    define ADC1_HAVE_TIMER           1
#    define ADC1_EXTSEL_VALUE         ADC_CR2_EXTSEL_T2CC2
#    define ADC1_TIMER_BASE           STM32_TIM2_BASE
#    define ADC1_TIMER_PCLK_FREQUENCY STM32_PCLK1_FREQUENCY
#elif defined(CONFIG_STM32_TIM3_ADC1)
#    define ADC1_HAVE_TIMER           1
#    define ADC1_EXTSEL_VALUE         ADC_CR2_EXTSEL_T3CC1
#    define ADC1_TIMER_BASE           STM32_TIM3_BASE
#    define ADC1_TIMER_PCLK_FREQUENCY STM32_PCLK1_FREQUENCY
#elif defined(CONFIG_STM32_TIM4_ADC1)
#    define ADC1_HAVE_TIMER           1
#    define ADC1_EXTSEL_VALUE         ADC_CR2_EXTSEL_T4CC4
#    define ADC1_TIMER_BASE           STM32_TIM4_BASE
#    define ADC1_TIMER_PCLK_FREQUENCY STM32_PCLK1_FREQUENCY
#elif defined(CONFIG_STM32_TIM5_ADC1)
#    define ADC1_HAVE_TIMER           1
#    define ADC1_EXTSEL_VALUE         ADC_CR2_EXTSEL_T5CC1
#    define ADC1_TIMER_BASE           STM32_TIM5_BASE
#    define ADC1_TIMER_PCLK_FREQUENCY STM32_PCLK1_FREQUENCY
#elif defined(CONFIG_STM32_TIM8_ADC1)
#    define ADC1_HAVE_TIMER           1
#    define ADC1_EXTSEL_VALUE         ??? which ????
#    define ADC1_TIMER_BASE           STM32_TIM8_BASE
#    define ADC1_TIMER_PCLK_FREQUENCY STM32_PCLK2_FREQUENCY
#else
#    undef  ADC1_HAVE_TIMER
#endif

#if defined(ADC1_HAVE_TIMER) && !defined(CONFIG_STM32_ADC1_SAMPLE_FREQUENCY)
#  error "CONFIG_STM32_ADC1_SAMPLE_FREQUENCY not defined"
#endif

#if defined(CONFIG_STM32_TIM1_ADC2)
#    define ADC2_HAVE_TIMER           1
#    define ADC2_EXTSEL_VALUE         ADC_CR2_EXTSEL_T1CC1
#    define ADC2_TIMER_BASE           STM32_TIM1_BASE
#    define ADC2_TIMER_PCLK_FREQUENCY STM32_PCLK2_FREQUENCY
#elif defined(CONFIG_STM32_TIM2_ADC2)
#    define ADC2_HAVE_TIMER           1
#    define ADC2_EXTSEL_VALUE         ADC_CR2_EXTSEL_T2CC2
#    define ADC2_TIMER_BASE           STM32_TIM2_BASE
#    define ADC2_TIMER_PCLK_FREQUENCY STM32_PCLK1_FREQUENCY
#elif defined(CONFIG_STM32_TIM3_ADC2)
#    define ADC2_HAVE_TIMER           1
#    define ADC2_EXTSEL_VALUE         ADC_CR2_EXTSEL_T3CC1
#    define ADC2_TIMER_BASE           STM32_TIM3_BASE
#    define ADC2_TIMER_PCLK_FREQUENCY STM32_PCLK1_FREQUENCY
#elif defined(CONFIG_STM32_TIM4_ADC2)
#    define ADC2_HAVE_TIMER           1
#    define ADC2_EXTSEL_VALUE         ADC_CR2_EXTSEL_T4CC4
#    define ADC2_TIMER_BASE           STM32_TIM4_BASE
#    define ADC2_TIMER_PCLK_FREQUENCY STM32_PCLK1_FREQUENCY
#elif defined(CONFIG_STM32_TIM5_ADC2)
#    define ADC2_HAVE_TIMER           1
#    define ADC2_EXTSEL_VALUE         ADC_CR2_EXTSEL_T5CC1
#    define ADC2_TIMER_BASE           STM32_TIM5_BASE
#    define ADC2_TIMER_PCLK_FREQUENCY STM32_PCLK1_FREQUENCY
#elif defined(CONFIG_STM32_TIM8_ADC2)
#    define ADC2_HAVE_TIMER           1
#    define ADC2_EXTSEL_VALUE         ADC_CR2_EXTSEL_T8CC1
#    define ADC2_TIMER_BASE           STM32_TIM8_BASE
#    define ADC2_TIMER_PCLK_FREQUENCY STM32_PCLK2_FREQUENCY
#else
#    undef  ADC2_HAVE_TIMER
#endif

#if defined(ADC2_HAVE_TIMER) && !defined(CONFIG_STM32_ADC2_SAMPLE_FREQUENCY)
#  error "CONFIG_STM32_ADC2_SAMPLE_FREQUENCY not defined"
#endif

#if defined(CONFIG_STM32_TIM1_ADC3)
#    define ADC3_HAVE_TIMER           1
#    define ADC3_EXTSEL_VALUE         ADC_CR2_EXTSEL_T1CC1
#    define ADC3_TIMER_BASE           STM32_TIM1_BASE
#    define ADC3_TIMER_PCLK_FREQUENCY STM32_PCLK2_FREQUENCY
#elif defined(CONFIG_STM32_TIM2_ADC3)
#    define ADC3_HAVE_TIMER           1
#    define ADC3_EXTSEL_VALUE         ADC_CR2_EXTSEL_T2CC2
#    define ADC3_TIMER_BASE           STM32_TIM2_BASE
#    define ADC3_TIMER_PCLK_FREQUENCY STM32_PCLK1_FREQUENCY
#elif defined(CONFIG_STM32_TIM3_ADC3)
#    define ADC3_HAVE_TIMER           1
#    define ADC3_EXTSEL_VALUE         ADC_CR2_EXTSEL_T3CC1
#    define ADC3_TIMER_BASE           STM32_TIM3_BASE
#    define ADC3_TIMER_PCLK_FREQUENCY STM32_PCLK1_FREQUENCY
#elif defined(CONFIG_STM32_TIM4_ADC3)
#    define ADC3_HAVE_TIMER           1
#    define ADC3_EXTSEL_VALUE         ADC_CR2_EXTSEL_T4CC4
#    define ADC3_TIMER_BASE           STM32_TIM4_BASE
#    define ADC3_TIMER_PCLK_FREQUENCY STM32_PCLK1_FREQUENCY
#elif defined(CONFIG_STM32_TIM5_ADC3)
#    define ADC3_HAVE_TIMER           1
#    define ADC3_EXTSEL_VALUE         ADC_CR2_EXTSEL_T5CC1
#    define ADC3_TIMER_BASE           STM32_TIM5_BASE
#    define ADC3_TIMER_PCLK_FREQUENCY STM32_PCLK1_FREQUENCY
#elif defined(CONFIG_STM32_TIM8_ADC3)
#    define ADC3_HAVE_TIMER           1
#    define ADC3_EXTSEL_VALUE         ADC_CR2_EXTSEL_T8CC1
#    define ADC3_TIMER_BASE           STM32_TIM8_BASE
#    define ADC3_TIMER_PCLK_FREQUENCY STM32_PCLK2_FREQUENCY
#else
#    undef  ADC3_HAVE_TIMER
#endif

#if defined(ADC3_HAVE_TIMER) && !defined(CONFIG_STM32_ADC3_SAMPLE_FREQUENCY)
#  error "CONFIG_STM32_ADC3_SAMPLE_FREQUENCY not defined"
#endif

#if defined(ADC1_HAVE_TIMER) || defined(ADC2_HAVE_TIMER) || defined(ADC3_HAVE_TIMER)
#  define ADC_HAVE_TIMER 1
#else
#  undef ADC_HAVE_TIMER
#endif

/* ADC interrupts ***********************************************************/

#ifdef CONFIG_STM32_STM32F10XX
#  define ADC_SR_ALLINTS (ADC_SR_AWD | ADC_SR_EOC | ADC_SR_JEOC)
#else
#  define ADC_SR_ALLINTS (ADC_SR_AWD | ADC_SR_EOC | ADC_SR_JEOC | ADC_SR_OVR)
#endif

#ifdef CONFIG_STM32_STM32F10XX
#  define ADC_CR1_ALLINTS (ADC_CR1_AWDIE | ADC_CR1_EOCIE | ADC_CR1_JEOCIE)
#else
#  define ADC_CR1_ALLINTS (ADC_CR1_AWDIE | ADC_CR1_EOCIE | ADC_CR1_JEOCIE | ADC_CR1_OVRIE)
#endif

/* Timer Setup **************************************************************/
/* Calculate timer divider values based upon ADCn_TIMER_PCLK_FREQUENCY and
 * CONFIG_STM32_ADCn_SAMPLE_FREQUENCY.
 */

#ifdef ADC1_HAVE_TIMER
#  warning "Missing Logic"
#endif

#ifdef ADC2_HAVE_TIMER
#  warning "Missing Logic"
#endif

#ifdef ADC3_HAVE_TIMER
#  warning "Missing Logic"
#endif

/* The maximum number of channels that can be sampled */

#define ADC_MAX_SAMPLES 16

/****************************************************************************
 * Private Types
 ****************************************************************************/
 
/* This structure describes the state of one ADC block */

struct stm32_dev_s
{
  uint8_t  irq;       /* Interrupt generated by this ADC block */
  uint8_t  nchannels; /* Number of channels */
  uint8_t  intf;      /* ADC interface number */
  uint8_t  current;   /* Current ADC channel being converted */
  xcpt_t   isr;       /* Interrupt handler for this ADC block */
  uint32_t base;      /* Base address of registers unique to this ADC block */
#ifdef ADC_HAVE_TIMER
  uint32_t tbase;     /* Base address of timer used by this ADC block */
  uint32_t extsel;    /* EXTSEL value used by this ADC block */
  uint32_t pclck;     /* The PCLK frequency that drivers this timer */
#endif

  uint8_t  chanlist[ADC_MAX_SAMPLES];
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/* ADC Register access */

static uint32_t adc_getreg(struct stm32_dev_s *priv, int offset);
static void adc_putreg(struct stm32_dev_s *priv, int offset, uint32_t value);
static void adc_rccreset(struct stm32_dev_s *priv, bool reset);

/* ADC Interrupt Handler */

static int adc_interrupt(FAR struct adc_dev_s *dev);
#if defined(CONFIG_STM32_STM32F10XX) && (defined(CONFIG_STM32_ADC1) || defined(CONFIG_STM32_ADC2))
static int adc12_interrupt(int irq, void *context);
#endif
#if defined(CONFIG_STM32_STM32F10XX) && defined (CONFIG_STM32_ADC3)
static int adc3_interrupt(int irq, void *context);
#endif
#ifdef CONFIG_STM32_STM32F40XX
static int adc123_interrupt(int irq, void *context);
#endif

/* ADC Driver Methods */

static void adc_reset(FAR struct adc_dev_s *dev);
static int  adc_setup(FAR struct adc_dev_s *dev);
static void adc_shutdown(FAR struct adc_dev_s *dev);
static void adc_rxint(FAR struct adc_dev_s *dev, bool enable);
static int  adc_ioctl(FAR struct adc_dev_s *dev, int cmd, unsigned long arg);
static void adc_enable(FAR struct stm32_dev_s *priv, bool enable);

#ifdef ADC_HAVE_TIMER
static int  adc_timinit(FAR struct stm32_dev_s *priv);
#endif
static void adc_startconv(FAR struct stm32_dev_s *priv, bool enable);

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* ADC interface operations */

static const struct adc_ops_s g_adcops =
{
  .ao_reset    = adc_reset,
  .ao_setup    = adc_setup,
  .ao_shutdown = adc_shutdown,
  .ao_rxint    = adc_rxint,
  .ao_ioctl    = adc_ioctl,
};

/* ADC1 state */

#ifdef CONFIG_STM32_ADC1
static struct stm32_dev_s g_adcpriv1 =
{
#ifdef CONFIG_STM32_STM32F10XX
  .irq         = STM32_IRQ_ADC12,
  .isr         = adc12_interrupt,
#else
  .irq         = STM32_IRQ_ADC,
  .isr         = adc123_interrupt,
#endif
  .intf        = 1,
  .base        = STM32_ADC1_BASE,
#ifdef ADC1_HAVE_TIMER
  .tbase       = ADC1_TIMER_BASE,
  .extsel      = ADC1_EXTSEL_VALUE,
  .pclck       = ADC1_TIMER_PCLK_FREQUENCY,
#endif
};

static struct adc_dev_s g_adcdev1 =
{
  .ad_ops = &g_adcops,
  .ad_priv= &g_adcpriv1,
};
#endif

/* ADC2 state */

#ifdef CONFIG_STM32_ADC2
static struct stm32_dev_s g_adcpriv2 =
{
#ifdef CONFIG_STM32_STM32F10XX
  .irq         = STM32_IRQ_ADC12,
  .isr         = adc12_interrupt,
#else
  .irq         = STM32_IRQ_ADC,
  .isr         = adc123_interrupt,
#endif
  .intf        = 2;
  .base        = STM32_ADC2_BASE,
#ifdef ADC2_HAVE_TIMER
  .tbase       = ADC2_TIMER_BASE,
  .extsel      = ADC2_EXTSEL_VALUE,
  .pclck       = ADC2_TIMER_PCLK_FREQUENCY,
#endif
};

static struct adc_dev_s g_adcdev2 =
{
  .ad_ops = &g_adcops,
  .ad_priv= &g_adcpriv2,
};
#endif

/* ADC3 state */

#ifdef CONFIG_STM32_ADC3
static struct stm32_dev_s g_adcpriv3 =
{
#ifdef CONFIG_STM32_STM32F10XX
  .irq         = STM32_IRQ_ADC3,
  .isr         = adc3_interrupt,
#else
  .irq         = STM32_IRQ_ADC,
  .isr         = adc123_interrupt,
#endif
  .intf        = 3;
  .base        = STM32_ADC3_BASE,
#ifdef ADC3_HAVE_TIMER
  .tbase       = ADC3_TIMER_BASE,
  .extsel      = ADC3_EXTSEL_VALUE,
  .pclck       = ADC3_TIMER_PCLK_FREQUENCY,
#endif
};

static struct adc_dev_s g_adcdev3 =
{
  .ad_ops = &g_adcops,
  .ad_priv= &g_adcpriv3,
};
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: adc_getreg
 *
 * Description:
 *   Read the value of an ADC register.
 *
 * Input Parameters:
 *   priv - A reference to the ADC block status
 *   offset - The offset to the register to read
 *
 * Returned Value:
 *
 ****************************************************************************/

static uint32_t adc_getreg(struct stm32_dev_s *priv, int offset)
{
  return getreg32(priv->base + offset);
}

/****************************************************************************
 * Name: adc_getreg
 *
 * Description:
 *   Read the value of an ADC register.
 *
 * Input Parameters:
 *   priv - A reference to the ADC block status
 *   offset - The offset to the register to read
 *
 * Returned Value:
 *
 ****************************************************************************/

static void adc_putreg(struct stm32_dev_s *priv, int offset, uint32_t value)
{
  putreg32(value, priv->base + offset);
}

/****************************************************************************
 * Name: tim_getreg
 *
 * Description:
 *   Read the value of an ADC timer register.
 *
 * Input Parameters:
 *   priv - A reference to the ADC block status
 *   offset - The offset to the register to read
 *
 * Returned Value:
 *   The current contents of the specified register
 *
 ****************************************************************************/

#ifdef HAVE_DMA
static uint32_t tim_getreg(struct stm32_dev_s *priv, int offset)
{
  return getreg32(priv->tbase + offset);
}
#endif

/****************************************************************************
 * Name: tim_putreg
 *
 * Description:
 *   Read the value of an ADC timer register.
 *
 * Input Parameters:
 *   priv - A reference to the ADC block status
 *   offset - The offset to the register to read
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

static void tim_putreg(struct stm32_dev_s *priv, int offset, uint32_t value)
{
  putreg32(value, priv->tbase + offset);
}
#endif

/****************************************************************************
 * Name: adc_timinit
 *
 * Description:
 *   Initialize the timer that drivers the ADC sampling for this channel using
 *   the pre-calculated timer divider definitions.
 *
 * Input Parameters:
 *   chan - A reference to the DAC channel state data
 *
 * Returned Value:
 *   Zero on success; a negated errno value on failure.
 *
 ****************************************************************************/

#ifdef ADC_HAVE_TIMER
static int adc_timinit(FAR struct stm32_dev_s *priv)
{
  /* Configure the time base: Timer period, prescaler, clock division,
   * counter mode (up).
   */
#warning "Missing Logic"

  /* Selection EXTSEL selection: update */
#warning "Missing Logic"
  
  /* Enable the counter */
#warning "Missing Logic"
}
#endif

/****************************************************************************
 * Name: adc_startconv
 *
 * Description:
 *   Start (or stop) the ADC conversion process
 *
 * Input Parameters:
 *   priv - A reference to the ADC block status
 *   enable - True: Start conversion
 *
 * Returned Value:
 *
 ****************************************************************************/

static void adc_startconv(struct stm32_dev_s *priv, bool enable)
{
  uint32_t regval;

  avdbg("enable: %d\n", enable);

  regval  = adc_getreg(priv, STM32_ADC_CR2_OFFSET);
  if (enable)
    {
      /* Start conversion of regular channles */

      regval |= ADC_CR2_SWSTART;
    }
  else
    {
      /* Disable the conversion of regular channels */

      regval &= ~ADC_CR2_SWSTART;
    }
  regval  = adc_getreg(priv, STM32_ADC_CR2_OFFSET);
}

/****************************************************************************
 * Name: adc_rccreset
 *
 * Description:
 *   Deinitializes the ADCx peripheral registers to their default 
 *   reset values. It could set all the ADCs configured.
 *
 * Input Parameters:
 *   regaddr - The register to read
 *   reset - Condition, set or reset
 *
 * Returned Value:
 *
 ****************************************************************************/

static void adc_rccreset(struct stm32_dev_s *priv, bool reset)
{
  uint32_t regval;
  uint32_t adcbit;

  /* Pick the appropriate bit in the APB2 reset register */

#ifdef CONFIG_STM32_STM32F10XX
  /* For the STM32 F1, there is an individual bit to reset each ADC. */

  switch (priv->intf)
    {
#ifdef CONFIG_STM32_ADC1
      case 1:
        adcbit = RCC_APB2RSTR_ADC1RST;
        break;
#endif
#ifdef CONFIG_STM32_ADC2
      case 2:
        adcbit = RCC_APB2RSTR_ADC2RST;
        break;
#endif
#ifdef CONFIG_STM32_ADC3
      case 3:
        adcbit = RCC_APB2RSTR_ADC3RST;
        break;
#endif
      default:
        return;
    }

#else
  /* For the STM32 F4, there is one common reset for all ADC block.
   * THIS will probably cause some problems!
   */

  adcbit = RCC_APB2RSTR_ADCRST;
#endif

  /* Set or clear the selected bit in the APB2 reset register */

  regval = getreg32(STM32_RCC_APB2RSTR);
  if (reset)
    {
      /* Enable  ADC reset state */

      regval |= adcbit;
    }
  else
    {
      /* Release ADC from reset state */

      regval &= ~adcbit;
    }
  putreg32(regval, STM32_RCC_APB2RSTR);
}

/*******************************************************************************
 * Name: adc_enable
 *
 * Description    : Enables or disables the specified ADC peripheral.
 *
 * Input Parameters:
 *
 *   enable - true:  enable ADC conversion
 *            false: disable ADC conversion
 *
 * Returned Value:
 *
 *******************************************************************************/

static void adc_enable(FAR struct stm32_dev_s *priv, bool enable)
{
  uint32_t regval;

  avdbg("enable: %d\n", enable);

  regval  = adc_getreg(priv, STM32_ADC_CR2_OFFSET);
  if (enable)
    {
      regval |= ADC_CR2_ADON;
    }
  else
    {
      regval &= ~ADC_CR2_ADON;
    }
  adc_putreg(priv, STM32_ADC_CR2_OFFSET, regval);

  /* Enable or disable conversions */

//adc_startconv(priv, enable);
}

/****************************************************************************
 * Name: adc_reset
 *
 * Description:
 *   Reset the ADC device.  Called early to initialize the hardware. This
 *   is called, before adc_setup() and on error conditions.
 *
 * Input Parameters:
 *
 * Returned Value:
 *
 ****************************************************************************/

static void adc_reset(FAR struct adc_dev_s *dev)
{
  FAR struct stm32_dev_s *priv = (FAR struct stm32_dev_s *)dev->ad_priv;
  irqstate_t flags;
  uint32_t regval;
  int offset;
  int i;

  avdbg("intf: %d\n", priv->intf);
  flags = irqsave();

  /* Enable  ADC reset state */

  adc_rccreset(priv, true);

  /* Release ADC from reset state */

  adc_rccreset(priv, false);
  
  /* Initialize the ADC data structures */

  /* Initialize the watchdog high threshold register */

  adc_putreg(priv, STM32_ADC_HTR_OFFSET, 0x00000fff);

  /* Initialize the watchdog low threshold register */

  adc_putreg(priv, STM32_ADC_LTR_OFFSET, 0x00000000);

#ifdef CONFIG_STM32_STM32F40XX  
  /* Initialize ADC Prescaler*/

  regval = getreg32(STM32_ADC_CCR_OFFSET);

  /* PCLK2 divided by 2 */

  regval &= ~ADC_CCR_ADCPRE_MASK;
  putreg32(regval,STM32_ADC_CCR_OFFSET);
#endif

  /* Initialize the same sample time for each ADC 55.5 cycles
   *
   * During sample cycles channel selection bits must remain unchanged.
   *
   *   000:   1.5 cycles
   *   001:   7.5 cycles
   *   010:  13.5 cycles
   *   011:  28.5 cycles
   *   100:  41.5 cycles
   *   101:  55.5 cycles
   *   110:  71.5 cycles
   *   111: 239.5 cycles
   */

  adc_putreg(priv, STM32_ADC_SMPR1_OFFSET, 0x00b6db6d);
  adc_putreg(priv, STM32_ADC_SMPR2_OFFSET, 0x00b6db6d);
  
  /* ADC CR1 Configuration */

  regval  = adc_getreg(priv, STM32_ADC_CR1_OFFSET);
  regval &= ~ADC_CR1_DUALMOD_MASK;
  regval &= ~ADC_CR1_SCAN;   /* Clear DUALMODE and SCAN bits */
  adc_putreg(priv, STM32_ADC_CR1_OFFSET, regval);

  /* Initialize the ADC_Mode (ADC_Mode_Independent)  */

  regval  = adc_getreg(priv, STM32_ADC_CR1_OFFSET);
  regval |= ADC_CR1_IND;

  /* Initialize the ADC_CR1_SCAN member DISABLE */

  regval &= ~ADC_CR1_SCAN;
  
  /* Initialize the Analog watchdog enable */

  regval |= ADC_CR1_AWDEN;

  /* AWDIE: Analog watchdog interrupt enable */

  regval |= ADC_CR1_AWDIE;

  /* EOCIE: Interrupt enable for EOC */
  
  regval |= ADC_CR1_EOCIE;

  adc_putreg(priv, STM32_ADC_CR1_OFFSET, regval);

  /* ADC1 CR2 Configuration */

  regval  = adc_getreg(priv, STM32_ADC_CR2_OFFSET);

  /* Clear CONT, ALIGN (Right = 0) and EXTTRIG bits */

  regval &= ~ADC_CR2_CONT;
  regval &= ~ADC_CR2_ALIGN;
  regval &= ~ADC_CR2_EXTSEL_MASK;

  /* EXTTRIG: External Trigger Conversion mode for regular channels enable*/
  
  //regval |= ADC_CR2_EXTTRIG;
  
  /* EXTSEL[2:0]: External event select for regular group
   * These bits select the external event used to trigger the start
   * of conversion of a regular group: 
   *    000: Timer 1 CC1 event
   *    001: Timer 1 CC2 event
   *    010: Timer 1 CC3 event
   *    011: Timer 2 CC2 event
   *    100: Timer 3 TRGO event
   *    101: Timer 4 CC4 event
   *    110: EXTI line11/TIM8_TRGO event (TIM8_TRGO is available only in high-density devices)
   *    111: SWSTART
   */

  /* Select trigger when SWSTART is set */
  //regval |= ADC_CR2_EXTSEL_SWSTART;

  adc_putreg(priv, STM32_ADC_CR2_OFFSET, regval);

  /* Configuration of the channel conversions */

  regval = adc_getreg(priv, STM32_ADC_SQR3_OFFSET) & ADC_SQR3_RESERVED;
  for (i = 0, offset = 0; i < priv->nchannels && i < 6; i++, offset += 5)
    {
      regval |= (uint32_t)priv->chanlist[i] << offset;
    }
  adc_putreg(priv, STM32_ADC_SQR3_OFFSET, regval);

  regval = adc_getreg(priv, STM32_ADC_SQR2_OFFSET) & ADC_SQR2_RESERVED;
  for (i = 6, offset = 0; i < priv->nchannels && i < 12; i++, offset += 5)
    {
      regval |= (uint32_t)priv->chanlist[i] << offset;
    }
  adc_putreg(priv, STM32_ADC_SQR2_OFFSET, regval);

  regval = adc_getreg(priv, STM32_ADC_SQR1_OFFSET) & ADC_SQR1_RESERVED;
  for (i = 12, offset = 0; i < priv->nchannels && i < 16; i++, offset += 5)
    {
      regval |= (uint32_t)priv->chanlist[i] << offset;
    }

  /* Set the number of conversions */

  DEBUGASSERT(priv->nchannels <= 16);

  regval |= ((uint32_t)priv->nchannels << ADC_SQR1_L_SHIFT);
  adc_putreg(priv, STM32_ADC_SQR1_OFFSET, regval);

  /* Set the channel index of the first conversion */

  priv->current = 0;

  /* Set ADON to wake up the ADC from Power Down state. */
  
  adc_enable(priv, true);
  adc_startconv(priv, true);
  irqrestore(flags);
  
  avdbg("SR: %08x CR1: 0x%08x  CR2: 0x%08x\n",
        adc_getreg(priv, STM32_ADC_SR_OFFSET),
        adc_getreg(priv, STM32_ADC_CR1_OFFSET),
        adc_getreg(priv, STM32_ADC_CR2_OFFSET));
  avdbg("SQR1: 0x%08x  SQR2: 0x%08x SQR3: 0x%08x\n",
        adc_getreg(priv, STM32_ADC_SQR1_OFFSET),
        adc_getreg(priv, STM32_ADC_SQR2_OFFSET),
        adc_getreg(priv, STM32_ADC_SQR3_OFFSET));
}

/****************************************************************************
 * Name: adc_setup
 *
 * Description:
 *   Configure the ADC. This method is called the first time that the ADC
 *   device is opened.  This will occur when the port is first opened.
 *   This setup includes configuring and attaching ADC interrupts.  Interrupts
 *   are all disabled upon return.
 *
 * Input Parameters:
 *
 * Returned Value:
 *
 ****************************************************************************/

static int adc_setup(FAR struct adc_dev_s *dev)
{
  FAR struct stm32_dev_s *priv = (FAR struct stm32_dev_s *)dev->ad_priv;
  int ret;

  avdbg("intf: %d\n", priv->intf);

  /* Attach the ADC interrupt */

  ret = irq_attach(priv->irq, priv->isr);
  if (ret == OK)
    {
      /* Enable the ADC interrupt */

      up_enable_irq(priv->irq);
    }

  return ret;
}

/****************************************************************************
 * Name: adc_shutdown
 *
 * Description:
 *   Disable the ADC.  This method is called when the ADC device is closed.
 *   This method reverses the operation the setup method.
 *
 * Input Parameters:
 *
 * Returned Value:
 *
 ****************************************************************************/

static void adc_shutdown(FAR struct adc_dev_s *dev)
{
  FAR struct stm32_dev_s *priv = (FAR struct stm32_dev_s *)dev->ad_priv;

  avdbg("intf: %d\n", priv->intf);

  /* Disable ADC interrupts and detach the ADC interrupt handler */

  up_disable_irq(priv->irq);
  irq_detach(priv->irq);

  /* Disable and reset the ADC module */
}

/****************************************************************************
 * Name: adc_rxint
 *
 * Description:
 *   Call to enable or disable RX interrupts.
 *
 * Input Parameters:
 *
 * Returned Value:
 *
 ****************************************************************************/

static void adc_rxint(FAR struct adc_dev_s *dev, bool enable)
{
  FAR struct stm32_dev_s *priv = (FAR struct stm32_dev_s *)dev->ad_priv;
  uint32_t regval;

  avdbg("intf: %d enable: %d\n", priv->intf, enable);

  regval = adc_getreg(priv, STM32_ADC_CR1_OFFSET);
  if (enable)
    {
      /* Enable the end-of-conversion ADC and analog watchdog interrupts */

      regval |= (ADC_CR1_EOCIE | ADC_CR1_AWDIE);
    }
  else
    {
      /* Disable all ADC interrupts */

      regval &= ~ADC_CR1_ALLINTS;
    }
  adc_putreg(priv, STM32_ADC_CR1_OFFSET, regval);
}

/****************************************************************************
 * Name: adc_ioctl
 *
 * Description:
 *   All ioctl calls will be routed through this method.
 *
 * Input Parameters:
 *
 * Returned Value:
 *
 ****************************************************************************/

static int  adc_ioctl(FAR struct adc_dev_s *dev, int cmd, unsigned long arg)
{
  avdbg("Entry\n");
  return -ENOTTY;
}

/****************************************************************************
 * Name: adc_interrupt
 *
 * Description:
 *   Common ADC interrupt handler.
 *
 * Input Parameters:
 *
 * Returned Value:
 *
 ****************************************************************************/

static int adc_interrupt(FAR struct adc_dev_s *dev)
{
  FAR struct stm32_dev_s *priv = (FAR struct stm32_dev_s *)dev->ad_priv;
  uint32_t adcsr;
  int32_t  value;

  avdbg("intf: %d\n", priv->intf);

  /* Identifies the interruption AWD or EOC */
  
  adcsr = adc_getreg(priv, STM32_ADC_SR_OFFSET);
  if ((adcsr & ADC_SR_AWD) != 0)
    {
      adbg(" Analog Watchdog, Value converted out of range!\n");
    }
  
  /* EOC: End of conversion */

  if ((adcsr & ADC_SR_EOC) != 0)
    {
      /* Read the converted value */

      value  = adc_getreg(priv, STM32_ADC_DR_OFFSET);
      value &= ADC_DR_DATA_MASK;
#ifdef ADC_DUALMODE
#error "not yet implemented"
      value &= ADC_DR_ADC2DATA_MASK;
#endif
    
      /* Give the ADC data to the ADC dirver.  adc_receive accepts 3 parameters:
       *
       * 1) The first is the ADC device instance for this ADC block.
       * 2) The second is the channel number for the data, and
       * 3) The third is the converted data for the channel.
       */

      avdbg("Calling adc_receive(priv, ch=%d, value=%d)\n",
            priv->chanlist[priv->current], value);

      adc_receive(dev, priv->chanlist[priv->current], value);
  
      /* Set the channel number of the next channel that will complete conversion */

      if (++priv->current >= priv->nchannels)
        {
          /* Restart the conversion sequence from the beginning */
#warning "Missing logic"
         
          /* Reset the index to the first channel to be converted */
        
          priv->current = 0;
        }
    }

  return OK;
}

/****************************************************************************
 * Name: adc12_interrupt
 *
 * Description:
 *   ADC12 interrupt handler for the STM32 F1 family.
 *
 * Input Parameters:
 *
 * Returned Value:
 *
 ****************************************************************************/

#if defined(CONFIG_STM32_STM32F10XX) && (defined(CONFIG_STM32_ADC1) || defined(CONFIG_STM32_ADC2))
static int adc12_interrupt(int irq, void *context)
{
  uint32_t regval;
  uint32_t pending;

  avdbg("irq: %d\n", irq);

  /* Check for pending ADC1 interrupts */

#ifdef CONFIG_STM32_ADC1
  regval  = getreg32(STM32_ADC1_SR);
  pending = regval & ADC_SR_ALLINTS;
  if (pending != 0)
    {
      adc_interrupt(&g_adcdev1);
      regval &= ~pending;
      putreg32(regval, STM32_ADC1_SR);
    }
#endif

  /* Check for pending ADC2 interrupts */

#ifdef CONFIG_STM32_ADC2
  regval  = getreg32(STM32_ADC2_SR);
  pending = regval & ADC_SR_ALLINTS;
  if (pending != 0)
    {
      adc_interrupt(&g_adcdev2);
      regval &= ~pending;
      putreg32(regval, STM32_ADC2_SR);
    }
#endif
  return OK;
}
#endif

/****************************************************************************
 * Name: adc3_interrupt
 *
 * Description:
 *   ADC1/2 interrupt handler for the STM32 F1 family.
 *
 * Input Parameters:
 *
 * Returned Value:
 *
 ****************************************************************************/

#if defined (CONFIG_STM32_STM32F10XX) && defined (CONFIG_STM32_ADC3)
static int adc3_interrupt(int irq, void *context)
{
  uint32_t regval;
  uint32_t pending;

  avdbg("irq: %d\n", irq);

  /* Check for pending ADC3 interrupts */

  regval  = getreg32(STM32_ADC3_SR);
  pending = regval & ADC_SR_ALLINTS;
  if (pending != 0)
    {
      adc_interrupt(&g_adcdev3);
      regval &= ~pending;
      putreg32(regval, STM32_ADC3_SR);
    }

  return OK;
}
#endif

/****************************************************************************
 * Name: adc123_interrupt
 *
 * Description:
 *   ADC1/2/3 interrupt handler for the STM32 F4 family.
 *
 * Input Parameters:
 *
 * Returned Value:
 *
 ****************************************************************************/

#ifdef CONFIG_STM32_STM32F40XX
static int adc123_interrupt(int irq, void *context)
{
  uint32_t regval;
  uint32_t pending;

  avdbg("irq: %d\n", irq);

  /* Check for pending ADC1 interrupts */

#ifdef CONFIG_STM32_ADC1
  regval  = getreg32(STM32_ADC1_SR);
  pending = regval & ADC_SR_ALLINTS;
  if (pending != 0)
    {
      adc_interrupt(&g_adcdev1);
      regval &= ~pending;
      putreg32(regval, STM32_ADC1_SR);
    }
#endif

  /* Check for pending ADC2 interrupts */

#ifdef CONFIG_STM32_ADC2
  regval = getreg32(STM32_ADC2_SR);
  pending = regval & ADC_SR_ALLINTS;
  if (pending != 0)
    {
      adc_interrupt(&g_adcdev2);
      regval &= ~pending;
      putreg32(regval, STM32_ADC2_SR);
    }
#endif

  /* Check for pending ADC3 interrupts */

#ifdef CONFIG_STM32_ADC3
  regval = getreg32(STM32_ADC3_SR);
  pending = regval & ADC_SR_ALLINTS;
  if (pending != 0)
    {
      adc_interrupt(&g_adcdev3);
      regval &= ~pending;
      putreg32(regval, STM32_ADC3_SR);
    }
#endif
  return OK;
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: stm32_adcinitialize
 *
 * Description:
 *   Initialize the ADC.
 *
 *   The logic is, save nchannels : # of channels (conversions) in ADC_SQR1_L
 *   Then, take the chanlist array and store it in the SQR Regs, 
 *     chanlist[0] -> ADC_SQR3_SQ1
 *     chanlist[1] -> ADC_SQR3_SQ2
 *     chanlist[2] -> ADC_SQR3_SQ3
 *     chanlist[3] -> ADC_SQR3_SQ4
 *     chanlist[4] -> ADC_SQR3_SQ5
 *     chanlist[5] -> ADC_SQR3_SQ6
 *     ...
 *     chanlist[15]-> ADC_SQR1_SQ16
 *
 *   up to
 *     chanlist[nchannels]
 *
 * Input Parameters:
 *   intf      - Could be {1,2,3} for ADC1, ADC2, or ADC3
 *   chanlist  - The list of channels
 *   nchannels - Number of channels
 *
 * Returned Value:
 *   Valid ADC device structure reference on succcess; a NULL on failure
 *
 ****************************************************************************/

struct adc_dev_s *stm32_adcinitialize(int intf, const uint8_t *chanlist, int nchannels)
{
  FAR struct adc_dev_s   *dev;
  FAR struct stm32_dev_s *priv;
  
  avdbg("intf: %d nchannels: %d\n", intf, nchannels);

#ifdef CONFIG_STM32_ADC1
  if (intf == 1)
    {
      adbg("ADC1 Selected\n");
      dev = &g_adcdev1;
    }
  else
#endif
#ifdef CONFIG_STM32_ADC2
  if (intf == 2)
    {
      adbg("ADC2 Selected\n");
      dev = &g_adcdev2;
    }
  else
#endif
#ifdef CONFIG_STM32_ADC3
  if (intf == 3)
    {
      adbg("ADC3 Selected\n");
      dev = &g_adcdev3;
    }
  else
#endif
    {
      adbg("No ADC interface defined\n");
      return NULL;
    }

  /* Configure the selected ADC */

  priv = dev->ad_priv;
  priv->nchannels = nchannels;
  memcpy(priv->chanlist, chanlist, nchannels);
  return dev;
}

#endif /* CONFIG_STM32_ADC || CONFIG_STM32_ADC2 || CONFIG_STM32_ADC3 */
#endif /* CONFIG_ADC */

