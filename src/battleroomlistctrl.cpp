/* Copyright (C) 2007 The SpringLobby Team. All rights reserved. */
//
// Class: BattleroomListCtrl
//

#include <wx/intl.h>
#include <wx/menu.h>
#include <wx/numdlg.h>
#include <wx/colordlg.h>
#include <wx/colour.h>
#include <wx/log.h>

#include <stdexcept>
#include <vector>

#include "battleroomlistctrl.h"
#include "iconimagelist.h"
#include "iunitsync.h"
#include "battle.h"
#include "ibattle.h"
#include "uiutils.h"
#include "ui.h"
#include "user.h"
#include "server.h"
#include "utils/debug.h"
#include "utils/conversion.h"
#include "uiutils.h"
#include "countrycodes.h"
#include "mainwindow.h"
#include "aui/auimanager.h"
#include "settings++/custom_dialogs.h"
#include "settings.h"

template<> SortOrder CustomVirtListCtrl<User*,BattleroomListCtrl>::m_sortorder = SortOrder();

IBattle* BattleroomListCtrl::s_battle = 0;

BEGIN_EVENT_TABLE( BattleroomListCtrl,  BattleroomListCtrl::BaseType )

  EVT_LIST_ITEM_RIGHT_CLICK( BRLIST_LIST, BattleroomListCtrl::OnListRightClick )
  EVT_MENU                 ( BRLIST_SPEC, BattleroomListCtrl::OnSpecSelect )
  EVT_MENU                 ( BRLIST_KICK, BattleroomListCtrl::OnKickPlayer )
  EVT_LIST_ITEM_ACTIVATED( BRLIST_LIST, BattleroomListCtrl::OnActivateItem )
//  EVT_MENU                 ( BRLIST_ADDCREATEGROUP, BattleroomListCtrl::OnPlayerAddToGroup )
//  EVT_MENU                 ( BRLIST_ADDTOGROUP, BattleroomListCtrl::OnPlayerAddToGroup )
  EVT_MENU                 ( BRLIST_RING, BattleroomListCtrl::OnRingPlayer )
  EVT_MENU                 ( BRLIST_COLOUR, BattleroomListCtrl::OnColourSelect )
  EVT_MENU                 ( BRLIST_HANDICAP, BattleroomListCtrl::OnHandicapSelect )
#if wxUSE_TIPWINDOW
#ifndef __WXMSW__ //disables tooltips on win
  EVT_MOTION(BattleroomListCtrl::OnMouseMotion)
#endif
#endif
END_EVENT_TABLE()


