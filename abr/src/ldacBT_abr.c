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

#include "ldacBT_abr.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define LDAC_ABR_OBSERVING_TIME_MS 500 /* [ms] the time length for storing Tx Queue Depth */
#define LDAC_ABR_PENALTY_MAX 8

typedef struct _tx_queue_param {
    int8_t *pHist;
    int32_t szHist;
    int32_t sum;
    int32_t cnt;
    int32_t idx;
} TxQ_INFO;

typedef struct _ldacbt_abr_param {
    TxQ_INFO TxQD_Info;
    int32_t cntToIncQuality;
    int32_t nSteadyState;
    int32_t nPenalty;
    int32_t EqmidSteady;
    int32_t numToEvaluate;
    /* thresholds */
    int32_t thCritical;
    int32_t thDangerousTrend;
    int32_t thSafety4HQSQ;
} LDAC_ABR_PARAMS;

#define clear_data(ptr, n) memset(ptr, 0, n)

#ifdef LOCAL_DEBUG
#include <android/log.h>
#define ABRDBG(fmt, ...) \
    __android_log_print(ANDROID_LOG_INFO, "***LDAC ABR***", \
            "%s@%s:%d::"fmt, __func__, __FILE__, __LINE__, ## __VA_ARGS__)
#else
#define ABRDBG(fmt, ...)
#endif /* LOCAL_DEBUG */

/* Get LDAC ABR handle */
HANDLE_LDAC_ABR ldac_ABR_get_handle(int interval_ms)
{
    HANDLE_LDAC_ABR hLdacAbr;
    hLdacAbr = (HANDLE_LDAC_ABR)malloc(sizeof(LDAC_ABR_PARAMS));
    if (hLdacAbr == NULL)
        return NULL;

    hLdacAbr->TxQD_Info.pHist = NULL;
    if (ldac_ABR_Init(hLdacAbr, interval_ms)) {
        ldac_ABR_free_handle(hLdacAbr);
        return NULL;
    }
    return hLdacAbr;
}

/* Free LDAC ABR handle */
void ldac_ABR_free_handle(HANDLE_LDAC_ABR hLdacAbr)
{
    if (hLdacAbr != NULL) {
        if (hLdacAbr->TxQD_Info.pHist)
            free(hLdacAbr->TxQD_Info.pHist);
        free(hLdacAbr);
    }
}

/* Initialize LDAC ABR */
int ldac_ABR_Init(HANDLE_LDAC_ABR hLdacAbr, int interval_ms)
{
    if (hLdacAbr == NULL)
        return -1;

    if (interval_ms <= 0)
        return -1;

    hLdacAbr->numToEvaluate = LDAC_ABR_OBSERVING_TIME_MS/interval_ms;
    hLdacAbr->TxQD_Info.sum = 0;
    hLdacAbr->TxQD_Info.cnt = 0;
    hLdacAbr->TxQD_Info.idx = 0;
    hLdacAbr->TxQD_Info.szHist = hLdacAbr->numToEvaluate + 1;

    if (hLdacAbr->TxQD_Info.pHist)
        free(hLdacAbr->TxQD_Info.pHist);

    hLdacAbr->TxQD_Info.pHist =
            (int8_t*)malloc(hLdacAbr->TxQD_Info.szHist * sizeof(int8_t));
    if (hLdacAbr->TxQD_Info.pHist == NULL)
        return -1;

    clear_data(hLdacAbr->TxQD_Info.pHist, hLdacAbr->TxQD_Info.szHist);

    hLdacAbr->nSteadyState = 0;
    hLdacAbr->nPenalty = 1;
    hLdacAbr->EqmidSteady = LDACBT_EQMID_MQ;
    hLdacAbr->cntToIncQuality = 6; /* = 3sec. keep same EQMID in first 3sec */
    /* thresholds */
    hLdacAbr->thCritical = 6;
    hLdacAbr->thDangerousTrend = 4;
    hLdacAbr->thSafety4HQSQ = 2;

    return 0;
}

/* Setup thresholds for LDAC ABR */
int ldac_ABR_set_threshold(HANDLE_LDAC_ABR hLdacAbr, int thCritical,
                             int thDangerousTrend, int thSafety4HQSQ )
{
    if (hLdacAbr == NULL)
        return -1;

    hLdacAbr->thCritical = thCritical;
    hLdacAbr->thDangerousTrend = thDangerousTrend;
    hLdacAbr->thSafety4HQSQ = thSafety4HQSQ;
    return 0;
}

/* LDAC ABR main process */
int ldac_ABR_Proc(HANDLE_LDAC_BT hLDAC, HANDLE_LDAC_ABR hLdacAbr,
                  int TxQueueDepth, int flgEnable)
{
    int flgPriorityChanged, encQModeID, i;
    int abrQualityModeID;
    int TxQD_curr, TxQD_prev;
    int qd, TxQ; // debug
    int aEqmidToBitrateSortedID[]={ 0, 1, 4, 2, 3, 5};

    if (hLDAC == NULL)
        return -1;

    if (hLdacAbr == NULL)
        return -1;

    encQModeID = ldacBT_get_eqmid(hLDAC);
    abrQualityModeID = aEqmidToBitrateSortedID[encQModeID];

    /* update */
    TxQD_curr = TxQueueDepth;
    if ((i=hLdacAbr->TxQD_Info.idx-1)<0)
        i = hLdacAbr->TxQD_Info.szHist-1;
    TxQD_prev = hLdacAbr->TxQD_Info.pHist[i];

    hLdacAbr->TxQD_Info.sum -=
            hLdacAbr->TxQD_Info.pHist[hLdacAbr->TxQD_Info.idx];
    hLdacAbr->TxQD_Info.pHist[hLdacAbr->TxQD_Info.idx] = (int8_t)TxQD_curr;
    if (++hLdacAbr->TxQD_Info.idx >= hLdacAbr->TxQD_Info.szHist)
        hLdacAbr->TxQD_Info.idx=0;
    hLdacAbr->TxQD_Info.sum += TxQD_curr;
    ++hLdacAbr->TxQD_Info.cnt;

    /* for debug */
    TxQ = TxQD_prev * 100 +TxQD_curr;
    qd = hLdacAbr->nPenalty *100*100 +
            hLdacAbr->nSteadyState * 100 + abrQualityModeID;

    /* judge */
    flgPriorityChanged = 0;
    if (TxQD_curr >= hLdacAbr->thCritical) {
        /* for quick action */
        ABRDBG("Critical: %d, %d", TxQ, qd);
        flgPriorityChanged = -1;
        if ((encQModeID == LDACBT_EQMID_HQ) || (encQModeID == LDACBT_EQMID_SQ)) {
            flgPriorityChanged = -2;
        }
    } else if ((TxQD_curr > hLdacAbr->thDangerousTrend) && (TxQD_curr > TxQD_prev)) {
        ABRDBG("Dangerous: %d, %d", TxQ, qd);
        flgPriorityChanged = -1;
    } else if ((TxQD_curr > hLdacAbr->thSafety4HQSQ) &&
        ((encQModeID == LDACBT_EQMID_HQ) || (encQModeID == LDACBT_EQMID_SQ))) {
        ABRDBG("Safety: %d, %d", TxQ, qd);
            flgPriorityChanged = -1;
    } else if (hLdacAbr->TxQD_Info.cnt >= hLdacAbr->numToEvaluate) {
        int32_t ave10;
        hLdacAbr->TxQD_Info.cnt = hLdacAbr->numToEvaluate;
        /* eanble average process */
        ave10 = (hLdacAbr->TxQD_Info.sum*10) / hLdacAbr->TxQD_Info.cnt;

        if (ave10 > 15) { /* if average in 0.5[s] was larger than 1.5 */
            ABRDBG("ave: %d, %d, %d", TxQ, qd, ave10);
            flgPriorityChanged = -1;
        } else {
            ++hLdacAbr->nSteadyState;
            qd = (hLdacAbr->nPenalty * 100 * 100 +
                    hLdacAbr->nSteadyState * 100 + abrQualityModeID);

            if (hLdacAbr->TxQD_Info.sum == 0) { /*ave10<1*/
                if (--hLdacAbr->cntToIncQuality <= 0) {
                    ABRDBG("inc1: %d, %d, %d", TxQ, qd, ave10);
                    flgPriorityChanged = 1;
                } else {
                    ABRDBG("reset: %d, %d, %d", TxQ, qd, ave10);
                    hLdacAbr->TxQD_Info.cnt = 0; /* reset the number of sample for average proc */
                }
            } else {
                /* 12 == 6[s] --> HQ:6[s], SQ:5[s], 492:4[s], 396:3[s], 330:2[s], 282:1[s] */
                hLdacAbr->cntToIncQuality = 12 - 2*abrQualityModeID;
                if (abrQualityModeID >= hLdacAbr->EqmidSteady)
                    hLdacAbr->cntToIncQuality *= hLdacAbr->nPenalty;
            }
        }
    } else {
        ABRDBG("Nothing %d", TxQ);
    }

    if (flgEnable) {
        if (flgPriorityChanged) {
            int abrQualityModeIDNew;
            if (flgPriorityChanged < 0) {
                for (i = 0; i > flgPriorityChanged; --i) {
                    encQModeID = ldacBT_get_eqmid(hLDAC);
                    if (aEqmidToBitrateSortedID[encQModeID] < 4)
                        ldacBT_alter_eqmid_priority(hLDAC, LDACBT_EQMID_INC_CONNECTION);
                    else
                        break;/* EQMID was already the ID of the highest connectivity */
                }

                encQModeID = ldacBT_get_eqmid(hLDAC);
                abrQualityModeIDNew = aEqmidToBitrateSortedID[encQModeID];

                if (hLdacAbr->nSteadyState < 3) {
                    hLdacAbr->EqmidSteady = abrQualityModeIDNew-1;
                    if (hLdacAbr->EqmidSteady < 0)
                        hLdacAbr->EqmidSteady = 0;
                    hLdacAbr->nPenalty*=2;
                    if(hLdacAbr->nPenalty > LDAC_ABR_PENALTY_MAX)
                        hLdacAbr->nPenalty=LDAC_ABR_PENALTY_MAX; /* MAX PENALTY */
                }
            } else {
                /* EQMID was already the ID of the highest sound quality */
                ldacBT_alter_eqmid_priority(hLDAC, LDACBT_EQMID_INC_QUALITY);
                encQModeID = ldacBT_get_eqmid(hLDAC);
                abrQualityModeIDNew = aEqmidToBitrateSortedID[encQModeID];

                if (abrQualityModeIDNew < hLdacAbr->EqmidSteady)
                    hLdacAbr->nPenalty = 1;

                if (abrQualityModeIDNew == 0) /* for HQ */
                    if (hLdacAbr->nSteadyState > 60)
                        hLdacAbr->nPenalty = 1;
            }

            hLdacAbr->nSteadyState = 0;
            /* reset the number of sample for average proc */
            hLdacAbr->TxQD_Info.cnt = 0;
            /* 12 == 6[s] --> HQ:6[s], SQ:5[s], 492:4[s], 396:3[s], 330:2[s], 282:1[s] */
            hLdacAbr->cntToIncQuality = 12 - 2*abrQualityModeIDNew;

            if (hLdacAbr->cntToIncQuality <= 0) {
                /* set minimum value.  e1 f == 0.5[s] */
                hLdacAbr->cntToIncQuality = 1;
            }

            //if (abrQualityModeIDNew > hLdacAbr->EqmidSteady)
            hLdacAbr->cntToIncQuality *= hLdacAbr->nPenalty;
            ABRDBG("QMODE NOW %d", encQModeID);
        }
    } else if (TxQueueDepth) {
        ABRDBG("flgenable false: %d ,%d", TxQ, qd);
    }

    return encQModeID;
}

