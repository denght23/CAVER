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

#include "animatormode.h"
#include "animatorscene.h"
#include "animatorview.h"
#include "animxmlparser.h"
#include "debug/xdebug.h"
#include "statistics/statsmode.h"
#include "animator/resizeablepixmap.h"

#include <QFile>
#include <QtCore/QDebug>
#include <QtCore/QtPlugin>
#include <QtGui/QAction>
#include <QtGui/QMenu>
#include <QtGui/QMessageBox>
#include <QtGui/QPushButton>
#include <QtGui/QGraphicsScene>
#include <QtGui/QGraphicsView>
#include <QtGui/QVBoxLayout>
#include <QtGui/QToolButton>
#include <QtGui/QMacStyle>
#include <QGraphicsSimpleTextItem>
#include <QFileDialog>
#include <QXmlStreamReader>
#include <QTimer>
#include <QApplication>
#include <QDesktopWidget>
#include <QThread>


namespace netanim {


AnimatorMode * pAnimatorMode = 0;

AnimatorMode::AnimatorMode():
     m_version(0),
     m_playing(false),
     m_updateRateTimer(0),
     m_currentTime(0),
     m_currentZoomFactor(1),
     m_showWiressCircles(false),
     m_parsedMaxSimulationTime(5000),
     m_oldTimelineValue(0),
     m_simulationCompleted(false),
     m_packetPersistTime(500),
     m_wPacketDetected(false),
     m_parsingXMLDialog(0),
     m_background(0)

{
    init();
}

void
AnimatorMode::init()
{
    m_state = APP_INIT;
    initControls();
    initToolbars();
    m_mainSplitter = new QSplitter;
    m_mainSplitter->addWidget(m_verticalToolbar);
    m_mainSplitter->addWidget(AnimatorView::getInstance());
    m_mainSplitter->addWidget(Packetstatisticsdialog::getInstance());
    m_mainSplitter->addWidget(NodePositionStatisticsDialog::getInstance());

    m_vLayout = new QVBoxLayout;
    m_vLayout->setSpacing(0);
    m_vLayout->addWidget(m_topToolBar);
    m_vLayout->addWidget(m_mainSplitter);
    m_vLayout->addWidget(m_bottomToolbar);

    m_centralWidget = new QWidget;
    m_centralWidget->setLayout(m_vLayout);
    setWindowTitle("NetAnim");
    setControlDefaults();
    m_state = APP_START;

    m_verticalToolbar->adjustSize();

}

AnimatorMode *
AnimatorMode::getInstance()
{
    if(!pAnimatorMode)
    {
        pAnimatorMode = new AnimatorMode;
    }
    return pAnimatorMode;
}

void
AnimatorMode::setFocus(bool focus)
{
    //focus?qDebug(QString("Animator Focus")):qDebug(QString("Animator lost Focus"));
    Q_UNUSED(focus);
}

void
AnimatorMode::setControlDefaults()
{
    // Top Horizontal toolbar

    initUpdateRate();
    m_gridButton->setChecked(false);
    showGridLinesSlot();
    showBatteryCapacitySlot();
    m_gridLinesSpinBox->setValue(GRID_LINES_DEFAULT);
    m_nodeSizeComboBox->setCurrentIndex(NODE_SIZE_DEFAULT);
    m_showNodeIdButton->setChecked(true);
    showNodeIdSlot();

    // Vertical toolbar

    m_showMetaButton->setChecked(true);
    showMetaSlot();
    m_packetStatsButton->setChecked(false);
    showPacketStatsSlot();
    m_showWirelessCirclesButton->setChecked(m_wPacketDetected);
    showWirelessCirclesSlot();


    // Bottom toolbar/Status bar

    m_parseProgressBar->setVisible(false);


    // Scene elements if any

    AnimatorScene::getInstance()->setSceneInfoText("Please select an XML trace file using the file load button on the top-left corner", true);


    enableAllToolButtons(false);
    m_fileOpenButton->setEnabled(true);
}


void
AnimatorMode::setToolButtonVector()
{
    m_toolButtonVector.push_back(m_playButton);
    m_toolButtonVector.push_back(m_updateRateSlider);
    m_toolButtonVector.push_back(m_fastRateLabel);
    m_toolButtonVector.push_back(m_slowRateLabel);
    m_toolButtonVector.push_back(m_gridButton);
    m_toolButtonVector.push_back(m_gridLinesLabel);
    m_toolButtonVector.push_back(m_gridLinesSpinBox);
    m_toolButtonVector.push_back(m_zoomInButton);
    m_toolButtonVector.push_back(m_zoomOutButton);
    m_toolButtonVector.push_back(m_nodeSizeLabel);
    m_toolButtonVector.push_back(m_nodeSizeComboBox);
    m_toolButtonVector.push_back(m_showNodeIdButton);
    m_toolButtonVector.push_back(m_qLcdNumber);
    m_toolButtonVector.push_back(m_saveButton);
    m_toolButtonVector.push_back(m_blockPacketsButton);
    m_toolButtonVector.push_back(m_resetButton);
    m_toolButtonVector.push_back(m_showIpButton);
    m_toolButtonVector.push_back(m_showMacButton);
    m_toolButtonVector.push_back(m_simulationTimeSlider);
    m_toolButtonVector.push_back(m_packetPersistenceLabel);
    m_toolButtonVector.push_back(m_packetPersistenceComboBox);
    m_toolButtonVector.push_back(m_unicastMatchButton);
    m_toolButtonVector.push_back(m_showRoutePathButton);
    m_toolButtonVector.push_back(m_addCustomImageButton);
}

void
AnimatorMode::setBottomToolbarWidgets()
{
    m_bottomToolbar->addWidget(m_bottomStatusLabel);
    m_bottomToolbar->addWidget(m_parseProgressBar);
}

void
AnimatorMode::setVerticalToolbarWidgets()
{
    m_verticalToolbar->addWidget(m_zoomInButton);
    m_verticalToolbar->addWidget(m_zoomOutButton);
    m_verticalToolbar->addSeparator();
    m_verticalToolbar->addWidget(m_showNodeIdButton);
    m_verticalToolbar->addSeparator();
    m_verticalToolbar->addWidget(m_showWirelessCirclesButton);
    m_verticalToolbar->addSeparator();
    m_verticalToolbar->addWidget(m_packetStatsButton);
    m_verticalToolbar->addWidget(m_nodePositionStatsButton);
    m_verticalToolbar->addWidget(m_blockPacketsButton);
    m_verticalToolbar->addWidget(m_saveButton);
    m_verticalToolbar->addWidget(m_resetButton);
    m_verticalToolbar->addWidget(m_showMetaButton);
    m_verticalToolbar->addWidget(m_batteryCapacityButton);
}

void
AnimatorMode::setTopToolbarWidgets()
{
    m_topToolBar->addWidget(m_fileOpenButton);
    m_topToolBar->addSeparator();
    m_topToolBar->addWidget(m_playButton);
    m_topToolBar->addSeparator();
    m_topToolBar->addWidget(m_fastRateLabel);
    m_topToolBar->addWidget(m_updateRateSlider);
    m_topToolBar->addWidget(m_slowRateLabel);
    m_topToolBar->addSeparator();
    //m_topToolBar->addWidget(m_packetPersistenceLabel);
    //m_topToolBar->addWidget(m_packetPersistenceComboBox);
    m_topToolBar->addWidget(m_timelineSliderLabel);
    m_topToolBar->addWidget(m_simulationTimeSlider);
    m_topToolBar->addWidget(m_qLcdNumber);
    m_topToolBar->addSeparator();
    m_topToolBar->addWidget(m_gridButton);
    m_topToolBar->addWidget(m_gridLinesLabel);
    m_topToolBar->addWidget(m_gridLinesSpinBox);
    m_topToolBar->addSeparator();
    m_topToolBar->addWidget(m_nodeSizeLabel);
    m_topToolBar->addWidget(m_nodeSizeComboBox);
    m_topToolBar->addSeparator();
    m_topToolBar->addWidget(m_showIpButton);
    m_topToolBar->addWidget(m_showMacButton);
    m_topToolBar->addWidget(m_unicastMatchButton);
    //m_topToolBar->addWidget(m_showRoutePathButton);
    //m_topToolBar->addWidget(m_testButton);
    m_topToolBar->addWidget(m_addCustomImageButton);
}


void
AnimatorMode::initControls()
{
    initLabels();
    m_fileOpenButton = new QToolButton;
    m_fileOpenButton->setToolTip("Open XML trace file");
    m_fileOpenButton->setIcon(QIcon(":/animator_resource/animator_fileopen.svg"));
    connect(m_fileOpenButton,SIGNAL(clicked()), this, SLOT(clickTraceFileOpenSlot()));


    m_zoomInButton = new QToolButton;
    m_zoomInButton->setToolTip("Zoom in");
    m_zoomInButton->setIcon(QIcon(":/animator_resource/animator_zoomin.svg"));
    connect(m_zoomInButton, SIGNAL(clicked()), this, SLOT(clickZoomInSlot()));


    m_zoomOutButton = new QToolButton;
    m_zoomOutButton->setToolTip("Zoom out");
    m_zoomOutButton->setIcon(QIcon(":/animator_resource/animator_zoomout.svg"));
    connect(m_zoomOutButton, SIGNAL(clicked()), this, SLOT(clickZoomOutSlot()));


    m_gridButton = new QToolButton;
    m_gridButton->setIcon(QIcon(":/animator_resource/animator_grid.svg"));
    m_gridButton->setCheckable(true);
    connect(m_gridButton, SIGNAL(clicked()), this, SLOT(showGridLinesSlot()));

    m_gridLinesSpinBox = new QSpinBox;
    m_gridLinesSpinBox->setToolTip("Set the number of grid lines");
    m_gridLinesSpinBox->setRange(GRID_LINES_LOW, GRID_LINES_HIGH);
    m_gridLinesSpinBox->setSingleStep(GRID_LINES_STEP);
    connect(m_gridLinesSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateGridLinesSlot(int)));

