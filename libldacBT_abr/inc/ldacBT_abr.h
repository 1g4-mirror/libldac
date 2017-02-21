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

typedef struct _ldacbt_abr_param * HANDLE_LDAC_ABR;
LDAC_ABR_API HANDLE_LDAC_ABR ldac_ABR_get_handle(int);
LDAC_ABR_API void ldac_ABR_free_handle(HANDLE_LDAC_ABR);
LDAC_ABR_API int ldac_ABR_Init(HANDLE_LDAC_ABR, int);
LDAC_ABR_API int ldac_ABR_set_threasholds(HANDLE_LDAC_ABR, int, int, int);
LDAC_ABR_API int ldac_ABR_Proc(HANDLE_LDAC_BT, HANDLE_LDAC_ABR, int, int);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _LDACBT_ABR_H_ */

