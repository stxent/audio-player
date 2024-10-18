/*
 * core/trace.h
 * Copyright (C) 2024 xent
 * Project is distributed under the terms of the MIT License
 */

#ifndef CORE_TRACE_H_
#define CORE_TRACE_H_
/*----------------------------------------------------------------------------*/
#include <xcore/error.h>
/*----------------------------------------------------------------------------*/
BEGIN_DECLS

enum Result debugTraceInit(void *, void *);
void debugTraceDeinit(void);

#ifdef ENABLE_DBG
void debugTrace(const char *, ...);
#else
#  define debugTrace(...) do {} while (0)
#endif

END_DECLS
/*----------------------------------------------------------------------------*/
#endif /* CORE_TRACE_H_ */