    m_nodeSizeComboBox = new QComboBox;
    m_nodeSizeComboBox->setToolTip("Node Size");
    QStringList nodeSizes;
    nodeSizes << "20%"
              << "40%"
              << "50%"
              << "60%"
              << "80%"
              << "100%"
              << "200%"
              << "300%"
              << "400%"
              << "500%"
              << "600%"
              << "900%"
              << "1000%"
              << "2000%";
    m_nodeSizeComboBox->addItems(nodeSizes);
    connect(m_nodeSizeComboBox, SIGNAL(currentIndexChanged(QString)), this, SLOT(updateNodeSizeSlot(QString)));


    m_packetPersistenceComboBox = new QComboBox;
    m_packetPersistenceComboBox->setToolTip("Duration for which the packet should be seen on screen. \
                                            Useful during wireless transmission which has low propagation delay");
    QStringList persistTimes;
    persistTimes << "1ms"
                 << "10ms"
                 << "50ms"
                 << "100ms"
                 << "250ms"
                 << "500ms"
                 << "1s";
    m_packetPersistenceComboBox->addItems(persistTimes);
    connect(m_packetPersistenceComboBox, SIGNAL(currentIndexChanged(QString)), this, SLOT(updatePacketPersistenceSlot(QString)));
    m_packetPersistenceComboBox->setCurrentIndex(PACKET_PERSIST_DEFAULT);


