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


#include "animatorview.h"
#include "animatorscene.h"
#include "debug/xdebug.h"

#include <QtGui/QGraphicsScene>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QFileDialog>
#include <QSvgGenerator>

namespace netanim {

AnimatorView * pAnimatorView = 0;

AnimatorView::AnimatorView(QGraphicsScene * scene) :
    QGraphicsView(scene),
    m_currentZoomFactor(1)
{
    setRenderHint(QPainter::Antialiasing);
}

AnimatorView *
AnimatorView::getInstance()
{
    if(!pAnimatorView)
    {
        pAnimatorView = new AnimatorView(AnimatorScene::getInstance());
    }
    return pAnimatorView;
}

void
AnimatorView::paintEvent(QPaintEvent *event)
{
    //qDebug(transform);
    QGraphicsView::paintEvent(event);
}


void
AnimatorView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
}

void
AnimatorView::mouseMoveEvent(QMouseEvent * event)
{
    scene()->invalidate();
    QGraphicsView::mouseMoveEvent(event);
}


AnimatorScene *
AnimatorView::getAnimatorScene()
{
    return  AnimatorScene::getInstance();
}

void
AnimatorView::updateTransform()
{
    QTransform transform;

    qreal minDimension = qMin(getAnimatorScene()->getSceneRect().width(), getAnimatorScene()->getSceneRect().height());

    qreal xScale = width()/minDimension;
    qreal yScale = height()/minDimension;
    //qDebug(width(), "Width");
    //qDebug(height(), "height");
    qreal minScale = qMin(xScale, yScale);
    transform.scale(minScale, minScale);
    getAnimatorScene()->setCurrentScale(minScale, minScale);
    setTransform(transform);
}

void
AnimatorView::setCurrentZoomFactor(qreal factor)
{
    if (m_currentZoomFactor < factor)
    {
        scale(1.1, 1.1);
    }
    else
    {
        scale(0.9, 0.9);
    }
    m_currentZoomFactor = factor;
    AnimatorScene::getInstance()->zoomEventComplete();
    update();
}

void
AnimatorView::wheelEvent(QWheelEvent *event)
{
    QGraphicsView::wheelEvent(event);
    update();
}

void
AnimatorView::save()
{
    QString fileName = "";
    fileName = QFileDialog::getSaveFileName();
    if(fileName == "")
    {
        return;
    }
    QSvgGenerator svgGen;
    svgGen.setFileName(fileName);
    svgGen.setSize(QSize(size()));
    QPainter painter(&svgGen);
    painter.setRenderHint(QPainter::Antialiasing);
    render(&painter);
}

void
AnimatorView::fitSceneWithinView()
{
    updateTransform();
}

void
AnimatorView::systemReset()
{
    m_currentZoomFactor = 1;
    resetTransform();
}

void
AnimatorView::postParse()
{
    fitSceneWithinView();
}

int
AnimatorView::getCurrentZoomFactor()
{
    return m_currentZoomFactor;
}

} // namespace netanim
