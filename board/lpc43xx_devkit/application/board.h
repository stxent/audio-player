/*
 * board/lpc43xx_devkit/application/board.h
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef BOARD_LPC43XX_DEVKIT_APPLICATION_BOARD_H_
#define BOARD_LPC43XX_DEVKIT_APPLICATION_BOARD_H_
/*----------------------------------------------------------------------------*/
#include "board_shared.h"
#include "player.h"
#include <dpm/bus_handler.h>
#include <halm/pin.h>
/*----------------------------------------------------------------------------*/
struct FsHandle;
struct Stream;

struct Board
{
  struct AnalogPackage analogPackage;
  struct ButtonPackage buttonPackage;
  struct CodecPackage codecPackage;
  struct Player player;

  struct
  {
    struct Interface *i2s;
    struct Stream *rx;
    struct Stream *tx;
  } audio;

  struct
  {
    struct FsHandle *handle;
    struct Timer *timer;
  } fs;

  struct
  {
    struct Pin blue;
    struct Pin green;
    struct Pin red;
    struct Pin whiteA;
    struct Pin whiteB;
    struct Interface *serial;
  } indication;

  struct
  {
    struct Interface *card;
    struct Interface *sdmmc;
    struct Interface *wrapper;
  } memory;

  struct
  {
    struct Interface *serial;
    struct Watchdog *watchdog;
    struct Pin power;
  } system;

  struct
  {
    uint8_t ampRetries;
    uint8_t codecRetries;

    bool mount;
    bool volume;
  } event;

  struct
  {
    struct Timer *timer;

    uint32_t idle;
    uint32_t loops;
  } debug;
};
/*----------------------------------------------------------------------------*/
BEGIN_DECLS

void appBoardInit(struct Board *);
int appBoardStart(struct Board *);

END_DECLS
/*----------------------------------------------------------------------------*/
#endif /* BOARD_LPC43XX_DEVKIT_APPLICATION_BOARD_H_ */