    m_testButton = new QToolButton;
    m_testButton->setText("T");
    connect(m_testButton, SIGNAL(clicked()), this, SLOT(testSlot()));

    m_showIpButton = new QToolButton;
    m_showIpButton->setText("IP");
    m_showIpButton->setToolTip("Show IPv4 Addresses");
    m_showIpButton->setCheckable(true);
    connect(m_showIpButton, SIGNAL(clicked()), this, SLOT(showIpSlot()));

    m_showMacButton = new QToolButton;
    m_showMacButton->setText("MAC");
    m_showMacButton->setToolTip("Show Mac Addresses");
    m_showMacButton->setCheckable(true);
    connect(m_showMacButton, SIGNAL(clicked()), this, SLOT(showMacSlot()));

    m_unicastMatchButton = new QToolButton;
    m_unicastMatchButton->setText("Unicast only for Wifi");
    m_unicastMatchButton->setToolTip("Show Wifi frames only if dest MAC is the node's unicast MAC or dest IP matches is the node's unicast IP");
    m_unicastMatchButton->setCheckable(true);
    connect(m_unicastMatchButton, SIGNAL(clicked()), this, SLOT(setUnicastMatchSlot()));

    m_showRoutePathButton = new QToolButton;
    m_showRoutePathButton->setText("Route-Path");
    m_showRoutePathButton->setToolTip("Show Route Paths. [EXPERIMENTAL]");
    m_showRoutePathButton->setCheckable(true);
    connect(m_showRoutePathButton, SIGNAL(clicked()), this, SLOT(showRoutePathSlot()));


    m_showNodeIdButton = new QToolButton;
    m_showNodeIdButton->setIcon(QIcon(":/animator_resource/animator_nodeid.svg"));
    m_showNodeIdButton->setToolTip("Show Node Id");
    m_showNodeIdButton->setCheckable(true);
    connect(m_showNodeIdButton, SIGNAL(clicked()), this, SLOT(showNodeIdSlot()));


    m_batteryCapacityButton = new QToolButton();
    m_batteryCapacityButton->setCheckable(true);
    m_batteryCapacityButton->setIcon(QIcon(":/animator_resource/battery_icon_4.png"));
    connect(m_batteryCapacityButton, SIGNAL(clicked()), this, SLOT(showBatteryCapacitySlot()));

    m_playButton = new QToolButton;
    m_playButton->setIcon(QIcon(":/animator_resource/animator_play.svg"));
    m_playButton->setToolTip("Play Animation");
    connect(m_playButton, SIGNAL(clicked()), this, SLOT(clickPlaySlot()));


    m_updateRateSlider = new QSlider(Qt::Horizontal);
    m_updateRateSlider->setToolTip("Animation update interval");
    m_updateRateSlider->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(m_updateRateSlider, SIGNAL(valueChanged(int)), this, SLOT(updateUpdateRateSlot(int)));
    m_updateRateSlider->setRange (0, UPDATE_RATE_SLIDER_MAX);

    m_simulationTimeSlider = new QSlider(Qt::Horizontal);
    m_simulationTimeSlider->setToolTip("Set Simulation Time");
    m_simulationTimeSlider->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(m_simulationTimeSlider, SIGNAL(valueChanged(int)), this, SLOT(updateTimelineSlot(int)));


