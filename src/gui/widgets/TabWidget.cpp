/*
 * TabWidget.cpp - tabwidget for LMMS
 *
 * Copyright (c) 2005-2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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


#include "TabWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QToolTip>
#include <QWheelEvent>

#include "DeprecationHelper.h"
#include "embed.h"
#include "FontHelper.h"

namespace lmms::gui
{

// --- Palette FL Studio (valeurs ≈, à verrouiller — cf. DESIGN-UI.md §1.1) ---------
// Regroupées ici en constantes nommées (règle du projet : pas d'hex éparpillés).
// Elles servent uniquement de *défauts* aux qproperty ci-dessous : le QSS du thème
// peut les surcharger à tout moment via qproperty-tabBevelLight / -tabBevelDark /
// -tabInactive.
namespace {
const QColor FL_BEVEL_LIGHT(0x5C, 0x61, 0x63); // #5C6163 — arête claire (haut/gauche)
const QColor FL_BEVEL_DARK(0x1B, 0x1E, 0x1F);  // #1B1E1F — arête sombre (bas/droite)
const QColor FL_TAB_INACTIVE(0x10, 0x12, 0x14); // ≈ #101214 — fond onglet inactif (plus sombre)
constexpr qreal FL_TAB_CORNER_RADIUS = 2.0;     // "coins ~2px" (DESIGN-UI.md §1.1/§1.3)

// Dessine un onglet au look FL Studio : face plate + bevel 1px + coins arrondis.
// Le bevel est la "signature FL" : arête claire en haut/gauche, sombre en bas/droite.
//  - rect        : géométrie de l'onglet
//  - face        : couleur de fond (actif rehaussé / inactif plus sombre)
//  - bevelLight  : couleur de l'arête claire (haut/gauche)
//  - bevelDark   : couleur de l'arête sombre (bas/droite)
void drawFlTab(QPainter& p, const QRectF& rect, const QColor& face,
			   const QColor& bevelLight, const QColor& bevelDark)
{
	// Face de l'onglet avec coins arrondis (~2px)
	QPainterPath path;
	path.addRoundedRect(rect, FL_TAB_CORNER_RADIUS, FL_TAB_CORNER_RADIUS);
	p.fillPath(path, face);

	// Bevel 1px : on trace d'abord le contour complet en clair…
	p.setPen(QPen(bevelLight, 1));
	p.drawPath(path);

	// …puis on repasse en sombre uniquement sur bas + droite, en suivant le coin
	// arrondi bas-droit (quadTo avec le centre du cercle du coin) pour un rendu net.
	QPainterPath darkEdge;
	darkEdge.moveTo(rect.left(), rect.bottom());
	darkEdge.lineTo(rect.right() - FL_TAB_CORNER_RADIUS, rect.bottom());
	darkEdge.quadTo(rect.right(), rect.bottom(),
					rect.right(), rect.bottom() - FL_TAB_CORNER_RADIUS);
	darkEdge.lineTo(rect.right(), rect.top());
	p.setPen(QPen(bevelDark, 1));
	p.drawPath(darkEdge);
}
} // namespace

TabWidget::TabWidget(const QString& caption, QWidget* parent, bool usePixmap,
					 bool resizable) :
	QWidget(parent),
	m_resizable(resizable),
	m_activeTab(0),
	m_caption(caption),
	m_usePixmap(usePixmap),
	m_tabText(0, 0, 0),
	m_tabTitleText(0, 0, 0),
	m_tabSelected(0, 0, 0),
	m_tabTextSelected(0, 0, 0),
	m_tabBackground(0, 0, 0),
	m_tabBorder(0, 0, 0),
	m_tabBevelLight(FL_BEVEL_LIGHT),
	m_tabBevelDark(FL_BEVEL_DARK),
	m_tabInactive(FL_TAB_INACTIVE)
{

	// Create taller tabbar when it's to display artwork tabs
	m_tabbarHeight = usePixmap ? GRAPHIC_TAB_HEIGHT : TEXT_TAB_HEIGHT;

	m_tabheight = caption.isEmpty() ? m_tabbarHeight - 3 : m_tabbarHeight - 4;

	setFont(adjustedToPixelSize(font(), DEFAULT_FONT_SIZE));

	setAutoFillBackground(true);
	QColor bg_color = QApplication::palette().color(QPalette::Active, QPalette::Window).darker(132);
	QPalette pal = palette();
	pal.setColor(QPalette::Window, bg_color);
	setPalette(pal);

}

void TabWidget::addTab(QWidget* w, const QString& name, const char* pixmap, int idx)
{
	// Append tab when position is not given
	if (idx < 0/* || m_widgets.contains(idx) == true*/)
	{
		while(m_widgets.contains(++idx) == true)
		{
		}
	}

	// Tab's width when it is a text tab. This isn't correct for artwork tabs, but it's fixed later during the PaintEvent
	int tab_width = fontMetrics().horizontalAdvance(name) + 10;

	// Register new tab
	widgetDesc d = {w, pixmap, name, tab_width};
	m_widgets[idx] = d;

	// Position tab's window
	if (!m_resizable)
	{
		w->setFixedSize(width() - 4, height() - m_tabbarHeight);
	}
	w->move(2, m_tabbarHeight - 1);
	w->hide();

	// Show tab's window if it's active
	if (m_widgets.contains(m_activeTab))
	{
		// make sure new tab doesn't overlap current widget
		m_widgets[m_activeTab].w->show();
		m_widgets[m_activeTab].w->raise();
	}
}




