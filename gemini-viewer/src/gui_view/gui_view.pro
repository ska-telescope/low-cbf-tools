DESTDIR = ../../bin
TARGET = gemini-viewer

CONFIG += qt c++14 warn_on exceptions_off rtti_off

# QT specifies which QT modules are used by the project
QT += widgets network
#QT -= core gui # core & gui included in QT by default. Uncomment to remove.

LIBS += -L../../lib -lgemini_comms
INCLUDEPATH += ../gemini_comms

SOURCES += main.cpp mainwindow.cpp event_model.cpp open_fpga_dlg.cpp \
    fpga_window.cpp \
    address_map.cpp \
    download_dialog.cpp
HEADERS += mainwindow.h event_model.h open_fpga_dlg.h \
    fpga_window.h \
    address_map.h \
    download_dialog.h

PRE_TARGETDEPS += ../../lib/libgemini_comms.a