    m_qLcdNumber = new QLCDNumber;
    m_qLcdNumber->setAutoFillBackground(true);
    m_qLcdNumber->setToolTip("Current simulation time");
    m_qLcdNumber->setStyleSheet("QLCDNumber {background:black; color: black;}");

    m_showWirelessCirclesButton = new QToolButton;
    m_showWirelessCirclesButton->setIcon(QIcon(":/animator_resource/animator_wirelesscircles.svg"));
    m_showWirelessCirclesButton->setToolTip("Toggle Show Wireless Circles Animation");
    m_showWirelessCirclesButton->setCheckable(true);
    connect(m_showWirelessCirclesButton, SIGNAL(clicked()), this, SLOT(showWirelessCirclesSlot()));


    m_packetStatsButton = new QToolButton;
    m_packetStatsButton->setIcon(QIcon(":/animator_resource/animator_packetstats.svg"));
    m_packetStatsButton->setToolTip("Packet filter and statistics");
    connect(m_packetStatsButton, SIGNAL(clicked()), this, SLOT(showPacketStatsSlot()));
    m_packetStatsButton->setCheckable(true);


    m_nodePositionStatsButton = new QToolButton;
    m_nodePositionStatsButton->setIcon(QIcon(":/animator_resource/animator_trajectory.svg"));
    m_nodePositionStatsButton->setToolTip("Node Position statistics");
    connect(m_nodePositionStatsButton, SIGNAL(clicked()), this, SLOT(showNodePositionStatsSlot()));
    m_nodePositionStatsButton->setCheckable(true);


    m_blockPacketsButton = new QToolButton;
    m_blockPacketsButton->setIcon(QIcon(":/animator_resource/animator_showpackets.svg"));
    m_blockPacketsButton->setToolTip("Show packets");
    connect(m_blockPacketsButton, SIGNAL(clicked()), this, SLOT(showPacketSlot()));
    m_blockPacketsButton->setCheckable(true);


    m_saveButton = new QToolButton;
    m_saveButton->setIcon(QIcon(":/animator_resource/animator_save.svg"));
    m_saveButton->setToolTip("Save canvas as an image in SVG format");
    connect(m_saveButton, SIGNAL(clicked()), this, SLOT(clickSaveSlot()));


    m_resetButton = new QToolButton;
    m_resetButton->setText("R");
    m_resetButton->setToolTip("Reset Simulation to start");
    connect(m_resetButton, SIGNAL(clicked()), this, SLOT(clickResetSlot()));

    m_showMetaButton = new QToolButton;
    m_showMetaButton->setText("M");
    m_showMetaButton->setCheckable(true);
    m_showMetaButton->setToolTip("Show Packet meta data");
    connect(m_showMetaButton, SIGNAL(clicked()), this, SLOT(showMetaSlot()));

    m_addCustomImageButton = new QToolButton;
    m_addCustomImageButton->setText("Background");
    m_addCustomImageButton->setToolTip("Add a custome image to the scene");
    connect(m_addCustomImageButton, SIGNAL(clicked()), this, SLOT(clickAddCustomImageSlot()));

    m_parseProgressBar = new QProgressBar;
    setLabelStyleSheet();
}

void
AnimatorMode::initLabels()
{
    m_gridLinesLabel = new QLabel("Lines");
    m_nodeSizeLabel = new QLabel("Node Size");
    m_packetPersistenceLabel = new QLabel("Persist");
    m_fastRateLabel = new QLabel("fast");
    m_fastRateLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_fastRateLabel->setFixedWidth(UPDATE_RATE_LABEL_WIDTH);
    m_slowRateLabel = new QLabel("slow");
    m_slowRateLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_slowRateLabel->setFixedWidth(UPDATE_RATE_LABEL_WIDTH);
    m_timelineSliderLabel = new QLabel("Sim time");
    m_timelineSliderLabel->setToolTip("Set current time");
    m_bottomStatusLabel = new QLabel;
}

void
AnimatorMode::initToolbars()
{
    m_topToolBar = new QToolBar;
    m_verticalToolbar = new QToolBar;
    m_bottomToolbar = new QToolBar;

    QSize iconSize(ICON_WIDTH_DEFAULT, ICON_HEIGHT_DEFAULT);
    m_topToolBar->setIconSize(iconSize);
    m_verticalToolbar->setIconSize(iconSize);
    m_verticalToolbar->setOrientation(Qt::Vertical);
    m_verticalToolbar->setFixedWidth(VERTICAL_TOOLBAR_WIDTH_DEFAULT);

    setTopToolbarWidgets();
    setVerticalToolbarWidgets();
    setBottomToolbarWidgets();
    setToolButtonVector();
}

QWidget *
AnimatorMode::getCentralWidget()
{
    return m_centralWidget;
}

