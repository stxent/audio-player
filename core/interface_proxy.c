/*
 * core/interface_proxy.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "interface_proxy.h"
#include <assert.h>
/*----------------------------------------------------------------------------*/
static enum Result interfaceInit(void *, const void *);
static void interfaceSetCallback(void *, void (*)(void *), void *);
static enum Result interfaceGetParam(void *, int, void *);
static enum Result interfaceSetParam(void *, int, const void *);
static size_t interfaceRead(void *, void *, size_t);
static size_t interfaceWrite(void *, const void *, size_t);
/*----------------------------------------------------------------------------*/
const struct InterfaceClass * const InterfaceProxy =
    &(const struct InterfaceClass){
    .size = sizeof(struct InterfaceProxy),
    .init = interfaceInit,
    .deinit = NULL, /* Use default destructor */

    .setCallback = interfaceSetCallback,
    .getParam = interfaceGetParam,
    .setParam = interfaceSetParam,
    .read = interfaceRead,
    .write = interfaceWrite
};
/*----------------------------------------------------------------------------*/
static enum Result interfaceInit(void *object, const void *configBase)
{
  const struct InterfaceProxyConfig * const config = configBase;
  struct InterfaceProxy * const interface = object;

  interface->pipe = config->pipe;
  interface->offset = 0;

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static void interfaceSetCallback(void *object, void (*callback)(void *),
    void *argument)
{
  struct InterfaceProxy * const interface = object;
  ifSetCallback(interface->pipe, callback, argument);
}
/*----------------------------------------------------------------------------*/
static enum Result interfaceGetParam(void *object, int parameter, void *data)
{
  struct InterfaceProxy * const interface = object;
  enum Result res;

  if ((enum IfParameter)parameter == IF_POSITION_64)
  {
    uint64_t position;

    res = ifGetParam(interface->pipe, IF_POSITION_64, &position);
    if (res == E_OK)
    {
      if (position >= interface->offset)
        *(uint64_t *)data = position - interface->offset;
      else
        res = E_INTERFACE;
    }
  }
  else if ((enum IfParameter)parameter == IF_SIZE_64)
  {
    uint64_t size;

    res = ifGetParam(interface->pipe, IF_SIZE_64, &size);
    if (res == E_OK)
    {
      if (size >= interface->offset)
        *(uint64_t *)data = size - interface->offset;
      else
        res = E_INTERFACE;
    }
  }
  else
  {
    res = ifGetParam(interface->pipe, parameter, data);
  }

  return res;
}
/*----------------------------------------------------------------------------*/
static enum Result interfaceSetParam(void *object, int parameter,
    const void *data)
{
  struct InterfaceProxy * const interface = object;

  if ((enum IfParameter)parameter == IF_POSITION_64)
  {
    const uint64_t position = *(const uint64_t *)data + interface->offset;
    return ifSetParam(interface->pipe, IF_POSITION_64, &position);
  }
  else
  {
    return ifSetParam(interface->pipe, parameter, data);
  }
}
/*----------------------------------------------------------------------------*/
static size_t interfaceRead(void *object, void *buffer, size_t length)
{
  struct InterfaceProxy * const interface = object;
  return ifRead(interface->pipe, buffer, length);
}
/*----------------------------------------------------------------------------*/
static size_t interfaceWrite(void *object, const void *buffer, size_t length)
{
  struct InterfaceProxy * const interface = object;
  return ifWrite(interface->pipe, buffer, length);
}
/*----------------------------------------------------------------------------*/
void interfaceProxySetOffset(void *object, uint64_t offset)
{
  struct InterfaceProxy * const interface = object;
  interface->offset = offset;
}
