/*
 * core/interface_proxy.h
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef CORE_INTERFACE_PROXY_H_
#define CORE_INTERFACE_PROXY_H_
/*----------------------------------------------------------------------------*/
#include <xcore/interface.h>
#include <stdint.h>
/*----------------------------------------------------------------------------*/
extern const struct InterfaceClass * const InterfaceProxy;

struct InterfaceProxyConfig
{
  /** Mandatory: underlying interface. */
  struct Interface *pipe;
};

struct InterfaceProxy
{
  struct Interface base;

  struct Interface *pipe;
  uint64_t offset;
};
/*----------------------------------------------------------------------------*/
BEGIN_DECLS

void interfaceProxySetOffset(void *, uint64_t);

END_DECLS
/*----------------------------------------------------------------------------*/
#endif /* CORE_INTERFACE_PROXY_H_ */