void
AnimatorMode::setLabelStyleSheet()
{
    QString labelStyleSheet = "QLabel {color: black; font: 10px}";
    m_nodeSizeLabel->setStyleSheet(labelStyleSheet);
    m_gridLinesLabel->setStyleSheet(labelStyleSheet);
    m_fastRateLabel->setStyleSheet(labelStyleSheet);
    m_slowRateLabel->setStyleSheet(labelStyleSheet);
    m_packetPersistenceLabel->setStyleSheet(labelStyleSheet);
    m_timelineSliderLabel->setStyleSheet(labelStyleSheet);
}

void
AnimatorMode::enableAllToolButtons(bool show)
{
    for(int i = 0; i < m_toolButtonVector.size(); ++i)
    {
        m_toolButtonVector[i]->setEnabled(show);
    }
}

QString
AnimatorMode::getTabName()
{
    return "Animator";
}

void
AnimatorMode::systemReset()
{
   m_state = SYSTEM_RESET_IN_PROGRESS;
   clickResetSlot();
   setControlDefaults();
   AnimatorView::getInstance()->systemReset();
   AnimatorScene::getInstance()->systemReset();
   m_state = SYSTEM_RESET_COMPLETE;
}

void
AnimatorMode::initUpdateRate()
{

    m_updateRates[0]  = 0.000001;
    m_updateRates[1]  = 0.000002;
    m_updateRates[2]  = 0.000004;
    m_updateRates[3]  = 0.000008;
    m_updateRates[4]  = 0.000016;
    m_updateRates[5]  = 0.000032;
    m_updateRates[6]  = 0.000064;
    m_updateRates[7]  = 0.000125;
    m_updateRates[8]  = 0.000250;
    m_updateRates[9]  = 0.000500;
    m_updateRates[10] = 0.001000;
    m_updateRates[11] = 0.002000;
    m_updateRates[12] = 0.004000;
    m_updateRates[13] = 0.008000;
    m_updateRates[14] = 0.016000;
    m_updateRates[15] = 0.032000;
    m_updateRates[16] = 0.064000;
    m_updateRates[17] = 0.125000;
    m_updateRates[18] = 0.250000;
    m_updateRates[19] = 0.500000;
    m_updateRates[20] = 1.000000;
    m_updateRates[21] = 2.000000;

    m_updateRateSlider->setValue(UPDATE_RATE_SLIDER_DEFAULT);
    if(m_updateRateTimer)
    {
        delete m_updateRateTimer;
    }
    m_updateRateTimer = new QTimer(this);
    m_updateRateTimer->setInterval(m_updateRates[UPDATE_RATE_SLIDER_DEFAULT] * 1000);
    connect(m_updateRateTimer, SIGNAL(timeout()), this, SLOT(updateRateTimeoutSlot()));
}

void
AnimatorMode::doWirelessDetectedAction()
{
    if (!m_wPacketDetected)
    {
        return;
    }
    m_showWirelessCirclesButton->setChecked(m_wPacketDetected);
    showWirelessCirclesSlot();
    m_updateRateSlider->setValue(UPDATE_RATE_SLIDER_WIRELESS_DEFAULT);
    updateUpdateRateSlot(UPDATE_RATE_SLIDER_WIRELESS_DEFAULT);
}

qreal
AnimatorMode::nodeSizeStringToValue(QString nodeSize)
{
    if(nodeSize == "20%")
        return 0.2;
    if(nodeSize == "40%")
        return 0.4;
    if(nodeSize == "50%")
        return 0.5;
    if(nodeSize == "60%")
        return 0.6;
    if(nodeSize == "80%")
        return 0.8;
    if(nodeSize == "100%")
        return 1;
    if(nodeSize == "200%")
        return 2;
    if(nodeSize == "300%")
        return 3;
    if(nodeSize == "400%")
        return 4;
    if(nodeSize == "500%")
        return 5;
    if(nodeSize == "600%")
        return 6;
    if(nodeSize == "900%")
        return 9;
    if(nodeSize == "1000%")
        return 10;
    if(nodeSize == "2000%")
        return 20;
    return 1;
}


void
AnimatorMode::externalPauseEvent()
{
    if(m_state != PLAYING)
    {
        return;
    }
    if(m_playButton->isEnabled())
    {
        clickPlaySlot();
    }
}

void
AnimatorMode::showParsingXmlDialog(bool show)
{
    if(!m_parsingXMLDialog)
    {
        m_parsingXMLDialog = new QDialog(this);
        m_parsingXMLDialog->setWindowTitle("Parsing XML trace file");
        QVBoxLayout * vboxLayout = new QVBoxLayout;
        vboxLayout->addWidget(new QLabel("Please Wait.Parsing XML trace file"));
        m_parsingXMLDialog->setLayout(vboxLayout);
    }
    if(show)
    {
        m_parsingXMLDialog->show();
        m_parsingXMLDialog->raise();
        m_parsingXMLDialog->activateWindow();
    }
    else
    {
        m_parsingXMLDialog->hide();
    }
}

