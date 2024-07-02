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

#ifndef NODEPOSITIONSTATISTICSDIALOG_H
#define NODEPOSITIONSTATISTICSDIALOG_H

#include "main/common.h"
#include <QDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QProgressBar>
#include <QLabel>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QPushButton>

namespace netanim {
class NodePositionStatisticsDialog : public QDialog
{
Q_OBJECT
public:
    static NodePositionStatisticsDialog * getInstance();
    void setMode(bool show);
private:
    NodePositionStatisticsDialog();
    QComboBox * m_nodePosStatsIdComboBox;
    QComboBox * m_nodePosStatsIdAlt2ComboBox;
    QComboBox * m_nodePosStatsIdAlt3ComboBox;
    QComboBox * m_nodePosStatsIdAlt4ComboBox;
    QCheckBox * m_nodePosStatsTrajectoryCheckBox;
    QTableWidget * m_nodePosStatsTable;
    QProgressBar * m_nodePosStatsProgressBar;
    QLabel * m_nodePosStatsProgressLabel;
    QPushButton * m_applyButton;

    QVBoxLayout * m_vLayout;
    QFormLayout * m_formLayout;

    void doShow();

public slots:
    void applyNodePosFilterSlot();
};

} // namespace netanim

#endif // NODEPOSITIONSTATISTICSDIALOG_H
