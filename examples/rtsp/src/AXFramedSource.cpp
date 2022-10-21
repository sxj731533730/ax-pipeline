/**********************************************************************************
 *
 * Copyright (c) 2019-2020 Beijing AXera Technology Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Beijing AXera Technology Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Beijing AXera Technology Co., Ltd.
 *
 **********************************************************************************/

#include "AXFramedSource.h"
#include <GroupsockHelper.hh>

AXFramedSource* AXFramedSource::createNew(UsageEnvironment& env) {
    return new AXFramedSource(env);
}

EventTriggerId AXFramedSource::eventTriggerId = 0;

unsigned AXFramedSource::referenceCount = 0;

AXFramedSource::AXFramedSource(UsageEnvironment& env)
: FramedSource(env) {
    if (referenceCount == 0) {
        // Any global initialization of the device would be done here:
        //%%% TO BE WRITTEN %%%
    }
    ++referenceCount;

    m_nTriggerID = envir().taskScheduler().createEventTrigger(deliverFrame);
    COMM_RTSP_PRT("referenceCount=%d, TriggerID=%d\n", referenceCount, m_nTriggerID);
    m_pRingBuf = new CAXRingBuffer(700000 /*gOptions.GetRTSPMaxFrmSize()*/, 2, "RTSP");
}

AXFramedSource::~AXFramedSource() {
    --referenceCount;

    envir().taskScheduler().deleteEventTrigger(m_nTriggerID);
    eventTriggerId = 0;
    COMM_RTSP_PRT("referenceCount=%d, TriggerID=%d\n", referenceCount, m_nTriggerID);
    delete(m_pRingBuf);

}

void AXFramedSource::AddFrameBuff(AX_U8 nChn, const AX_U8* pBuf, AX_U32 nLen, AX_U64 nPts/*=0*/, AX_BOOL bIFrame/*=AX_FALSE*/) {
    CAXRingElement ele((AX_U8*)pBuf, nLen, nChn, nPts, bIFrame);
    m_pRingBuf->Put(ele);
    envir().taskScheduler().triggerEvent(m_nTriggerID, this);
}

void AXFramedSource::doGetNextFrame() {
    _deliverFrame();
}

void AXFramedSource::deliverFrame(void* clientData) {
    ((AXFramedSource*)clientData)->_deliverFrame();
}

void AXFramedSource::_deliverFrame() {
    if (!isCurrentlyAwaitingData()) {
        return; // we're not ready for the data yet
    }

    CAXRingElement *element = m_pRingBuf->Get();
    if (!element) {
        return;
    }
    u_int8_t* pNewFrameDataStart = element->pBuf;
    AX_U32 nNewFrameSize = element->nSize;

    if (0 == nNewFrameSize || nullptr == pNewFrameDataStart) {
        return;
    }
    // Deliver the data here:
    if (nNewFrameSize > fMaxSize) {
        COMM_RTSP_PRT("Exceeding max frame size: newFrameSize:%u > fMaxSize:%u", nNewFrameSize, fMaxSize);
        fFrameSize = fMaxSize;
        fNumTruncatedBytes = nNewFrameSize - fMaxSize;
    } else {
        fFrameSize = nNewFrameSize;
    }
    gettimeofday(&fPresentationTime, NULL); // If you have a more accurate time - e.g., from an encoder - then use that instead.
    // If the device is *not* a 'live source' (e.g., it comes instead from a file or buffer), then set "fDurationInMicroseconds" here.
    memmove(fTo, pNewFrameDataStart, fFrameSize);

    m_pRingBuf->Pop();

    // After delivering the data, inform the reader that it is now available:
    FramedSource::afterGetting(this);
}

unsigned AXFramedSource::maxFrameSize() const {
  // By default, this source has no maximum frame size.
  return 700000; //gOptions.GetRTSPMaxFrmSize();
}