void
AnimatorMode::setCurrentTime(qreal currentTime)
{
    //qDebug(currentTime, "Setting current time");
    m_currentTime = currentTime;
}

void
AnimatorMode::setProgressBarRange(uint64_t rxCount)
{
    m_parseProgressBar->setMaximum(rxCount);
    m_parseProgressBar->setVisible(true);
}

void
AnimatorMode::setMaxSimulationTime(double maxTime)
{
    m_parsedMaxSimulationTime = maxTime;
    m_simulationTimeSlider->setRange(0, m_parsedMaxSimulationTime);
}

bool
AnimatorMode::parseXMLTraceFile(QString traceFileName)
{
    m_wPacketDetected = false;
    m_rxCount = 0;
    Animxmlparser parser(traceFileName);
    if(!parser.isFileValid())
    {
        showPopup("Trace file is invalid");
        m_fileOpenButton->setEnabled(true);
        return false;
    }
    preParse();

    showParsingXmlDialog(true);
    m_rxCount = parser.getRxCount();
    setProgressBarRange(m_rxCount);

    parser.doParse();
    showParsingXmlDialog(false);
    setMaxSimulationTime(parser.getMaxSimulationTime());

    postParse();
    return true;
}

void
AnimatorMode::preParse()
{
    AnimatorScene::getInstance()->preParse();
}

void
AnimatorMode::postParse()
{
    enableAllToolButtons(true);
    doWirelessDetectedAction();
    m_showNodeIdButton->setChecked(true);
    showNodeIdSlot();
    m_gridButton->setChecked(true);
    showGridLinesSlot();
    AnimatorView::getInstance()->postParse();
    AnimatorScene::getInstance()->postParse();
    AnimatorScene::getInstance()->setNodeSize(nodeSizeStringToValue(m_nodeSizeComboBox->currentText()));
    update();
    m_bottomStatusLabel->setText("Parsing complete:Click Play");
    m_parseProgressBar->reset();
    m_showMetaButton->setChecked(AnimPktMgr::getInstance()->getMetaInfoSeen());
    resetBackground();
}

void
AnimatorMode::setWPacketDetected()
{
   m_wPacketDetected = true;
}

void
AnimatorMode::setVersion(double version)
{
    m_version = version;
}

qreal
AnimatorMode::getCurrentNodeSize()
{
   return nodeSizeStringToValue(m_nodeSizeComboBox->currentText());
}

QGraphicsPixmapItem *
AnimatorMode::getBackground()
{
    return m_background;
}

bool
AnimatorMode::keepAppResponsive()
{
    if(m_appResponsiveTimer.elapsed() > APP_RESPONSIVE_INTERVAL)
    {
       QApplication::processEvents();
       m_appResponsiveTimer.restart();
       return true;
    }
    return false;
}

void
AnimatorMode::resetBackground()
{
    if(!m_background)
        return;
    m_background->setZValue(-1000);
    m_background->setPos(0, 0);
    m_background->setFlags(QGraphicsItem::ItemIsMovable|QGraphicsItem::ItemIsSelectable);


    // Modify below as required
    m_background->scale(0.3, 0.3);
    m_background->scale(0.5, 1);
}

void
AnimatorMode::setParsingCount(uint64_t parsingCount)
{
    m_bottomStatusLabel->setText("Parsing Count:" + QString::number(parsingCount) + "/" + QString::number(m_rxCount));
    m_parseProgressBar->setValue(parsingCount);
}

void
AnimatorMode::doSimulationCompleted()
{
    m_updateRateTimer->stop();
    m_playButton->setEnabled(false);
    QApplication::processEvents();
    clickResetSlot();
    m_bottomStatusLabel->setText("Simulation Completed");
}
bool
AnimatorMode::checkSimulationCompleted()
{
   // QDEBUG("Current Time:" + QString::number(m_currentTime));
   // QDEBUG("Max Time:" + QString::number(m_parsedMaxSimulationTime));

    if(m_currentTime > m_parsedMaxSimulationTime)
    {
       QApplication::processEvents();
       m_simulationCompleted = true;
       m_state = SIMULATION_COMPLETE;
       return true;
    }
    return false;
}

void
AnimatorMode::timerCleanup()
{
    m_updateRateTimer->stop();
    m_currentTime = 0;
    m_qLcdNumber->display(0);
    fflush(stdout);
}

void
AnimatorMode::showPopup(QString msg)
{
    if(m_state == APP_INIT)
    {
        return;
    }
    QMessageBox msgBox;
    msgBox.setText(msg);
    msgBox.exec();
}

void
AnimatorMode::showAnimatorView(bool show)
{
    if(m_state != APP_START)
    {
        enableAllToolButtons(show);
    }
    AnimatorView::getInstance()->setVisible(show);
}

void
AnimatorMode::showPackets(bool show)
{
    m_blockPacketsButton->setChecked(!show);
    showPacketSlot();
}

