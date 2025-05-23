#include "koboplatformintegration.h"

#include <QtEventDispatcherSupport/private/qgenericunixeventdispatcher_p.h>
#include <QtFbSupport/private/qfbbackingstore_p.h>
#include <QtFbSupport/private/qfbwindow_p.h>
#include <QtFontDatabaseSupport/private/qgenericunixfontdatabase_p.h>
#include <QtGui/private/qguiapplication_p.h>
#include <QtServiceSupport/private/qgenericunixservices_p.h>
#include <qpa/qplatforminputcontextfactory_p.h>

#if QT_CONFIG(libinput)
#include <QtInputSupport/private/qlibinputhandler_p.h>
#endif

#if QT_CONFIG(evdev)
#include <QtInputSupport/private/qevdevkeyboardmanager_p.h>
#include <QtInputSupport/private/qevdevmousemanager_p.h>
#endif

#include "qevdevtouchmanager_p.h"

KoboPlatformIntegration::KoboPlatformIntegration(const QStringList &paramList)
    : m_paramList(paramList),
      m_primaryScreen(nullptr),
      m_inputContext(nullptr),
      m_fontDb(new QGenericUnixFontDatabase),
      m_services(new QGenericUnixServices),
      debug(false)
{
    koboDevice = determineDevice();

    if (!m_primaryScreen)
        m_primaryScreen = new KoboFbScreen(paramList, &koboDevice);
}

KoboPlatformIntegration::~KoboPlatformIntegration()
{
    QWindowSystemInterface::handleScreenRemoved(m_primaryScreen);
}

void KoboPlatformIntegration::initialize()
{
    if (m_primaryScreen->initialize())
        QWindowSystemInterface::handleScreenAdded(m_primaryScreen);
    else
        qDebug("kobofb: Failed to initialize screen");

    m_inputContext = QPlatformInputContextFactory::create();

    createInputHandlers();

    qDebug("Platform plugin: Finished initialization.");
    qDebug() << "git hash commit:" << GIT_COMMIT_HASH << "git user:" << GIT_USER << "compilation time:" << COMP_TIME;
}

bool KoboPlatformIntegration::hasCapability(QPlatformIntegration::Capability cap) const
{
    switch (cap)
    {
        case ThreadedPixmaps:
            return false;
        case WindowManagement:
            return false;
        default:
            return QPlatformIntegration::hasCapability(cap);
    }
}

QPlatformBackingStore *KoboPlatformIntegration::createPlatformBackingStore(QWindow *window) const
{
    return new QFbBackingStore(window);
}

QPlatformWindow *KoboPlatformIntegration::createPlatformWindow(QWindow *window) const
{
    return new QFbWindow(window);
}

QAbstractEventDispatcher *KoboPlatformIntegration::createEventDispatcher() const
{
    return createUnixEventDispatcher();
}

QList<QPlatformScreen *> KoboPlatformIntegration::screens() const
{
    QList<QPlatformScreen *> list;
    list.append(m_primaryScreen);
    return list;
}

QPlatformFontDatabase *KoboPlatformIntegration::fontDatabase() const
{
    return m_fontDb.data();
}

QPlatformServices *KoboPlatformIntegration::services() const
{
    return m_services.data();
}

