/* -*- mode: c++; c-basic-offset:4 -*-
    tabwidget.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2007 Klarälvdalens Datakonsult AB

    Kleopatra is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Kleopatra is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

    In addition, as a special exception, the copyright holders give
    permission to link the code of this program with any edition of
    the Qt library by Trolltech AS, Norway (or with modified versions
    of Qt that use the same license as Qt), and distribute linked
    combinations including the two.  You must obey the GNU General
    Public License in all respects for all of the code used other than
    Qt.  If you modify this file, you may extend this exception to
    your version of the file, but you are not obligated to do so.  If
    you do not wish to do so, delete this exception statement from
    your version.
*/

#include "tabwidget.h"

#include <models/keylistmodel.h>
#include <models/keylistsortfilterproxymodel.h>

#include <kleo/keyfilter.h>
#include <kleo/keyfiltermanager.h>

#include <KTabWidget>
#include <KConfigGroup>

#include <QGridLayout>
#include <QTimer>
#include <QResizeEvent>
#include <QSortFilterProxyModel>
#include <QTreeView>
#include <QToolButton>
#include <QAction>

#include <map>
#include <cassert>

using namespace Kleo;
using namespace boost;

namespace {
class Page : public QWidget {
    Q_OBJECT
    Page( const Page & other );
public:
    Page( QAbstractItemModel * sourceModel, const QString & title, const QString & id, const QString & text, QWidget * parent=0 );
    Page( QAbstractItemModel * sourceModel, const KConfigGroup & group, QWidget * parent=0 );
    ~Page();

    QAbstractItemView * view() const { return m_view; }

    void saveTo( KConfigGroup & group ) const;

    QString stringFilter() const { return m_stringFilter; }
    void setStringFilter( const QString & filter ); 

    const shared_ptr<KeyFilter> & keyFilter() const { return m_keyFilter; }
    void setKeyFilter( const shared_ptr<KeyFilter> & filter );

    QString title() const { return m_title.isEmpty() && m_keyFilter ? m_keyFilter->name() : m_title ; }
    void setTitle( const QString & title );

    bool canBeClosed() const { return m_canBeClosed; }
    void setCanBeClosed( bool on );

    Page * clone() const { return new Page( *this ); }

Q_SIGNALS:
    void titleChanged( const QString & title );
    void stringFilterChanged( const QString & filter );
    void keyFilterChanged( const boost::shared_ptr<Kleo::KeyFilter> & filter );
    void canBeClosedChanged( bool closable );

protected:
    void resizeEvent( QResizeEvent * e ) {
        QWidget::resizeEvent( e );
        m_view->resize( e->size() );
    }

private:
    void init( QAbstractItemModel * model );

private:
    KeyListSortFilterProxyModel m_proxy;
    QTreeView * m_view;

    QString m_stringFilter;
    shared_ptr<KeyFilter> m_keyFilter;
    QString m_title;
    bool m_canBeClosed;
};
} // anon namespace

Page::Page( const Page & other )
    : QWidget( 0 ),
      m_proxy(),
      m_view( new QTreeView( this ) ),
      m_stringFilter( other.m_stringFilter ),
      m_keyFilter( other.m_keyFilter ),
      m_title( other.m_title ),
      m_canBeClosed( other.m_canBeClosed )
{
    init( other.m_proxy.sourceModel() );
}

Page::Page( QAbstractItemModel * model, const QString & title, const QString & id, const QString & text, QWidget * parent )
    : QWidget( parent ),
      m_proxy(),
      m_view( new QTreeView( this ) ),
      m_stringFilter( text ),
      m_keyFilter( KeyFilterManager::instance()->keyFilterByID( id ) ),
      m_title( title ),
      m_canBeClosed( true )
{
    init( model );
}

Page::Page( QAbstractItemModel * model, const KConfigGroup & group, QWidget * parent )
    : QWidget( parent ),
      m_proxy(),
      m_view( new QTreeView( this ) ),
      m_stringFilter( group.readEntry( "string-filter" ) ),
      m_keyFilter( KeyFilterManager::instance()->keyFilterByID( group.readEntry( "key-filter" ) ) ),
      m_title( group.readEntry( "title" ) ),
      m_canBeClosed( !group.isImmutable() )
{
    init( model );
}

void Page::init( QAbstractItemModel * model ) {
    KDAB_SET_OBJECT_NAME( m_proxy );
    KDAB_SET_OBJECT_NAME( m_view );

    assert( model );
    m_proxy.setSourceModel( model );
    m_proxy.setFilterFixedString( m_stringFilter );
    m_proxy.setKeyFilter( m_keyFilter );
    m_view->setModel( &m_proxy );
}

Page::~Page() {}

