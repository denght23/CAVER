#include "textbubble.h"
#include "debug/xdebug.h"
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QMessageBox>
#include <iostream>

using namespace std;
namespace netanim {

QRectF br;

TextBubble::TextBubble(QString title, QString content)
{
    content += '\0';
    QString str = title + "\n";
    QStringList list =  content.split('^');
    foreach (QString s, list)
    {
        str += s + "\n";
    }
    setText(str);
    setFrameStyle(QFrame::Panel | QFrame::Sunken);
    adjustSize();
    setTextInteractionFlags(Qt::TextSelectableByMouse);

}
TextBubble::~TextBubble()
{

}


} //namespace netanim


