#include "kobofbscreen.h"

#include <QtFbSupport/private/qfbwindow_p.h>
#include <QtFbSupport/private/qfbcursor_p.h>

#include <QtGui/QPainter>

// force the compiler to link i2c-tools
extern "C"
{
#include "i2c-tools/include/i2c/smbus.h"
    __s32 (*i2c_smbus_read_byte_fp)(int) = &i2c_smbus_read_byte;
}

#define SMALLTHRESHOLD1 60
#define SMALLTHRESHOLD2 40
#define FULLSCREENTOLERANCE 80

static QImage::Format determineFormat(int fbfd, int depth)
{
    fb_var_screeninfo info;
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &info))
        return QImage::Format_Invalid;

    const fb_bitfield rgba[4] = {info.red, info.green, info.blue, info.transp};

    QImage::Format format = QImage::Format_Invalid;

    switch (depth)
    {
        case 32:
        {
            const fb_bitfield argb8888[4] = {{16, 8, 0}, {8, 8, 0}, {0, 8, 0}, {24, 8, 0}};
            const fb_bitfield abgr8888[4] = {{0, 8, 0}, {8, 8, 0}, {16, 8, 0}, {24, 8, 0}};
            if (memcmp(rgba, argb8888, 4 * sizeof(fb_bitfield)) == 0)
            {
                format = QImage::Format_ARGB32;
            }
            else if (memcmp(rgba, argb8888, 3 * sizeof(fb_bitfield)) == 0)
            {
                format = QImage::Format_RGB32;
            }
            else if (memcmp(rgba, abgr8888, 3 * sizeof(fb_bitfield)) == 0)
            {
                //format = QImage::Format_RGB32;
                // pixeltype =  QScreen::BGRPixel;
                // fixed for correct colours on Libra Color and probably other Kaleido screens
                format = QImage::Format_RGBA8888;
            }
            break;
        }
        case 24:
        {
            const fb_bitfield rgb888[4] = {{16, 8, 0}, {8, 8, 0}, {0, 8, 0}, {0, 0, 0}};
            const fb_bitfield bgr888[4] = {{0, 8, 0}, {8, 8, 0}, {16, 8, 0}, {0, 0, 0}};
            if (memcmp(rgba, rgb888, 3 * sizeof(fb_bitfield)) == 0)
            {
                format = QImage::Format_RGB888;
            }
            else if (memcmp(rgba, bgr888, 3 * sizeof(fb_bitfield)) == 0)
            {
                format = QImage::Format_BGR888;
                // pixeltype = BGRPixel;
            }
            break;
        }
        case 8:
            format = QImage::Format_Grayscale8;
            break;
        default:
            break;
    }

    return format;
}

KoboFbScreen::KoboFbScreen(const QStringList &args, KoboDeviceDescriptor *koboDevice)
    : koboDevice(koboDevice),
      mArgs(args),
      mFbFd(-1),
      debug(false),
      mFbScreenImage(),
      mBytesPerLine(0),
      mBlitter(0),
      fbink_state({0}),
      memmapInfo({0}),
      fbink_cfg({0}),
      useHardwareDithering(false),
      useSoftwareDithering(true)
{
    useHardwareDithering = false; // TODO: What even is this?
    setDefaultWaveform();
}

void KoboFbScreen::setDefaultWaveform()
{
    if(debug) qDebug() << "Set default waveform called";
    waveFormFullscreen = WFM_GC16;
    waveFormPartial = WFM_AUTO;
    waveFormFast = WFM_A2;

    if (koboDevice->isREAGL)
    {
        this->waveFormPartial = WFM_REAGLD;
        this->waveFormFast = WFM_DU;
    }
    else if (koboDevice->mark == 7)
    {
        this->waveFormPartial = WFM_REAGL;
        this->waveFormFast = WFM_DU;
    }
    else if (koboDevice->isSunxi)
    {
        this->waveFormPartial = WFM_REAGL;
        this->waveFormFast = WFM_DU;
    }
}

KoboFbScreen::~KoboFbScreen()
{
    uint8_t grayscale = originalBpp == 8 ? GRAYSCALE_8BIT : 0;

    if (fbink_set_fb_info(mFbFd, originalRotation, originalBpp, grayscale, &fbink_cfg) != EXIT_SUCCESS)
        qDebug() << "Failed to set original rotation and bpp.";

    if (mFbFd != -1)
        fbink_close(mFbFd);

    delete mBlitter;
}