void Page::saveTo( KConfigGroup & group ) const {

    group.writeEntry( "title", m_title );
    group.writeEntry( "string-filter", m_stringFilter );
    group.writeEntry( "key-filter", m_keyFilter ? m_keyFilter->id() : QString() );

}

void Page::setStringFilter( const QString & filter ) {
    if ( filter == m_stringFilter )
        return;
    m_stringFilter = filter;
    m_proxy.setFilterFixedString( filter ); 
    emit stringFilterChanged( filter );
}

void Page::setKeyFilter( const shared_ptr<KeyFilter> & filter ) {
    if ( filter == m_keyFilter || filter && m_keyFilter && filter->id() == m_keyFilter->id() )
        return;
    const QString oldTitle = title();
    m_keyFilter = filter;
    m_proxy.setKeyFilter( filter );
    const QString newTitle = title();
    emit keyFilterChanged( filter );
    if ( oldTitle != newTitle )
        emit titleChanged( newTitle );
}

void Page::setTitle( const QString & t ) {
    if ( t == m_title )
        return;
    const QString oldTitle = title();
    m_title = t;
    const QString newTitle = title();
    if ( oldTitle != newTitle )
        emit titleChanged( newTitle );
}

void Page::setCanBeClosed( bool on ) {
    if ( on == m_canBeClosed )
        return;
    m_canBeClosed = on;
    emit canBeClosedChanged( on );
}

//
//
// TabWidget
//
//

class TabWidget::Private {
    friend class ::TabWidget;
    TabWidget * const q;
public:
    explicit Private( TabWidget * qq );
    ~Private() {};

private:
    void currentIndexChanged( int index );
    void slotPageTitleChanged( const QString & title );
    void slotPageKeyFilterChanged( const shared_ptr<KeyFilter> & filter );
    void slotPageStringFilterChanged( const QString & filter );
    void slotPageCanBeClosedChanged( bool on );

    Page * currentPage() const {
        assert( !tabWidget.currentWidget() || qobject_cast<Page*>( tabWidget.currentWidget() ) );
        return static_cast<Page*>( tabWidget.currentWidget() );
    }
    Page * page( unsigned int idx ) const {
        assert( !tabWidget.widget( idx ) || qobject_cast<Page*>( tabWidget.widget( idx ) ) );
        return static_cast<Page*>( tabWidget.widget( idx ) );
    }

    Page * senderPage() const {
        QObject * const sender = q->sender();
        assert( !sender || qobject_cast<Page*>( sender ) );
        return static_cast<Page*>( sender );
    }

    bool isSenderCurrentPage() const {
        Page * const sp = senderPage();
        return sp && sp == currentPage();
    }

    QAbstractItemView * addView( Page * page );
    void setCornerAction( QAction * action, Qt::Corner corner );

private:
    KTabWidget tabWidget;
};

TabWidget::Private::Private( TabWidget * qq )
    : q( qq ),
      tabWidget( q )
{
    KDAB_SET_OBJECT_NAME( tabWidget );

    tabWidget.setTabBarHidden( true );
    tabWidget.setTabReorderingEnabled( true );

    connect( &tabWidget, SIGNAL(currentChanged(int)),
             q, SLOT(currentIndexChanged(int)) );
}

void TabWidget::Private::currentIndexChanged( int index ) {
    if ( const Page * const page = this->page( index ) ) {
        emit q->currentViewChanged( page->view() );
        emit q->enableCloseCurrentTabAction( page->canBeClosed() && tabWidget.count() > 1 );
        emit q->keyFilterChanged( page->keyFilter() );
        emit q->stringFilterChanged( page->stringFilter() );
    } else {
        emit q->currentViewChanged( 0 );
        emit q->enableCloseCurrentTabAction( false );
        emit q->keyFilterChanged( shared_ptr<KeyFilter>() );
        emit q->stringFilterChanged( QString() );
    }
}

void TabWidget::Private::slotPageTitleChanged( const QString & ) {
    if ( Page * const page = senderPage() )
        tabWidget.setTabText( tabWidget.indexOf( page ), page->title() );
}

void TabWidget::Private::slotPageKeyFilterChanged( const shared_ptr<KeyFilter> & kf ) {
    if ( isSenderCurrentPage() )
        emit q->keyFilterChanged( kf );
}

void TabWidget::Private::slotPageStringFilterChanged( const QString & filter ) {
    if ( isSenderCurrentPage() )
        emit q->stringFilterChanged( filter );
}

void TabWidget::Private::slotPageCanBeClosedChanged( bool on ) {
    if ( isSenderCurrentPage() )
        emit q->enableCloseCurrentTabAction( on && tabWidget.count() > 1 );
}