void
PacketPersistThread::msleep(unsigned long interval)
{
   QThread::msleep(interval);
}

// Slots
void
AnimatorMode::showMetaSlot()
{
    if(!AnimatorScene::getInstance()->setShowMeta(m_showMetaButton->isChecked(), (m_state == SYSTEM_RESET_IN_PROGRESS) || (m_state == APP_INIT)))
    {
        m_showMetaButton->setChecked(false);
    }
}


void
AnimatorMode::clickSaveSlot()
{
    AnimatorView::getInstance()->save();
}

 void
 AnimatorMode::showPacketSlot()
 {
    AnimatorScene::getInstance()->setBlockPacketRendering(m_blockPacketsButton->isChecked());
    if(m_blockPacketsButton->isChecked())
    {
        m_blockPacketsButton->setToolTip("Show packets");
    }
    else
    {
        m_blockPacketsButton->setToolTip("Don't Show packets");
    }
 }


 void
 AnimatorMode::clickResetSlot()
 {
    timerCleanup();
    AnimatorScene::getInstance()->softReset();
    m_playing = false;
    m_playButton->setIcon(QIcon(":/animator_resource/animator_play.svg"));
    m_playButton->setToolTip("Play Animation");
    m_playButton->setEnabled(true);
 }

 void
 AnimatorMode::updateTimelineSlot(int value)
 {
    if(value == m_oldTimelineValue)
         return;
    m_oldTimelineValue = value;
    m_currentTime = value;
    if(m_nodePositionStatsButton->isChecked())
    {
        if(!m_playing)
            clickPlaySlot();
    }
    updateRateTimeoutSlot();
 }

 void
 AnimatorMode::showNodePositionStatsSlot()
 {
     showAnimatorView(true);
     showPackets(!m_nodePositionStatsButton->isChecked());
     if(!AnimatorScene::getInstance()->showNodePosStats(m_nodePositionStatsButton->isChecked()))
     {
         m_nodePositionStatsButton->setChecked(false);
         showPackets(true);
     }
 }

 void
 AnimatorMode::showRoutePathSlot()
 {
     AnimatorScene::getInstance()->setShowRoutePath(m_showRoutePathButton->isChecked());
 }

