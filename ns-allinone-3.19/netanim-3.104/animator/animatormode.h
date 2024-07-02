/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: John Abraham <john.abraham@gatech.edu>
 */

#ifndef AnimatorPlugin_H
#define AnimatorPlugin_H

#include "main/common.h"
#include "animatorconstants.h"
#include "animatorscene.h"
#include "animatorview.h"
#include "animator/resizeablepixmap.h"

#include <QWidget>
#include <QtGui/QToolButton>
#include <QtGui/QSpinBox>
#include <QtGui/QSlider>
#include <QtGui/QLCDNumber>
#include <QVBoxLayout>
#include <QSplitter>
#include <QToolBar>
#include <QStatusBar>
#include <QTime>
#include <QThread>


namespace netanim {

class PacketPersistThread: public QThread
{
public:
    void msleep(unsigned long);
};

class AnimatorMode: public Mode
{
    Q_OBJECT

public:
    // Getters

    static AnimatorMode * getInstance();
    QWidget * getCentralWidget();
    QString getTabName();
    qreal getCurrentNodeSize();
    QGraphicsPixmapItem * getBackground();

    // Setters

    void setParsingCount(uint64_t parsingCount);
    void setVersion(double version);
    void setWPacketDetected();
    void setFocus(bool focus);
    void setCurrentTime(qreal currentTime);

    // Actions

    bool keepAppResponsive();
    void showPopup(QString msg);
    void externalPauseEvent();

private:

    // state
    typedef enum {
        APP_INIT,
        APP_START,
        SYSTEM_RESET_IN_PROGRESS,
        SYSTEM_RESET_COMPLETE,
        PLAYING,
        PAUSING,
        SIMULATION_COMPLETE
    } AnimatorModeState_t;
    double m_version;
    bool m_playing;
    AnimatorModeState_t m_state;
    QTimer * m_updateRateTimer;
    double m_currentTime;
    qreal m_currentZoomFactor;
    bool m_showWiressCircles;
    double m_updateRates[UPDATE_RATE_SLIDER_MAX];
    double m_currentUpdateRate;
    double m_parsedMaxSimulationTime;
    int m_oldTimelineValue;
    QVector <QWidget *> m_toolButtonVector;
    QTime m_appResponsiveTimer;
    bool m_simulationCompleted;
    qreal m_packetPersistTime;
    bool m_wPacketDetected;
    uint64_t m_rxCount;



    //controls
    QVBoxLayout * m_vLayout;
    QLabel * m_gridLinesLabel;
    QLabel * m_nodeSizeLabel;
    QToolButton * m_gridButton;
    QToolButton * m_batteryCapacityButton;
    QSpinBox * m_gridLinesSpinBox;
    QComboBox * m_nodeSizeComboBox;
    QToolButton * m_testButton;
    QToolButton * m_showIpButton;
    QToolButton * m_showMacButton;
    QToolButton * m_showNodeIdButton;
    QToolButton * m_playButton;
    QToolButton * m_fileOpenButton;
    QToolButton * m_zoomInButton;
    QToolButton * m_zoomOutButton;
    QToolButton * m_showWirelessCirclesButton;
    QSlider * m_updateRateSlider;
    QLabel * m_fastRateLabel;
    QLabel * m_slowRateLabel;
    QLCDNumber * m_qLcdNumber;
    QWidget * m_centralWidget;
    QDialog * m_parsingXMLDialog;
    QToolBar * m_topToolBar;
    QToolButton * m_packetStatsButton;
    QSplitter * m_mainSplitter;
    QToolButton * m_nodePositionStatsButton;
    QToolButton * m_nodeTrajectoryButton;
    QLabel * m_timelineSliderLabel;
    QToolBar * m_verticalToolbar;
    QLabel * m_pktFilterFromLabel;
    QComboBox * m_pktFilterFromComboBox;
    QLabel * m_pktFilterToLabel;
    QComboBox * m_pktFilterToComboBox;
    QToolButton * m_blockPacketsButton;
    QToolBar * m_bottomToolbar;
    QLabel * m_bottomStatusLabel;
    QToolButton * m_saveButton;
    QToolButton * m_resetButton;
    QToolButton * m_showMetaButton;
    QComboBox * m_packetPersistenceComboBox;
    QLabel * m_packetPersistenceLabel;
    QProgressBar * m_parseProgressBar;
    QSlider * m_simulationTimeSlider;
    QToolButton * m_addCustomImageButton;
    QToolButton * m_unicastMatchButton;
    QToolButton * m_showRoutePathButton;
    QGraphicsPixmapItem * m_background;

    //functions
    AnimatorMode();
    bool parseXMLTraceFile(QString traceFileName);
    void setLabelStyleSheet();
    void initUpdateRate();
    void enableAllToolButtons(bool show);
    qreal nodeSizeStringToValue(QString nodeSize);
    void systemReset();
    void preParse();
    void postParse();
    void doWirelessDetectedAction();
    void initToolbars();
    void initLabels();
    void initControls();
    void setTopToolbarWidgets();
    void setVerticalToolbarWidgets();
    void setBottomToolbarWidgets();
    void setToolButtonVector();
    void setControlDefaults();
    bool checkSimulationCompleted();
    void doSimulationCompleted();
    void timerCleanup();
    void showParsingXmlDialog(bool show);
    void setProgressBarRange(uint64_t rxCount);
    void init();
    void showAnimatorView(bool show);
    void showPackets(bool show);
    void setMaxSimulationTime(double maxTime);
    void resetBackground();


private slots:
    void testSlot();

    void clickTraceFileOpenSlot();
    void clickZoomInSlot();
    void clickZoomOutSlot();
    void clickSaveSlot();
    void clickResetSlot();
    void clickPlaySlot();
    void clickAddCustomImageSlot();

    void updatePacketPersistenceSlot(QString value);
    void updateTimelineSlot(int value);
    void updateRateTimeoutSlot();
    void updateGridLinesSlot(int value);
    void updateNodeSizeSlot(QString value);
    void updateUpdateRateSlot(int);

    void showGridLinesSlot();
    void showNodeIdSlot();
    void showMetaSlot();
    void showPacketSlot();
    void showWirelessCirclesSlot();
    void showPacketStatsSlot();
    void showNodePositionStatsSlot();
    void showIpSlot();
    void showMacSlot();
    void setUnicastMatchSlot();
    void showRoutePathSlot();
    void showBatteryCapacitySlot();

};


} // namespace netanim

#endif // AnimatorPlugin_H
