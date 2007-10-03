/* Copyright (C) 2007 The SpringLobby Team. All rights reserved. */


#include <wx/arrstr.h>
#include <stdexcept>

#include "unitsyncthread.h"
#include "utils.h"
#include "iunitsync.h"
#include "ui.h"

#define LOCK_CACHE wxCriticalSectionLocker lock_criticalsection(m_lock)

DEFINE_LOCAL_EVENT_TYPE( wxEVT_UNITSYNC_CACHE )


BEGIN_EVENT_TABLE( UnitSyncThread, wxEvtHandler )

    EVT_COMMAND ( CACHE_MINIMAP, wxEVT_UNITSYNC_CACHE, UnitSyncThread::OnMinimapCached )

END_EVENT_TABLE();


UnitSyncThread::UnitSyncThread( Ui& ui ):
  m_ui(ui),
  m_delay(-1),
  m_should_terminate(false),
  m_should_pause(false)
{
}


UnitSyncThread::~UnitSyncThread()
{
}


void* UnitSyncThread::Entry()
{
  _CacheLoop();
  return 0;
}


void UnitSyncThread::OnExit()
{
}



void UnitSyncThread::AddMapInfoOrder( const wxString& map, int startdelay )
{
  LOCK_CACHE;
  m_delay = startdelay;
  _AddJob( JT_MAPINFO, map );
}


void UnitSyncThread::AddMinimapOrder( const wxString& map, int startdelay )
{
  debug_func("");
  LOCK_CACHE;
  debug("");
  m_delay = startdelay;
  _AddJob( JT_MINIMAP, map );
}


void UnitSyncThread::AddModUnitsOrder( const wxString& mod, int startdelay )
{
  LOCK_CACHE;
  m_delay = startdelay;
  _AddJob( JT_UNITS, mod );
}


void UnitSyncThread::Pause()
{
  LOCK_CACHE;
  m_should_pause = true;
}


void UnitSyncThread::Resume()
{
  LOCK_CACHE;
  m_should_pause = false;
  //if ( !IsRunning() ) wxThread::Resume();
}



void UnitSyncThread::_CacheLoop()
{
  JobType job;
  wxString params;
  while ( !TestDestroy() ) {

    while ( m_should_pause ) Sleep( 100 );

    _GetNextJob( job, params );

    switch ( job ) {
      case JT_MAPINFO : _DoMapInfoJob( params ); break;
      case JT_MINIMAP : _DoMinimapJob( params ); break;
      case JT_UNITS   : _DoUnitsJob( params ); break;
      case JT_PAUSE   : Sleep( 100 ); break;
    };

  }
}


bool UnitSyncThread::_GetNextJob( JobType& jobtype, wxString& params )
{
  LOCK_CACHE;
  if ( m_orders.GetCount() == 0 ) {
    jobtype = JT_PAUSE;
    return false;
  }

  wxString Order = m_orders.Item(0);
  m_orders.RemoveAt( 0 );
  ASSERT_LOGIC( Order.Length() > 0, "Bad order" );
  jobtype = Order[0];
  params = Order.Remove( 0, 1 );
  return true;
}


void UnitSyncThread::_AddJob( const wxChar& prefix, const wxString& params )
{
  m_orders.Add( prefix + params );

/*  if ( m_should_pause ) return;
  if ( !IsRunning() ) wxThread::Resume();*/
}


void UnitSyncThread::_DoMapInfoJob( const wxString& map )
{

}


void UnitSyncThread::_DoMinimapJob( const wxString& map )
{
  usync()->CacheMinimap( map );
  m_last_job = map;
  wxCommandEvent event( wxEVT_UNITSYNC_CACHE, CACHE_MINIMAP );
  AddPendingEvent(event);
}


void UnitSyncThread::_DoUnitsJob( const wxString& mod )
{
}


void UnitSyncThread::OnMinimapCached( wxCommandEvent& event )
{
  debug_func("");
  LOCK_CACHE;
  m_ui.OnMinimapCached( m_last_job );
}