bool KoboFbScreen::initialize()
{
    QRegularExpression fbRx("fb=(.*)");
    QRegularExpression sizeRx("size=(\\d+)x(\\d+)");
    QRegularExpression dpiRx("logicaldpitarget=(\\d+)");

    QString fbDevice;
    QRect userGeometry;
    int logicalDpiTarget = 0;

    // Parse arguments
    for (const QString &arg : qAsConst(mArgs))
    {
        QRegularExpressionMatch match;
        if (arg.contains(sizeRx, &match))
            userGeometry.setSize(QSize(match.captured(1).toInt(), match.captured(2).toInt()));
        else if (arg.contains(fbRx, &match))
            fbDevice = match.captured(1);
        else if (arg.contains(dpiRx, &match))
            logicalDpiTarget = match.captured(1).toInt();
        else if (arg.startsWith("debug"))
            debug = true;
        else if (arg.startsWith("mouse"))
            mouse = true;
        else if (arg.startsWith("motiondebug"))
        {
            motionDebug = true;
            qDebug() << "Motion debug enabled. Debug information for screen and mouse will be printed";
        }
    }

    fbink_cfg.is_verbose = debug;
    fbink_cfg.is_quiet = !debug;

    // Open framebuffer and keep it around, then setup globals.
    if ((mFbFd = fbink_open()) < EXIT_SUCCESS)
    {
        qDebug() << "Failed to open the framebuffer";
        return false;
    }

    if (fbink_init(mFbFd, &fbink_cfg) < EXIT_SUCCESS)
    {
        qDebug() << "Failed to initialize FBInk.";
        return false;
    }

    fbink_get_state(&fbink_cfg, &fbink_state);
    originalBpp = fbink_state.bpp;
    originalRotation = fbink_state.current_rota;

    // Don't listed to the sys interface, that's a very bad idea as Niluje explained.
    // But we use native FBInk so that's good?
    setScreenRotation(getScreenRotation(), originalBpp);

    QFbScreen::initializeCompositor();

    if (mFbScreenImage.isNull())
        qDebug() << "Error, mFbScreenImage is invalid!";

    if (logicalDpiTarget > 0)
    {
        //        qputenv("QT_SCALE_FACTOR_ROUNDING_POLICY ", "PassThrough");
        qputenv("QT_SCREEN_SCALE_FACTORS",
                QString::number(koboDevice->dpi / (double)logicalDpiTarget, 'g', 8).toLatin1());
    }

    // Even if cursor is disabled, because of cursor function override this still needs to be here to prevent a randomly-appearing segmentation fault.
    mCursor = new QFbCursor(this);
    if(mouse)
    {
        previousPosition = mCursor->pos();
        mouseTimer = new QTimer(this);
        connect(mouseTimer, &QTimer::timeout, this, &KoboFbScreen::mouseMoveChecker);
        mouseTimer->start(slowRefresh);
        if(debug)
            qDebug() << "Initialized cursor with the screen";
        if(standbyCursorFile.exists())
        {
            standbyCursor->load(standbyCursorFile.fileName());
            if(debug)
                qDebug() << "Using custom standby cursor";
        }
        else
        {
            if(debug)
                qDebug() << "Using default standby cursor";
            standbyCursor = new QImage("://resources/standby_cursor.png");
        }
    }

    return true;
}

bool KoboFbScreen::setScreenRotation(ScreenRotation r, int bpp)
{
    int8_t rota_native = fbink_rota_canonical_to_native(r);
    uint8_t grayscale = bpp == 8 ? GRAYSCALE_8BIT : 0;

    int rv = 0;
    if ((rv = fbink_set_fb_info(mFbFd, rota_native, bpp, grayscale, &fbink_cfg)) < EXIT_SUCCESS)
        qDebug() << "Failed to set rotation and bpp:" << rv;

    fbink_get_state(&fbink_cfg, &fbink_state);

    mBytesPerLine = fbink_state.scanline_stride;
    koboDevice->width = fbink_state.screen_width;
    koboDevice->height = fbink_state.screen_height;

    if (debug)
        qDebug() << "Screen info:" << fbink_state.screen_width << fbink_state.screen_height
                 << "rotation:" << fbink_state.current_rota
                 << "rotation canonical:" << fbink_rota_native_to_canonical(fbink_state.current_rota)
                 << "bpp:" << fbink_state.bpp;

    mGeometry = {0, 0, koboDevice->width, koboDevice->height};

    mPhysicalSize = QSizeF(koboDevice->physicalWidth, koboDevice->physicalHeight);

    if ((memmapInfo.bufferPtr = fbink_get_fb_pointer(mFbFd, &memmapInfo.bufferSize)) == NULL)
    {
        qDebug() << "Failed to get fb data or memmap screen";
        return false;
    }

    if (debug)
        qDebug() << "Allocated screen buffer. Stride:" << fbink_state.scanline_stride
                 << "buffer size:" << memmapInfo.bufferSize;

    mDepth = fbink_state.bpp;
    mFormat = determineFormat(mFbFd, mDepth);

    mFbScreenImage =
        QImage(memmapInfo.bufferPtr, mGeometry.width(), mGeometry.height(), mBytesPerLine, mFormat);
        //QImage(memmapInfo.bufferPtr, mGeometry.width(), mGeometry.height(), mBytesPerLine, tamere);

    // mFbScreenImage = mFbScreenImage.rgbSwapped();

    return true;
}