BattleroomListCtrl::BattleroomListCtrl( wxWindow* parent, IBattle* battle, Ui& ui, bool readonly )
    : CustomVirtListCtrl< User *,BattleroomListCtrl>(parent, BRLIST_LIST, wxDefaultPosition, wxDefaultSize,
                wxSUNKEN_BORDER | wxLC_REPORT | wxLC_SINGLE_SEL, _T("BattleroomListCtrl"), 10, 3, &CompareOneCrit,
                true /*highlight*/, UserActions::ActHighlight, !readonly /*periodic sort*/ ),
	m_battle(battle),
	m_popup(0),
    m_sel_user(0),
    m_sides(0),
    m_spec_item(0),
    m_handicap_item(0),
    m_ui(ui),
    m_ro(readonly)
{
  GetAui().manager->AddPane( this, wxLEFT, _T("battleroomlistctrl") );

  wxListItem col;

    const int hd = wxLIST_AUTOSIZE_USEHEADER;

#if defined(__WXMAC__)
/// on mac, autosize does not work at all
    const int widths[10] = {20,20,20,170,140,130,110,28,28,28}; //!TODO revise plox
#else
    const int widths[10] = {hd,hd,hd,hd,hd,170,hd,hd,80,130};
#endif

    AddColumn( 0, widths[0], _T("Status"), _T("Player/Bot") );
    AddColumn( 1, widths[1], _T("Faction"), _T("Faction icon") );
    AddColumn( 2, widths[2], _T("Colour"), _T("Teamcolour") );
    AddColumn( 3, widths[3], _T("Country"), _T("Country") );
    AddColumn( 4, widths[4], _T("Rank"), _T("Rank") );
    AddColumn( 5, widths[5], _("Nickname"), _T("Ingame name"));
    AddColumn( 6, widths[6], _("Team"), _T("Team number") );
    AddColumn( 7, widths[7], _("Ally"), _T("Ally number") );
    AddColumn( 8, widths[8], _("CPU"), _T("CPU speed (might not be accurate)") );
    AddColumn( 9, widths[9], _("Resource Bonus"), _T("Resource Bonus") );

    if ( m_sortorder.size() == 0 ) {
        m_sortorder[0].col = 6;
        m_sortorder[0].direction = true;
        m_sortorder[1].col = 7;
        m_sortorder[1].direction = true;
        m_sortorder[2].col = 5;
        m_sortorder[2].direction = true;
    }

  	if ( !m_ro )
	{
		m_popup = new UserMenu(this, this);
		wxMenu* m_teams;
		m_teams = new wxMenu();

		for ( unsigned int i = 0; i < SPRING_MAX_TEAMS; i++ )
		{
			wxMenuItem* team = new wxMenuItem( m_teams, BRLIST_TEAM + i, wxString::Format( _T("%d"), i+1 ) , wxEmptyString, wxITEM_NORMAL );
			m_teams->Append( team );
			Connect( BRLIST_TEAM + i, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( BattleroomListCtrl::OnTeamSelect ) );
		}
		m_popup->Append( -1, _("Team"), m_teams );

		wxMenu* m_allies = new wxMenu();
		for ( unsigned int i = 0; i < SPRING_MAX_ALLIES; i++ )
		{
			wxMenuItem* ally = new wxMenuItem( m_allies, BRLIST_ALLY + i, wxString::Format( _T("%d"), i+1 ) , wxEmptyString, wxITEM_NORMAL );
			m_allies->Append( ally );
			Connect( BRLIST_ALLY + i, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( BattleroomListCtrl::OnAllySelect ) );
		}
		m_popup->Append( -1, _("Ally"), m_allies );

		m_sides = new wxMenu();
		try
		{
			wxArrayString sides = usync().GetSides( m_battle->GetHostModName() );
			for ( unsigned int i = 0; i < sides.GetCount(); i++ )
			{
				wxMenuItem* side = new wxMenuItem( m_sides, BRLIST_SIDE + i, sides[i], wxEmptyString, wxITEM_NORMAL );
				m_sides->Append( side );
				Connect( BRLIST_SIDE + i, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( BattleroomListCtrl::OnSideSelect ) );
			}
		} catch (...) {}
		m_popup->Append( -1, _("Side"), m_sides );

		m_popup->AppendSeparator();

		wxMenuItem* m_colours = new wxMenuItem( m_popup, BRLIST_COLOUR, _("Set color"), wxEmptyString, wxITEM_NORMAL );
		m_popup->Append( m_colours );

		m_popup->AppendSeparator();

		m_handicap_item = new wxMenuItem( m_popup, BRLIST_HANDICAP, _("Set Resource Bonus"), wxEmptyString, wxITEM_NORMAL );
		m_popup->Append( m_handicap_item );

		m_popup->AppendSeparator();

		m_spec_item = new wxMenuItem( m_popup, BRLIST_SPEC, wxString( _("Spectator") ) , wxEmptyString, wxITEM_CHECK );
		m_popup->Append( m_spec_item );

		m_popup->AppendSeparator();

		wxMenuItem* kick = new wxMenuItem( m_popup, BRLIST_KICK, wxString( _("Kick") ) , wxEmptyString, wxITEM_NORMAL );
		m_popup->Append( kick );
		wxMenuItem* ring = new wxMenuItem( m_popup, BRLIST_RING, wxString( _("Ring") ) , wxEmptyString, wxITEM_NORMAL );
		m_popup->Append( ring );

	}
}


BattleroomListCtrl::~BattleroomListCtrl()
{

}

void BattleroomListCtrl::SetBattle( IBattle* battle )
{
	m_battle = battle;
}

IBattle& BattleroomListCtrl::GetBattle()
{
	ASSERT_EXCEPTION( m_battle, _T("m_battle == 0") );
	return *m_battle;
}

void BattleroomListCtrl::AddUser( User& user )
{
    if ( AddItem( &user ) )
        return;

    wxLogWarning( _T("user already in battleroom list.") );
}

void BattleroomListCtrl::RemoveUser( User& user )
{
    if ( RemoveItem( &user ) )
        return;

    wxLogError( _T("Didn't find the user to remove in battleroomlistctrl.") );
}

