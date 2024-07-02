HEADERS += \
    animator/mobilitymanager.h \
    animator/animxmlparser.h \
    animator/animpacket.h \
    animator/animnode.h \
    animator/animatorview.h \
    animator/animatorscene.h \
    debug/xdebug.h \
    animator/animatormode.h \
    main/netanim.h \
    main/common.h \
    animator/packetstatisticsdialog.h \
    animator/nodepositionstatisticsdialog.h \
    animator/nodetrajectorydialog.h \
    animator/animlink.h \
    animator/linkupdatemanager.h \
    animator/animatorconstants.h \
    animator/nodeupdatemanager.h \
    statistics/statsmode.h \
    statistics/interfacestatsscene.h \
    statistics/statsview.h \
    statistics/statisticsconstants.h \
    statistics/textbubble.h \
    statistics/timevalue.h \
    statistics/routingxmlparser.h \
    statistics/routingstatsscene.h \
    statistics/flowmonstatsscene.h \
    statistics/flowmonxmlparser.h \
    animator/resizeablepixmap.h



SOURCES += \
    animator/mobilitymanager.cpp \
    animator/animxmlparser.cpp \
    animator/animpacket.cpp \
    animator/animnode.cpp \
    animator/animatorview.cpp \
    animator/animatorscene.cpp \
    debug/xdebug.cpp \
    animator/animatormode.cpp \
    main/netanim.cpp \
    animator/packetstatisticsdialog.cpp \
    animator/nodepositionstatisticsdialog.cpp \
    animator/nodetrajectorydialog.cpp \
    animator/animlink.cpp \
    animator/linkupdatemanager.cpp \
    animator/nodeupdatemanager.cpp \
    main/main.cpp \
    statistics/statsmode.cpp \
    statistics/interfacestatsscene.cpp \
    statistics/statsview.cpp \
    statistics/textbubble.cpp \
    statistics/routingxmlparser.cpp \
    statistics/routingstatsscene.cpp \
    statistics/flowmonstatsscene.cpp \
    statistics/flowmonxmlparser.cpp \
    animator/resizeablepixmap.cpp


RESOURCES += \
    animator/animator.qrc

QT += svg

macx {
 CONFIG -= app_bundle
 QMAKESPEC = macx-g++
}

# DEFINES +="Q_MOC_OUTPUT_REVISION=62"

### Uncomment the line below to enable ns-3 related features
# include(ns3.pro)












