TabWidget::TabWidget( QWidget * p, Qt::WindowFlags f )
    : QWidget( p, f ), d( new Private( this ) )
{

}

TabWidget::~TabWidget() {}

void TabWidget::setOpenNewTabAction( QAction * action ) {
    d->setCornerAction( action, Qt::TopLeftCorner );
    if ( !action )
        return;
    connect( action, SIGNAL(triggered()), this, SLOT(newTab()) );
}

void TabWidget::setDuplicateCurrentTabAction( QAction * action ) {
    if ( !action )
        return;
    connect( action, SIGNAL(triggered()), this, SLOT(duplicateCurrentTab()) );
}

void TabWidget::setCloseCurrentTabAction( QAction * action ) {
    d->setCornerAction( action, Qt::TopRightCorner );
    if ( !action )
        return;
    connect( action, SIGNAL(triggered()), this, SLOT(closeCurrentTab()) );
    connect( this, SIGNAL(enableCloseCurrentTabAction(bool)), action, SLOT(setEnabled(bool)) );
    const Page * const page = d->currentPage();
    action->setEnabled( count() > 1 && page && page->canBeClosed() );
}

void TabWidget::Private::setCornerAction( QAction * action, Qt::Corner corner ) {
    if ( !action )
        return;
    QToolButton * b = new QToolButton;
    b->setDefaultAction( action );
    tabWidget.setCornerWidget( b, corner );
}

void TabWidget::setStringFilter( const QString & filter ) {
    if ( Page * const page = d->currentPage() )
        page->setStringFilter( filter );
}

void TabWidget::setKeyFilter( const shared_ptr<KeyFilter> & filter ) {
    if ( Page * const page = d->currentPage() )
        page->setKeyFilter( filter );
}

QAbstractItemView * TabWidget::currentView() const {
    if ( Page * const page = d->currentPage() )
        return page->view();
    else
        return 0;
}

unsigned int TabWidget::count() const {
    return d->tabWidget.count();
}

void TabWidget::newTab() {
    //addView( QString(), QString() );
}

void TabWidget::closeCurrentTab() {
    const int current = d->tabWidget.currentIndex();
    if ( current < 0 || count() <= 1 )
        return;
    d->tabWidget.removeTab( current );
    d->tabWidget.setTabBarHidden( d->tabWidget.count() < 2 );
}

void TabWidget::duplicateCurrentTab() {
    const Page * const page = d->currentPage();
    if ( !page )
        return;
    Page * const clone = page->clone();
    assert( clone );
    clone->setCanBeClosed( true );
    d->addView( clone );
}

void TabWidget::resizeEvent( QResizeEvent * e ) {
    QWidget::resizeEvent( e );
    d->tabWidget.resize( e->size() );
}

QAbstractItemView * TabWidget::addView( AbstractKeyListModel * model, const QString & title, const QString & id, const QString & text ) {
    return d->addView( new Page( model, title, id, text ) );
}

QAbstractItemView * TabWidget::addView( AbstractKeyListModel * model, const KConfigGroup & group ) {
    return d->addView( new Page( model, group ) );
}

QAbstractItemView * TabWidget::Private::addView( Page * page ) {
    if ( !page )
        return 0;

    connect( page, SIGNAL(titleChanged(QString)),
             q, SLOT(slotPageTitleChanged(QString)) );
    connect( page, SIGNAL(keyFilterChanged(boost::shared_ptr<Kleo::KeyFilter>)),
             q, SLOT(slotPageKeyFilterChanged(boost::shared_ptr<Kleo::KeyFilter>)) );
    connect( page, SIGNAL(stringFilterChanged(QString)),
             q, SLOT(slotPageStringFilterChanged(QString)) );
    connect( page, SIGNAL(canBeClosedChanged(bool)),
             q, SLOT(slotPageCanBeClosedChanged(bool)) );

    QAbstractItemView * const previous = q->currentView(); 
    tabWidget.addTab( page, page->title() );
    tabWidget.setTabBarHidden( tabWidget.count() < 2 );
    // work around a bug in QTabWidget (tested with 4.3.2) not emitting currentChanged() when the first widget is inserted
    QAbstractItemView * const current = q->currentView(); 
    if ( previous != current )
        currentIndexChanged( tabWidget.currentIndex() );
    emit q->enableCloseCurrentTabAction( tabWidget.count() > 1 && currentPage() && currentPage()->canBeClosed() );
    return page->view();
}

void TabWidget::saveTab( unsigned int idx, KConfigGroup & group ) const {
    if ( const Page * const page = d->page( idx ) )
        page->saveTo( group );
}

#include "moc_tabwidget.cpp"
#include "tabwidget.moc"
