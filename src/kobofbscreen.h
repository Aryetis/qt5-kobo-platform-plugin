#ifndef QKOBOFBSCREEN_H
#define QKOBOFBSCREEN_H

#include <QtFbSupport/private/qfbcursor_p.h>
#include <QtFbSupport/private/qfbscreen_p.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <unistd.h>

#include <cstring>

#include "dither.h"
#include "eink/mxcfb-kobo.h"
#include "einkenums.h"
#include "fbink.h"
#include "kobodevicedescriptor.h"

class QPainter;
class QFbCursor;

class KoboFbScreen : public QFbScreen
{
    Q_OBJECT
public:
    KoboFbScreen(const QStringList &args, KoboDeviceDescriptor *koboDevice);
    ~KoboFbScreen();

    bool initialize() override;

    void setFullScreenRefreshMode(WaveForm waveform);

    void clearScreen(bool waitForCompleted);

    void enableDithering(bool softwareDithering, bool hardwareDithering);

    void doManualRefresh(const QRect &region, bool forceMode = false, WFM_MODE_INDEX_T waveformMode = WFM_AUTO);

    bool setScreenRotation(ScreenRotation r, int bpp = 8);

    ScreenRotation getScreenRotation();

    QRegion doRedraw() override;

    QFbCursor *mCursor;

    QPlatformCursor *cursor() const override { return mCursor; } // Very important for mouse support

    void mouseMoveChecker();

    void setFlashing(bool v);

    void toggleNightMode();

private:
    void ditherRegion(const QRect &region);

    KoboDeviceDescriptor *koboDevice;

    QStringList mArgs;
    int mFbFd;
    bool debug = false;

    QImage mFbScreenImage;
    int mBytesPerLine;
    QImage mScreenImageDither;

    QPainter *mBlitter;

    FBInkState fbink_state;

    struct
    {
        unsigned char *bufferPtr;
        size_t bufferSize;
    } memmapInfo;

    FBInkConfig fbink_cfg;

    bool waitForRefresh;
    bool useHardwareDithering;
    bool useSoftwareDithering;

    WFM_MODE_INDEX_T waveFormFullscreen;
    WFM_MODE_INDEX_T waveFormPartial;
    WFM_MODE_INDEX_T waveFormFast;

    int originalRotation;
    int originalBpp;

    bool renderCursor = false;
    bool mouse = false;
    bool motionDebug = false;
    QTimer* mouseTimer;
    QPoint previousPosition;
    bool changedTime = false;
    int slowRefresh = 300; // This speed determines how fast will it switch to fastRefresh when the mouse starts moving
    int fastRefresh = 125; // This speeds determines how often cursor is updated when moving
    int cyclesUntilSlow = 1; // fastRefresh * cyclesUntilSlow time & this also cleans the previous cursor if it stops
    int countCycles = 0;
    QVector<QRect> savedCursorRects;
    QRect dirtyRect;

    QImage cleanStopFragment;
    QFile standbyCursorFile{"standby_cursor.png"};
    QImage* standbyCursor;
    QRect stopRect = QRect{0, 0, 0, 0};

    bool flashingEnabled = true;
    bool nightMode = false;

};

#endif  // QKOBOFBSCREEN_H
