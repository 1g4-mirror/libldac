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

#include <stdlib.h>
#include <string.h>

#define LDAC_ABR_OBSERVING_TIME_MS 500 /* [ms] the time length for storing Tx Queue Depth */
#define LDAC_ABR_PENALTY_MAX 8

/* Number of observing count to judge whether EQMID may be increase.
 * Those count can convert in time by following formula:
 *    Time [ms] = (Count - ConvertedId) * LDAC_ABR_OBSERVING_TIME_MS
 *    where ConvertedId is the value which converted EQMID by aEqmidToBitrateSortedID[].
 * Therefore, using the default value of 12, the observation time in each ConvertedId
 * is as follows:
 *       ------------------------------------------
 *      | ConvertedId          | 0 | 1 | 2 | 3 | 4 |
 *      | observation time [s] | 6 | 5 | 4 | 3 | 2 |
 *       ------------------------------------------
 */
#define LDAC_ABR_OBSERVING_COUNT_TO_JUDGE_INC_QUALITY 12
#define LDAC_ABR_OBSERVING_COUNT_FOR_INIT 6 /* = 3sec. keep same EQMID in first 3sec */
/* Default value for thresholds */
#define LDAC_ABR_THRESHOLD_CRITICAL_DEFAULT 6
#define LDAC_ABR_THRESHOLD_DANGEROUSTREND_DEFAULT 4
#define LDAC_ABR_THRESHOLD_SAFETY_FOR_HQSQ_DEFAULT 2
/* Number of steady state count to judge */
#define LDAC_ABR_NUM_STEADY_STATE_TO_JUDGE_STEADY 3
/* Number of steady state count to reset for LDACBT_EQMID_HQ */
#define LDAC_ABR_NUM_STEADY_STATE_TO_RESET_PENALTY_FOR_HQ 60


typedef struct _tx_queue_param
{
    unsigned char *pHist;
    unsigned int szHist;
    int  sum;
    unsigned int  cnt;
    unsigned int idx;
} TxQ_INFO;

typedef struct _ldacbt_abr_param
{
    TxQ_INFO TxQD_Info;
    int  cntToIncQuality;
    int  nSteadyState;
    int  nPenalty;
    int  EqmidSteady;
    unsigned int  numToEvaluate;
    /* thresholds */
    unsigned int  thCritical;
    unsigned int  thDangerousTrend;
    unsigned int  thSafety4HQSQ;
} LDAC_ABR_PARAMS;

#define clear_data(ptr, n) memset(ptr, 0, n)

