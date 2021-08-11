#ifndef EINKREFRESHTHREAD_H
#define EINKREFRESHTHREAD_H

#include <QMutex>
#include <QQueue>
#include <QRect>
#include <QThread>
#include <QTime>
#include <QWaitCondition>
#include <algorithm>

#include "eink.h"
#include "eink_sunxi.h"
#include "kobodevicedescriptor.h"
#include "refreshmode.h"

class EinkrefreshThread : public QThread
{
public:
    EinkrefreshThread();
    EinkrefreshThread(int fb, QRect screenRect, int marker, bool waitCompleted,
                      PartialRefreshMode partialRefreshMode, WaveForm fullscreenWaveForm, bool dithering);
    ~EinkrefreshThread();

    void initialize(int fb, FBInkKoboSunxi *sunxiCtx, KoboDeviceDescriptor *koboDevice, int marker,
                    bool waitCompleted, PartialRefreshMode partialRefreshMode, bool dithering);

    void setPartialRefreshMode(PartialRefreshMode partialRefreshMode);

    void setFullScreenRefreshMode(WaveForm waveform);
    void clearScreen(bool waitForCompleted);

    void enableDithering(bool dithering);

    void refresh(const QRect &r);
    void doExit();

protected:
    virtual void run();

private:
    QWaitCondition waitCondition;
    QQueue<QRect> queue;
    QMutex mutexWaitCondition;
    QMutex mutexQueue;

    KoboDeviceDescriptor *koboDevice;

    FBInkKoboSunxi *sunxiCtx;

    char exitFlag;
    int fb;
    QRect screenRect;
    unsigned int marker;
    bool waitCompleted;
    PartialRefreshMode partialRefreshMode;
    WaveForm waveFormFullscreen;
    WaveForm waveFormPartial;
    WaveForm waveFormFast;
    bool dithering;
};

#endif  // EINKREFRESHTHREAD_H