void BattleroomListCtrl::UpdateUser( User& user )
{
    if ( !user.BattleStatus().spectator )
        icons().SetColourIcon( user.BattleStatus().team, user.BattleStatus().colour );
    wxArrayString sides = usync().GetSides( m_battle->GetHostModName() );
    ASSERT_EXCEPTION( user.BattleStatus().side < (long)sides.GetCount(), _T("Side index too high") );
    user.SetSideiconIndex( icons().GetSideIcon( m_battle->GetHostModName(), user.BattleStatus().side ) );
    int index = GetIndexFromData( &user );
    UpdateUser( index );
}

wxListItemAttr * BattleroomListCtrl::GetItemAttr(long item) const
{
    if ( item == -1 || item >= (long)m_data.size())
        return NULL;

    const User& user = *GetDataFromIndex( item );
    bool is_bot = user.BattleStatus().IsBot();

    if ( !is_bot ) {
        return HighlightItemUser( user.GetNick() );
    }

    return NULL;
}

int BattleroomListCtrl::GetItemColumnImage(long item, long column) const
{
    if ( item == -1 || item >= (long)m_data.size())
        return -1;

    const User& user = *GetDataFromIndex( item );
    bool is_bot = user.BattleStatus().IsBot();
    bool is_spec = user.BattleStatus().spectator;

    switch ( column ) {
        case 0: {
            if ( !is_bot ) {
                if ( m_battle->IsFounder( user ) ) {
                    return icons().GetHostIcon( is_spec );
                }
                else {
                    return icons().GetReadyIcon( is_spec, user.BattleStatus().ready, user.BattleStatus().sync, is_bot );
                }
            }
            else
                return icons().ICON_BOT;
        }
        case 2: return is_spec ? -1: icons().GetColourIcon( user.BattleStatus().team );
        case 3: return is_bot ? -1 : icons().GetFlagIcon( user.GetCountry() );
        case 4: return is_bot ? -1 : icons().GetRankIcon( user.GetStatus().rank );
        case 1: return is_spec ? -1 : user.GetSideiconIndex();
        case 6:
        case 7:
        case 8:
        case 9:
        case 5: return -1;
        default: {
            wxLogWarning( _T("column oob in BattleroomListCtrl::OnGetItemColumnImage") );
            return -1;
        }
    }
}

wxString BattleroomListCtrl::GetItemText(long item, long column) const
{
    if ( item == -1 || item >= (long)m_data.size())
        return _T("");

    const User& user = *GetDataFromIndex( item );
    bool is_bot = user.BattleStatus().IsBot();
    bool is_spec = user.BattleStatus().spectator;

    switch ( column ) {
        case 1: {
            try {
                wxArrayString sides = usync().GetSides( m_battle->GetHostModName() );
                ASSERT_EXCEPTION( user.BattleStatus().side < (long)sides.GetCount(), _T("Side index too high") );
            }
            catch ( ... ) {
                return wxString::Format( _T("s%d"), user.BattleStatus().side + 1 );
            }
            return _T("");
        }
        case 5: return is_bot ? user.GetNick() + _T(" (") + user.BattleStatus().owner + _T(")") : user.GetNick();
        case 6: return is_spec ? _T("") : wxString::Format( _T("%d"), user.BattleStatus().team + 1 );
        case 7: return is_spec ? _T("") : wxString::Format( _T("%d"), user.BattleStatus().ally + 1 );
        case 8: {
            if (!is_bot )
                return wxString::Format( _T("%.1f GHz"), user.GetCpu() / 1000.0 );
            else { //!TODO could prolly be cached
                wxString botname = user.BattleStatus().aishortname;
                if ( !user.BattleStatus().aiversion.IsEmpty() ) botname += _T(" ") + user.BattleStatus().aiversion;
                if ( !usync().VersionSupports( IUnitSync::USYNC_GetSkirmishAI ) )
                {
                    if ( botname.Contains(_T('.')) ) botname = botname.BeforeLast(_T('.'));
                    if ( botname.Contains(_T('/')) ) botname = botname.AfterLast(_T('/'));
                    if ( botname.Contains(_T('\\')) ) botname = botname.AfterLast(_T('\\'));
                    if ( botname.Contains(_T("LuaAI:")) ) botname = botname.AfterFirst(_T(':'));
                }
                return botname;
            }
        }
        case 9: return is_spec ? _T("") : wxString::Format( _T("%d%%"), user.BattleStatus().handicap );
        case 0:
        case 2:
        case 3:
        case 4: return _T("");
        default: {
            wxLogWarning( _T("column oob in BattleroomListCtrl::OnGetItemText") );
            return _T("");
        }
    }
}

