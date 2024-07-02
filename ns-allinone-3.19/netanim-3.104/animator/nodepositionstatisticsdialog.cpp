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
#include "nodepositionstatisticsdialog.h"
#include "mobilitymanager.h"
#include "animatorscene.h"
#include <QVBoxLayout>
#include <QFormLayout>

namespace netanim {

NodePositionStatisticsDialog * pNodePositionStatisticsDlg = 0;
NodePositionStatisticsDialog *
NodePositionStatisticsDialog::getInstance()
{
    if(!pNodePositionStatisticsDlg)
    {
        pNodePositionStatisticsDlg = new NodePositionStatisticsDialog;
    }
    return pNodePositionStatisticsDlg;
}

NodePositionStatisticsDialog::NodePositionStatisticsDialog():m_nodePosStatsProgressBar(0)
{
    hide();
    setWindowTitle("Node Position statistics");
}

void
NodePositionStatisticsDialog::setMode(bool show)
{
    if(!show)
    {
        NodeMobilityMgr::getInstance()->hideAllTrajectoryPaths();
        hide();
        qDeleteAll(children());
        m_nodePosStatsProgressBar = 0;
        m_nodePosStatsTable = 0;
    }
    else
    {
        doShow();
    }
}

void
NodePositionStatisticsDialog::doShow()
{

    m_vLayout = new QVBoxLayout;
    m_formLayout = new QFormLayout;
    m_applyButton = new QPushButton("Apply");

    m_nodePosStatsIdComboBox = new QComboBox;
    m_nodePosStatsIdAlt2ComboBox = new QComboBox;
    m_nodePosStatsIdAlt3ComboBox = new QComboBox;
    m_nodePosStatsIdAlt4ComboBox = new QComboBox;
    m_nodePosStatsTrajectoryCheckBox = new QCheckBox;
    m_nodePosStatsProgressLabel = new QLabel;
    m_nodePosStatsProgressBar = new QProgressBar;

    m_vLayout->setSpacing(5);
    QLabel * entryCountLabel = new QLabel(QString::number(NodeMobilityMgr::getInstance()->getEntryCount()));
    m_formLayout->addRow("Entry count", entryCountLabel);

    QStringList nodeList;
    QStringList nodeListAlt;
    nodeList << "All";
    nodeListAlt << "None";
    for(uint32_t i = 0; i < AnimatorScene::getInstance()->getNodeCount(); ++i)
    {
        nodeList << QString::number(i);
        nodeListAlt << QString::number(i);
    }
    m_nodePosStatsIdComboBox->clear();
    m_nodePosStatsIdAlt2ComboBox->clear();
    m_nodePosStatsIdAlt3ComboBox->clear();
    m_nodePosStatsIdAlt4ComboBox->clear();

    m_nodePosStatsIdComboBox->addItems(nodeList);
    m_nodePosStatsIdAlt2ComboBox->addItems(nodeListAlt);
    m_nodePosStatsIdAlt3ComboBox->addItems(nodeListAlt);
    m_nodePosStatsIdAlt4ComboBox->addItems(nodeListAlt);


    m_applyButton->setToolTip("Apply filter");
    m_applyButton->adjustSize();
    //applyButton.setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    connect(m_applyButton, SIGNAL(clicked()), this, SLOT(applyNodePosFilterSlot()));

    m_nodePosStatsProgressLabel->setText("Parsing progress:");
    m_nodePosStatsProgressBar->setRange(0, entryCountLabel->text().toUInt());
    m_nodePosStatsProgressBar->setVisible(false);
    m_nodePosStatsProgressLabel->setVisible(false);


    m_nodePosStatsTable = new QTableWidget;
    m_nodePosStatsTable->setVisible(false);

    m_formLayout->setSpacing(5);
    m_formLayout->addRow("Node Id", m_nodePosStatsIdComboBox);
    m_formLayout->addRow("Add Node Id", m_nodePosStatsIdAlt2ComboBox);
    m_formLayout->addRow("Add Node Id", m_nodePosStatsIdAlt3ComboBox);
    m_formLayout->addRow("Add Node Id", m_nodePosStatsIdAlt4ComboBox);
    m_formLayout->setFormAlignment(Qt::AlignLeft);
    m_formLayout->setLabelAlignment(Qt::AlignLeft);

    m_formLayout->addWidget(m_applyButton);
    m_formLayout->addRow("Show Trajectory", m_nodePosStatsTrajectoryCheckBox);
    m_formLayout->addWidget(m_nodePosStatsProgressLabel);
    m_formLayout->addWidget(m_nodePosStatsProgressBar);

    m_vLayout->addLayout(m_formLayout);
    m_vLayout->addWidget(m_nodePosStatsTable);
    setLayout(m_vLayout);
    setMinimumWidth(m_applyButton->sizeHint().width() * 2);
    setVisible(true);
    setMinimumWidth(NODE_POS_STATS_DLG_WIDTH_MIN);
}


void
NodePositionStatisticsDialog::applyNodePosFilterSlot()
{
    if(!m_nodePosStatsProgressBar)
    {
        QDEBUG("applyNodePosFilterSlot called abnormally. If this happens too often please report this scenario to the developer");
        return;
    }
    m_nodePosStatsProgressBar->setVisible(true);
    m_nodePosStatsProgressLabel->setVisible(true);
    if(m_nodePosStatsTrajectoryCheckBox->isChecked())
    {
        AnimatorScene::getInstance()->showAllLinkItems(false);
        AnimatorScene::getInstance()->showAllNodeItems(false);
    }
    else
    {
        AnimatorScene::getInstance()->showAllLinkItems(true);
        AnimatorScene::getInstance()->showAllNodeItems(true);
    }
    NodeMobilityMgr::getInstance()->populateNodePosTable(m_nodePosStatsIdComboBox->currentText(),
                                            m_nodePosStatsIdAlt2ComboBox->currentText(),
                                            m_nodePosStatsIdAlt3ComboBox->currentText(),
                                            m_nodePosStatsIdAlt4ComboBox->currentText(),
                                            m_nodePosStatsTable,
                                            m_nodePosStatsTrajectoryCheckBox->isChecked(),
                                            m_nodePosStatsProgressBar);



    m_nodePosStatsTable->resizeColumnsToContents();
    setMinimumWidth(m_nodePosStatsTable->sizeHint().width()+5);
    m_nodePosStatsProgressBar->setVisible(false);
    m_nodePosStatsProgressLabel->setVisible(false);
}


} // namespace netanim
