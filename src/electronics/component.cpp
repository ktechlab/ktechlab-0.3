/***************************************************************************
 *   Copyright (C) 2004-2005 by David Saxton                               *
 *   david@bluehaze.org                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include <cmath>
#include <kdebug.h>
#include <qbitarray.h>
#include <qpainter.h>
#include <qwidget.h>
#include <qwmatrix.h>

#include "canvasitemparts.h"
#include "circuitdocument.h"
#include "component.h"
#include "src/core/ktlconfig.h"
#include "itemdocumentdata.h"
#include "pin.h"
#include "simulator.h"

#include "ccvs.h"
#include "switch.h"
#include "vcvs.h"

const int dipWidth = 112;
const int pairSep = 32;

Component::Component(ICNDocument *icnDocument, bool newItem, const QString &id)
		: CNItem(icnDocument, newItem, id),
		m_angleDegrees(0),
		b_flipped(false) {
	m_pCircuitDocument = dynamic_cast<CircuitDocument*>(icnDocument);

	for (int i = 0; i < 4; ++i) {
		m_pPNode[i] = 0;
		m_pNNode[i] = 0;
	}

	// Get configuration options
	slotUpdateConfiguration();

	// And finally register this :-)
	if (icnDocument)
		icnDocument->registerItem(this);
}

Component::~Component() {
	removeElements();
	Simulator::self()->detachComponent(this);
}

void Component::removeItem() {
	if (b_deleted) return;

	Simulator::self()->detachComponent(this);
	CNItem::removeItem();
}

void Component::removeElements(bool setPinsInterIndependent) {
	const ElementMapList::iterator end = m_elementMapList.end();
	for (ElementMapList::iterator it = m_elementMapList.begin(); it != end; ++it) {
		Element *e = (*it).e;

		if(e) {
			emit elementDestroyed(e);
			e->componentDeleted();
		}
	}

	m_elementMapList.clear();

	const SwitchList::iterator swEnd = m_switchList.end();
	for(SwitchList::iterator it = m_switchList.begin(); it != swEnd; ++it) {
		Switch *sw = *it;

		if(!sw) continue;

		emit switchDestroyed(sw);

		delete sw;
	}

	m_switchList.clear();

	if (setPinsInterIndependent)
		setAllPinsInterIndependent();
}

void Component::removeElement(Element *element, bool setPinsInterIndependent) {
	if(!element) return;

	emit elementDestroyed(element);

	element->componentDeleted();

	const ElementMapList::iterator end = m_elementMapList.end();
	for (ElementMapList::iterator it = m_elementMapList.begin(); it != end;) {
		ElementMapList::iterator next = it;
		++next;

		if ((*it).e == element)
			m_elementMapList.remove(it);

		it = next;
	}

	if(setPinsInterIndependent)
		rebuildPinInterDepedence();
}

void Component::removeSwitch(Switch *sw) {
	if (!sw) return;

	emit switchDestroyed(sw);
	m_switchList.remove(sw);

	delete sw;

	m_pCircuitDocument->requestAssignCircuits();
}

void Component::setNodalCurrents() {

	const ElementMapList::iterator end = m_elementMapList.end();
	for (ElementMapList::iterator it = m_elementMapList.begin(); it != end; ++it) {
		ElementMap m = (*it);

		for (int i = 0; i < 4; i++) {
			if (m.n[i]) {
				m.n[i]->mergeCurrent(m.e->m_cnodeI[i]);
			}
		}
	}
}

void Component::initPainter(QPainter &p) {
	CNItem::initPainter(p);

	if (!b_flipped && (m_angleDegrees % 360 == 0))
		return;

	p.save();
	p.translate(int(x()), int(y()));

	if (b_flipped) p.scale(-1, 1);

	p.rotate(m_angleDegrees);
	p.translate(-int(x()), -int(y()));
}

void Component::deinitPainter(QPainter &p) {
	if(!b_flipped && (m_angleDegrees % 360 == 0))
		return;

	p.restore();
}

void Component::setAngleDegrees(int degrees) {
	if (!p_icnDocument) return;

	degrees = ((degrees % 360) + 360) % 360;

	if (m_angleDegrees == degrees) return;

	updateConnectorPoints(false);
	m_angleDegrees = degrees;
	itemPointsChanged();
	updateAttachedPositioning();
	p_icnDocument->requestRerouteInvalidatedConnectors();

	emit orientationChanged();
}

void Component::setFlipped(bool flipped) {
	if(!p_icnDocument) return;
	if(flipped == b_flipped) return;

	updateConnectorPoints(false);
	b_flipped = flipped;
	itemPointsChanged();
	updateAttachedPositioning();
	p_icnDocument->requestRerouteInvalidatedConnectors();

	emit orientationChanged();
}

void Component::itemPointsChanged() {
	QPointArray transformedPoints = transMatrix(m_angleDegrees, b_flipped, 0, 0, false).map(m_itemPoints);
// 	transformedPoints.translate( int(x()), int(y()) );
	setPoints(transformedPoints);
}

void Component::restoreFromItemData(const ItemData &itemData) {
	CNItem::restoreFromItemData(itemData);

	setAngleDegrees(int(itemData.angleDegrees));
	setFlipped(itemData.flipped);
}

ItemData Component::itemData() const {
	ItemData itemData = CNItem::itemData();
	itemData.angleDegrees = m_angleDegrees;
	itemData.flipped = b_flipped;
	return itemData;
}

QWMatrix Component::transMatrix(int angleDegrees, bool flipped, int x, int y, bool inverse) {
	QWMatrix m;
	m.translate(x, y);

	if (inverse) {
		m.rotate(-angleDegrees);

		if (flipped) m.scale(-1, 1);
	} else {
		if (flipped) m.scale(-1, 1);

		m.rotate(angleDegrees);
	}

	m.translate(-x, -y);

	m.setTransformationMode(QWMatrix::Areas);
	return m;
}

void Component::finishedCreation() {
	CNItem::finishedCreation();
	updateAttachedPositioning();
}

void Component::updateAttachedPositioning() {
	const double RPD = M_PI / 180.0;

	if (b_deleted || !m_bDoneCreation)
		return;

	//BEGIN Transform the nodes
	const NodeInfoMap::iterator end = m_nodeMap.end();

	for (NodeInfoMap::iterator it = m_nodeMap.begin(); it != end; ++it) {
		if (!it.data().node)
			kdError() << k_funcinfo << "Node in nodemap is null" << endl;
		else {
			int nx = int((std::cos(m_angleDegrees * RPD) * it.data().x) - (std::sin(m_angleDegrees * RPD) * it.data().y));
			int ny = int((std::sin(m_angleDegrees * RPD) * it.data().x) + (std::cos(m_angleDegrees * RPD) * it.data().y));

			if (b_flipped) nx = -nx;

#define round_8(x) (((x) > 0) ? int(((x)+4)/8)*8 : int(((x)-4)/8)*8)
			nx = round_8(nx);
			ny = round_8(ny);
#undef round_8

			int newDir = (((m_angleDegrees + it.data().orientation) % 360) + 360) % 360;

			if (b_flipped)
				newDir = (((180 - newDir) % 360) + 360) % 360;

			it.data().node->move(nx + x(), ny + y());
			it.data().node->setOrientation(newDir);
		}
	}
	//END Transform the nodes

	//BEGIN Transform the GuiParts
	QWMatrix m;

	if (b_flipped) m.scale(-1, 1);

	m.rotate(m_angleDegrees);
	m.setTransformationMode(QWMatrix::Areas);

	const TextMap::iterator textMapEnd = m_textMap.end();
	for(TextMap::iterator it = m_textMap.begin(); it != textMapEnd; ++it) {
		QRect newPos = m.mapRect(it.data()->recommendedRect());
		it.data()->move(newPos.x() + x(), newPos.y() + y());
		it.data()->setGuiPartSize(newPos.width(), newPos.height());
		it.data()->setAngleDegrees(m_angleDegrees);
	}

	const WidgetMap::iterator widgetMapEnd = m_widgetMap.end();
	for(WidgetMap::iterator it = m_widgetMap.begin(); it != widgetMapEnd; ++it) {
		QRect newPos = m.mapRect(it.data()->recommendedRect());
		it.data()->move(newPos.x() + x(), newPos.y() + y());
		it.data()->setGuiPartSize(newPos.width(), newPos.height());
		it.data()->setAngleDegrees(m_angleDegrees);
	}
	//END Transform the GuiParts
}

void Component::drawPortShape(QPainter &p) {
	int h = height();
	int w = width() - 1;
	int _x = int(x() + offsetX());
	int _y = int(y() + offsetY());

	double roundSize = 8;
	double slantIndent = 8;

	const double DPR = 180.0 / M_PI;
	double inner = std::atan(h / slantIndent);	// Angle for slight corner
	double outer = M_PI - inner;		// Angle for sharp corner

	int inner16 = int(16 * inner * DPR);
	int outer16 = int(16 * outer * DPR);

	p.save();
	p.setPen(Qt::NoPen);
	p.drawPolygon(areaPoints());
	p.restore();

	initPainter(p);

	// Left line
	p.drawLine(int(_x), int(_y + roundSize / 2), int(_x), int(_y + h - roundSize / 2));
	// Right line
	p.drawLine(int(_x + w),	int(_y - slantIndent + h - roundSize / 2), int(_x + w),	int(_y + slantIndent + roundSize / 2));

	// Bottom line
	p.drawLine(int(_x + (1 - std::cos(outer)) * (roundSize / 2)), int(_y + h + (std::sin(outer) - 1) * (roundSize / 2)),
	           int(_x + w + (std::cos(inner) - 1) * (roundSize / 2)), int(_y + h - slantIndent + (std::sin(inner) - 1) * (roundSize / 2)));
	// Top line
	p.drawLine(int(_x + w + (std::cos(outer) - 1) * (roundSize / 2)), int(_y + slantIndent + (1 - std::sin(inner)) * (roundSize / 2)), int(_x + (1 - std::cos(inner)) * (roundSize / 2)), int(_y + (1 - std::sin(outer)) * (roundSize / 2)));

	// Top left
	p.drawArc(int(_x), int(_y), int(roundSize), int(roundSize), 90 << 4,	outer16);

	// Bottom left
	p.drawArc(int(_x), int(_y + h - roundSize), int(roundSize), int(roundSize), 180 << 4,	outer16);

	// Top right
	p.drawArc(int(_x + w - roundSize), int(_y + slantIndent), int(roundSize), int(roundSize), 0, inner16);

	// Bottom right
	p.drawArc(int(_x + w - roundSize), int(_y - slantIndent + h - roundSize), int(roundSize), int(roundSize), 270 << 4, inner16);

	deinitPainter(p);
}

void Component::initDIP(const QStringList &pins) {
	const int numPins = pins.size();
	const int numSide = numPins / 2 + numPins % 2;

	// Pins along left
	for(int i = 0; i < numSide; i++) {
		if (!pins[i].isEmpty()) {
			const int nodeX = -8 + offsetX();
			const int nodeY = ((i + 1) << 4) + offsetY();
			ECNode *node = ecNodeWithID(pins[i]);

			if (node) {
				m_nodeMap[pins[i]].x = nodeX;
				m_nodeMap[pins[i]].y = nodeY;
				m_nodeMap[pins[i]].orientation = 0;
			} else	createPin(nodeX, nodeY, 0, pins[i]);
		}
	}

	// Pins along right
	for(int i = numSide; i < numPins; i++) {
		if(!pins[i].isEmpty()) {
			const int nodeX = width() + 8 + offsetX();
			const int nodeY = ((2 * numSide - i) << 4) + offsetY();
			ECNode *node = ecNodeWithID(pins[i]);

			if(node) {
				m_nodeMap[pins[i]].x = nodeX;
				m_nodeMap[pins[i]].y = nodeY;
				m_nodeMap[pins[i]].orientation = 180;
			} else	createPin(nodeX, nodeY, 180, pins[i]);
		}
	}

	updateAttachedPositioning();
}

void Component::initDIPSymbol(const QStringList & pins, int _width) {
	const int numPins = pins.size();
	const int numSide = numPins / 2 + numPins % 2;

	setSize(-(_width - (_width % 16)) / 2, -(numSide + 1) * 8, _width, (numSide + 1) * 16, true);

	QWidget tmpWidget;
	QPainter p(&tmpWidget);

	p.setFont(font());

	// Pins along left
	for(int i = 0; i < numSide; i++) {
		if (!pins[i].isEmpty()) {
			const QString text = *pins.at(i);

			const int _top = (i + 1) * 16 - 8 + offsetY();
			const int _width = width() / 2 - 6;
			const int _left = 6 + offsetX();
			const int _height = 16;

			QRect br = p.boundingRect(QRect(_left, _top, _width, _height), Qt::AlignLeft, text);
			addDisplayText(text, br, text);
		}
	}

	// Pins along right
	for(int i = numSide; i < numPins; i++) {
		if(!pins[i].isEmpty()) {
			const QString text = *pins.at(i);

			const int _top = (2 * numSide - i) * 16 - 8 + offsetY();
			const int _width = width() / 2 - 6;
			const int _left = (width() / 2) + offsetX();
			const int _height = 16;

			QRect br = p.boundingRect(QRect(_left, _top, _width, _height), Qt::AlignRight, text);
			addDisplayText(text, br, text);
		}
	}

	updateAttachedPositioning();
}

void Component::init1PinLeft(int h1) {
	if(h1 == -1) h1 = offsetY() + height() / 2;

	m_pNNode[0] = createPin(offsetX() - 8, h1, 0, "n1");
}

void Component::init2PinLeft(int h1, int h2) {
	if(h1 == -1) h1 = offsetY() + 8;
	if(h2 == -1) h2 = offsetY() + height() - 8;

	m_pNNode[0] = createPin(offsetX() - 8, h1, 0, "n1");
	m_pNNode[1] = createPin(offsetX() - 8, h2, 0, "n2");
}

void Component::init3PinLeft(int h1, int h2, int h3) {
	if(h1 == -1) h1 = offsetY() + 8;
	if(h2 == -1) h2 = offsetY() + height() / 2;
	if(h3 == -1) h3 = offsetY() + height() - 8;

	m_pNNode[0] = createPin(offsetX() - 8, h1, 0, "n1");
	m_pNNode[1] = createPin(offsetX() - 8, h2, 0, "n2");
	m_pNNode[2] = createPin(offsetX() - 8, h3, 0, "n3");
}

void Component::init4PinLeft(int h1, int h2, int h3, int h4) {
	if(h1 == -1) h1 = offsetY() + 8;
	if(h2 == -1) h2 = offsetY() + 24;
	if(h3 == -1) h3 = offsetY() + height() - 24;
	if(h4 == -1) h4 = offsetY() + height() - 8;

	m_pNNode[0] = createPin(offsetX() - 8, h1, 0, "n1");
	m_pNNode[1] = createPin(offsetX() - 8, h2, 0, "n2");
	m_pNNode[2] = createPin(offsetX() - 8, h3, 0, "n3");
	m_pNNode[3] = createPin(offsetX() - 8, h4, 0, "n4");
}

void Component::init1PinRight(int h1) {
	if(h1 == -1) h1 = offsetY() + height() / 2;

	m_pPNode[0] = createPin(offsetX() + width() + 8, h1, 180, "p1");
}

void Component::init2PinRight(int h1, int h2) {
	if(h1 == -1) h1 = offsetY() + 8;
	if(h2 == -1) h2 = offsetY() + height() - 8;

	m_pPNode[0] = createPin(offsetX() + width() + 8, h1, 180, "p1");
	m_pPNode[1] = createPin(offsetX() + width() + 8, h2, 180, "p2");
}

void Component::init3PinRight(int h1, int h2, int h3) {
	if(h1 == -1) h1 = offsetY() + 8;
	if(h2 == -1) h2 = offsetY() + height() / 2;
	if(h3 == -1) h3 = offsetY() + height() - 8;

	m_pPNode[0] = createPin(offsetX() + width() + 8, h1, 180, "p1");
	m_pPNode[1] = createPin(offsetX() + width() + 8, h2, 180, "p2");
	m_pPNode[2] = createPin(offsetX() + width() + 8, h3, 180, "p3");
}

void Component::init4PinRight(int h1, int h2, int h3, int h4) {
	if(h1 == -1) h1 = offsetY() + 8;
	if(h2 == -1) h2 = offsetY() + 24;
	if(h3 == -1) h3 = offsetY() + height() - 24;
	if(h4 == -1) h4 = offsetY() + height() - 8;

	m_pPNode[0] = createPin(offsetX() + width() + 8, h1, 180, "p1");
	m_pPNode[1] = createPin(offsetX() + width() + 8, h2, 180, "p2");
	m_pPNode[2] = createPin(offsetX() + width() + 8, h3, 180, "p3");
	m_pPNode[3] = createPin(offsetX() + width() + 8, h4, 180, "p4");
}

ECNode* Component::ecNodeWithID(const QString &ecNodeId) {
	if(!p_icnDocument) {
// 		kdDebug() << "Warning: ecNodeWithID("<<ecNodeId<<") does not exist\n";
		return createPin(0, 0, 0, ecNodeId);
	}

	return dynamic_cast<ECNode*>(p_icnDocument->nodeWithID(nodeId(ecNodeId)));
}

void Component::slotUpdateConfiguration() {
	const LogicConfig logicConfig = LogicIn::getConfig();
	const ElementMapList::iterator end = m_elementMapList.end();

	for(ElementMapList::iterator it = m_elementMapList.begin(); it != end; ++it) {
		if(LogicIn *logicIn = dynamic_cast<LogicIn*>((*it).e))
			logicIn->setLogic(logicConfig);
	}
}

void Component::setup1pinElement(Element *ele, Pin *a) {
	QValueList<Pin*> pins;
	pins << a;

	ElementMapList::iterator it = handleElement(ele, pins);
	setInterDependent(it, pins);
}

void Component::setup2pinElement(Element *ele, Pin *a, Pin *b) {
	QValueList<Pin*> pins;
	pins << a << b;

	ElementMapList::iterator it = handleElement(ele, pins);
	setInterDependent(it, pins);
}

void Component::setup3pinElement(Element *ele, Pin *a, Pin *b, Pin *c) {
	QValueList<Pin*> pins;
	pins << a << b << c;

	ElementMapList::iterator it = handleElement(ele, pins);
	setInterDependent(it, pins);
}

void Component::setup4pinElement(Element *ele, Pin *a, Pin *b, Pin *c, Pin *d) {
	QValueList<Pin*> pins;
	pins << a << b << c << d;

	ElementMapList::iterator it = handleElement(ele, pins);
	setInterDependent(it, pins);
}

// FIXME: MEMORY LEAK CENTRAL!!!
// We don't have anything to clean up after these calls to 'new'!!!!!
// this entire class is due for a redesign too. =(

CCVS *Component::createCCVS(Pin *n0, Pin *n1, Pin *n2, Pin *n3, double gain) {
	CCVS *e = new CCVS(gain);

	QValueList<Pin*> pins;
	pins << n0 << n1 << n2 << n3;

	ElementMapList::iterator it = handleElement(e, pins);
	setInterCircuitDependent(it, pins);

	pins.clear();
	pins << n0 << n1;
	setInterGroundDependent(it, pins);

	pins.clear();
	pins << n2 << n3;
	setInterGroundDependent(it, pins);

	return e;
}

VCVS *Component::createVCVS(Pin *n0, Pin *n1, Pin *n2, Pin *n3, double gain) {
	VCVS *e = new VCVS(gain);

	QValueList<Pin*> pins;
	pins << n0 << n1 << n2 << n3;

	ElementMapList::iterator it = handleElement(e, pins);
	setInterCircuitDependent(it, pins);

	pins.clear();
	pins << n0 << n1;
	setInterGroundDependent(it, pins);

	pins.clear();
	pins << n2 << n3;
	setInterGroundDependent(it, pins);
	return e;
}

Switch *Component::createSwitch(Pin *n0, Pin *n1, bool open) {
	// Note that a Switch is not really an element (although in many cases it
	// behaves very much like one).

	Switch *e = new Switch(this, n0, n1, open ? Switch::Open : Switch::Closed);
	m_switchList.append(e);
	n0->addSwitch(e);
	n1->addSwitch(e);
	emit switchCreated(e);
	return e;
}

ElementMapList::iterator Component::handleElement(Element *e, const QValueList<Pin*> &pins) {
	if(!e) return m_elementMapList.end();

	ElementMap em;
	em.e = e;

	int at = 0;
	QValueList<Pin*>::ConstIterator end = pins.end();

	for(QValueList<Pin*>::ConstIterator it = pins.begin(); it != end; ++it) {
		(*it)->addElement(e);
		em.n[at++] = *it;
	}

	ElementMapList::iterator it = m_elementMapList.append(em);

	emit elementCreated(e);
	return it;
}

void Component::setInterDependent(ElementMapList::iterator it, const QValueList<Pin*> & pins) {
	setInterCircuitDependent(it, pins);
	setInterGroundDependent(it, pins);
}

void Component::setInterCircuitDependent(ElementMapList::iterator it, const QValueList<Pin*> & pins) {
	QValueList<Pin*>::ConstIterator end = pins.end();

	for(QValueList<Pin*>::ConstIterator it1 = pins.begin(); it1 != end; ++it1) {
		for(QValueList<Pin*>::ConstIterator it2 = pins.begin(); it2 != end; ++it2) {
			(*it1)->addCircuitDependentPin(*it2);
		}
	}

	(*it).interCircuitDependent.append(pins);
}

void Component::setInterGroundDependent(ElementMapList::iterator it, const QValueList<Pin*> & pins) {
	QValueList<Pin*>::ConstIterator end = pins.end();

	for(QValueList<Pin*>::ConstIterator it1 = pins.begin(); it1 != end; ++it1) {
		for(QValueList<Pin*>::ConstIterator it2 = pins.begin(); it2 != end; ++it2) {
			(*it1)->addGroundDependentPin(*it2);
		}
	}

	(*it).interGroundDependent.append(pins);
}

void Component::rebuildPinInterDepedence() {
	setAllPinsInterIndependent();

	// Rebuild dependencies
	ElementMapList::iterator emlEnd = m_elementMapList.end();

	for(ElementMapList::iterator it = m_elementMapList.begin(); it != emlEnd; ++it) {
		// Many copies of the pin lists as these will be affected when we call setInter*Dependent
		PinListList list = (*it).interCircuitDependent;

		PinListList::iterator depEnd = list.end();

		for(PinListList::iterator depIt = list.begin(); depIt != depEnd; ++depIt)
			setInterCircuitDependent(it, *depIt);

		list = (*it).interGroundDependent;
		depEnd = list.end();

		for(PinListList::iterator depIt = list.begin(); depIt != depEnd; ++depIt)
			setInterGroundDependent(it, *depIt);
	}
}

void Component::setAllPinsInterIndependent() {
	NodeInfoMap::iterator nmEnd = m_nodeMap.end();

	for(NodeInfoMap::iterator it = m_nodeMap.begin(); it != nmEnd; ++it) {
		PinVector pins = (static_cast<ECNode*>(it.data().node))->pins();
		PinVector::iterator pinsEnd = pins.end();

		for(PinVector::iterator pinsIt = pins.begin(); pinsIt != pinsEnd; ++pinsIt) {
			if (*pinsIt)
				(*pinsIt)->removeDependentPins();
		}
	}
}

void Component::initElements(const uint stage) {
	const ElementMapList::iterator end = m_elementMapList.end();

	if(stage == 1) {
		for(ElementMapList::iterator it = m_elementMapList.begin(); it != end; ++it) {
			(*it).e->add_initial_dc();
		}

		return;
	}

	for(ElementMapList::iterator it = m_elementMapList.begin(); it != end; ++it) {
		ElementMap m = *it;

		if(m.n[3]) {
			m.e->setCNodes(m.n[0]->eqId(), m.n[1]->eqId(), m.n[2]->eqId(), m.n[3]->eqId());
		} else if(m.n[2]) {
			m.e->setCNodes(m.n[0]->eqId(), m.n[1]->eqId(), m.n[2]->eqId());
		} else if(m.n[1]) {
			m.e->setCNodes(m.n[0]->eqId(), m.n[1]->eqId());
		} else if(m.n[0]) {
			m.e->setCNodes(m.n[0]->eqId());
		}
	}
}

ECNode *Component::createPin(double x, double y, int orientation, const QString &name) {
	return dynamic_cast<ECNode*>(createNode(x, y, orientation, name, Node::ec_pin));
}

// static
double Component::voltageLength(double v) {
	double v_max = 1e1;
	double v_min = 1e-1;

	v = std::abs(v);

	if(v >= v_max) return 1.0;
	else if(v <= v_min) return 0.0;
	else return std::log(v / v_min) / std::log(v_max / v_min);
}

// static
QColor Component::voltageColor(double v) {
	double prop = voltageLength(v);

	if(v >= 0)
		return QColor(int(255 * prop), int(166 * prop), 0);
	else return QColor(0, int(136 * prop), int(255 * prop));
}

//BEGIN class ElementMap
ElementMap::ElementMap() {
	e = 0;

	for(int i = 0; i < 4; ++i)
		n[i] = 0;
}
//END class ElementMap

#include "component.moc"