void BattleroomListCtrl::UpdateUser( const int& index )
{
    Freeze();
    RefreshItem( index );
    Thaw();
    MarkDirtySort();
}

void BattleroomListCtrl::OnListRightClick( wxListEvent& event )
{
    wxLogDebugFunc( _T("") );
	if ( m_ro ) return;
	int index = event.GetIndex();

    if ( index == -1 || index >= (long)m_data.size()) return;

    User& user = *GetDataFromIndex( event.GetIndex() );
    m_sel_user = &user; //this is set for event handlers

    if ( user.BattleStatus().IsBot() )
    {
        wxLogMessage(_T("Bot"));

        int item = m_popup->FindItem( _("Spectator") );
        m_popup->Enable( item, false );
        m_popup->Check( item, false );
        m_popup->Enable( m_popup->FindItem( _("Ring") ), false );
        m_popup->Enable( m_popup->FindItem( _("Kick") ),true);
    }
    else
    {
        wxLogMessage(_T("User"));
        int item = m_popup->FindItem( _("Spectator") );
        m_popup->Check( item, m_sel_user->BattleStatus().spectator );
        m_popup->Enable( item, true );
        m_popup->Enable( m_popup->FindItem( _("Ring") ), true );
        bool isSelUserMe =  ( m_ui.IsThisMe(user) );
        m_popup->Enable( m_popup->FindItem( _("Kick") ),!isSelUserMe);
    }

    wxLogMessage(_T("Popup"));
    m_popup->EnableItems( !user.BattleStatus().IsBot(), GetSelectedUserNick() );//this updates groups, therefore we need to update the connection to evt handlers too
    std::vector<long> groups_ids = m_popup->GetGroupIds();
    for (std::vector<long>::const_iterator it = groups_ids.begin(); it != groups_ids.end(); ++it) {
        Connect( *it, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( BattleroomListCtrl::OnUserMenuAddToGroup ), 0, this );
    }
    Connect( GROUP_ID_NEW, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( BattleroomListCtrl::OnUserMenuCreateGroup), 0, this );
    PopupMenu( m_popup );
    wxLogMessage(_T("Done"));
}


void BattleroomListCtrl::OnTeamSelect( wxCommandEvent& event )
{
  wxLogDebugFunc( _T("") );
  int team = event.GetId() - BRLIST_TEAM;
	if( m_sel_user ) ((Battle*)m_battle)->ForceTeam( *m_sel_user, team );
}


void BattleroomListCtrl::OnAllySelect( wxCommandEvent& event )
{
  wxLogDebugFunc( _T("") );
  int ally = event.GetId() - BRLIST_ALLY;
	if( m_sel_user ) ((Battle*)m_battle)->ForceAlly( *m_sel_user, ally );
}


void BattleroomListCtrl::OnColourSelect( wxCommandEvent& /*unused*/ )
{
  wxLogDebugFunc( _T("") );

	wxColour CurrentColour = m_sel_user->BattleStatus().colour;
	CurrentColour = GetColourFromUser(this, CurrentColour);
	if ( !CurrentColour.IsColourOk() ) return;
	if( m_sel_user ) ((Battle*)m_battle)->ForceColour( *m_sel_user, CurrentColour );

}


void BattleroomListCtrl::OnSideSelect( wxCommandEvent& event )
{
  wxLogDebugFunc( _T("") );
  int side = event.GetId() - BRLIST_SIDE;
  if( m_sel_user ) ((Battle*)m_battle)->ForceSide( *m_sel_user, side );
}


void BattleroomListCtrl::OnHandicapSelect( wxCommandEvent& /*unused*/ )
{
  wxLogDebugFunc( _T("") );
  if( !m_sel_user ) return;
  long handicap = wxGetNumberFromUser( _("Please enter a value between 0 and 100"), _("Set Resource Bonus"), _T(""), m_sel_user->BattleStatus().handicap, 0, 100, (wxWindow*)&ui().mw(), wxDefaultPosition );
	if ( handicap != -1 )
	{
     ((Battle*)m_battle)->SetHandicap( *m_sel_user, handicap );
  }
}


