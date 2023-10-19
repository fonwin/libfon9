// \file fon9/F9cardDevel.h
//
// 用於開發階段, 包含 [量測延遲] 所需的相關檔案.
//
// \author fonwinz@gmail.com
#ifndef __fon9_F9cardDevel_h__
#define __fon9_F9cardDevel_h__

#ifdef F9CARD
#include "f9/LatencyMeasure.h"
#include "f9/CpuCache.h"
#else
#  define f9_LatencyMeasure_Push(locName)
#  define f9_DCachePrefetchRd(addr,locality)
#  define f9_DCachePrefetchWr(addr,locality)
#endif

#endif//__fon9_F9cardDevel_h__
