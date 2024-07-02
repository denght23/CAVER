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


#include "packetstatisticsdialog.h"
#include "animatormode.h"
#include "animatorview.h"
#include <QCloseEvent>
#include <QFormLayout>

namespace netanim {

Packetstatisticsdialog * pPacketStatisticsDlg = 0;

Packetstatisticsdialog::Packetstatisticsdialog():
    m_packetStatsTable(0),
    m_allowClose(true)
{
    hide();
    setWindowTitle("Packet statistics");
}

Packetstatisticsdialog *
Packetstatisticsdialog::getInstance()
{
    if(!pPacketStatisticsDlg)
    {
        pPacketStatisticsDlg = new Packetstatisticsdialog;
    }
    return pPacketStatisticsDlg;
}

void
Packetstatisticsdialog::setMode(bool show)
{
    if(!show)
    {
        hide();
        if(m_packetStatsTable)
        {
            delete m_packetStatsTable;
            m_packetStatsTable = 0;
        }
        qDeleteAll(children());
    }
    else
    {
        doShow();
    }
}

void
Packetstatisticsdialog::closeEvent(QCloseEvent *event)
{
    if(m_allowClose)
    {
        QDialog::closeEvent(event);
    }
    else
    {
        AnimatorMode::getInstance()->showPopup("Please Wait. Parsing in progress");
        event->ignore();
    }
}

void
Packetstatisticsdialog::setAllowClose(bool allow)
{
    m_allowClose = allow;
}


void
Packetstatisticsdialog::doShow()
{
    resize(AnimatorView::getInstance()->viewport()->size());

    m_packetStatsFromIdComboBox = new QComboBox;
    m_packetStatsToIdComboBox = new QComboBox;
    m_packetStatsFbTxLineEdit = new QLineEdit;
    m_packetStatsRegexEdit = new QLineEdit;
    m_packetStatsTable = new QTableWidget;
    m_packetStatsProtocolListWidget = new QListWidget;
    m_packetStatsProtocolFilterSelectAllButton = new QPushButton;
    m_packetStatsProtocolFilterDeSelectAllButton = new QPushButton;
    m_packetStatsProgressBar = new QProgressBar;
    m_packetStatsProgressLabel = new QLabel;
    m_vLayout = new QVBoxLayout;
    m_hLayout = new QHBoxLayout;
    m_formLayout = new QFormLayout;


    m_vLayout->setSpacing(FORM_LAYOUT_SPACING_DEFAULT);
    QLabel * packetCountLabel = new QLabel(QString::number(AnimPktMgr::getInstance()->getPacketCount()));
    m_formLayout->addRow("Packet count", packetCountLabel);



    QStringList nodeList;
    nodeList << "All";
    for(uint32_t i = 0; i < AnimatorScene::getInstance()->getNodeCount(); ++i)
    {
        nodeList << QString::number(i);
    }
    m_packetStatsFromIdComboBox->clear();
    m_packetStatsToIdComboBox->clear();
    m_packetStatsFromIdComboBox->addItems(nodeList);
    m_packetStatsToIdComboBox->addItems(nodeList);

    QLabel * fbTxLabel = new QLabel("Transmission Time >=");
    fbTxLabel->setToolTip("First bit transmission time greater than and equal to");
    m_packetStatsFbTxLineEdit->setText("0");

    m_packetStatsRegexEdit->setText(".*");

    QPushButton * applyButton = new QPushButton("Apply filter");
    applyButton->setToolTip("Apply filter");
    applyButton->adjustSize();
    applyButton->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    applyButton->setGeometry(0, 0, 5, 5);
    connect(applyButton, SIGNAL(clicked()), this, SLOT(applyPacketFilterSlot()));


    m_packetStatsProgressLabel->setText("Parsing progress:");
    m_packetStatsProgressBar->setRange(0, AnimPktMgr::getInstance()->getPacketCount());
    m_packetStatsProgressBar->setVisible(false);
    m_packetStatsProgressLabel->setVisible(false);


    m_packetStatsTable = new QTableWidget;
    m_packetStatsTable->setVisible(false);

    m_packetStatsProtocolListWidget = new QListWidget(this);
    m_packetStatsProtocolFilterSelectAllButton->setText("Select All");
    m_packetStatsProtocolFilterDeSelectAllButton->setText("DeSelect All");
    connect(m_packetStatsProtocolFilterSelectAllButton, SIGNAL(clicked()), this, SLOT(packetFilterProtocolSelectAllSlot()));
    connect(m_packetStatsProtocolFilterDeSelectAllButton, SIGNAL(clicked()), this, SLOT(packetFilterProtocolDeSelectAllSlot()));


    QListWidgetItem * all = new QListWidgetItem(m_packetStatsProtocolListWidget);
    PacketStatsProtocolCheckBox * allPacketsCB = new PacketStatsProtocolCheckBox("All Packets");
    m_packetStatsProtocolListWidget->setItemWidget(all, allPacketsCB);
    allPacketsCB->setChecked(true);
    m_packetStatsProtocolListWidget->addItem(all);
    QVector <QString> protocolStrings;
    protocolStrings.push_back("Ethernet");
    protocolStrings.push_back("Ppp");
    protocolStrings.push_back("Wifi");
    protocolStrings.push_back("Arp");
    protocolStrings.push_back("Ipv4");
    protocolStrings.push_back("Icmpv4");
    protocolStrings.push_back("Udp");
    protocolStrings.push_back("Tcp");
    protocolStrings.push_back("Aodv");
    protocolStrings.push_back("Olsr");
    protocolStrings.push_back("Dsdv");
    for (QVector <QString>::const_iterator i = protocolStrings.begin();
         i != protocolStrings.end();
         ++i)
    {
        QListWidgetItem * newItem = new QListWidgetItem(m_packetStatsProtocolListWidget);
        m_packetStatsProtocolListWidget->setItemWidget(newItem, new PacketStatsProtocolCheckBox(*i));
        m_packetStatsProtocolListWidget->addItem(newItem);
    }

    m_formLayout->setSpacing(FORM_LAYOUT_SPACING_DEFAULT);
    m_formLayout->setFormAlignment(Qt::AlignLeft);
    m_formLayout->setLabelAlignment(Qt::AlignLeft);
    m_formLayout->addRow("From Node Id", m_packetStatsFromIdComboBox);
    m_formLayout->addRow("To Node Id", m_packetStatsToIdComboBox);
    m_formLayout->addRow("Transmission time >=", m_packetStatsFbTxLineEdit);
    m_formLayout->addRow("Regex", m_packetStatsRegexEdit);
    m_formLayout->addWidget(applyButton);
    m_formLayout->addWidget(m_packetStatsProgressLabel);
    m_formLayout->addWidget(m_packetStatsProgressBar);
    m_formLayout->addWidget(m_packetStatsProtocolFilterSelectAllButton);
    m_formLayout->addWidget(m_packetStatsProtocolFilterDeSelectAllButton);


    m_vLayout->addLayout(m_formLayout);
    m_vLayout->addWidget(m_packetStatsProtocolListWidget);

    m_hLayout->addLayout(m_vLayout);
    m_hLayout->addWidget(m_packetStatsTable);
    m_hLayout->setStretchFactor(m_packetStatsTable, 5);

    setLayout(m_hLayout);
    setVisible(true);
    m_packetStatsProgressBar->reset();

}


void
Packetstatisticsdialog::applyPacketFilterSlot()
{
    QStringList headerList;
    headerList << "Tx Time"
               << "From Node Id"
               << "To Node Id"
               << " Meta Info";
    m_packetStatsTable->clearContents();
    m_packetStatsTable->setRowCount(0);
    m_packetStatsTable->setColumnCount(headerList.count());
    m_packetStatsTable->setHorizontalHeaderLabels(headerList);

    uint32_t progressBarValue = 0;
    m_packetStatsProgressBar->setVisible(true);
    m_packetStatsProgressLabel->setVisible(true);

    setAllowClose(false);
    for(AnimPktMgr::AnimPacketMap_t::const_iterator i = AnimPktMgr::getInstance()->getPackets()->begin();
        i != AnimPktMgr::getInstance()->getPackets()->end();
        ++i)
    {
        AnimPacket * pkt = i->second;
        if(!m_packetStatsTable)
        {
            return;
        }
        if(pkt->m_fbTx < m_packetStatsFbTxLineEdit->text().toDouble())
        {
            pkt->m_selected = false;
            continue;

        }
        if((m_packetStatsFromIdComboBox->currentText() != "All") &&
                (pkt->m_fromId != m_packetStatsFromIdComboBox->currentText().toUInt()))
        {
            pkt->m_selected = false;
            continue;
        }
        if((m_packetStatsToIdComboBox->currentText() != "All") &&
                (pkt->m_toId != m_packetStatsToIdComboBox->currentText().toUInt()))
        {
            pkt->m_selected = false;
            continue;
        }
        QString metaInfo = "";
        if((metaInfo = QString(pkt->getMeta(getProtocolFilterFlags()).c_str())) == "")
        {
            pkt->m_selected = false;
            continue;
        }
        QRegExp rx(m_packetStatsRegexEdit->text());
        if (rx.indexIn(metaInfo) == -1)
        {
            pkt->m_selected = false;
            continue;
        }
        pkt->m_selected = true;

        int row = m_packetStatsTable->rowCount();
        m_packetStatsTable->insertRow(row);
        QTableWidgetItem * wiFbTx = new QTableWidgetItem(QString::number(pkt->m_fbTx));
        QTableWidgetItem * wiFromId = new QTableWidgetItem(QString::number(pkt->m_fromId));
        QTableWidgetItem * wiToId = new QTableWidgetItem(QString::number(pkt->m_toId));
        QTableWidgetItem * wiMeta = new QTableWidgetItem(metaInfo);
        m_packetStatsTable->setItem(row, 0, wiFbTx);
        m_packetStatsTable->setItem(row, 1, wiFromId);
        m_packetStatsTable->setItem(row, 2, wiToId);
        m_packetStatsTable->setItem(row, 3, wiMeta);
        m_packetStatsProgressBar->setValue(progressBarValue++);
        AnimatorMode::getInstance()->keepAppResponsive();
    }
    m_packetStatsTable->setVisible(true);
    setAllowClose(true);
    m_packetStatsProgressBar->setVisible(false);
    m_packetStatsProgressLabel->setVisible(false);
    m_packetStatsTable->adjustSize();
    m_packetStatsTable->resizeColumnsToContents();
}


void
Packetstatisticsdialog::packetFilterProtocolSelectAllSlot()
{
    int rowCount = m_packetStatsProtocolListWidget->count();
    for (int row = 0; row < rowCount; ++row)
    {
      PacketStatsProtocolCheckBox * pCheckBox =
              dynamic_cast<PacketStatsProtocolCheckBox *>
              (m_packetStatsProtocolListWidget->itemWidget(m_packetStatsProtocolListWidget->item(row)));
      pCheckBox->setChecked(true);
    }
}

void
Packetstatisticsdialog::packetFilterProtocolDeSelectAllSlot()
{
    int rowCount = m_packetStatsProtocolListWidget->count();
    for (int row = 0; row < rowCount; ++row)
    {
      PacketStatsProtocolCheckBox * pCheckBox =
              dynamic_cast<PacketStatsProtocolCheckBox *>
              (m_packetStatsProtocolListWidget->itemWidget(m_packetStatsProtocolListWidget->item(row)));
      pCheckBox->setChecked(false);
    }

}


int
Packetstatisticsdialog::getProtocolFilterFlags()
{
    int flags = AnimPacket::None;
    int rowCount = m_packetStatsProtocolListWidget->count();
    for (int row = 0; row < rowCount; ++row)
    {
      PacketStatsProtocolCheckBox * pCheckBox =
              dynamic_cast<PacketStatsProtocolCheckBox *>
              (m_packetStatsProtocolListWidget->itemWidget(m_packetStatsProtocolListWidget->item(row)));
      if(pCheckBox && pCheckBox->isChecked())
      {
       QString protocol = pCheckBox->getProtocol();
       if(protocol == "All Packets")
           flags  |= AnimPacket::All;
       if(protocol == "Arp")
           flags  |= AnimPacket::Arp;
       if(protocol == "Tcp")
           flags  |= AnimPacket::Tcp;
       if(protocol == "Udp")
           flags  |= AnimPacket::Udp;
       if(protocol == "Icmpv4")
           flags  |= AnimPacket::Icmp;
       if(protocol == "Ipv4")
           flags  |= AnimPacket::Ipv4;
       if(protocol == "Ppp")
           flags  |= AnimPacket::Ppp;
       if(protocol == "Ethernet")
           flags  |= AnimPacket::Ethernet;
       if(protocol == "Wifi")
           flags  |= AnimPacket::Wifi;
       if(protocol == "Dsdv")
           flags  |= AnimPacket::Dsdv;
       if(protocol == "Aodv")
           flags  |= AnimPacket::Aodv;
       if(protocol == "Olsr")
           flags  |= AnimPacket::Olsr;
      }

    }
    return flags;

}


PacketStatsProtocolCheckBox::PacketStatsProtocolCheckBox(QString protocolName):
   QCheckBox(protocolName), m_protocolName(protocolName)
{

}


QString
PacketStatsProtocolCheckBox::getProtocol()
{
    return m_protocolName;
}




} // namespace netanim