void BattleroomListCtrl::OnSpecSelect( wxCommandEvent& /*unused*/ )
{
  wxLogDebugFunc( _T("") );
  if ( m_sel_user ) ((Battle*)m_battle)->ForceSpectator( *m_sel_user, m_spec_item->IsChecked() );
}


void BattleroomListCtrl::OnKickPlayer( wxCommandEvent& /*unused*/ )
{
  wxLogDebugFunc( _T("") );
	if ( m_sel_user ) ((Battle*)m_battle)->KickPlayer( *m_sel_user );
}


void BattleroomListCtrl::OnRingPlayer( wxCommandEvent& /*unused*/ )
{
  wxLogDebugFunc( _T("") );
  if ( m_sel_user ) ((Battle*)m_battle)->GetServer().Ring( m_sel_user->GetNick() );
}

void BattleroomListCtrl::Sort()
{
    if ( m_data.size() > 0 )
    {
        SaveSelection();
        s_battle = m_battle;
        SLInsertionSort( m_data, m_comparator );
        RestoreSelection();
    }
}

int BattleroomListCtrl::CompareOneCrit(DataType u1, DataType u2, int col, int dir)
{
    switch ( col ) {
        case 0: return dir * CompareStatus( u1, u2, s_battle );
        case 1: return dir * CompareSide( u1, u2 );
        case 2: return dir * CompareColor( u1, u2 );
        case 3: return dir * compareSimple( u1, u2 );
        case 4: return dir * CompareRank( u1, u2 );
        case 5: return dir * u1->GetNick().CmpNoCase( u2->GetNick() );
        case 6: return dir * CompareTeam( u1, u2 );
        case 7: return dir * CompareAlly( u1, u2 );
        case 8: return dir * CompareCpu( u1, u2 );
        case 9: return dir * CompareHandicap( u1, u2 );
        default: return 0;
    }
}

int BattleroomListCtrl::CompareStatus(const DataType user1, const DataType user2, const IBattle* m_battle )
{
  int status1 = 0;
  if ( user1->BattleStatus().IsBot() )
  {
    status1 = 9;
  }
  else
  {
    if ( &m_battle->GetFounder() != user1 )
      status1 = 1;
    if ( user1->BattleStatus().ready )
      status1 += 5;
    if ( user1->BattleStatus().sync )
      status1 += 7;
    if ( user1->BattleStatus().spectator )
      status1 += 10;
  }

  int status2 = 0;
  if ( user1->BattleStatus().IsBot() )
  {
    status2 = 9;
  }
  else
  {
    if ( &m_battle->GetFounder() != user2 )
      status2 = 1;
    if ( user2->BattleStatus().ready )
      status2 += 5;
    if ( user2->BattleStatus().sync )
      status2 += 7;
    if ( user2->BattleStatus().spectator )
      status2 += 10;
  }

  if ( status1 < status2 )
      return -1;
  if ( status1 > status2 )
      return 1;

  return 0;
}

int BattleroomListCtrl::CompareSide(const DataType user1, const DataType user2)
{
	if ( !user1 && !user2 ) return 0;
	else if ( !user1 ) return 1;
	else if ( !user2 ) return -1;

  int side1;
  if ( user1->BattleStatus().spectator )
    side1 = 1000;
  else
    side1 = user1->BattleStatus().side;

  int side2;
  if ( user2->BattleStatus().spectator )
    side2 = 1000;
  else
    side2 = user2->BattleStatus().side;

  if ( side1 < side2 )
      return -1;
  if ( side1 > side2 )
      return 1;

  return 0;
}

int BattleroomListCtrl::CompareColor(const DataType user1, const DataType user2)
{
	if ( !user1 && !user2 ) return 0;
	else if ( !user1 ) return 1;
	else if ( !user2 ) return -1;

  int color1_r, color1_g, color1_b;

	if ( user1->BattleStatus().spectator ) return -1;
	color1_r = user1->BattleStatus().colour.Red();
	color1_g = user1->BattleStatus().colour.Green();
	color1_b = user1->BattleStatus().colour.Blue();

  int color2_r, color2_g, color2_b;

	if ( user2->BattleStatus().spectator ) return 1;
	color2_r = user2->BattleStatus().colour.Red();
	color2_g = user2->BattleStatus().colour.Green();
	color2_b = user2->BattleStatus().colour.Blue();

  if ( (color1_r + color1_g + color1_b)/3 < (color2_r + color2_g + color2_b)/3 )
      return -1;
  if ( (color1_r + color1_g + color1_b)/3 > (color2_r + color2_g + color2_b)/3 )
      return 1;

  return 0;
}


