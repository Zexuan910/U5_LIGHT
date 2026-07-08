#include "touch_controller.h"

#include "cst816t.h"

#define TOUCH_CONTROLLER_FRAME_MS        16UL
#define TOUCH_CONTROLLER_RELEASE_MISSES  3U

static TouchControllerSample touch_current = {0U, 0U, 0U};
static uint32_t touch_next_sample_ms = 0U;
static volatile UBYTE touch_irq_pending = 0U;
static UBYTE touch_release_misses = 0U;

void TouchController_Init(void)
{
  CST816T_Init();
  touch_current.pressed = 0U;
  touch_current.x = 0U;
  touch_current.y = 0U;
  touch_next_sample_ms = 0U;
  touch_irq_pending = 0U;
  touch_release_misses = 0U;
}

void TouchController_NotifyInterrupt(void)
{
  touch_irq_pending = 1U;
}

TouchControllerSample TouchController_Sample(uint32_t now_ms)
{
  UWORD x = 0U;
  UWORD y = 0U;
  UBYTE irq_pending;
  UBYTE int_low;

  if ((touch_next_sample_ms != 0U) && ((int32_t)(now_ms - touch_next_sample_ms) < 0))
  {
    return touch_current;
  }
  touch_next_sample_ms = now_ms + TOUCH_CONTROLLER_FRAME_MS;

  __disable_irq();
  irq_pending = touch_irq_pending;
  touch_irq_pending = 0U;
  __enable_irq();
  int_low = (HAL_GPIO_ReadPin(TP_INT_GPIO_Port, TP_INT_Pin) == GPIO_PIN_RESET) ? 1U : 0U;

  if ((CST816T_ReadTouch(&x, &y) != 0U) ||
      (((irq_pending != 0U) || (int_low != 0U)) &&
       (CST816T_ReadTouchLoose(&x, &y) != 0U)))
  {
    touch_current.pressed = 1U;
    touch_current.x = x;
    touch_current.y = y;
    touch_release_misses = 0U;
    return touch_current;
  }

  if (touch_current.pressed != 0U)
  {
    touch_release_misses++;
    if (touch_release_misses < TOUCH_CONTROLLER_RELEASE_MISSES)
    {
      return touch_current;
    }
  }

  touch_current.pressed = 0U;
  touch_release_misses = 0U;
  return touch_current;
}
