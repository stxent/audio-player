/*
 * board/lpc43xx_devkit/tests/blinking_led/main.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board_shared.h"
#include <halm/timer.h>
#include <assert.h>
/*----------------------------------------------------------------------------*/
static void onTimerOverflow(void *argument)
{
  *(bool *)argument = true;
}
/*----------------------------------------------------------------------------*/
int main(void)
{
  bool event = false;

  boardSetupClock();

  const struct Pin led = pinInit(BOARD_LED_R_PIN);
  pinOutput(led, true);

  struct Timer * const timer = boardMakeMountTimer();
  assert(timer != NULL);
  timerSetOverflow(timer, timerGetFrequency(timer) / 2);
  timerSetCallback(timer, onTimerOverflow, &event);
  timerEnable(timer);

  while (1)
  {
    while (!event)
      barrier();
    event = false;

    pinToggle(led);
  }

  return 0;
}
