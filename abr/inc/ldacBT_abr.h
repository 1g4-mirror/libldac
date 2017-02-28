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
 * Arguments
 *   Interval in ms for LDAC encoding.
 * Return
 *   HANDLE_LDAC_ABR for success, NULL for failure.
 */
LDAC_ABR_API HANDLE_LDAC_ABR ldac_ABR_get_handle(int);

/* Release of LDAC ABR handle.
 * Arguments
 *   HANDLE_LDAC_ABR.
 * Return
 *   None.
 */
LDAC_ABR_API void ldac_ABR_free_handle(HANDLE_LDAC_ABR);

/* Initialize LDAC ABR.
 * Arguments
 *   HANDLE_LDAC_ABR, interval.
 * Return
 *   0 success, other fail.
 */
LDAC_ABR_API int ldac_ABR_Init(HANDLE_LDAC_ABR, int);

/* Setup thresholds for LDAC ABR.
 * Arguments
 *   HANDLE_LDAC_ABR, thCritical,thDangerousTrend, thSafety4HQSQ.
 * Return
 *   0 success, other fail.
 */
LDAC_ABR_API int ldac_ABR_set_threshold(HANDLE_LDAC_ABR, int, int, int);

/* LDAC ABR main process.
 * Arguments
 *   HANDLE_LDAC_BT, HANDLE_LDAC_ABR, TxQueueDepth, flgenable.
 * Return
 *   updated Encode Quality Mode Index.
 */
LDAC_ABR_API int ldac_ABR_Proc(HANDLE_LDAC_BT, HANDLE_LDAC_ABR, int, int);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _LDACBT_ABR_H_ */

