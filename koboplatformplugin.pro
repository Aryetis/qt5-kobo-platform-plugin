TARGET = kobo

# Used to set a different QTDIR
include(koboplatformplugin.pri)
isEmpty(CUSTOM_QTDIR) {
QTDIR = /mnt/onboard/.adds/qt-linux-5.15-kobo
} else {
QTDIR = $${CUSTOM_QTDIR}
}
CROSS_TC = arm-kobo-linux-gnueabihf

TEMPLATE = lib
CONFIG += plugin

DEFINES += QT_NO_FOREACH

QMAKE_CXXFLAGS += -Wno-missing-field-initializers

QMAKE_RPATHDIR += ../../lib

QT +=  widgets \
    core-private gui-private \
    service_support-private eventdispatcher_support-private input_support-private \
    fb_support-private fontdatabase_support-private


INCLUDEPATH += $$PWD/src

# FBINK
DEFINES += FBINK_FOR_KOBO
FBInkBuildEvent.input = $$PWD/FBink/*.c $$PWD/FBink/*.h
PHONY_DEPS = .
FBInkBuildEvent.input = PHONY_DEPS
FBInkBuildEvent.output = FBInk
FBInkBuildEvent.clean_commands = $(MAKE) -C $$PWD/FBInk distclean

FBInkBuildEvent.name = building FBInk
FBInkBuildEvent.CONFIG += no_link target_predeps
QMAKE_EXTRA_COMPILERS += FBInkBuildEvent

INCLUDEPATH += $$PWD/FBInk

CONFIG(debug, debug|release) {
    FBInkBuildEvent.commands = CROSS_TC=$$CROSS_TC MINIMAL=1 DRAW=1 DEBUG=1 KOBO=true $(MAKE) -C $$PWD/FBInk pic
    LIBS += -L$$PWD/FBInk/Debug -l:libfbink.a
}

CONFIG(release, debug|release) {
    FBInkBuildEvent.commands = CROSS_TC=$$CROSS_TC MINIMAL=1 DRAW=1 KOBO=true $(MAKE) -C $$PWD/FBInk pic
    LIBS += -L$$PWD/FBInk/Release -l:libfbink.a
}

SOURCES = src/main.cpp \
          src/dither.cpp \
          src/kobodevicedescriptor.cpp \
          src/kobofbscreen.cpp \
          src/koboplatformintegration.cpp \
          src/qevdevtouchdata.cpp \
          src/qevdevtouchdata2.cpp \
          src/qevdevtouchhandlerthread.cpp \
          src/qevdevtouchmanager.cpp \
          src/qevdevtouchhandler.cpp

HEADERS = \
          src/dither.h \
          src/einkenums.h \
          src/kobodevicedescriptor.h \
          src/kobofbscreen.h \
          src/koboplatformfunctions.h \
          src/koboplatformintegration.h \
          src/qevdevtouchdata.h \
          src/qevdevtouchdata2.h \
          src/qevdevtouchfilter_p.h \
          src/qevdevtouchhandler.h \
          src/qevdevtouchhandlerthread.h \
          src/qevdevtouchmanager_p.h


OTHER_FILES += \

target.path =$$QTDIR/plugins/platforms
INSTALLS += target

DISTFILES += \
    koboplatformplugin.json

RESOURCES += \
    resources.qrc

DESTDIR = build/ereader
OBJECTS_DIR = build/ereader/obj
MOC_DIR = build/ereader/moc
RCC_DIR = build/ereader/rcc
UI_DIR = build/ereader/ui

# https://forum.qt.io/topic/150792/how-to-get-git-commit-id-during-build-and-display-it-as-version-information/8
GIT_PATH=$$system(which git)
!isEmpty(GIT_PATH) {
    BUILD_VERSION=$$system($$GIT_PATH rev-parse HEAD)
    BUILD_USER=$$system($$GIT_PATH config user.name)
}
isEmpty(BUILD_VERSION) {
    message("Error: Compilation stopped. Git not found")
    CONFIG += invalid_configuration
}
isEmpty(BUILD_USER) {
    message("Error: Compilation stopped. Git user not found")
    CONFIG += invalid_configuration
}
DEFINES += "GIT_COMMIT_HASH=\"\\\"$$BUILD_VERSION\\\"\""
DEFINES += "GIT_USER=\"\\\"$$BUILD_USER\\\"\""

DATE_PATH=$$system(which date)
!isEmpty(DATE_PATH) {
    COMPILATION_DATE=$$system($$DATE_PATH)
}
isEmpty(COMPILATION_DATE) {
    message("Error: Compilation stopped. date not found")
    CONFIG += invalid_configuration
}
DEFINES += "COMP_TIME=\"\\\"$$COMPILATION_DATE\\\"\""