#ifdef LOCAL_DEBUG
#include <android/log.h>
#define ABRDBG(fmt, ... ) \
    __android_log_print( ANDROID_LOG_INFO, "******** LDAC ABR ********",\
            "%s@%s:%d::"fmt, __func__, __FILE__, __LINE__, ## __VA_ARGS__ )
#else
#define ABRDBG(fmt, ...)
#endif /* LOCAL_DEBUG */

/* Get LDAC ABR handle */
HANDLE_LDAC_ABR ldac_ABR_get_handle(void)
{
    HANDLE_LDAC_ABR hLdacAbr;
    ABRDBG( "" );
    if ((hLdacAbr = (HANDLE_LDAC_ABR)malloc(sizeof(LDAC_ABR_PARAMS))) == NULL) {
        return NULL;
    }
    hLdacAbr->TxQD_Info.pHist = NULL;
    return hLdacAbr;
}

/* Free LDAC ABR handle */
void ldac_ABR_free_handle(HANDLE_LDAC_ABR hLdacAbr)
{
    ABRDBG( "" );
    if (hLdacAbr != NULL) {
        if (hLdacAbr->TxQD_Info.pHist) {
            free(hLdacAbr->TxQD_Info.pHist);
        }
        free(hLdacAbr);
    }
}

/* Initialize LDAC ABR */
int ldac_ABR_Init( HANDLE_LDAC_ABR hLdacAbr, unsigned int interval_ms )
{
    ABRDBG( "" );
    if (hLdacAbr == NULL) return -1;
    if (interval_ms == 0) return -1;
    if (interval_ms > LDAC_ABR_OBSERVING_TIME_MS) return -1;

    hLdacAbr->numToEvaluate = LDAC_ABR_OBSERVING_TIME_MS / interval_ms;
    hLdacAbr->TxQD_Info.sum = 0;
    hLdacAbr->TxQD_Info.cnt = 0;
    hLdacAbr->TxQD_Info.idx = 0;
    hLdacAbr->TxQD_Info.szHist = hLdacAbr->numToEvaluate + 1;
    if (hLdacAbr->TxQD_Info.pHist) free(hLdacAbr->TxQD_Info.pHist);
    if ((hLdacAbr->TxQD_Info.pHist =
            (unsigned char*)malloc(hLdacAbr->TxQD_Info.szHist * sizeof(unsigned char))) == NULL){
        return -1;
    }
    clear_data(hLdacAbr->TxQD_Info.pHist, hLdacAbr->TxQD_Info.szHist * sizeof(unsigned char));

    hLdacAbr->nSteadyState = 0;
    hLdacAbr->nPenalty = 1;
    hLdacAbr->EqmidSteady = 0;
    hLdacAbr->cntToIncQuality = LDAC_ABR_OBSERVING_COUNT_FOR_INIT;
    /* thresholds */
    hLdacAbr->thCritical = LDAC_ABR_THRESHOLD_CRITICAL_DEFAULT;
    hLdacAbr->thDangerousTrend = LDAC_ABR_THRESHOLD_DANGEROUSTREND_DEFAULT;
    hLdacAbr->thSafety4HQSQ = LDAC_ABR_THRESHOLD_SAFETY_FOR_HQSQ_DEFAULT;

    return 0;
}

/* Setup thresholds for LDAC ABR */
int ldac_ABR_set_thresholds( HANDLE_LDAC_ABR hLdacAbr, unsigned int thCritical,
                        unsigned int thDangerousTrend, unsigned int thSafety4HQSQ )
{
    ABRDBG( "" );
    if (hLdacAbr == NULL) return -1;
    if (thCritical < thDangerousTrend) return -1;
    if (thDangerousTrend < thSafety4HQSQ) return -1;
    hLdacAbr->thCritical = thCritical;
    hLdacAbr->thDangerousTrend = thDangerousTrend;
    hLdacAbr->thSafety4HQSQ = thSafety4HQSQ;
    return 0;
}

/* LDAC ABR main process */
int ldac_ABR_Proc( HANDLE_LDAC_BT hLDAC, HANDLE_LDAC_ABR hLdacAbr,
              unsigned int TxQueueDepth, unsigned int flagEnable)
{
    int nStepsToChangeEQMID, abrQualityModeID, encQModeID, i;
    unsigned int TxQD_curr, TxQD_prev;
#ifdef LOCAL_DEBUG
    int qd, TxQ; // for debug
#endif
    /* A table for converting EQMID to ID sorted in descending order by bit rate */
    const int aEqmidToBitrateSortedID[]={ 0, 1, 4, 2, 3};
    const int sizeOfEqmidToBitrateSortedIdTable = (int)(sizeof(aEqmidToBitrateSortedID)
                                                     / sizeof(aEqmidToBitrateSortedID[0]));

    if (hLDAC == NULL) return -1;
    if (hLdacAbr == NULL) return -1;

    encQModeID = ldacBT_get_eqmid(hLDAC);
    abrQualityModeID = -1;
    if ((LDACBT_EQMID_HQ <= encQModeID) && (encQModeID < sizeOfEqmidToBitrateSortedIdTable)) {
        abrQualityModeID = aEqmidToBitrateSortedID[encQModeID];
    }
#ifdef LOCAL_DEBUG
    ABRDBG( "[LDAC ABR] - abrQualityModeID : %d -- encQModeID : %d -- TxQue : %d --------------",
            abrQualityModeID, encQModeID, TxQueueDepth);
#endif
    /* check for the situation when unsupported eqmid was return from ldacBT_get_eqmid(). */
    if (abrQualityModeID < 0) return encQModeID; /* return current eqmid. */

    /* update */
    TxQD_curr = TxQueueDepth;
    if ((i = hLdacAbr->TxQD_Info.idx - 1) < 0 ) i = hLdacAbr->TxQD_Info.szHist - 1;
    TxQD_prev = hLdacAbr->TxQD_Info.pHist[i];

    hLdacAbr->TxQD_Info.sum -= hLdacAbr->TxQD_Info.pHist[hLdacAbr->TxQD_Info.idx];
    hLdacAbr->TxQD_Info.pHist[hLdacAbr->TxQD_Info.idx] = (unsigned char)TxQD_curr;
    if (++hLdacAbr->TxQD_Info.idx >= hLdacAbr->TxQD_Info.szHist) hLdacAbr->TxQD_Info.idx = 0;

    hLdacAbr->TxQD_Info.sum += TxQD_curr;
    ++hLdacAbr->TxQD_Info.cnt;

#ifdef LOCAL_DEBUG
    qd  = (abrQualityModeID    * 100000000);
    qd += (hLdacAbr->nPenalty  *   1000000);
    qd += (hLdacAbr->cntToIncQuality *1000);
    qd += (hLdacAbr->nSteadyState);
    TxQ = TxQD_prev * 100 + TxQD_curr;
#endif

    /* judge */
    nStepsToChangeEQMID = 0;
    if (TxQD_curr >= hLdacAbr->thCritical) {
        /* for Critical situation */
        ABRDBG("Critical: %d, %d", TxQ, qd);
        nStepsToChangeEQMID = -1;
        if ((encQModeID == LDACBT_EQMID_HQ) || (encQModeID == LDACBT_EQMID_SQ)) {
            nStepsToChangeEQMID = -2;
        }
    }
    else if ((TxQD_curr > hLdacAbr->thDangerousTrend) && (TxQD_curr > TxQD_prev)) {
        ABRDBG("Dangerous: %d, %d", TxQ, qd);
        nStepsToChangeEQMID = -1;
    }
    else if ((TxQD_curr > hLdacAbr->thSafety4HQSQ) &&
             ((encQModeID == LDACBT_EQMID_HQ) || (encQModeID == LDACBT_EQMID_SQ))) {
        ABRDBG("Safety4HQSQ: %d, %d", TxQ, qd);
        nStepsToChangeEQMID = -1;
    }
    else if (hLdacAbr->TxQD_Info.cnt >= hLdacAbr->numToEvaluate) {
        int ave10;
        hLdacAbr->TxQD_Info.cnt = hLdacAbr->numToEvaluate;
        /* eanble average process */
        ave10 = (hLdacAbr->TxQD_Info.sum * 10) / hLdacAbr->TxQD_Info.cnt;

        if (ave10 > 15) { /* if average of TxQue_Count in 0.5[s] was larger than 1.5 */
            ABRDBG("ave: %d, %d, %d", TxQ, qd, ave10);
            nStepsToChangeEQMID = -1;
        }
        else {
            ++hLdacAbr->nSteadyState;
#ifdef LOCAL_DEBUG
            qd  = (abrQualityModeID    * 100000000);
            qd += (hLdacAbr->nPenalty  *   1000000);
            qd += (hLdacAbr->cntToIncQuality *1000);
            qd += (hLdacAbr->nSteadyState);
#endif

            if (hLdacAbr->TxQD_Info.sum == 0) {
                if (--hLdacAbr->cntToIncQuality <= 0) {
                    ABRDBG("inc1: %d, %d, %d", TxQ, qd, ave10);
                    nStepsToChangeEQMID = 1;
                }
                else {
                    ABRDBG("reset: %d, %d, %d", TxQ, qd, ave10);
                    hLdacAbr->TxQD_Info.cnt = 0; // reset the number of sample for average proc.
                }
            }
            else {
                ABRDBG( "reset cntToIncQuality, %d,%d, %d", TxQ,qd, ave10);
                hLdacAbr->cntToIncQuality = LDAC_ABR_OBSERVING_COUNT_TO_JUDGE_INC_QUALITY
                                            - 2 * abrQualityModeID;
                if (abrQualityModeID >= hLdacAbr->EqmidSteady) {
                    hLdacAbr->cntToIncQuality *= hLdacAbr->nPenalty;
                }
            }
        }
    }
#ifdef LOCAL_DEBUG
    else {
        ABRDBG("Nothing %d", TxQ);
    }
#endif
    if (flagEnable) {
        if (nStepsToChangeEQMID) {
            int abrQualityModeIDNew;
            if (nStepsToChangeEQMID < 0) {
                for (i = 0; i > nStepsToChangeEQMID; --i) {
                    if (ldacBT_alter_eqmid_priority(hLDAC, LDACBT_EQMID_INC_CONNECTION)) {
#ifdef LOCAL_DEBUG
                        int err;
                        err = ldacBT_get_error_code(hLDAC);
                        ABRDBG("Info@%d : %d ,%d, %d", __LINE__, LDACBT_API_ERR(err), LDACBT_HANDLE_ERR(err), LDACBT_BLOCK_ERR(err));
#endif
                        break;// EQMID was already the ID of the highest connectivity.
                    }
                }

                encQModeID = ldacBT_get_eqmid(hLDAC);
                abrQualityModeIDNew = abrQualityModeID;
                if (encQModeID >= 0) {
                    if (encQModeID < sizeOfEqmidToBitrateSortedIdTable) {
                        abrQualityModeIDNew = aEqmidToBitrateSortedID[encQModeID];
                    }
                }

                if (hLdacAbr->nSteadyState < LDAC_ABR_NUM_STEADY_STATE_TO_JUDGE_STEADY) {
                    hLdacAbr->EqmidSteady = abrQualityModeIDNew - 1;
                    if (hLdacAbr->EqmidSteady < 0) hLdacAbr->EqmidSteady = 0;
                    hLdacAbr->nPenalty *= 2;
                    if(hLdacAbr->nPenalty > LDAC_ABR_PENALTY_MAX) {
                        hLdacAbr->nPenalty = LDAC_ABR_PENALTY_MAX; // MAX PENALTY
                    }
                }
            }
            else {
                if (ldacBT_alter_eqmid_priority( hLDAC, LDACBT_EQMID_INC_QUALITY )) {
#ifdef LOCAL_DEBUG
                    int err;
                    err = ldacBT_get_error_code(hLDAC);
                    ABRDBG("Info@%d : %d ,%d, %d", __LINE__, LDACBT_API_ERR(err), LDACBT_HANDLE_ERR(err), LDACBT_BLOCK_ERR(err));
#endif
                    ;// EQMID was already the ID of the highest sound quality.
                }
                encQModeID = ldacBT_get_eqmid(hLDAC);
                abrQualityModeIDNew = abrQualityModeID;
                if (encQModeID >= 0) {
                    if (encQModeID < sizeOfEqmidToBitrateSortedIdTable) {
                        abrQualityModeIDNew = aEqmidToBitrateSortedID[encQModeID];
                    }
                }
                if (abrQualityModeIDNew < hLdacAbr->EqmidSteady) {
                    hLdacAbr->nPenalty = 1;
                }
                if (abrQualityModeIDNew == aEqmidToBitrateSortedID[0]) { /* for HQ */
                    if (hLdacAbr->nSteadyState > LDAC_ABR_NUM_STEADY_STATE_TO_RESET_PENALTY_FOR_HQ) {
                        hLdacAbr->nPenalty = 1;
                    }
                }
            }

            hLdacAbr->nSteadyState = 0;
            // reset the number of sample for average proc.
            hLdacAbr->TxQD_Info.cnt = 0;
            hLdacAbr->cntToIncQuality = LDAC_ABR_OBSERVING_COUNT_TO_JUDGE_INC_QUALITY
                                        - 2 * abrQualityModeIDNew;
            if (hLdacAbr->cntToIncQuality <= 0) {
                // set minimum value.  e1 f == 0.5[s]
                hLdacAbr->cntToIncQuality = 1;
            }
            hLdacAbr->cntToIncQuality *= hLdacAbr->nPenalty;
            ABRDBG("EQMID NOW %d", encQModeID);
        }
    }
#ifdef LOCAL_DEBUG
    else if (TxQueueDepth) {
        ABRDBG("flagEnable false: %d ,%d", TxQ, qd);
    }
#endif

    return encQModeID;
}
