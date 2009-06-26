/***************************************************************************
 *   Copyright (C) 2009 Julian Bäume <julian@svg4all.de>                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "circuitapplet.h"
#include "componentapplet.h"
#include "interfaces/component/componentmimedata.h"

#include <Plasma/DataEngine>
#include <Plasma/Theme>
#include <Plasma/FrameSvg>
#include <QGraphicsSceneDragDropEvent>
#include <QPainter>
#include <QMap>
#include <QString>
#include <KDebug>

CircuitApplet::CircuitApplet( QObject *parent, const QVariantList &args )
    :   Plasma::Containment( parent, args ),
        m_theme( new Plasma::Theme() ),
        m_bg( new Plasma::FrameSvg() )
{
    setAspectRatioMode(Plasma::IgnoreAspectRatio);
    setBackgroundHints(NoBackground);
    setAcceptDrops( true );
    foreach ( QVariant arg, args ) {
        if ( arg.canConvert(QVariant::Map) && arg.toMap().contains( "circuitName" ) ) {
            setCircuitName( arg.toMap().value( "circuitName" ).toString() );
        }
    }
}

CircuitApplet::~CircuitApplet()
{
    delete m_theme;
    clearApplets();
    qDeleteAll( m_components.values() );
}

void CircuitApplet::init()
{
    m_theme->setThemeName( "default" );
    KConfigGroup cg = config( "circuit" );
    m_componentTheme = cg.readEntry( "componentTheme", "din" );

    m_componentSize = QSizeF( cg.readEntry("componentWidth", "64").toInt(), cg.readEntry("componentHeight", "64").toInt() );

    if ( !m_theme->currentThemeHasImage( "ktechlab/circuit-background" ) ) {
        kDebug() << "no background image for circuit found in theme" << m_theme->themeName();
    }
    m_bg.setImagePath( "ktechlab/circuit-background" );
}

void CircuitApplet::dropEvent( QGraphicsSceneDragDropEvent *event )
{
    if (!event->mimeData()->hasFormat("application/x-icomponent")) {
        return;
    }
    const KTechLab::ComponentMimeData *mimeData = qobject_cast<const KTechLab::ComponentMimeData*>(event->mimeData());

    //FIXME: implement me!
    //do something with mimeData here. it should be added to the document using the document
    //DataEngine and services. use mimeData->createComponent() to create a new component.
    kDebug() << "Dropping item @"<< event->scenePos() << "type:" << mimeData->data("application/x-icomponent");
}

void CircuitApplet::paintInterface( QPainter *p, const QStyleOptionGraphicsItem *option, const QRect &contentsRect )
{
    //draw background
    if ( m_bg.frameSize() != contentsRect.size() ) {
        m_bg.resizeFrame( contentsRect.size() );
    }
    m_bg.paintFrame(p, QPointF( contentsRect.left(), contentsRect.top() ));
}

void CircuitApplet::dataUpdated( const QString &name, const Plasma::DataEngine::Data &data )
{
    Plasma::DataEngine *docEngine = dataEngine( "ktechlabdocument" );
    kDebug() << "isContainment() ==" << isContainment();

    if ( data["mime"].toString().endsWith("circuit") ) {
        const QStringList &itemSources = data["itemList"].toStringList();
        foreach ( const QString &source, itemSources ) {
            if ( !m_components.contains(source) ) {
                ComponentApplet *component = new ComponentApplet( this, m_theme );
                kDebug() << "before connecting";
                docEngine->connectSource( m_circuitName + "/" + source, component );
                kDebug() << "after connecting";

                //addApplet( component, position, false );
                m_components.insert( source, component );
            }
        }
    }
    kDebug() << "Difference between components and applets" << (m_components.size() - applets().size());
}

void CircuitApplet::setupData()
{
    Plasma::DataEngine *docEngine = dataEngine( "ktechlabdocument" );
    if ( !docEngine ) {
        kWarning() << "No document engine found" << endl;
        return;
    }

    docEngine->connectSource( m_circuitName, this );
}

void CircuitApplet::setCircuitName( const QString &name )
{
    //if name didn't change, do nothing
    if ( m_circuitName == name ) {
        return;
    }

    m_circuitName = name;
    setupData();
}

#include "circuitapplet.moc"

// vim: sw=4 sts=4 et tw=100