int BattleroomListCtrl::CompareRank(const DataType user1, const DataType user2)
{
	if ( !user1 && !user2 ) return 0;
	else if ( !user1 ) return 1;
	else if ( !user2 ) return -1;

  int rank1;
  if ( user1->BattleStatus().IsBot() )
    rank1 = 1000;
  else
    rank1 = user1->GetStatus().rank;

  int rank2;
  if ( user2->BattleStatus().IsBot() )
    rank2 = 1000;
  else
    rank2 = user2->GetStatus().rank;

  if ( rank1 < rank2 )
      return -1;
  if ( rank1 > rank2 )
      return 1;

  return 0;
}

int BattleroomListCtrl::CompareTeam(const DataType user1, const DataType user2)
{
	if ( !user1 && !user2 ) return 0;
	else if ( !user1 ) return 1;
	else if ( !user2 ) return -1;

  int team1;
  if ( user1->BattleStatus().spectator )
    team1 = 1000;
  else
    team1 = user1->BattleStatus().team;

  int team2;
  if ( user2->BattleStatus().spectator )
    team2 = 1000;
  else
    team2 = user2->BattleStatus().team;

  if ( team1 < team2 )
      return -1;
  if ( team1 > team2 )
      return 1;

  return 0;
}

int BattleroomListCtrl::CompareAlly(const DataType user1, const DataType user2)
{
	if ( !user1 && !user2 ) return 0;
	else if ( !user1 ) return 1;
	else if ( !user2 ) return -1;

  int ally1;
  if ( user1->BattleStatus().spectator )
    ally1 = 1000;
  else
    ally1 = user1->BattleStatus().ally;

  int ally2;
  if ( user2->BattleStatus().spectator )
    ally2 = 1000;
  else
    ally2 = user2->BattleStatus().ally;

  if ( ally1 < ally2 )
      return -1;
  if ( ally1 > ally2 )
      return 1;

  return 0;
}

int BattleroomListCtrl::CompareCpu(const DataType user1, const DataType user2)
{
	if ( !user1 && !user2 ) return 0;
	else if ( !user1 ) return 1;
	else if ( !user2 ) return -1;

  if ( user1->BattleStatus().IsBot() )
  {
    wxString ailib1 = user1->BattleStatus().aishortname.Upper() + _T(" ") + user1->BattleStatus().aiversion.Upper();
    if ( user2->BattleStatus().IsBot() )
    {
      wxString ailib2 = user2->BattleStatus().aishortname.Upper() + _T(" ") + user2->BattleStatus().aiversion.Upper();
      if ( ailib1 < ailib2 )
        return -1;
      if ( ailib1 > ailib2 )
        return 1;
      return 0;
    } else {
      return 1;
    }
  }
  else
  {
    int cpu1 = user1->GetCpu();
    if ( user1->BattleStatus().IsBot() )
    {
      return -1;
    } else {
      int cpu2 = user2->GetCpu();
      if ( cpu1 < cpu2 )
        return -1;
      if ( cpu1 > cpu2 )
        return 1;
      return 0;
    }
  }
}

int BattleroomListCtrl::CompareHandicap(const DataType user1, const DataType user2)
{
  int handicap1;
	if ( user1->BattleStatus().spectator )
    handicap1 = 1000;
  else
    handicap1 = user1->BattleStatus().handicap;

  int handicap2;
	if ( user2->BattleStatus().spectator )
    handicap2 = 1000;
  else
    handicap2 = user2->BattleStatus().handicap;

  if ( handicap1 < handicap2 )
      return -1;
  if ( handicap1 > handicap2 )
      return 1;

  return 0;
}