void TabWidget::setActiveTab(int idx)
{
	if (m_widgets.contains(idx))
	{
		int old_active = m_activeTab;
		m_activeTab = idx;
		m_widgets[m_activeTab].w->raise();
		m_widgets[m_activeTab].w->show();
		if (old_active != idx && m_widgets.contains(old_active))
		{
			m_widgets[old_active].w->hide();
		}
		update();
	}
}


// Return the index of the tab at position "pos"
int TabWidget::findTabAtPos(const QPoint& pos)
{

	if (pos.y() > 1 && pos.y() < m_tabbarHeight - 1)
	{
		int cx = ((m_caption == "") ? 4 : 14) + fontMetrics().horizontalAdvance(m_caption);

		for (widgetStack::iterator it = m_widgets.begin(); it != m_widgets.end(); ++it)
		{
			int const currentWidgetWidth = it->nwidth;

			if (pos.x() >= cx && pos.x() <= cx + currentWidgetWidth)
			{
				return(it.key());
			}
			cx += currentWidgetWidth;
		}
	}

	// Haven't found any tab at position "pos"
	return(-1);
}


// Overload the QWidget::event handler to display tooltips (from https://doc.qt.io/qt-4.8/qt-widgets-tooltips-example.html)
bool TabWidget::event(QEvent* event)
{

	if (event->type() == QEvent::ToolTip)
	{
		auto helpEvent = static_cast<QHelpEvent*>(event);

		int idx = findTabAtPos(helpEvent->pos());

		if (idx != -1)
		{
			// Display tab's tooltip
			QToolTip::showText(helpEvent->globalPos(), m_widgets[idx].name);
		}
		else
		{
			// The tooltip event doesn't relate to any tab, let's ignore it
			QToolTip::hideText();
			event->ignore();
		}

		return true;
	}

	// not a Tooltip event, let's propagate it to the other event handlers
	return QWidget::event(event);
}


// Activate tab when clicked
void TabWidget::mousePressEvent(QMouseEvent* me)
{
	// Find index of tab that has been clicked
	const int idx = findTabAtPos(position(me));

	// When found, activate tab that has been clicked
	if (idx != -1)
	{
		setActiveTab(idx);
		update();
		return;
	}
}




