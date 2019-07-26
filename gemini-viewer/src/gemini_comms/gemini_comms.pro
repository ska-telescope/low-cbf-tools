TEMPLATE = lib

CONFIG += staticlib \
          c++14 \
          warn_on \
          exceptions_off \
          rtti_off

QT += network

DESTDIR = ../../lib

HEADERS += gemini_comms.h pub_client.h

SOURCES += gemini_comms.cpp pub_client.cpp

