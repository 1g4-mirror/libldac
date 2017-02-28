/*
 * Copyright (C) 2014 - 2017 Sony Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _LDACBT_ABR_H_
#define _LDACBT_ABR_H_

/* This file contains the definitions, declarations and macros for an implementation of
 * LDAC ABR mode.
 *
 * The basic flow of the ABR processing is as follows;
 * - The program creates a handle of LDAC ABR api using ldac_ABR_get_handle().
 * - The program initializes the handle for ABR using ldac_ABR_Init().
 * - The program sets thresholds of the handle using ldac_ABR_set_threshold().
 * - The program adjusts bitrate of LDAC encoder by using ldac_ABR_Proc().
 *       The ABR handle ajusts eqmid based on TxQueueDepth which is passed from program.
 *       The ABR handle calls LDAC encode api ldacBT_alter_eqmid_priority() to ajust eqmid.
 *       The ABR handle calls LDAC encode api ldacBT_get_eqmid() to get current eqmid.
 * - The handle may be released with ldac_ABR_free_handle().
 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef LDAC_ABR_API
#define LDAC_ABR_API
#endif /* LDAC_ABR_API */

#include <ldacBT.h> /* HANDLE_LDAC_BT */

/* LDAC ABR handle type*/
typedef struct _ldacbt_abr_param * HANDLE_LDAC_ABR;

/* Allocation of LDAC ABR handle.
 *  Format
 *      HANDLE_LDAC_ABR  ldacBT_get_handle( int );
 *  Arguments
 *      interval_ms    int    Interval in ms for LDAC encoding.
 *  Return value
 *      HANDLE_LDAC_ABR for success, NULL for failure.
 */
LDAC_ABR_API HANDLE_LDAC_ABR ldac_ABR_get_handle(int interval_ms);

/* Release of LDAC ABR handle.
 *  Format
 *      void  ldac_ABR_free_handle( HANDLE_LDAC_ABR );
 *  Arguments
 *      hLdacAbr    HANDLE_LDAC_ABR    LDAC ABR handle.
 *  Return value
 *      None.
 */
LDAC_ABR_API void ldac_ABR_free_handle(HANDLE_LDAC_ABR hLdacAbr);

/* Initialize LDAC ABR.
 *  Format
 *      int  ldac_ABR_Init( HANDLE_LDAC_ABR, int );
 *  Arguments
 *      hLdacAbr        HANDLE_LDAC_ABR    LDAC ABR handle.
 *      interval_ms     int                interval in ms for LDAC encoding.
 *  Return value
 *      0 success, other fail.
 */
LDAC_ABR_API int ldac_ABR_Init(HANDLE_LDAC_ABR hLdacAbr, int interval_ms);

/* Setup thresholds for LDAC ABR.
 *  Format
 *      int ldac_ABR_set_threshold( HANDLE_LDAC_ABR, int, int, int );
 *  Arguments
 *      hLdacAbr            HANDLE_LDAC_ABR    LDAC ABR handle.
 *      thCritical          int                threshold for critical.
 *      thDangerousTrend    int                threshold for Dangerous.
 *      thSafety4HQSQ       int                threshold for Safety.
 *  Return value
 *      0 success, other fail.
 */
LDAC_ABR_API int ldac_ABR_set_threshold(HANDLE_LDAC_ABR hLdacAbr, int thCritical,
                                          int thDangerousTrend, int thSafety4HQSQ);

/* LDAC ABR main process.
 *  Format
 *      int  ldac_ABR_Proc( HANDLE_LDAC_BT, HANDLE_LDAC_ABR, int, int );
 *  Arguments
 *      hLdacBt        HANDLE_LDAC_BT    LDAC handle.
 *      hLdacAbr       HANDLE_LDAC_ABR   LDAC ABR handle.
 *      TxQueueDepth   int               depth of TX queue.
 *      flgenable      int
 *  Return value
 *      updated Encode Quality Mode Index.
 */
LDAC_ABR_API int ldac_ABR_Proc(HANDLE_LDAC_BT hLdacBt, HANDLE_LDAC_ABR hLdacAbr,
                                 int TxQueueDepth, int flgEnable);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _LDACBT_ABR_H_ */

