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

#include "debug/xdebug.h"
#include "animator/animatorscene.h"
#include "animator/animatormode.h"
#include "routingxmlparser.h"
#include "statisticsconstants.h"
#include "statsmode.h"
#include "statsview.h"
#include "interfacestatsscene.h"
#include "routingstatsscene.h"
#include "flowmonstatsscene.h"
#include "flowmonxmlparser.h"
#include "textbubble.h"
#include "timevalue.h"

#include <vector>
#include <map>
#include <QGraphicsProxyWidget>
#include <QFileDialog>
#include <QApplication>
#include <QMessageBox>

namespace netanim {

StatsMode * pStatsMode = 0;

StatsMode *
StatsMode::getInstance()
{
    if(!pStatsMode)
    {
        pStatsMode = new StatsMode();
    }
    return pStatsMode;
}

StatsMode::StatsMode():
    m_parsingXMLDialog(0),
    m_fileOpenButton(0),
    m_qLcdNumber(0),
    m_simulationTimeSlider(0),
    m_flowMonFileButton(0),
    m_statType(StatsMode::IPMAC),
    m_currentFontSize(10)
{
    init();
}

QWidget *
StatsMode::getCentralWidget()
{
    return m_centralWidget;
}

QString
StatsMode::getTabName()
{
    return "Stats";
}

qreal
StatsMode::getCurrentTime()
{
    return m_currentTime;
}

void
StatsMode::init()
{
    initToolbars();

    StatsView::getInstance()->setScene(InterfaceStatsScene::getInstance());
    m_hLayout = new QHBoxLayout;
    m_hLayout->addWidget(m_nodeToolbarScrollArea);
    m_hLayout->addWidget(StatsView::getInstance());

    m_vLayout = new QVBoxLayout;
    m_vLayout->addWidget(m_topToolbar);
    m_vLayout->setSpacing(0);
    m_vLayout->addLayout(m_hLayout);
    m_vLayout->addWidget(m_bottomToolbar);
    m_centralWidget = new QWidget;
    m_centralWidget->setLayout(m_vLayout);
    setWindowTitle("NetAnim");

    statTypeChangedSlot(IPMAC);
    initControls();
}

void
StatsMode::initControls()
{
   m_fontSizeSpinBox->setValue(STATS_FONTSIZE_DEFAULT);
}

void
StatsMode::initToolbars()
{
    initNodeToolbar();
    initTopToolbar();
    initBottomToolbar();
}

void
StatsMode::initTopToolbar()
{
    m_topToolbar = new QToolBar;
    m_statTypeComboBox = new QComboBox;
    connect(m_statTypeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(statTypeChangedSlot(int)));
    m_statTypeComboBox->addItem("IP-MAC");
    m_statTypeComboBox->addItem("Routing");
    m_statTypeComboBox->addItem("Flow-monitor");
    m_topToolbar->addWidget(m_statTypeComboBox);
    m_fileOpenButton = new QToolButton;
    m_fileOpenButton->setEnabled(false);
    m_fileOpenButton->setToolTip("Open Routing XML trace file");
    m_fileOpenButton->setIcon(QIcon(":/animator_resource/animator_fileopen.svg"));
    connect(m_fileOpenButton,SIGNAL(clicked()), this, SLOT(clickRoutingTraceFileOpenSlot()));
    m_topToolbar->addWidget(m_fileOpenButton);
    QSize iconSize(ICON_WIDTH_DEFAULT, ICON_HEIGHT_DEFAULT);
    m_topToolbar->setIconSize(iconSize);

    m_simulationTimeSlider = new QSlider(Qt::Horizontal);
    m_simulationTimeSlider->setToolTip("Set Simulation Time");
    m_simulationTimeSlider->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(m_simulationTimeSlider, SIGNAL(valueChanged(int)), this, SLOT(updateTimelineSlot(int)));


    m_qLcdNumber = new QLCDNumber;
    m_qLcdNumber->setAutoFillBackground(true);
    m_qLcdNumber->setToolTip("Current simulation time");
    m_qLcdNumber->setStyleSheet("QLCDNumber {background:black; color: black;}");

    m_simTimeLabel = new QLabel("Sim Time");
    m_topToolbar->addWidget(m_simTimeLabel);
    m_topToolbar->addWidget(m_simulationTimeSlider);
    m_topToolbar->addWidget(m_qLcdNumber);

    m_fontSizeLabel = new QLabel("Font Size");
    m_fontSizeSpinBox = new QSpinBox;
    connect(m_fontSizeSpinBox, SIGNAL(valueChanged(int)), this, SLOT(fontSizeSlot(int)));
    m_fontSizeSpinBox->setRange(1, STATS_FONTSIZE_MAX);
    m_topToolbar->addWidget(m_fontSizeLabel);
    m_topToolbar->addWidget(m_fontSizeSpinBox);

    m_flowMonFileButton = new QPushButton("FlowMon file");
    connect(m_flowMonFileButton,SIGNAL(clicked()), this, SLOT(clickFlowMonTraceFileOpenSlot()));
    m_topToolbar->addWidget(m_flowMonFileButton);

}

void
StatsMode::initBottomToolbar()
{
    m_parseProgressBar = new QProgressBar;
    m_bottomStatusLabel = new QLabel;
    m_bottomToolbar = new QToolBar;
    m_bottomToolbar->addWidget(m_bottomStatusLabel);
    m_bottomToolbar->addWidget(m_parseProgressBar);
}


void
StatsMode::initNodeToolbar()
{
    m_nodeToolbarScrollArea = new QScrollArea;
    m_nodeToolbar = new QToolBar;
    m_nodeToolbar->setOrientation(Qt::Vertical);
    m_nodeToolbarScrollArea->setVisible(false);
    m_selectAllNodesButton = new QPushButton ("All");
    connect(m_selectAllNodesButton, SIGNAL(clicked()), this, SLOT(selectAllNodesSlot()));
    m_deselectAllNodesButton = new QPushButton ("None");
    connect(m_deselectAllNodesButton, SIGNAL(clicked()), this, SLOT(deselectAllNodesSlot()));


   /* QPushButton * testButton = new QPushButton("Test");
    connect(testButton, SIGNAL(clicked()), this, SLOT(testSlot()));
    m_nodeToolbar->addWidget(testButton);
*/

}

void
StatsMode::setMaxSimulationTime(double maxTime)
{
    m_parsedMaxSimulationTime = maxTime;
    m_simulationTimeSlider->setRange(0, m_parsedMaxSimulationTime * SIMTIME_SLIDER_MULTIPLIER);
}

void
StatsMode::setMinSimulationTime(double minTime)
{
    m_simulationTimeSlider->setRange(minTime * SIMTIME_SLIDER_MULTIPLIER, m_parsedMaxSimulationTime * SIMTIME_SLIDER_MULTIPLIER);
}

void
StatsMode::setParsingCount(uint64_t parsingCount)
{
    m_bottomStatusLabel->setText("Parsing Count:" + QString::number(parsingCount) + "/" + QString::number(m_rtCount));
    m_parseProgressBar->setValue(parsingCount);
}

void
StatsMode::setProgressBarRange(uint64_t rxCount)
{
    m_parseProgressBar->setMaximum(rxCount);
    m_parseProgressBar->setVisible(true);
}

void
StatsMode::setFocus(bool focus)
{
    //focus?qDebug(QString("Stats Focus")):qDebug(QString("Stats lost Focus"));
    if(focus)
    {
        AnimatorMode::getInstance()->externalPauseEvent();
        InterfaceStatsScene::getInstance()->reloadContent();
        RoutingStatsScene::getInstance()->reloadContent();
        FlowMonStatsScene::getInstance()->reloadContent();
    }
}

qreal
StatsMode::getCurrentFontSize()
{
    return m_currentFontSize;
}

uint32_t
StatsMode::getCurrentNodeCount()
{
    if(m_statType == IPMAC)
    {
        return AnimatorScene::getInstance()->getNodeCount();
    }
    else if (m_statType == Routing)
    {
        return RoutingStatsScene::getInstance()->getNodeCount();
    }
    else if (m_statType == FlowMon)
    {
        return FlowMonStatsScene::getInstance()->getNodeCount();
    }
    return 0;
}

void
StatsMode::addNodesToToolbar(bool zeroIndexed)
{
    m_activeNodes.clear();
    m_nodeToolbarScrollArea->setVisible(false);
    m_hLayout->removeWidget(m_nodeToolbarScrollArea);
    initNodeToolbar();

    uint32_t currentNodeCount = getCurrentNodeCount();
    if(!currentNodeCount)
    {
        return;
    }
    m_nodeButtonVector.clear();
    m_nodeToolbar->addWidget(m_selectAllNodesButton);
    m_nodeToolbar->addWidget(m_deselectAllNodesButton);

    uint32_t i = 0;
    if(!zeroIndexed)
    {
        ++i;
    }
    for (; i < currentNodeCount; ++i)
    {
        NodeButton * button = new NodeButton(i);
        m_nodeButtonVector.push_back(button);
        m_nodeToolbar->addWidget(button);
        m_nodeToolbarScrollArea->setVisible(true);
        if (i<INITIAL_NODES_ENABLED_DEFAULT)
        {
            button->click();
            button->setChecked(true);
        }
    }
    m_nodeToolbarScrollArea->setWidget(m_nodeToolbar);
    m_nodeToolbarScrollArea->setMaximumWidth(m_nodeToolbar->width());
    m_nodeToolbarScrollArea->setVisible(true);
    m_nodeToolbarScrollArea->setMaximumHeight(1000);
    m_hLayout->insertWidget(0, m_nodeToolbarScrollArea);

}

void
StatsMode::systemReset()
{
    InterfaceStatsScene::getInstance()->systemReset();
    addNodesToToolbar();

}

void
StatsMode::showParsingXmlDialog(bool show)
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

bool
StatsMode::parseRoutingXMLTraceFile(QString traceFileName)
{
    m_rtCount = 0;
    RoutingXmlparser parser(traceFileName);
    if(!parser.isFileValid())
    {
        showPopup("Trace file is invalid");
        m_fileOpenButton->setEnabled(true);
        return false;
    }
    routingPreParse();

    showParsingXmlDialog(true);
    m_rtCount = parser.getRtCount();
    setProgressBarRange(m_rtCount);

    parser.doParse();
    showParsingXmlDialog(false);
    setMaxSimulationTime(parser.getMaxSimulationTime());
    setMinSimulationTime(parser.getMinSimulationTime());

    routingPostParse();
    return true;
}

bool
StatsMode::parseFlowMonXMLTraceFile(QString traceFileName)
{
    Q_UNUSED(traceFileName);
    FlowMonXmlparser parser(traceFileName);
    if(!parser.isFileValid())
    {
        showPopup("Trace file is invalid");
        m_fileOpenButton->setEnabled(true);
        return false;
    }
    flowMonPreParse();
    showParsingXmlDialog(true);
    parser.doParse();
    showParsingXmlDialog(false);
    flowMonPostParse();
    return true;
}

void
StatsMode::showPopup(QString msg)
{
    QMessageBox msgBox;
    msgBox.setText(msg);
    msgBox.exec();
}


void
StatsMode::routingPostParse()
{
    m_bottomStatusLabel->setText("Parsing Complete");

}

void
StatsMode::routingPreParse()
{
    m_bottomStatusLabel->setText("Parsing...Please Wait");
    m_parseProgressBar->reset();
    m_parseProgressBar->setVisible(false);
}

void
StatsMode::flowMonPostParse()
{
    m_bottomStatusLabel->setText("Parsing Complete");

}

void
StatsMode::flowMonPreParse()
{
    m_bottomStatusLabel->setText("Parsing...Please Wait");
    m_parseProgressBar->reset();
    m_parseProgressBar->setVisible(false);
}


void
StatsMode::testSlot()
{
    qDebug(QString("testSlot"));

    for(ActiveNodesMap_t::const_iterator i = m_activeNodes.begin();
        i != m_activeNodes.end();
        ++i)
    {
        qDebug(QString::number(i->first));
    }

}

void
StatsMode::fontSizeSlot(int value)
{
    m_currentFontSize = value;
    RoutingStatsScene::getInstance()->reloadContent(true);
    InterfaceStatsScene::getInstance()->reloadContent(true);
    FlowMonStatsScene::getInstance()->reloadContent(true);

}

void
StatsMode::updateTimelineSlot(int value)
{
   if(value == m_oldTimelineValue)
        return;
   m_oldTimelineValue = value;
   m_currentTime = (qreal)value/SIMTIME_SLIDER_MULTIPLIER;
   m_qLcdNumber->display(m_currentTime);
   RoutingStatsScene::getInstance()->reloadContent();
}

void
StatsMode::enableFlowMonControls(bool enable)
{
    if(m_flowMonFileButton)
    {
        m_flowMonFileButton->setEnabled(enable);
    }
}

void
StatsMode::enableIpMacControls(bool enable)
{
    Q_UNUSED(enable);
}

void
StatsMode::enableRoutingStatsControls(bool enable)
{
    if(m_fileOpenButton)
    {
        m_fileOpenButton->setEnabled(enable);
    }
    if(m_simulationTimeSlider)
    {
        m_simulationTimeSlider->setEnabled(enable);
    }
    if(m_qLcdNumber)
    {
        m_qLcdNumber->setEnabled(enable);
    }
}


void
StatsMode::enableControlsForState()
{
    if(m_statType == Routing)
    {
        enableRoutingStatsControls(true);
        enableFlowMonControls(false);
        enableIpMacControls(false);
    }
    else if (m_statType == IPMAC)
    {
        enableRoutingStatsControls(false);
        enableFlowMonControls(false);
        enableIpMacControls(true);

    }
    else if (m_statType == FlowMon)
    {
        enableRoutingStatsControls(false);
        enableFlowMonControls(true);
        enableIpMacControls(false);
    }

}

void
StatsMode::statTypeChangedSlot(int index)
{
    m_statType = (StatType_t) index;
    if(m_fileOpenButton)
    {
        m_fileOpenButton->setEnabled(m_statType == Routing);
    }
    if(m_statType == Routing)
    {
        StatsView::getInstance()->setScene(RoutingStatsScene::getInstance());
    }
    else if (m_statType == IPMAC)
    {
        StatsView::getInstance()->setScene(InterfaceStatsScene::getInstance());
    }
    else if (m_statType == FlowMon)
    {
        StatsView::getInstance()->setScene(FlowMonStatsScene::getInstance());
    }
    enableControlsForState();
}

void
StatsMode::selectAllNodesSlot()
{
    if(m_nodeButtonVector.empty())
    {
        return;
    }
    for(NodeButtonVector_t::const_iterator i = m_nodeButtonVector.begin();
        i != m_nodeButtonVector.end();
        ++i)
    {
        NodeButton * button = *i;
        button->setChecked(true);
        button->buttonClickedSlot();
    }
}


void
StatsMode::deselectAllNodesSlot()
{
    if(m_nodeButtonVector.empty())
    {
        return;
    }
    for(NodeButtonVector_t::const_iterator i = m_nodeButtonVector.begin();
        i != m_nodeButtonVector.end();
        ++i)
    {
        NodeButton * button = *i;
        button->setChecked(false);
        button->buttonClickedSlot();
    }
}

void
StatsMode::clickRoutingTraceFileOpenSlot()
{
    RoutingStatsScene::getInstance()->systemReset();

    QFileDialog fileDialog;
    fileDialog.setFileMode(QFileDialog::ExistingFiles);
    QString traceFileName = "";
    if(fileDialog.exec())
    {
        traceFileName = fileDialog.selectedFiles().at(0);
        //qDebug((traceFileName));
        if(traceFileName != "")
        {
            if(parseRoutingXMLTraceFile(traceFileName))
            {
                m_fileOpenButton->setEnabled(true);
                addNodesToToolbar();
            }
        }
    }
    QApplication::processEvents();

}

void
StatsMode::clickFlowMonTraceFileOpenSlot()
{
    FlowMonStatsScene::getInstance()->systemReset();

    QFileDialog fileDialog;
    fileDialog.setFileMode(QFileDialog::ExistingFiles);
    QString traceFileName = "";
    if(fileDialog.exec())
    {
        traceFileName = fileDialog.selectedFiles().at(0);
        //qDebug((traceFileName));
        if(traceFileName != "")
        {
            if(parseFlowMonXMLTraceFile(traceFileName))
            {
                m_fileOpenButton->setEnabled(true);
                addNodesToToolbar(false);
            }
        }
    }
    QApplication::processEvents();

}

void
StatsMode::setNodeActive(uint32_t nodeId, bool active)
{
    //qDebug("NodeId:" + QString::number(nodeId) + QString::number(active));
    InterfaceStatsScene::getInstance()->systemReset();
    if(m_activeNodes.find(nodeId) == m_activeNodes.end())
    {
        if(active)
        {
            m_activeNodes[nodeId] = active;
        }
        InterfaceStatsScene::getInstance()->reloadContent();
        RoutingStatsScene::getInstance()->reloadContent();
        FlowMonStatsScene::getInstance()->reloadContent();

        return;
    }
    if(active)
    {
        m_activeNodes[nodeId] = active;
    }
    else
    {
        m_activeNodes.erase(nodeId);
    }
    InterfaceStatsScene::getInstance()->reloadContent();
    RoutingStatsScene::getInstance()->reloadContent();
    FlowMonStatsScene::getInstance()->reloadContent();


}

bool
StatsMode::isNodeActive(uint32_t nodeId)
{
    return (m_activeNodes.find(nodeId)!=m_activeNodes.end());
}


NodeButton::NodeButton(uint32_t nodeId): QPushButton(QString::number(nodeId)),m_nodeId(nodeId)
{
    connect(this, SIGNAL(clicked()), this, SLOT(buttonClickedSlot()));
    setCheckable(true);
}

void
NodeButton::buttonClickedSlot()
{
    setNodeActive(isChecked());
}

void
NodeButton::setNodeActive(bool active)
{
    StatsMode::getInstance()->setNodeActive(m_nodeId, active);
}


} // namespace netanim