void TabWidget::resizeEvent(QResizeEvent*)
{
	if (!m_resizable)
	{
		for (const auto& widget : m_widgets)
		{
			widget.w->setFixedSize(width() - 4, height() - m_tabbarHeight);
		}
	}
}




void TabWidget::paintEvent(QPaintEvent* pe)
{
	QPainter p(this);
	p.setFont(adjustedToPixelSize(font(), DEFAULT_FONT_SIZE));

	// Draw background
	QBrush bg_color = p.background();
	p.fillRect(0, 0, width() - 1, height() - 1, bg_color);

	// Draw external borders
	p.setPen(tabBorder());
	p.drawRect(0, 0, width() - 1, height() - 1);

	// Draw tabs' bar background
	p.fillRect(1, 1, width() - 2, m_tabheight + 2, tabBackground());

	// Draw title, if any
	if (!m_caption.isEmpty())
	{
		p.setPen(tabTitleText());
		p.drawText(5, 11, m_caption);
	}

	// Calculate the tabs' x (tabs are painted next to the caption)
	int tab_x_offset = m_caption.isEmpty() ? 4 : 14 + fontMetrics().horizontalAdvance(m_caption);

	// Compute tabs' width depending on the number of tabs (only applicable for artwork tabs)
	widgetStack::iterator first = m_widgets.begin();
	widgetStack::iterator last = m_widgets.end();
	int tab_width = width();
	if (first != last)
	{
		tab_width = (width() - tab_x_offset) / std::distance(first, last);
	}

	// Draw all tabs
	for (widgetStack::iterator it = first ; it != last ; ++it)
	{
		auto & currentWidgetDesc = *it;
		const bool isActive = (it.key() == m_activeTab);

		// Fond de l'onglet : actif rehaussé (plus clair), inactif plus sombre.
		const QColor tabFace = isActive ? tabSelected() : tabInactive();

		// Draw a text tab or a artwork tab.
		if (m_usePixmap)
		{
			// Fixes tab's width, because original size is only correct for text tabs
			currentWidgetDesc.nwidth = tab_width;

			// Get artwork
			QPixmap artwork(embed::getIconPixmap(currentWidgetDesc.pixmap));

			// Onglet artwork : face + bevel FL + coins arrondis (cf. drawFlTab).
			drawFlTab(p, QRectF(tab_x_offset, 0, currentWidgetDesc.nwidth, m_tabbarHeight - 1),
					  tabFace, tabBevelLight(), tabBevelDark());

			// Draw artwork
			p.drawPixmap(tab_x_offset + (currentWidgetDesc.nwidth - artwork.width()) / 2, 1, artwork);
		}
		else
		{
			// Onglet texte : face + bevel FL + coins arrondis (cf. drawFlTab).
			drawFlTab(p, QRectF(tab_x_offset, 2, currentWidgetDesc.nwidth - 6, m_tabbarHeight - 4),
					  tabFace, tabBevelLight(), tabBevelDark());

			// Texte de l'onglet (couleur selon l'état actif/inactif).
			p.setPen(isActive ? tabTextSelected() : tabText());
			p.drawText(tab_x_offset + 3, m_tabheight + 1, currentWidgetDesc.name);
		}

		// Next tab's horizontal position
		tab_x_offset += currentWidgetDesc.nwidth;
	}
}




// Switch between tabs with mouse wheel
void TabWidget::wheelEvent(QWheelEvent* we)
{
	if (we->position().toPoint().y() > m_tabheight)
	{
		return;
	}

	we->accept();
	int dir = (we->angleDelta().y() < 0) ? 1 : -1;
	int tab = m_activeTab;
	while(tab > -1 && static_cast<int>(tab) < m_widgets.count())
	{
		tab += dir;
		if (m_widgets.contains(tab))
		{
			break;
		}
	}
	setActiveTab(tab);
}




