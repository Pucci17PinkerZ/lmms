/*
 * LcdWidget.cpp - a widget for displaying numbers in LCD style
 *
 * Copyright (c) 2005-2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
 * Copyright (c) 2008 Paul Giblock <pgllama/at/gmail.com>
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 */



#include <QStyleOptionFrame>
#include <QPainter>
#include <QPolygonF>

#include <cstdint>

#include "LcdWidget.h"
#include "DeprecationHelper.h"
#include "embed.h"
#include "FontHelper.h"


namespace lmms::gui
{

namespace {

// 7-segment bit patterns, indexed by [0-9], then 10 = ' ', 11 = '-'.
// Bits: A(top)=0x01, B(top-right)=0x02, C(bottom-right)=0x04, D(bottom)=0x08,
//       E(bottom-left)=0x10, F(top-left)=0x20, G(middle)=0x40.
constexpr std::uint8_t kSegPatterns[12] =
{
	0x3F, // 0
	0x06, // 1
	0x5B, // 2
	0x4F, // 3
	0x66, // 4
	0x6D, // 5
	0x7D, // 6
	0x07, // 7
	0x7F, // 8
	0x6F, // 9
	0x00, // ' '
	0x40, // '-'
};

// Horizontal segment bar: left tip at (x, y + th/2), spans len, thickness th.
QPolygonF hBar(qreal x, qreal y, qreal len, qreal th)
{
	const qreal t = th / 2.0;
	QPolygonF poly;
	poly << QPointF(x, y + t)
	     << QPointF(x + t, y)
	     << QPointF(x + len - t, y)
	     << QPointF(x + len, y + t)
	     << QPointF(x + len - t, y + th)
	     << QPointF(x + t, y + th);
	return poly;
}

// Vertical segment bar: top tip at (x + th/2, y), spans len, thickness th.
QPolygonF vBar(qreal x, qreal y, qreal len, qreal th)
{
	const qreal t = th / 2.0;
	QPolygonF poly;
	poly << QPointF(x + t, y)
	     << QPointF(x + th, y + t)
	     << QPointF(x + th, y + len - t)
	     << QPointF(x + t, y + len)
	     << QPointF(x, y + len - t)
	     << QPointF(x, y + t);
	return poly;
}

// Draw one 7-segment digit inside cell rect c for pattern pat.
// `on` is the lit-segment color, `off` the faint "ghost" underlay (may be transparent).
void drawSevenSeg(QPainter& p, const QRectF& c, std::uint8_t pat, const QColor& on, const QColor& off)
{
	const qreal w = c.width();
	const qreal h = c.height();
	const qreal th = qMax<qreal>(1.5, w * 0.22); // segment thickness

	const qreal hx = c.left() + th;          // horizontal inset
	const qreal hlen = w - 2.0 * th;         // horizontal length
	const qreal vlen = (h - 3.0 * th) / 2.0; // vertical length
	const qreal midTop = c.top() + (h - th) / 2.0;
	const qreal botTop = c.top() + (h + th) / 2.0;

	p.setPen(Qt::NoPen);

	// Ghost underlay (all 7 segments faint) for the classic FL "8" background.
	if (off.isValid() && off.alpha() > 0)
	{
		p.setBrush(off);
		p.drawPolygon(hBar(hx, c.top(), hlen, th));                  // A
		p.drawPolygon(hBar(hx, midTop, hlen, th));                   // G
		p.drawPolygon(hBar(hx, c.top() + h - th, hlen, th));         // D
		p.drawPolygon(vBar(c.left(), c.top() + th, vlen, th));       // F
		p.drawPolygon(vBar(c.left() + w - th, c.top() + th, vlen, th)); // B
		p.drawPolygon(vBar(c.left(), botTop, vlen, th));             // E
		p.drawPolygon(vBar(c.left() + w - th, botTop, vlen, th));    // C
	}

	p.setBrush(on);
	if (pat & 0x01) { p.drawPolygon(hBar(hx, c.top(), hlen, th)); }                  // A
	if (pat & 0x40) { p.drawPolygon(hBar(hx, midTop, hlen, th)); }                   // G
	if (pat & 0x08) { p.drawPolygon(hBar(hx, c.top() + h - th, hlen, th)); }         // D
	if (pat & 0x20) { p.drawPolygon(vBar(c.left(), c.top() + th, vlen, th)); }       // F
	if (pat & 0x02) { p.drawPolygon(vBar(c.left() + w - th, c.top() + th, vlen, th)); } // B
	if (pat & 0x10) { p.drawPolygon(vBar(c.left(), botTop, vlen, th)); }             // E
	if (pat & 0x04) { p.drawPolygon(vBar(c.left() + w - th, botTop, vlen, th)); }    // C
}

} // namespace

LcdWidget::LcdWidget(QWidget* parent, const QString& name, bool leadingZero) :
	LcdWidget(1, parent, name, leadingZero)
{
}




LcdWidget::LcdWidget(int numDigits, QWidget* parent, const QString& name, bool leadingZero) :
	LcdWidget(numDigits, QString("19green"), parent, name, leadingZero)
{
}




LcdWidget::LcdWidget(int numDigits, const QString& style, QWidget* parent, const QString& name, bool leadingZero) :
	QWidget( parent ),
	m_label(),
	m_textColor( 255, 255, 255 ),
	m_textShadowColor( 64, 64, 64 ),
	m_digitColor( 11, 213, 86 ),
	m_digitOffColor( 11, 213, 86, 36 ),
	m_digitBackgroundColor( 0, 0, 0 ),
	m_lcdVectorial( false ),
	m_numDigits(numDigits),
	m_seamlessLeft(false),
	m_seamlessRight(false),
	m_leadingZero(leadingZero)
{
	initUi( name, style );
}

void LcdWidget::setValue(int value)
{
	QString s = m_textForValue[value];
	if (s.isEmpty())
	{
		s = QString::number(value);
		if (m_leadingZero)
		{
			s = s.rightJustified(m_numDigits, '0');
		}
	}

	if (m_display != s)
	{
		m_display = s;

		update();
	}
}

void LcdWidget::setValue(float value)
{
	if (-1 < value && value < 0)
	{
		QString s = QString::number(static_cast<int>(value));
		s.prepend('-');
		
		if (m_display != s)
		{
			m_display = s;
			update();
		}
	}
	else
	{
		setValue(static_cast<int>(value));
	}
}




QColor LcdWidget::textColor() const
{
	return m_textColor;
}

void LcdWidget::setTextColor( const QColor & c )
{
	m_textColor = c;
}




QColor LcdWidget::textShadowColor() const
{
	return m_textShadowColor;
}

void LcdWidget::setTextShadowColor( const QColor & c )
{
	m_textShadowColor = c;
}




QColor LcdWidget::digitColor() const
{
	return m_digitColor;
}

void LcdWidget::setDigitColor( const QColor & c )
{
	m_digitColor = c;
	update();
}




QColor LcdWidget::digitOffColor() const
{
	return m_digitOffColor;
}

void LcdWidget::setDigitOffColor( const QColor & c )
{
	m_digitOffColor = c;
	update();
}




QColor LcdWidget::digitBackgroundColor() const
{
	return m_digitBackgroundColor;
}

void LcdWidget::setDigitBackgroundColor( const QColor & c )
{
	m_digitBackgroundColor = c;
	update();
}




bool LcdWidget::lcdVectorial() const
{
	return m_lcdVectorial;
}

void LcdWidget::setLcdVectorial( bool v )
{
	m_lcdVectorial = v;
	update();
}




void LcdWidget::paintEvent( QPaintEvent* )
{
	QPainter p( this );

	if (m_lcdVectorial)
	{
		paintVectorial(p);
		return;
	}

	QSize cellSize( m_cellWidth, m_cellHeight );

	QRect cellRect( 0, 0, m_cellWidth, m_cellHeight );

	int margin = 1;  // QStyle::PM_DefaultFrameWidth;
	//int lcdWidth = m_cellWidth * m_numDigits + (margin*m_marginWidth)*2;

//	p.translate( width() / 2 - lcdWidth / 2, 0 ); 
	p.save();

	// Don't skip any space and don't draw margin on the left side in seamless mode
	if (m_seamlessLeft)
	{
		p.translate(0, margin);
	}
	else
	{
		p.translate(margin, margin);
		// Left Margin
		p.drawPixmap(cellRect, m_lcdPixmap,
			QRect(QPoint(charsPerPixmap * m_cellWidth, isEnabled() ? 0 : m_cellHeight), cellSize));

		p.translate(m_marginWidth, 0);
	}

	// Padding
	for( int i=0; i < m_numDigits - m_display.length(); i++ ) 
	{
		p.drawPixmap(cellRect, m_lcdPixmap, QRect(QPoint(10 * m_cellWidth, isEnabled() ? 0 : m_cellHeight), cellSize));
		p.translate( m_cellWidth, 0 );
	}

	// Digits
	for (const auto& digit : m_display)
	{
		int val = digit.digitValue();
		if( val < 0 ) 
		{
			if (digit == '-') val = 11;
			else
				val = 10;
		}
		p.drawPixmap(cellRect, m_lcdPixmap, QRect(QPoint(val * m_cellWidth, isEnabled() ? 0 : m_cellHeight), cellSize));
		p.translate( m_cellWidth, 0 );
	}

	// Right Margin
	p.drawPixmap(QRect(0, 0, m_seamlessRight ? 0 : m_marginWidth - 1, m_cellHeight), m_lcdPixmap,
		QRect(charsPerPixmap * m_cellWidth, isEnabled() ? 0 : m_cellHeight, m_cellWidth / 2, m_cellHeight));

	p.restore();

	// Border
	// When either the left or right edge is seamless, the border drawing must be done
	// by the encapsulating class (usually LcdFloatSpinBox).
	if (!m_seamlessLeft && !m_seamlessRight)
	{
		QStyleOptionFrame opt;
		opt.initFrom(this);
		opt.state = QStyle::State_Sunken;
		opt.rect = QRect(0, 0, m_cellWidth * m_numDigits + (margin + m_marginWidth) * 2 - 1,
			m_cellHeight + (margin * 2));

		style()->drawPrimitive(QStyle::PE_Frame, &opt, &p, this);
	}

	p.resetTransform();

	paintLabel(p);

}




void LcdWidget::paintLabel( QPainter & p )
{
	// Label
	if( !m_label.isEmpty() )
	{
		p.setFont(adjustedToPixelSize(p.font(), DEFAULT_FONT_SIZE));
		p.setPen( textShadowColor() );
		p.drawText(width() / 2 -
				p.fontMetrics().horizontalAdvance(m_label) / 2 + 1,
						height(), m_label);
		p.setPen( textColor() );
		p.drawText(width() / 2 -
				p.fontMetrics().horizontalAdvance(m_label) / 2,
						height() - 1, m_label);
	}
}




void LcdWidget::paintVectorial( QPainter & p )
{
	const int margin = 1;

	// LCD panel background (inside the sunken frame).
	const QRect bg(m_seamlessLeft ? 0 : margin,
	               margin,
	               (m_seamlessRight ? width() : width() - margin) - (m_seamlessLeft ? 0 : margin),
	               m_cellHeight);
	p.fillRect(bg, m_digitBackgroundColor);

	p.save();
	p.setRenderHint(QPainter::Antialiasing, true);

	if (m_seamlessLeft)
	{
		p.translate(0, margin);
	}
	else
	{
		p.translate(margin, margin);
		p.translate(m_marginWidth, 0); // skip left cap space
	}

	QColor on = m_digitColor;
	if (!isEnabled()) { on.setAlpha(qRound(on.alpha() * 0.4)); }
	QColor off = isEnabled() ? m_digitOffColor : QColor(0, 0, 0, 0);

	// Padding (leading blanks) — nothing lit, just advance.
	for (int i = 0; i < m_numDigits - m_display.length(); ++i)
	{
		p.translate(m_cellWidth, 0);
	}

	// Digits.
	for (const QChar& ch : m_display)
	{
		int val = ch.digitValue();
		if (val < 0) { val = (ch == '-') ? 11 : 10; }
		else if (val > 9) { val = 10; }
		drawSevenSeg(p, QRectF(0, 0, m_cellWidth, m_cellHeight), kSegPatterns[val], on, off);
		p.translate(m_cellWidth, 0);
	}

	p.restore();
	p.resetTransform();

	// Border — skipped in seamless mode (drawn by the encapsulating widget).
	if (!m_seamlessLeft && !m_seamlessRight)
	{
		QStyleOptionFrame opt;
		opt.initFrom(this);
		opt.state = QStyle::State_Sunken;
		opt.rect = QRect(0, 0, m_cellWidth * m_numDigits + (margin + m_marginWidth) * 2 - 1,
		                 m_cellHeight + (margin * 2));
		style()->drawPrimitive(QStyle::PE_Frame, &opt, &p, this);
	}

	paintLabel(p);
}




void LcdWidget::setLabel( const QString& label )
{
	m_label = label;
	updateSize();
}




void LcdWidget::setMarginWidth( int width )
{
	m_marginWidth = width;

	updateSize();
}




void LcdWidget::updateSize()
{
	const int marginX1 = m_seamlessLeft ? 0 : 1 + m_marginWidth;
	const int marginX2 = m_seamlessRight ? 0 : 1 + m_marginWidth;
	const int marginY = 1;
	if (m_label.isEmpty())
	{
		setFixedSize(
			m_cellWidth * m_numDigits + marginX1 + marginX2,
			m_cellHeight + (2 * marginY)
		);
	}
	else
	{
		setFixedSize(
			qMax<int>(
				m_cellWidth * m_numDigits + marginX1 + marginX2,
				QFontMetrics(adjustedToPixelSize(font(), DEFAULT_FONT_SIZE)).horizontalAdvance(m_label)
			),
			m_cellHeight + (2 * marginY) + 9
		);
	}

	update();
}




void LcdWidget::initUi(const QString& name , const QString& style)
{
	setEnabled( true );

	setWindowTitle( name );

	// We should make a factory for these or something.
	//m_lcdPixmap = embed::getIconPixmap(QString("lcd_" + style).toUtf8().constData());
	//m_lcdPixmap = embed::getIconPixmap("lcd_19green"); // TODO!!

	m_lcdPixmap = embed::getIconPixmap(QString("lcd_" + style).toUtf8().constData());
	m_cellWidth = m_lcdPixmap.size().width() / LcdWidget::charsPerPixmap;
	m_cellHeight = m_lcdPixmap.size().height() / 2;

	m_marginWidth =  m_cellWidth / 2;

	updateSize();
}

} // namespace lmms::gui