ScreenRotation KoboFbScreen::getScreenRotation()
{
    return (ScreenRotation)fbink_rota_native_to_canonical(fbink_state.current_rota);
}

FBInkConfig *KoboFbScreen::getFBInkConfig()
{
    return &fbink_cfg;
}

FBInkState *KoboFbScreen::getFBInkState()
{
    return &fbink_state;
}

void KoboFbScreen::setFullScreenRefreshMode(WaveForm waveform)
{
    if(debug)
        qDebug() << "setFullScreenRefreshMode called";
    this->waveFormFullscreen = waveform;
}

void KoboFbScreen::setPartialScreenRefreshMode(WaveForm waveform)
{
    if(debug)
        qDebug() << "setPartialScreenRefreshMode called";
    this->waveFormPartial = waveform;
}

void KoboFbScreen::setFastScreenRefreshMode(WaveForm waveform)
{
    if(debug)
        qDebug() << "setFastScreenRefreshMode called";
    this->waveFormFast = waveform;
}

void KoboFbScreen::clearScreen(bool waitForCompleted)
{
    FBInkRect r = {0, 0, static_cast<unsigned short>(mGeometry.width()),
                   static_cast<unsigned short>(mGeometry.height())};
    fbink_cls(mFbFd, &fbink_cfg, &r, true);

    waitForRefresh(waitForCompleted);
}

void KoboFbScreen::enableDithering(bool softwareDithering, bool hardwareDithering)
{
    useHardwareDithering = hardwareDithering;
    useSoftwareDithering = softwareDithering;

    if (useSoftwareDithering)
        mScreenImageDither = mScreenImage;
}

void KoboFbScreen::ditherRegion(const QRect &region)
{
    if (mScreenImageDither.size() != mScreenImage.size())
        mScreenImageDither = mScreenImage;

    // only dither the lines that were updated
    int updateHeight = region.height();
    int offset = region.top() * mScreenImage.width();

    ditherBuffer(mScreenImageDither.bits() + offset, mScreenImage.bits() + offset, mScreenImage.width(),
                 updateHeight);
    //    ditherFloydSteinberg(mScreenImageDither.bits() + offset, mScreenImage.bits() + offset,
    //                         mScreenImage.width(), updateHeight);
}

void KoboFbScreen::doManualRefresh(const QRect &region, bool forceMode, WFM_MODE_INDEX_T waveformMode)
{
    bool isFullRefresh = region.width() >= mGeometry.width() - FULLSCREENTOLERANCE &&
                         region.height() >= mGeometry.height() - FULLSCREENTOLERANCE;

    bool isSmall = (region.width() < SMALLTHRESHOLD1 && region.height() < SMALLTHRESHOLD1) ||
                   (region.width() + region.height() < SMALLTHRESHOLD2);

    if (isFullRefresh)
        fbink_cfg.wfm_mode = this->waveFormFullscreen;
    else if (isSmall)
        fbink_cfg.wfm_mode = this->waveFormFast;
    else
        fbink_cfg.wfm_mode = this->waveFormPartial;

    // Needed for mouse
    if(forceMode)
        fbink_cfg.wfm_mode = waveformMode;

    if(flashingEnabled == true)
        fbink_cfg.is_flashing = isFullRefresh;
    else
        fbink_cfg.is_flashing = false;

    int rv = fbink_refresh(mFbFd, region.top(), region.left(), region.width(), region.height(), &fbink_cfg);

// Even more logs, don't compile them at default
#if true == false
#warning "additionall debug values are added. This should not be enabled in default builds"
    if(debug)
    {
        qDebug() << "fbink refresh exit value:" << rv;
        qDebug() << "errno value:" << errno;
        // qDebug() << "waitForRefresh value is:" << waitForRefresh;
    }
#endif

    if (rv != EXIT_SUCCESS && errno == EPERM)
    {
        if(debug)
            qDebug() << "QPA: Detected framebuffer freeze, attempting to fix ...";
        unsigned long arg = VESA_NO_BLANKING;
        if (ioctl(mFbFd, FBIOBLANK, arg) == EXIT_SUCCESS)
            rv = fbink_refresh(mFbFd, region.top(), region.left(), region.width(), region.height(), &fbink_cfg);
    }

    if (rv == EXIT_SUCCESS)
        waitForRefresh();
}