// Let parent widgets know how much space this tab widget needs
QSize TabWidget::minimumSizeHint() const
{
	if (m_resizable)
	{
		int maxWidth = 0, maxHeight = 0;
		for (const auto& widget : m_widgets)
		{
			maxWidth = std::max(maxWidth, widget.w->minimumSizeHint().width());
			maxHeight = std::max(maxHeight, widget.w->minimumSizeHint().height());
		}
		// "-1" :
		// in "addTab", under "Position tab's window", the widget is
		// moved up by 1 pixel
		return QSize(maxWidth + 4, maxHeight + m_tabbarHeight - 1);
	}
	else { return QWidget::minimumSizeHint(); }
}




QSize TabWidget::sizeHint() const
{
	if (m_resizable)
	{
		int maxWidth = 0, maxHeight = 0;
		for (const auto& widget : m_widgets)
		{
			maxWidth = std::max(maxWidth, widget.w->sizeHint().width());
			maxHeight = std::max(maxHeight, widget.w->sizeHint().height());
		}
		// "-1" :
		// in "addTab", under "Position tab's window", the widget is
		// moved up by 1 pixel
		return QSize(maxWidth + 4, maxHeight + m_tabbarHeight - 1);
	}
	else { return QWidget::sizeHint(); }
}




// Return the color to be used to draw a TabWidget's title text (if any)
QColor TabWidget::tabTitleText() const
{
	return m_tabTitleText;
}

// Set the color to be used to draw a TabWidget's title text (if any)
void TabWidget::setTabTitleText(const QColor& c)
{
	m_tabTitleText = c;
}

// Return the color to be used to draw a TabWidget's text (if any)
QColor TabWidget::tabText() const
{
	return m_tabText;
}

// Set the color to be used to draw a TabWidget's text (if any)
void TabWidget::setTabText(const QColor& c)
{
	m_tabText = c;
}

// Return the color to be used to highlight a TabWidget'selected tab (if any)
QColor TabWidget::tabSelected() const
{
	return m_tabSelected;
}

// Set the color to be used to highlight a TabWidget'selected tab (if any)
void TabWidget::setTabSelected(const QColor& c)
{
	m_tabSelected = c;
}

// Return the text color of the selected tab
QColor TabWidget::tabTextSelected() const
{
	return m_tabTextSelected;
}

// Set the text color of the selected tab
void TabWidget::setTabTextSelected(const QColor& c)
{
	m_tabTextSelected = c;
}

// Return the color to be used for the TabWidget's background
QColor TabWidget::tabBackground() const
{
	return m_tabBackground;
}

// Set the color to be used for the TabWidget's background
void TabWidget::setTabBackground(const QColor& c)
{
	m_tabBackground = c;
}

// Return the color to be used for the TabWidget's borders
QColor TabWidget::tabBorder() const
{
	return m_tabBorder;
}

// Set the color to be used for the TabWidget's borders
void TabWidget::setTabBorder(const QColor& c)
{
	m_tabBorder = c;
}

// --- Bevel FL Studio (ajout Phase 3, cf. DESIGN-UI.md §1.1/§1.3) ------------------

// Return the light edge color (top/left) of the tabs' bevel
QColor TabWidget::tabBevelLight() const
{
	return m_tabBevelLight;
}

// Set the light edge color (top/left) of the tabs' bevel
void TabWidget::setTabBevelLight(const QColor& c)
{
	m_tabBevelLight = c;
}

// Return the dark edge color (bottom/right) of the tabs' bevel
QColor TabWidget::tabBevelDark() const
{
	return m_tabBevelDark;
}

// Set the dark edge color (bottom/right) of the tabs' bevel
void TabWidget::setTabBevelDark(const QColor& c)
{
	m_tabBevelDark = c;
}

// Return the background color of inactive tabs (darker than the raised active tab)
QColor TabWidget::tabInactive() const
{
	return m_tabInactive;
}

// Set the background color of inactive tabs
void TabWidget::setTabInactive(const QColor& c)
{
	m_tabInactive = c;
}


} // namespace lmms::gui
