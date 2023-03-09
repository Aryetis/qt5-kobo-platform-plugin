#ifndef KOBODEVICEDESCRIPTOR_H
#define KOBODEVICEDESCRIPTOR_H

#include <QtCore>
#include <iostream>

enum KoboDevice
{
    Unknown,
    KoboTouchAB,
    KoboTouchC,
    KoboMini,
    KoboGlo,
    KoboGloHD,
    KoboTouch2,
    KoboAura,
    KoboAuraHD,
    KoboAuraH2O,
    KoboAuraH2O2_v1,
    KoboAuraH2O2_v2,
    KoboAuraOne,
    KoboAura2,
    KoboAura2_v2,
    KoboClaraHD,
    KoboForma,
    KoboLibraH2O,
    KoboNia,
    KoboElipsa
};

struct TouchscreenSettings
{
    bool swapXY = true;
    bool invertX = true;
    bool invertY = false;
    bool hasMultitouch = true;
};

struct KoboDeviceDescriptor
{
    KoboDevice device = Unknown;
    QString modelName = "";
    int modelNumber = 0;
    int mark = 6;
    int dpi;
    int width;
    int height;
    qreal physicalWidth;
    qreal physicalHeight;

    // New devices *may* be REAGL-aware, but generally don't expect explicit REAGL requests, default to not.
    bool isREAGL = false;

    // unused for now
    bool hasGSensor = false;

    TouchscreenSettings touchscreenSettings;

    // MXCFB_WAIT_FOR_UPDATE_COMPLETE ioctls are generally reliable
    bool hasReliableMxcWaitFor = true;
    // Sunxi devices require a completely different fb backend...
    bool isSunxi = false;

    // Stable path to the NTX input device
    QString ntxDev = "/dev/input/event0";
    // Stable path to the Touch input device
    QString touchDev = "/dev/input/event1";
};

KoboDeviceDescriptor determineDevice();

#endif  // KOBODEVICEDESCRIPTOR_H