void KoboFbScreen::setFlashing(bool v)
{
    if (debug) qDebug() << "Setting flashing to:" << v;
    flashingEnabled = v;
}

void KoboFbScreen::toggleNightMode()
{
    // In the future, to preserve this across launches and apps, create a file in dev so user apps will see it too, then check for it
    nightMode = !nightMode;
    if (debug)
        qDebug() << "toggleNightMode called, it is now:" << nightMode;
    if(fbink_state.can_hw_invert)
    {
        fbink_cfg.is_nightmode = nightMode;
        if (debug)
            qDebug() << "Using hardware night mode";
    }
    else
    {
        // is_inverted doesnt work for me? why?
        if (debug)
            qDebug() << "Hardware night mode not available, using software which does not work?";
        fbink_cfg.is_inverted = nightMode;
    }
}


QRegion KoboFbScreen::doRedraw()
{
    QElapsedTimer* t;
    if (motionDebug)
    {
        t = new QElapsedTimer();
        t->start();
    }
    QRegion touched = QFbScreen::doRedraw();

    if (touched.isEmpty())
        return touched;

    QRect r(*touched.begin());
    for (const QRect &rect : touched)
        r = r.united(rect);

    if (!mBlitter)
        mBlitter = new QPainter(&mFbScreenImage);

    if (useSoftwareDithering)
        ditherRegion(r);

    mBlitter->setCompositionMode(QPainter::CompositionMode_Source);
    for (const QRect &rect : touched)
    {
        if(mouse)
        {
            if(motionDebug && rect.x() == mCursor->pos().x() && rect.y() == mCursor->pos().y())
                qDebug() << "Detected overwriting cursor position";
            // This function can be called only once, then it sets to 0,0
            dirtyRect = mCursor->dirtyRect();
            if(motionDebug)
                qDebug() << "Dirty cursor position:" << dirtyRect;
            if(rect == dirtyRect && renderCursor == false)
            {
                if(motionDebug)
                    qDebug() << "Requested cursor overwrite even though it's not allowed. Saving this fragment for later.";
                savedCursorRects.push_back(dirtyRect);
            }
            else
                mBlitter->drawImage(rect, useSoftwareDithering ? mScreenImageDither : mScreenImage, rect);
        }
        else
            mBlitter->drawImage(rect, useSoftwareDithering ? mScreenImageDither : mScreenImage, rect);
    }

    doManualRefresh(r);

    if (motionDebug)
    {
        qDebug() << "Painted region" << touched << "in" << t->elapsed() << "ms";
        delete[] t;
    } 

    return touched;
}

