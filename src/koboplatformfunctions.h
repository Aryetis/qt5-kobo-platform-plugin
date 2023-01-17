#ifndef KOBOPLATFORMFUNCTIONS_H
#define KOBOPLATFORMFUNCTIONS_H

#include <QRect>
#include <QtCore/QByteArray>
#include <QtGui/QGuiApplication>

#include "einkenums.h"
#include "kobodevicedescriptor.h"

class KoboPlatformFunctions
{
public:

    typedef void (*setFullScreenRefreshModeType)(WaveForm waveform);
    static QByteArray setFullScreenRefreshModeIdentifier()
    {
        return QByteArrayLiteral("setFullScreenRefreshMode");
    }

    static void setFullScreenRefreshMode(WaveForm waveform)
    {
        auto func = reinterpret_cast<setFullScreenRefreshModeType>(
            QGuiApplication::platformFunction(setFullScreenRefreshModeIdentifier()));
        if (func)
            func(waveform);
    }

    typedef void (*clearScreenType)(bool waitForCompleted);
    static QByteArray clearScreenIdentifier() { return QByteArrayLiteral("clearScreen"); }

    static void clearScreen(bool waitForCompleted)
    {
        auto func =
            reinterpret_cast<clearScreenType>(QGuiApplication::platformFunction(clearScreenIdentifier()));
        if (func)
            func(waitForCompleted);
    }

    typedef void (*enableDitheringType)(bool softwareDithering, bool hardwareDithering);
    static QByteArray enableDitheringIdentifier() { return QByteArrayLiteral("enableDithering"); }

    static void enableDithering(bool softwareDithering, bool hardwareDithering)
    {
        auto func = reinterpret_cast<enableDitheringType>(
            QGuiApplication::platformFunction(enableDitheringIdentifier()));
        if (func)
            func(softwareDithering, hardwareDithering);
    }

    typedef void (*doManualRefreshType)(QRect region);
    static QByteArray doManualRefreshIdentifier() { return QByteArrayLiteral("doManualRefresh"); }

    static void doManualRefresh(QRect region)
    {
        auto func = reinterpret_cast<doManualRefreshType>(
            QGuiApplication::platformFunction(doManualRefreshIdentifier()));
        if (func)
            func(region);
    }

    typedef KoboDeviceDescriptor (*getKoboDeviceDescriptorType)();
    static QByteArray getKoboDeviceDescriptorIdentifier()
    {
        return QByteArrayLiteral("getKoboDeviceDescriptor");
    }

    static KoboDeviceDescriptor getKoboDeviceDescriptor()
    {
        auto func = reinterpret_cast<getKoboDeviceDescriptorType>(
            QGuiApplication::platformFunction(getKoboDeviceDescriptorIdentifier()));
        if (func)
            return func();

        return KoboDeviceDescriptor();
    }
};

#endif  // KOBOPLATFORMFUNCTIONS_H