void
AnimatorMode::showPacketStatsSlot()
{
    showAnimatorView(!m_packetStatsButton->isChecked());
    if(!AnimatorScene::getInstance()->showPacketStats(m_packetStatsButton->isChecked()))
    {
        showAnimatorView(true);
        m_packetStatsButton->setChecked(false);
    }
}

 void
 AnimatorMode::showWirelessCirclesSlot()
 {
    m_showWiressCircles = m_showWirelessCirclesButton->isChecked();
    AnimatorScene::getInstance()->setShowWirelessCircles(m_showWiressCircles);
 }

 void
 AnimatorMode::clickZoomInSlot()
 {
    AnimatorView::getInstance()->setCurrentZoomFactor(++m_currentZoomFactor);
 }

 void
 AnimatorMode::clickZoomOutSlot()
 {
     if(m_currentZoomFactor == 1)
         return;
     AnimatorView::getInstance()->setCurrentZoomFactor(--m_currentZoomFactor);
 }

 void
 AnimatorMode::updateRateTimeoutSlot()
 {
     AnimatorScene::getInstance()->invalidate();
     m_updateRateTimer->stop();
     m_simulationTimeSlider->setValue(m_currentTime);
     m_qLcdNumber->display(m_currentTime);
     fflush(stdout);
     if(m_playing)
     {
         keepAppResponsive();

         AnimatorScene::getInstance()->purgeOldPackets(m_currentTime);
         qreal lastTime = m_currentTime;
         AnimatorScene::getInstance()->timeToUpdate(m_currentTime);
         if(m_state == SIMULATION_COMPLETE)
         {
             doSimulationCompleted();
             return;
         }
         checkSimulationCompleted();
         QApplication::processEvents();
         if(lastTime == m_currentTime)
         {
             m_currentTime += 0.0001;
         }
         if(lastTime > m_currentTime)
         {
             m_currentTime = PACKET_TIME_MAX;
         }
         m_updateRateTimer->start();
     }
 }

 void
 AnimatorMode::updateUpdateRateSlot(int value)
 {
     m_currentUpdateRate = m_updateRates[value];
     if(m_updateRateTimer)
     {
         m_updateRateTimer->setInterval(m_currentUpdateRate * 1000);
     }
     AnimatorScene::getInstance()->setCurrentUpdateRate(m_currentUpdateRate);
 }

 void
 AnimatorMode::clickAddCustomImageSlot()
 {
     QFileDialog fileDialog;
     fileDialog.setFileMode(QFileDialog::ExistingFiles);
     QString traceFileName = "";
     if(fileDialog.exec())
     {
         traceFileName = fileDialog.selectedFiles().at(0);
         //qDebug((traceFileName));
         if(traceFileName != "")
         {
             QPixmap pixmap(traceFileName);
             m_background = new QGraphicsPixmapItem(pixmap);
             AnimatorScene::getInstance()->addItem(m_background);
             resetBackground();
         }
     }

 }

 void
 AnimatorMode::clickTraceFileOpenSlot()
 {
    StatsMode::getInstance()->systemReset();
    systemReset();

    QFileDialog fileDialog;
    fileDialog.setFileMode(QFileDialog::ExistingFiles);
    QString traceFileName = "";
    if(fileDialog.exec())
    {
        traceFileName = fileDialog.selectedFiles().at(0);
        //qDebug((traceFileName));
        if(traceFileName != "")
        {
            if(parseXMLTraceFile(traceFileName))
                m_fileOpenButton->setEnabled(true);
        }
    }
    StatsMode::getInstance()->systemReset();
    QApplication::processEvents();
 }

 void
 AnimatorMode::clickPlaySlot()
 {
    m_playing = !m_playing;
    if(m_playing)
    {
        m_state = PLAYING;
        m_bottomStatusLabel->setText("Playing");
        if(m_simulationCompleted)
        {
            AnimatorScene::getInstance()->softReset();
            m_simulationCompleted = false;
        }
        m_appResponsiveTimer.restart();
        AnimatorScene::getInstance()->setNodeSize(nodeSizeStringToValue(m_nodeSizeComboBox->currentText()));
        AnimatorScene::getInstance()->prepareForPlay();
        m_playButton->setIcon(QIcon(":/animator_resource/animator_pause.svg"));
        m_playButton->setToolTip("Pause Animation");
        m_updateRateTimer->start();

    }
    else
    {
        m_state = PAUSING;
        m_bottomStatusLabel->setText("Not Playing");
        m_playButton->setIcon(QIcon(":/animator_resource/animator_play.svg"));
        m_playButton->setToolTip("Play Animation");
        m_updateRateTimer->stop();
    }
 }


 void
 AnimatorMode::testSlot()
 {
    AnimatorScene::getInstance()->test();
 }


 void
 AnimatorMode::updateNodeSizeSlot(QString value)
 {
    AnimatorScene::getInstance()->setNodeSize(nodeSizeStringToValue(value));
 }

 void
 AnimatorMode::updatePacketPersistenceSlot(QString value)
 {
    /* persistTimes << "10ms"
                  << "50ms"
                  << "100ms"
                  << "250ms"
                  << "500ms"
                  << "1s";*/
     if(value == "1ms")
         m_packetPersistTime = 1;
     if(value == "10ms")
         m_packetPersistTime = 10;
     if(value == "50ms")
         m_packetPersistTime = 50;
     if(value == "100ms")
         m_packetPersistTime = 100;
     if(value == "250ms")
         m_packetPersistTime = 250;
     if(value == "500ms")
         m_packetPersistTime = 500;
     if(value == "1s")
         m_packetPersistTime = 2000;

 }

 void
 AnimatorMode::showNodeIdSlot()
 {
    AnimatorScene::getInstance()->setShowNodeId(m_showNodeIdButton->isChecked());
    if(m_showNodeIdButton->isChecked())
    {
        m_showNodeIdButton->setToolTip("Don't show Node Id");
    }
    else
    {
        m_showNodeIdButton->setToolTip("Show Node Id");
    }
 }

 void
  AnimatorMode::showBatteryCapacitySlot()
  {
	 AnimatorScene::getInstance()->setShowBatteryCapacity(m_batteryCapacityButton->isChecked());
	 if(m_batteryCapacityButton->isChecked())
	 {
		 m_batteryCapacityButton->setToolTip("Don't show remaining capacity");
	 }
	 else
	 {
		 m_batteryCapacityButton->setToolTip("Show remaining capacity");
	 }
  }


 void
 AnimatorMode::showIpSlot()
 {
     AnimatorScene::getInstance()->setShowInterfaceTexts(m_showIpButton->isChecked(), m_showMacButton->isChecked());
 }

 void
 AnimatorMode::showMacSlot()
 {
     AnimatorScene::getInstance()->setShowInterfaceTexts(m_showIpButton->isChecked(), m_showMacButton->isChecked());
 }

 void
 AnimatorMode::showGridLinesSlot()
 {
     AnimatorScene::getInstance()->setShowGrid(m_gridButton->isChecked());
     if(m_gridButton->isChecked())
     {
         m_gridButton->setToolTip("Turn OFF Grid");
     }
     else
     {
         m_gridButton->setToolTip("Turn ON Grid");
     }
 }

 void
 AnimatorMode::setUnicastMatchSlot()
 {
    AnimatorScene::getInstance()->setUnicastMatch(m_unicastMatchButton->isChecked());
 }


 void
 AnimatorMode::updateGridLinesSlot(int value)
 {
    AnimatorScene::getInstance()->setGridLinesCount(value);
 }





} // namespace netanim