void KoboFbScreen::mouseMoveChecker()
{
    // Increase speed of cursor rendering if it's moving
    if(previousPosition != mCursor->pos())
    {
        if(changedTime == false)
        {
            if (motionDebug) qDebug() << "Cleaning at not moving cursor:" << stopRect;
            mBlitter->setCompositionMode(QPainter::CompositionMode_Source);
            mBlitter->drawImage(previousPosition, cleanStopFragment);
            // We need full actually, and the default is small
            doManualRefresh(stopRect, true, this->waveFormPartial);
            waitForRefresh(true);
            doManualRefresh(stopRect, true, this->waveFormPartial);
            /* Debug
            QImage tmp{"/cursor.png"};
            mBlitter->drawImage(QRect{stopRect.x(), stopRect.y(), 100, 100}, tmp, QRect{0, 100, 100, 100});
            doManualRefresh(QRect{stopRect.x(), stopRect.y(), 100, 100});
        */
        }

        if (motionDebug)
            qDebug() << "Mouse moved:" << mCursor->pos();
        if (motionDebug)
            qDebug() << "Cursor needs refreshing:" << mCursor->isDirty();

        // Save the clean position of the cursor
        // To avoid ghosting
        stopRect.setX(mCursor->pos().x());
        stopRect.setY(mCursor->pos().y());
        // Get the size of the latest cursor
        int x;
        int y;
        int fallbackSize = 48;
        if(savedCursorRects.length() != 0)
        {
            x = savedCursorRects[savedCursorRects.length() - 1].width();
            y = savedCursorRects[savedCursorRects.length() - 1].height();
            if(x == 0)
                x = fallbackSize;
            if(y == 0)
                y = fallbackSize;
        }
        else
        {
            y = fallbackSize;
            x = fallbackSize;
        }
        stopRect.setWidth(x);
        stopRect.setHeight(y);
        if(motionDebug && x == fallbackSize && y == fallbackSize)
            qDebug() << "Failed to get cursor size";
        if(useSoftwareDithering)
            cleanStopFragment = mScreenImageDither.copy(stopRect);
        else
            cleanStopFragment = mScreenImage.copy(stopRect);
        /* Debug
        cleanStopFragment.save("/tmp/cleanStopFragment.png", nullptr, -1);
    */

        // Actually request rendering it
        renderCursor = true;
        if(mCursor->pos() != previousPosition)
        {
            mCursor->updateMouseStatus();
            mCursor->drawCursor(*mBlitter);
            doManualRefresh(stopRect, true, this->waveFormFast);
        }

        mBlitter->setCompositionMode(QPainter::CompositionMode_Source);
        // Clean previous ones
        for(int i = 0; i < savedCursorRects.length(); i++)
        {
            if (motionDebug)
                qDebug() << "Clearing previous cursor:" << savedCursorRects[i];
            mBlitter->drawImage(savedCursorRects[i], useSoftwareDithering ? mScreenImageDither : mScreenImage, savedCursorRects[i]);
            doManualRefresh(savedCursorRects[i]);
        }
        if (motionDebug)
            qDebug() << "Cleared previous cursor count:" << savedCursorRects.length();
        // Save the latest width and height of the cursor to clean it in slow start
        savedCursorRects.clear();
        renderCursor = false;

        countCycles = 0;
        if(changedTime == false)
        {
            if (motionDebug)
                qDebug() << "Rendering mouse and starting fast refresh mode";
            changedTime = true;
            mouseTimer->stop();
            mouseTimer->start(fastRefresh);
        }
        previousPosition = mCursor->pos();
    }
    else
    {
       if(changedTime == true)
        {
            if(countCycles == cyclesUntilSlow)
            {
                if (motionDebug)
                    qDebug() << "Mouse stopped moving, returning to slow refreshes";

                // Make sure the cursor is visible
                waitForRefresh(true);
                mBlitter->setCompositionMode(QPainter::CompositionMode_Source);
                mBlitter->drawImage(mCursor->pos(), *standbyCursor);
                QRect cursorStandbyRect{mCursor->pos().x(), mCursor->pos().y(), standbyCursor->width(), standbyCursor->height()};
                doManualRefresh(cursorStandbyRect, true, this->waveFormPartial);

                changedTime = false;
                countCycles = 0;
                mouseTimer->stop();
                mouseTimer->start(slowRefresh);
            }
            else
                countCycles = countCycles + 1;
       }
    }
}


void KoboFbScreen::doSunxiPenRefresh()
{
    // NOTE: On sunxi, send a !pen refresh on pen up because otherwise the driver
    // softlocks,
    //       and ultimately trips a reboot watchdog...
    // NOTE: Nickel also toggles pen mode *off* before doing that...
    //       Let's do the same, as we can still somewhat reliably kill the kernel
    //       one way or another otherwise...
    // NOTE: That means that any other competing refresh is potentially dangerous:
    //       make sure only pen refreshes are sent while in pen mode!
    fbink_sunxi_toggle_ntx_pen_mode(mFbFd, false);

    const WFM_MODE_INDEX_T pen_wfm = fbink_cfg.wfm_mode;
    fbink_cfg.wfm_mode = WFM_GL16;
    fbink_refresh(mFbFd, 0, 0, 0, 0, &fbink_cfg);
    fbink_cfg.wfm_mode = pen_wfm;

    fbink_sunxi_toggle_ntx_pen_mode(mFbFd, true);
}

void KoboFbScreen::waitForRefresh(bool force)
{
    if (koboDevice->requiresWaitForCall == true || force == true)
    {
        if (koboDevice->hasReliableMxcWaitFor)
        {
            if(debug)
                qDebug() << "Doing a probably good wait method";
            fbink_wait_for_complete(mFbFd, LAST_MARKER);
        }
        else
        {
            if(debug)
                qDebug() << "Doing horrible wait method...";
            usleep(500);
        }
    }
}
