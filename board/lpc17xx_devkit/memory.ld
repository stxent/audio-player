/*
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

ENTRY(RESET_ISR)
EXTERN(vectorTable)

MEMORY
{
  FLASH (rx) : ORIGIN = ${FLASH_ORIGIN}, LENGTH = ${FLASH_LENGTH}

  GPIO (rw)  : ORIGIN = 0x2009C000, LENGTH = 16K
  APB0 (rw)  : ORIGIN = 0x40000000, LENGTH = 512K
  APB1 (rw)  : ORIGIN = 0x40080000, LENGTH = 512K
  AHB (rw)   : ORIGIN = 0x50000000, LENGTH = 2M
  PPB (rw)   : ORIGIN = 0xE0000000, LENGTH = 1M

  /* Local SRAM */
  SRAM0 (rwx) : ORIGIN = 0x10000000, LENGTH = 32K
  /* AHB SRAM Banks */
  SRAM1 (rwx) : ORIGIN = 0x2007C000, LENGTH = 16K
  /* AHB SRAM Banks */
  SRAM2 (rwx) : ORIGIN = 0x20080000, LENGTH = 16K
}

SECTIONS
{
  .text : ALIGN(4)
  {
    _stext = .;

    *(.vectors)
    *(.text)
    *(.text*)
    *(.rodata)
    *(.rodata*)

    _etext = .;
  } >FLASH =0xFF

  .data_ptr : ALIGN(4)
  {
    _sidata = .;
  } >FLASH =0xFF

  .data : ALIGN(4)
  {
    _sdata = .;

    *(.data)
    *(.data*)

    _edata = .;
  } >SRAM0 AT >FLASH

  .bss : ALIGN(4)
  {
    _sbss = .;

    *(.bss)
    *(.bss*)
    *(COMMON)

    _ebss = .;
  } >SRAM0

  .sram1 : ALIGN(4)
  {
    *(.sram1)
    *(.sram1*)
  } >SRAM1

  .sram2 : ALIGN(4)
  {
    *(.sram2)
    *(.sram2*)

    . = ALIGN(4);
    heapStart = .;
  } >SRAM2

  .gpio :
  {
    GPIO_DOMAIN = .;
  } >GPIO

  .apb0 :
  {
    APB0_DOMAIN = .;
  } >APB0

  .apb1 :
  {
    APB1_DOMAIN = .;
  } >APB1

  .ahb :
  {
    AHB_DOMAIN = .;
  } >AHB

  .ppb :
  {
    PPB_DOMAIN = .;
  } >PPB

  /DISCARD/ :
  {
    *(.init)
    *(.fini)

    *(.ARM.exidx*)
    *(.gnu.linkonce.armexidx.*)

    *(.ARM.extab*)
    *(.gnu.linkonce.armextab.*)
  }

  PROVIDE(end = heapStart);
  PROVIDE(_stack = ORIGIN(SRAM0) + LENGTH(SRAM0));
}
