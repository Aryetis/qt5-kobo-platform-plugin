#ifndef QLINUXFBINTEGRATION_H
#define QLINUXFBINTEGRATION_H

#include <qpa/qplatformintegration.h>
#include <qpa/qplatformnativeinterface.h>
#include <qpa/qplatformcursor.h>

#include <QtCore/QRegularExpression>

#include "kobofbscreen.h"
#include "koboplatformfunctions.h"

class QAbstractEventDispatcher;
class QFbVtHandler;
class QPlatformCursor;

class KoboPlatformIntegration : public QPlatformIntegration, public QPlatformNativeInterface
{
public:
    explicit KoboPlatformIntegration(const QStringList &paramList);
    ~KoboPlatformIntegration();

    void initialize() override;
    bool hasCapability(QPlatformIntegration::Capability cap) const override;

    QPlatformWindow *createPlatformWindow(QWindow *window) const override;
    QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const override;

    QAbstractEventDispatcher *createEventDispatcher() const override;

    QPlatformFontDatabase *fontDatabase() const override;
    QPlatformServices *services() const override;
    QPlatformInputContext *inputContext() const override { return m_inputContext; }

    QPlatformNativeInterface *nativeInterface() const override;

    QList<QPlatformScreen *> screens() const;

    QFunctionPointer platformFunction(const QByteArray &function) const override;

    KoboDeviceDescriptor *deviceDescriptor();

private:
    void createInputHandlers();
    static void setFullScreenRefreshModeStatic(WaveForm waveform);
    static void setPartialScreenRefreshModeStatic(WaveForm waveform);
    static void setFastScreenRefreshModeStatic(WaveForm waveform);
    static void setDefaultWaveformStatic();
    static void setFlashingStatic(bool v);
    static void toggleNightModeStatic();
    static void clearScreenStatic(bool waitForCompleted);
    static void enableDitheringStatic(bool softwareDithering, bool hardwareDithering);
    static void doManualRefreshStatic(QRect region);
    static KoboDeviceDescriptor getKoboDeviceDescriptorStatic();

    KoboDeviceDescriptor koboDevice;

    QStringList m_paramList;
    KoboFbScreen *m_primaryScreen;
    QPlatformInputContext *m_inputContext;
    QScopedPointer<QPlatformFontDatabase> m_fontDb;
    QScopedPointer<QPlatformServices> m_services;

    bool debug = false; // Default
};

#endif  // QLINUXFBINTEGRATION_H
