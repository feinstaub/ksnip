/*
 *  Copyright (C) 2016 Damir Porobic <https://github.com/damirporobic>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */
#include "SnippingArea.h"

SnippingArea::SnippingArea(QWidget* parent) : QWidget(parent),
    mCursorFactory(new CursorFactory()),
    mConfig(KsnipConfig::instance()),
    mBackground(nullptr)
{
    // Make the frame span across the screen and show above any other widget
    setWindowFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint | Qt::Tool);

    QWidget::setCursor(*mCursorFactory->createSnippingCursor());
}

SnippingArea::~SnippingArea()
{
    delete mCursorFactory;
    delete mBackground;
}

void SnippingArea::showWithoutBackground()
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    clearBackgroundImage();
    show();
}

void SnippingArea::showWithBackground(const QPixmap& background)
{
    setAttribute(Qt::WA_TranslucentBackground, false);
    setBackgroundImage(background);
    show();
}

void SnippingArea::show()
{
    init();
    setFixedSize(QDesktopWidget().size());
    QWidget::show();
    activateWindow();
}

void SnippingArea::setBackgroundImage(const QPixmap& background)
{
    clearBackgroundImage();
    mBackground = new QPixmap(background);
}

void SnippingArea::clearBackgroundImage()
{
    if (mBackground != nullptr) {
        delete mBackground;
    }
}

void SnippingArea::init()
{
    mCursorRulerEnabled = mConfig->cursorRulerEnabled();
    mCursorInfoEnabled = mConfig->cursorInfoEnabled();
    setMouseTracking(mCursorRulerEnabled || mCursorInfoEnabled);
    mMouseIsDown = false;
}

void SnippingArea::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }

    mMouseDownPosition = event->pos();
    updateCapturedArea(mMouseDownPosition, event->pos());
    mMouseIsDown = true;
}

void SnippingArea::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }

    mMouseIsDown = false;
    hide();
    emit areaSelected(mCaptureArea);
}

void SnippingArea::mouseMoveEvent(QMouseEvent* event)
{
    if (mMouseIsDown) {
        updateCapturedArea(mMouseDownPosition, event->pos());
    }
    update();
    QWidget::mouseMoveEvent(event);
}

void SnippingArea::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);

    if (mBackground != nullptr) {
        painter.drawPixmap(geometry(), *mBackground);
    }

    if (mMouseIsDown) {
        painter.setClipRegion(QRegion(geometry()).subtracted(QRegion(mCaptureArea)));
    }

    painter.setBrush(QColor(0, 0, 0, 150));
    painter.drawRect(geometry());

    if (mCursorRulerEnabled && !mMouseIsDown) {
        drawCursorRuler(painter);
    }

    if (mCursorInfoEnabled) {
        if (mMouseIsDown) {
            drawCursorSizeInfo(painter);
        } else {
            drawCursorPositionInfo(painter);
        }
    }

    if (mMouseIsDown) {
        painter.setPen(QPen(Qt::red, 4, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        painter.drawRect(mCaptureArea);
    }

    QWidget::paintEvent(event);
}

void SnippingArea::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        emit cancel();
        close();
    }
    QWidget::keyPressEvent(event);
}

void SnippingArea::updateCapturedArea(const QPoint& pos1, const QPoint& pos2)
{
    mCaptureArea = MathHelper::getRectBetweenTwoPoints(pos1, pos2);
}

QString SnippingArea::createPositionInfoText(int number1, int number2) const
{
    return QString::number(number1) + ", " + QString::number(number2);
}

void SnippingArea::drawCursorRuler(QPainter& painter) const
{
    auto pos = QCursor::pos() + QPoint(2, 2);
    painter.setPen(QPen(Qt::red, 1, Qt::DotLine, Qt::SquareCap, Qt::MiterJoin));
    painter.drawLine(QPointF(pos.x(), geometry().bottom()), QPointF(pos.x(), geometry().top()));
    painter.drawLine(QPointF(geometry().left(), pos.y()), QPointF(geometry().right(), pos.y()));
}

void SnippingArea::drawCursorPositionInfo(QPainter& painter) const
{
    auto pos = QCursor::pos();
    auto text = createPositionInfoText(pos.x(), pos.y());
    auto textBoundingRect = getTextBounding(painter, text);
    textBoundingRect.moveTopLeft(pos + QPoint(10, 8));
    auto boxRect = textBoundingRect;
    textBoundingRect.adjust(0, 0, 4, 4);
    boxRect.adjust(-3, 0, 4, 0);

    painter.setPen(QPen(Qt::red, 1));
    painter.setBrush(QColor(0, 0, 0, 200));

    painter.drawRoundedRect(boxRect, 2, 2);
    painter.drawText(textBoundingRect, text);
}

void SnippingArea::drawCursorSizeInfo(QPainter& painter) const
{
    painter.setPen(QPen(Qt::red, 1));
    drawCursorWidthInfo(painter);
    drawCursorHeightInfo(painter);
}

void SnippingArea::drawCursorWidthInfo(QPainter& painter) const
{
    QPoint lineOffset(0, -10);
    QPoint lineEndOffset(0, -3);
    QPoint widthTextOffset(0, -8);
    QLine lineEndLeft(QPoint(0,0), QPoint(0,6));
    QLine lineEndRight(lineEndLeft);
    QLine line(mCaptureArea.topLeft(), mCaptureArea.topRight());
    auto widthText = QString::number(mCaptureArea.width());

    line.translate(lineOffset);
    lineEndLeft.translate(line.p1() + lineEndOffset);
    lineEndRight.translate(line.p2() + lineEndOffset);

    auto lineCenter = MathHelper::getLineCenter(line);
    auto widthTextBoundingRect = getTextBounding(painter, widthText);
    widthTextBoundingRect.moveCenter(lineCenter + widthTextOffset);
    widthTextBoundingRect.adjust(0,0,2,2);

    painter.drawLine(line);
    painter.drawLine(lineEndLeft);
    painter.drawLine(lineEndRight);
    painter.drawText(widthTextBoundingRect, widthText);
}

void SnippingArea::drawCursorHeightInfo(QPainter& painter) const
{
    QPoint lineOffset(-10, 0);
    QPoint lineEndOffset(-3, 0);
    QPoint heightTextOffset(-5, 0);
    QLine lineEndTop(QPoint(0,0), QPoint(6,0));
    QLine lineEndBottom(lineEndTop);
    QLine line(mCaptureArea.topLeft(), mCaptureArea.bottomLeft());
    auto heightText = QString::number(mCaptureArea.height());

    line.translate(lineOffset);
    lineEndTop.translate(line.p1() + lineEndOffset);
    lineEndBottom.translate(line.p2() + lineEndOffset);

    auto lineCenter = MathHelper::getLineCenter(line);
    auto heightTextBoundingRect = getTextBounding(painter, heightText);
    heightTextOffset.setX(heightTextOffset.x() - (heightTextBoundingRect.width() / 2));
    heightTextBoundingRect.moveCenter(lineCenter + heightTextOffset);

    painter.drawLine(line);
    painter.drawLine(lineEndTop);
    painter.drawLine(lineEndBottom);
    painter.drawText(heightTextBoundingRect, heightText);
}


QRect SnippingArea::getTextBounding(const QPainter& painter, const QString& text) const
{
    auto fontMetric = painter.fontMetrics();
    return fontMetric.boundingRect(text);
}