void BattleroomListCtrl::SetTipWindowText( const long item_hit, const wxPoint& position)
{
    if ( item_hit < 0 || item_hit >= (long)m_data.size() )
        return;

    const User& user = *GetDataFromIndex( item_hit );

    int column = getColumnFromPosition( position );
    if (column > (int)m_colinfovec.size() || column < 0)
    {
        m_tiptext = _T("");
    }
    else
    {
        switch (column)
        {
        case 0: // is bot?
            m_tiptext = _("");

            if ( user.BattleStatus().IsBot() )
                m_tiptext += _("AI (bot)\n");
            else
                m_tiptext += _("Human\n");

            if ( user.BattleStatus().spectator )
                m_tiptext += _("Spectator\n");
            else
                m_tiptext += _("Player\n");

            if ( m_battle->IsFounder( user ) )
                m_tiptext += _("Host\n");
            else
                m_tiptext += _("Client\n");

            if ( user.BattleStatus().ready )
                m_tiptext += _("Ready\n");
            else
                m_tiptext += _("Not ready\n");

            if  ( user.BattleStatus().sync == SYNC_SYNCED )
                m_tiptext += _("In sync");
            else
                m_tiptext += _("Not in sync");
            break;
        case 1: // icon
            if ( user.BattleStatus().spectator )
                m_tiptext = _T("Spectators have no side");
            else
            {
								wxArrayString sides = usync().GetSides( m_battle->GetHostModName() );
								int side = user.BattleStatus().side;
								if ( side < (int)sides.GetCount() ) m_tiptext = sides[side];
            }
            break;

        case 3: // country
            m_tiptext = user.BattleStatus().IsBot() ? _T("This bot is from nowhere particular") : GetFlagNameFromCountryCode(user.GetCountry());
            break;
        case 4: // rank
            m_tiptext = user.BattleStatus().IsBot() ? _T("This bot has no rank") : user.GetRankName(user.GetStatus().rank);
            break;

        case 5: //name
            m_tiptext = user.BattleStatus().IsBot() ?user.GetNick() : user.GetNick();
            break;

        case 8: // cpu
            m_tiptext = user.BattleStatus().IsBot() ? ( user.BattleStatus().aishortname + _T(" ") + user.BattleStatus().aiversion ) : m_colinfovec[column].tip;
            break;

        default:
            m_tiptext =m_colinfovec[column].tip;
            break;
        }
    }
}

void BattleroomListCtrl::OnUserMenuAddToGroup( wxCommandEvent& event )
{
    int id  = event.GetId();
    wxString groupname = m_popup->GetGroupByEvtID(id);
    wxString nick = GetSelectedUserNick();
    useractions().AddUserToGroup( groupname, nick );
    Disconnect( GROUP_ID_NEW, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( BattleroomListCtrl::OnUserMenuCreateGroup), 0, this );
}

void BattleroomListCtrl::OnUserMenuDeleteFromGroup( wxCommandEvent& /*unused*/ )
{
    wxString nick = GetSelectedUserNick();
    useractions().RemoveUser( nick );
}

void BattleroomListCtrl::OnUserMenuCreateGroup( wxCommandEvent& /*unused*/ )
{
    wxString name;
    if ( ui().AskText( _("Enter name"),
        _("Please enter the name for the new group.\nAfter clicking ok you will be taken to adjust its settings."), name ) )
    {
        wxString nick = GetSelectedUserNick();

        useractions().AddGroup( name );
        useractions().AddUserToGroup( name, nick );
        ui().mw().ShowConfigure( MainWindow::OPT_PAGE_GROUPS );
    }
}

wxString BattleroomListCtrl::GetSelectedUserNick()
{
    if ( m_selected_index < 0 || m_selected_index >= (long)m_data.size() )
        return wxEmptyString;
    else
        return m_data[m_selected_index]->GetNick();
}

void BattleroomListCtrl::OnActivateItem( wxListEvent& /*unused*/ )
{
    if ( m_ro )
        return;
    if ( m_selected_index < 0 || m_selected_index >= (long)m_data.size() )
        return;

    const User* usr = m_data[m_selected_index];
    if ( usr != NULL && !usr->BattleStatus().IsBot() )
        ui().mw().OpenPrivateChat( *usr );
}

int BattleroomListCtrl::GetIndexFromData(const DataType& data) const
{
    const User* user = data;
    static long seekpos;
    seekpos = clamp( seekpos, 0l , (long)m_data.size() );
    int index = seekpos;

    for ( DataCIter f_idx = m_data.begin() + seekpos; f_idx != m_data.end() ; ++f_idx )
    {
        if ( user == *f_idx )
        {
            seekpos = index;
            return seekpos;
        }
        index++;
    }
    //it's ok to init with seekpos, if it had changed this would not be reached
    int r_index = seekpos - 1;
    for ( DataRevCIter r_idx = m_data.rbegin() + ( m_data.size() - seekpos ); r_idx != m_data.rend() ; ++r_idx )
    {
        if ( user == *r_idx )
        {
            seekpos = r_index;
            return seekpos;
        }
        r_index--;
    }

    return -1;
}