void KoboPlatformIntegration::createInputHandlers()
{
    QRegularExpression touchDevRx("touchscreen_device=(.*)");
    QRegularExpression touchSwapXYRx("touchscreen_swap_xy=(.*)");
    QRegularExpression touchInvXRx("touchscreen_invert_x=(.*)");
    QRegularExpression touchInvYRx("touchscreen_invert_y=(.*)");
    QRegularExpression touchRangeXRx("touchscreen_max_range_x=(.*)");
    QRegularExpression touchRangeYRx("touchscreen_max_range_y=(.*)");
    QRegularExpression touchRangeFlipRx("touchscreen_flip_axes_limit=(.*)");

    bool useHWScreenLimits = true;
    bool manualRangeFlip = false;
    int touchRangeX = 0;
    int touchRangeY = 0;
    bool legacytouchhandler = false;
    bool keyboard = false;
    bool mouse = false;

    auto screenrot = m_primaryScreen->getScreenRotation();

    for (const QString &arg : qAsConst(m_paramList))
    {
        if (arg.contains("debug")) {
            debug = true;
        }
        QRegularExpressionMatch match;
        // Those debug messages will not be seen if 'debug' is the latest argument
        if (arg.startsWith("keyboard"))
        {
            if (debug)
                qDebug() << "Keyboard support enabled";
            keyboard = true;
        }
        if (arg.startsWith("mouse"))
        {
            if (debug)
                qDebug() << "Mouse support enabled";
            mouse = true;
        }
        if (arg.contains(touchSwapXYRx, &match) && match.captured(1).toInt() > 0)
        {
            koboDevice.touchscreenSettings.swapXY = true;
        }
        if (arg.contains(touchInvXRx, &match) && match.captured(1).toInt() > 0)
        {
            koboDevice.touchscreenSettings.invertX = true;
        }
        if (arg.contains(touchInvYRx, &match) && match.captured(1).toInt() > 0)
        {
            koboDevice.touchscreenSettings.invertY = true;
        }
        if (arg.contains(touchRangeXRx, &match))
        {
            touchRangeX = match.captured(1).toInt();
        }
        if (arg.contains(touchRangeYRx, &match))
        {
            touchRangeY = match.captured(1).toInt();
        }
        if (arg.contains(touchRangeFlipRx, &match) && match.captured(1).toInt() > 0)
        {
            manualRangeFlip = true;
        }

        if (koboDevice.mark < 7)
        {
            // forcing usage of moderntouchhandler/QEvdevTouchScreenData2 on old devices,
            // known to cause problems with some old devices such as Kobo Glo
            legacytouchhandler = !arg.contains("moderntouchhandler");
        }
        else
        {
            // forcing usage of legacytouchhandler on new devices, not recommended
            legacytouchhandler = arg.contains("legacytouchhandler");
        }
    }

    bool flipTouchscreenAxes = koboDevice.touchscreenSettings.swapXY ^ (screenrot & 1);
    if (manualRangeFlip)
        flipTouchscreenAxes = !flipTouchscreenAxes;
    if (useHWScreenLimits && touchRangeX == 0)
        touchRangeX = flipTouchscreenAxes ? koboDevice.height : koboDevice.width;
    if (useHWScreenLimits && touchRangeY == 0)
        touchRangeY = flipTouchscreenAxes ? koboDevice.width : koboDevice.height;

    QString evdevTouchArgs = ""; // This is experimental, I yeeted out fixed paths
    if (koboDevice.touchscreenSettings.swapXY)
        evdevTouchArgs += ":swapxy";
    if (koboDevice.touchscreenSettings.invertX)
        evdevTouchArgs += ":invertx";
    if (koboDevice.touchscreenSettings.invertY)
        evdevTouchArgs += ":inverty";
    if (touchRangeX > 0)
        evdevTouchArgs += QString(":hw_range_x_max=%1").arg(touchRangeX);
    if (touchRangeY > 0)
        evdevTouchArgs += QString(":hw_range_y_max=%1").arg(touchRangeY);
    if (legacytouchhandler)
        evdevTouchArgs += ":legacytouchhandler";

    evdevTouchArgs += QString(":screenwidth=%1").arg(koboDevice.width);
    evdevTouchArgs += QString(":screenheight=%1").arg(koboDevice.height);

    evdevTouchArgs += QString(":screenrotation=%1").arg(screenrot * 90);

    new QEvdevTouchManager("EvdevTouch", evdevTouchArgs, this, m_primaryScreen);
    if (debug)
        qDebug() << "device:" << koboDevice.modelName << koboDevice.modelNumber << '\n'
                 << "screen:" << koboDevice.width << koboDevice.height << "dpi:" << koboDevice.dpi
                 << "rotation:" << screenrot;

    // A bit of inspiration: https://github.com/librereader/qpa-einkfb
    if (keyboard == true or mouse == true)
    {
        bool libinputBool = false;
        bool evdevBool = false;  // Probably not needed

#if QT_CONFIG(libinput)
        libinputBool = true;
        if (debug)
            qDebug() << "Using libinput as input backend for peripherals";

        if (keyboard or mouse)
        {
            new QLibInputHandler(QLatin1String("libinput"), QString());
            if (debug)
                qDebug() << "Created instance of QLibInputHandler";
        }
#else
#warning libinput libraries are missing - If you do not care, want to debug something unrelated comment this warning, but if you want to ship a InkBox OS binary, you need to make this work
#endif
        if (debug and !libinputBool)
            qDebug() << "Input backend 'libinput' not found";

#if QT_CONFIG(evdev)
        if (!libinputBool)
        {
            evdevBool = true;
            if (debug)
                qDebug() << "Additional input libraries aren't available; using evdev";
            if (keyboard)
            {
                new QEvdevKeyboardManager(QLatin1String("EvdevKeyboard"), QString(), this);
                if (debug)
                    qDebug() << "Created instance of QEvdevKeyboardManager";
            }
            if (mouse)
            {
                new QEvdevMouseManager(QLatin1String("EvdevMouse"), QString(), this);
                if (debug)
                    qDebug() << "Created instance of QEvdevMouseManager";
            }
        }
#endif

        if (mouse and (evdevBool or libinputBool))
        {
            // This is actually needed to make the cursor appear... Very important
            QMouseEvent *event =
                new QMouseEvent(QEvent::MouseMove, QPoint(koboDevice.width / 2, koboDevice.height / 2),
                                Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            m_primaryScreen->mCursor->pointerEvent(*event);
        }

        if ((libinputBool or evdevBool) and debug)
        {
            qDebug() << "WARNING: Additional command line arguments may be required for input. Please read "
                        "the documentation.";
        }
    }
}

QPlatformNativeInterface *KoboPlatformIntegration::nativeInterface() const
{
    return const_cast<KoboPlatformIntegration *>(this);
}

KoboDeviceDescriptor *KoboPlatformIntegration::deviceDescriptor()
{
    return &koboDevice;
}

QFunctionPointer KoboPlatformIntegration::platformFunction(const QByteArray &function) const
{
    if (function == KoboPlatformFunctions::setFullScreenRefreshModeIdentifier())
        return QFunctionPointer(setFullScreenRefreshModeStatic);
    else if (function == KoboPlatformFunctions::setPartialScreenRefreshModeIdentifier())
        return QFunctionPointer(setPartialScreenRefreshModeStatic);
    else if (function == KoboPlatformFunctions::setFastScreenRefreshModeIdentifier())
        return QFunctionPointer(setFastScreenRefreshModeStatic);
    else if (function == KoboPlatformFunctions::setDefaultWaveformIdentifier())
        return QFunctionPointer(setDefaultWaveformStatic);
    else if (function == KoboPlatformFunctions::setFlashingIdentifier())
        return QFunctionPointer(setFlashingStatic);
    else if (function == KoboPlatformFunctions::toggleNightModeIdentifier())
        return QFunctionPointer(toggleNightModeStatic);
    else if (function == KoboPlatformFunctions::clearScreenIdentifier())
        return QFunctionPointer(clearScreenStatic);
    else if (function == KoboPlatformFunctions::enableDitheringIdentifier())
        return QFunctionPointer(enableDitheringStatic);
    else if (function == KoboPlatformFunctions::doManualRefreshIdentifier())
        return QFunctionPointer(doManualRefreshStatic);
    else if (function == KoboPlatformFunctions::getKoboDeviceDescriptorIdentifier())
        return QFunctionPointer(getKoboDeviceDescriptorStatic);
    return 0;
}

void KoboPlatformIntegration::setFullScreenRefreshModeStatic(WaveForm waveform)
{
    KoboPlatformIntegration *self =
        static_cast<KoboPlatformIntegration *>(QGuiApplicationPrivate::platformIntegration());
    self->m_primaryScreen->setFullScreenRefreshMode(waveform);
}

void KoboPlatformIntegration::setPartialScreenRefreshModeStatic(WaveForm waveform)
{
    KoboPlatformIntegration *self =
        static_cast<KoboPlatformIntegration *>(QGuiApplicationPrivate::platformIntegration());
    self->m_primaryScreen->setPartialScreenRefreshMode(waveform);
}

void KoboPlatformIntegration::setFastScreenRefreshModeStatic(WaveForm waveform)
{
    KoboPlatformIntegration *self =
        static_cast<KoboPlatformIntegration *>(QGuiApplicationPrivate::platformIntegration());
    self->m_primaryScreen->setFastScreenRefreshMode(waveform);
}

void KoboPlatformIntegration::setDefaultWaveformStatic()
{
    KoboPlatformIntegration *self =
        static_cast<KoboPlatformIntegration *>(QGuiApplicationPrivate::platformIntegration());
    self->m_primaryScreen->setDefaultWaveform();
}

void KoboPlatformIntegration::setFlashingStatic(bool v)
{
    KoboPlatformIntegration *self =
        static_cast<KoboPlatformIntegration *>(QGuiApplicationPrivate::platformIntegration());
    self->m_primaryScreen->setFlashing(v);
}

void KoboPlatformIntegration::toggleNightModeStatic()
{
    KoboPlatformIntegration *self =
        static_cast<KoboPlatformIntegration *>(QGuiApplicationPrivate::platformIntegration());
    self->m_primaryScreen->toggleNightMode();
}

void KoboPlatformIntegration::clearScreenStatic(bool waitForCompleted)
{
    KoboPlatformIntegration *self =
        static_cast<KoboPlatformIntegration *>(QGuiApplicationPrivate::platformIntegration());
    self->m_primaryScreen->clearScreen(waitForCompleted);
}

void KoboPlatformIntegration::enableDitheringStatic(bool softwareDithering, bool hardwareDithering)
{
    KoboPlatformIntegration *self =
        static_cast<KoboPlatformIntegration *>(QGuiApplicationPrivate::platformIntegration());
    self->m_primaryScreen->enableDithering(softwareDithering, hardwareDithering);
}

void KoboPlatformIntegration::doManualRefreshStatic(QRect region)
{
    KoboPlatformIntegration *self =
        static_cast<KoboPlatformIntegration *>(QGuiApplicationPrivate::platformIntegration());
    self->m_primaryScreen->doManualRefresh(region);
}

KoboDeviceDescriptor KoboPlatformIntegration::getKoboDeviceDescriptorStatic()
{
    KoboPlatformIntegration *self =
        static_cast<KoboPlatformIntegration *>(QGuiApplicationPrivate::platformIntegration());
    return *self->deviceDescriptor();
}
