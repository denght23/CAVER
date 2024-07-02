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


#ifndef PACKETSTATISTICSDIALOG_H
#define PACKETSTATISTICSDIALOG_H

#include "main/common.h"
#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QTableWidget>
#include <QListWidget>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>

namespace netanim {

class PacketStatsProtocolCheckBox : public QCheckBox
{
public:
    PacketStatsProtocolCheckBox(QString protocolName);
    QString getProtocol();
private:
    QString m_protocolName;
};



class Packetstatisticsdialog : public QDialog
{
Q_OBJECT

public:
    static Packetstatisticsdialog * getInstance();
    void setAllowClose(bool allow);
    void setMode(bool show);
    QComboBox * m_packetStatsFromIdComboBox;
    QComboBox * m_packetStatsToIdComboBox;
    QLineEdit * m_packetStatsFbTxLineEdit;
    QLineEdit * m_packetStatsRegexEdit;
    QTableWidget * m_packetStatsTable;
    QListWidget * m_packetStatsProtocolListWidget;
    QPushButton * m_packetStatsProtocolFilterSelectAllButton;
    QPushButton * m_packetStatsProtocolFilterDeSelectAllButton;
    QProgressBar * m_packetStatsProgressBar;
    QLabel * m_packetStatsProgressLabel;
    QHBoxLayout * m_hLayout;
    QVBoxLayout * m_vLayout;
    QFormLayout * m_formLayout;


protected:
    void closeEvent(QCloseEvent * event);
protected slots:
    void applyPacketFilterSlot();
    void packetFilterProtocolSelectAllSlot();
    void packetFilterProtocolDeSelectAllSlot();

private:
    Packetstatisticsdialog();
    bool m_allowClose;
    int getProtocolFilterFlags();
    void doShow();


};

} // namespace netanim

#endif // PACKETSTATISTICSDIALOG_H
