/* Copyright (C) 2007 The SpringLobby Team. All rights reserved. */

#include <wx/dynlib.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/image.h>
#include <wx/intl.h>
#include <wx/msgdlg.h>
#include <wx/mstream.h>
#include <wx/stdpaths.h>
#include <wx/string.h>
#include <wx/file.h>
//#include <wx/txtstrm.h>
//#include <wx/wfstream.h>
#include <wx/textfile.h>

#include <stdexcept>

#include "springunitsync.h"
#include "utils.h"
#include "settings.h"


#define LOCK_UNITSYNC wxCriticalSectionLocker lock_criticalsection(m_lock)


struct SpringMapInfo
{
  char* description;
  int tidalStrength;
  int gravity;
  float maxMetal;
  int extractorRadius;
  int minWind;
  int maxWind;

  int width;
  int height;
  int posCount;
  StartPos positions[16];

  char* author;
};


struct CachedMapInfo
{
  char name[256];
  char author[256];
  char description[256];

  int tidalStrength;
  int gravity;
  float maxMetal;
  int extractorRadius;
  int minWind;
  int maxWind;

  int width;
  int height;

  int posCount;
  StartPos positions[16];
};


struct UnitSyncColour {
  unsigned int b : 5;
  unsigned int g : 6;
  unsigned int r : 5;
};


IUnitSync* usync()
{
  static SpringUnitSync* m_sync = 0;
  if (!m_sync)
    m_sync = new SpringUnitSync;
  return m_sync;
}


void* SpringUnitSync::_GetLibFuncPtr( const std::string& name )
{
  ASSERT_LOGIC( m_loaded, _T("Unitsync not loaded"));
  void* ptr = m_libhandle->GetSymbol(WX_STRING(name));
  ASSERT_RUNTIME( ptr, WX_STRING(("Couldn't load " + name + " from unitsync library")) );
  return ptr;
}


bool SpringUnitSync::LoadUnitSyncLib( const wxString& springdir, const wxString& unitsyncloc )
{
  wxLogDebugFunc( _T("") );
  LOCK_UNITSYNC;
  return _LoadUnitSyncLib( springdir, unitsyncloc );
}


bool SpringUnitSync::_LoadUnitSyncLib( const wxString& springdir, const wxString& unitsyncloc )
{
  if ( m_loaded ) return true;

  wxSetWorkingDirectory( springdir );

  // Load the library.
  std::string loc = STD_STRING(unitsyncloc);

  wxLogMessage( _T("Loading from: ") + WX_STRING(loc) );

  // Check if library exists
  if ( !wxFileName::FileExists( WX_STRING(loc)) ) {
    wxLogError( _T("File not found: ") + WX_STRING(loc) );
    return false;
  }

  try {
    m_libhandle = new wxDynamicLibrary(WX_STRING(loc));
    if (!m_libhandle->IsLoaded()) {
      wxLogError( _T("wxDynamicLibrary created, but not loaded!") );
      delete m_libhandle;
      m_libhandle = 0;
    }
  } catch(...) {
    m_libhandle = 0;
  }

  if (m_libhandle == 0) {
    wxLogError( _T("Couldn't load the unitsync library") );
    return false;
  }

  m_loaded = true;

  // Load all function from library.
  try {
    m_init = (InitPtr)_GetLibFuncPtr("Init");
    m_uninit = (UnInitPtr)_GetLibFuncPtr("UnInit");

    m_get_map_count = (GetMapCountPtr)_GetLibFuncPtr("GetMapCount");
    m_get_map_checksum = (GetMapChecksumPtr)_GetLibFuncPtr("GetMapChecksum");
    m_get_map_name = (GetMapNamePtr)_GetLibFuncPtr("GetMapName");
    m_get_map_info_ex = (GetMapInfoExPtr)_GetLibFuncPtr("GetMapInfoEx");
    m_get_minimap = (GetMinimapPtr)_GetLibFuncPtr("GetMinimap");
    m_get_mod_checksum = (GetPrimaryModChecksumPtr)_GetLibFuncPtr("GetPrimaryModChecksum");
    m_get_mod_index = (GetPrimaryModIndexPtr)_GetLibFuncPtr("GetPrimaryModIndex");
    m_get_mod_name = (GetPrimaryModNamePtr)_GetLibFuncPtr("GetPrimaryModName");
    m_get_mod_count = (GetPrimaryModCountPtr)_GetLibFuncPtr("GetPrimaryModCount");
    m_get_mod_archive = (GetPrimaryModArchivePtr)_GetLibFuncPtr("GetPrimaryModArchive");

    m_get_side_count = (GetSideCountPtr)_GetLibFuncPtr("GetSideCount");
    m_get_side_name = (GetSideNamePtr)_GetLibFuncPtr("GetSideName");

    m_add_all_archives = (AddAllArchivesPtr)_GetLibFuncPtr("AddAllArchives");

    m_get_unit_count = (GetUnitCountPtr)_GetLibFuncPtr("GetUnitCount");
    m_get_unit_name = (GetUnitNamePtr)_GetLibFuncPtr("GetUnitName");
    m_get_unit_full_name = (GetFullUnitNamePtr)_GetLibFuncPtr("GetFullUnitName");
    m_proc_units_nocheck = (ProcessUnitsNoChecksumPtr)_GetLibFuncPtr("ProcessUnitsNoChecksum");

    m_init_find_vfs = (InitFindVFSPtr)_GetLibFuncPtr("InitFindVFS");
    m_find_files_vfs = (FindFilesVFSPtr)_GetLibFuncPtr("FindFilesVFS");
    m_open_file_vfs = (OpenFileVFSPtr)_GetLibFuncPtr("OpenFileVFS");
    m_file_size_vfs = (FileSizeVFSPtr)_GetLibFuncPtr("FileSizeVFS");
    m_read_file_vfs = (ReadFileVFSPtr)_GetLibFuncPtr("ReadFileVFS");
    m_close_file_vfs = (CloseFileVFSPtr)_GetLibFuncPtr("CloseFileVFS");

    m_get_spring_version = (GetSpringVersionPtr)_GetLibFuncPtr("GetSpringVersion");

    m_init( true, 1 );
    m_map_count = m_get_map_count();
    m_mod_count = m_get_mod_count();
    m_current_mod = "";
    m_side_count = 0;
  }
  catch ( ... ) {
    wxLogError( _T("Failed to load Unitsync lib.") );
    _FreeUnitSyncLib();
    return false;
  }

  return true;
}


void SpringUnitSync::FreeUnitSyncLib()
{
  wxLogDebugFunc( _T("") );
  LOCK_UNITSYNC;
  _FreeUnitSyncLib();
}


void SpringUnitSync::_FreeUnitSyncLib()
{
  if ( !m_loaded ) return;
  m_uninit();

  delete m_libhandle;
  m_libhandle = 0;

  m_loaded = false;
}


bool SpringUnitSync::IsLoaded()
{
  LOCK_UNITSYNC;
  return m_loaded;
}


std::string SpringUnitSync::GetSpringVersion()
{
  wxLogDebugFunc( _T("") );
  LOCK_UNITSYNC;
  if ( !m_loaded ) return "";
  std::string SpringVersion = m_get_spring_version();
  return SpringVersion;
}


int SpringUnitSync::GetNumMods()
{
  wxLogDebugFunc( _T("") );
  LOCK_UNITSYNC;
  if ( !m_loaded ) return 0;
  return m_mod_count;
}


int SpringUnitSync::GetModIndex( const std::string& name )
{
  wxLogDebugFunc( _T("name = \"") + WX_STRING(name) + _T("\"") );
  LOCK_UNITSYNC;
  return _GetModIndex( name );
}


int SpringUnitSync::_GetModIndex( const std::string& name )
{
  if ( !m_loaded ) return -1;
  for ( int i = 0; i < m_mod_count; i++ ) {
    std::string cmp = m_get_mod_name( i );
    if ( name == cmp ) return i;
  }
  //debug_warn( "Mod not found." );
  return -1;
}


bool SpringUnitSync::ModExists( const std::string& modname )
{
  wxLogDebugFunc( _T("modname = \"") + WX_STRING(modname) + _T("\"") );
  LOCK_UNITSYNC;
  return _ModExists( modname );
}


bool SpringUnitSync::_ModExists( const std::string& modname )
{
  if ( !m_loaded ) return false;
  return _GetModIndex( modname ) >= 0;
}


UnitSyncMod SpringUnitSync::GetMod( const std::string& modname )
{
  wxLogDebugFunc( _T("modname = \"") + WX_STRING(modname) + _T("\"") );
  LOCK_UNITSYNC;

  UnitSyncMod m;
  if ( !m_loaded ) return m;

  int i = _GetModIndex( modname );
  ASSERT_LOGIC( i >= 0, _T("Mod does not exist") );

  return _GetMod( i );
}


UnitSyncMod SpringUnitSync::GetMod( int index )
{
  wxLogDebugFunc( _T("") );
  LOCK_UNITSYNC;
  return _GetMod( index );
}


UnitSyncMod SpringUnitSync::_GetMod( int index )
{
  UnitSyncMod m;

  m.name = m_get_mod_name( index );
  m.hash = i2s(m_get_mod_checksum( index ));

  return m;
}


int SpringUnitSync::GetNumMaps()
{
  wxLogDebugFunc( _T("") );
  LOCK_UNITSYNC;

  if ( !m_loaded ) return 0;
  return m_map_count;
}


bool SpringUnitSync::MapExists( const std::string& mapname )
{
  wxLogDebugFunc( _T("") );
  LOCK_UNITSYNC;

  if ( !m_loaded ) return false;
  bool succ = false;
  try {
    succ = _GetMapIndex( mapname ) >= 0;
  } catch (...) { return false; }
  return succ;
}


bool SpringUnitSync::MapExists( const std::string& mapname, const std::string hash )
{
  wxLogDebugFunc( _T("") );
  LOCK_UNITSYNC;

  if ( !m_loaded ) return false;
  try {
    int i = _GetMapIndex( mapname );
    if ( i >= 0 ) {
      return ( i2s(m_get_map_checksum( i )) == hash );
    }
  } catch (...) {}
  return false;
}


UnitSyncMap SpringUnitSync::GetMap( const std::string& mapname, bool getmapinfo )
{
  wxLogDebugFunc( _T("") );
  LOCK_UNITSYNC;

  return _GetMap( mapname, getmapinfo );
}


UnitSyncMap SpringUnitSync::_GetMap( const std::string& mapname, bool getmapinfo )
{
  int i = _GetMapIndex( mapname );
  ASSERT_LOGIC( i >= 0, _T("Map does not exist") );

  return _GetMap( i, getmapinfo );
}


MapInfo SpringUnitSync::_GetMapInfoEx( const std::string& mapname )
{
  wxLogDebugFunc( _T("") );
  MapCacheType::iterator i = m_mapinfo.find(mapname);
  if ( i != m_mapinfo.end() ) {
    wxLogMessage( _T("GetMapInfoEx cache lookup.") );
    MapInfo info;
    CachedMapInfo cinfo = i->second;
    _ConvertSpringMapInfo( cinfo, info );
    return info;
  }

  wxLogMessage( _T("GetMapInfoEx cache lookup failed.") );

  char tmpdesc[256];
  char tmpauth[256];

  SpringMapInfo tm;
  tm.description = &tmpdesc[0];
  tm.author = &tmpauth[0];

  m_get_map_info_ex( mapname.c_str(), &tm, 0 );

  MapInfo info;
  _ConvertSpringMapInfo( tm, info );

  CachedMapInfo cinfo;
  _ConvertSpringMapInfo( tm, cinfo, mapname );
  m_mapinfo[mapname] = cinfo;

  return info;
}


UnitSyncMap SpringUnitSync::GetMap( int index, bool getmapinfo )
{
  wxLogDebugFunc( _T("") );
  LOCK_UNITSYNC;

  return _GetMap( index, getmapinfo );
}


UnitSyncMap SpringUnitSync::_GetMap( int index, bool getmapinfo )
{
  UnitSyncMap m;
  if ( !m_loaded ) return m;

  m.name = m_get_map_name( index );
  m.hash = i2s(m_get_map_checksum( index ));

  if ( getmapinfo ) m.info = _GetMapInfoEx( m.name );

  return m;
}


int SpringUnitSync::GetMapIndex( const std::string& name )
{
  wxLogDebugFunc( _T("") );
  LOCK_UNITSYNC;

  return _GetMapIndex( name );
}


int SpringUnitSync::_GetMapIndex( const std::string& name )
{
  if ( !m_loaded ) return -1;
  for ( int i = 0; i < m_map_count; i++ ) {
    std::string cmp = m_get_map_name( i );
    if ( name == cmp )
      return i;
  }
  return -1;
}


std::string SpringUnitSync::GetModArchive( int index )
{
  wxLogDebugFunc( _T("") );
  LOCK_UNITSYNC;

  return _GetModArchive( index );
}


std::string SpringUnitSync::_GetModArchive( int index )
{
  if ( (!m_loaded) || (index < 0) ) return "unknown";
  ASSERT_LOGIC( index < m_mod_count, _T("Bad index") );

  return m_get_mod_archive( index );;
}


void SpringUnitSync::SetCurrentMod( const std::string& modname )
{
  wxLogDebugFunc( _T("") );
  LOCK_UNITSYNC;

  wxLogDebugFunc(WX_STRING(("modname = \"" + modname + "\"")));
  if ( m_current_mod != modname ) {
    m_uninit();
    m_init( true, 1 );
    m_add_all_archives( _GetModArchive( _GetModIndex( modname ) ).c_str() );
    m_current_mod = modname;
    m_side_count = m_get_side_count();
    m_mod_units.Clear();
  }
}


int SpringUnitSync::GetSideCount()
{
  wxLogDebugFunc( _T("") );
  LOCK_UNITSYNC;

  if ( (!m_loaded) || (!_ModExists(m_current_mod)) ) return 0;
  return m_side_count;
}


std::string SpringUnitSync::GetSideName( int index )
{
  wxLogDebugFunc( _T("") );
  LOCK_UNITSYNC;

  if ( (!m_loaded) || (index < 0) || (!_ModExists(m_current_mod)) ) return "unknown";
  m_add_all_archives( _GetModArchive( _GetModIndex( m_current_mod ) ).c_str() );
  if ( index >= m_side_count ) return "unknown";
  ASSERT_LOGIC( m_side_count > index, _T("Side index too high.") );
  return m_get_side_name( index );
}


wxImage SpringUnitSync::GetSidePicture( const std::string& SideName )
{
  wxLogDebugFunc( _T("") );
  LOCK_UNITSYNC;

  m_add_all_archives( _GetModArchive( _GetModIndex( m_current_mod ) ).c_str() );
  wxLogDebugFunc( _T("SideName = \"") + WX_STRING(SideName) + _T("\"") );
  wxString ImgName = _T("SidePics");
  ImgName += _T("/");
  ImgName += WX_STRING( SideName ).Upper();
  ImgName += _T(".bmp");

  int ini = m_open_file_vfs(STD_STRING(ImgName).c_str());
  ASSERT_RUNTIME( ini, _T("cannot find side image") );

  int FileSize = m_file_size_vfs(ini);
  if (FileSize == 0) {
    m_close_file_vfs(ini);
    ASSERT_RUNTIME( FileSize, _T("side image has size 0") );
  }

  char* FileContent = new char [FileSize];
  m_read_file_vfs(ini, FileContent, FileSize);
  wxMemoryInputStream FileContentStream( FileContent, FileSize );

  wxImage ret( FileContentStream, wxBITMAP_TYPE_ANY, -1);
  delete[] FileContent;
  return ret;
}


wxArrayString SpringUnitSync::GetAIList()
{
  wxLogDebugFunc( _T("") );
  LOCK_UNITSYNC;

  if ( !m_loaded ) return wxArrayString();

  wxString AiSearchPath = wxString(_T("AI/Bot-libs/*")) + DLL_EXTENSION;
  int ini = m_init_find_vfs ( AiSearchPath.mb_str( wxConvUTF8 ) ); // FIXME this isn't exactly portable
  int BufferSize = 400;
  char * FilePath = new char [BufferSize];
  wxArrayString ret;

  do
  {
    ini = m_find_files_vfs ( ini, FilePath, BufferSize );
    wxString FileName = wxString ( FilePath, wxConvUTF8 );
    FileName = FileName.AfterLast ( '/' ); // strip the file path
    FileName = FileName.BeforeLast( '.' ); //strip the file extension
    if ( ret.Index( FileName ) == wxNOT_FOUND ) ret.Add ( FileName ); // don't add duplicates
  } while (ini != 0);

  return ret;
}


wxString SpringUnitSync::GetBotLibPath( const wxString& botlibname )
{
  wxLogDebugFunc( _T("") );
  LOCK_UNITSYNC;

  if ( !m_loaded ) return wxEmptyString;

  wxLogDebugFunc( _T("botlibname = \"") + botlibname + _T("\"") );
  wxString search = _T("AI/Bot-libs/") + botlibname + _T("*");
  int ini = m_init_find_vfs ( search.mb_str( wxConvUTF8 ) );
  int BufferSize = 400;
  char * FilePath = new char [BufferSize];

  do
  {
    ini = m_find_files_vfs ( ini, FilePath, BufferSize );
    wxString FileName = wxString( FilePath, wxConvUTF8 );
    if ( !FileName.Contains ( _T(".dll") ) && !FileName.Contains (  _T(".so") ) ) continue; // FIXME this isn't exactly portable
    wxLogMessage( _T("AIdll: ") + FileName );
    return FileName;
  } while (ini != 0);

  return wxEmptyString;
}


int SpringUnitSync::GetNumUnits()
{
  wxLogDebugFunc( _T("") );
  LOCK_UNITSYNC;

  if ( !m_loaded ) return 0;
  m_add_all_archives( _GetModArchive( _GetModIndex( m_current_mod ) ).c_str() );
  m_proc_units_nocheck();

  return m_get_unit_count();
}


wxString _GetCachedModUnitsFileName( const wxString& mod )
{
  wxString path = sett().GetCachePath(); //wxStandardPaths::Get().GetUserDataDir() + wxFileName::GetPathSeparator() + _T("cache") + wxFileName::GetPathSeparator();
  wxString fname = WX_STRING( mod );
  fname.Replace( _T("."), _T("_") );
  fname.Replace( _T(" "), _T("_") );
  wxLogMessage( path );
  return path + fname + _T(".units");
}


wxArrayString SpringUnitSync::GetUnitsList()
{
  wxLogDebugFunc( _T("") );
  LOCK_UNITSYNC;

  if (!m_loaded) return wxArrayString();
  if ( m_mod_units.GetCount() > 0 ) return m_mod_units;

  wxArrayString ret;
  if (!m_loaded) return ret;

  wxString path = _GetCachedModUnitsFileName( WX_STRING(m_current_mod) );
  try {

    ASSERT_RUNTIME( wxFileName::FileExists( path ), _T("Cache file does not exist") );
    wxTextFile f;
    ASSERT_RUNTIME( f.Open(path), _T("Failed to open file") );
    ASSERT_RUNTIME( f.GetLineCount() > 0, _T("File empty") );

    wxString str;
    for ( str = f.GetFirstLine(); !f.Eof(); str = f.GetNextLine() ) ret.Add( str );

    return ret;

  } catch(...) {}

  m_add_all_archives( _GetModArchive( _GetModIndex( m_current_mod ) ).c_str() );
  while ( m_proc_units_nocheck() );
  for ( int i = 0; i < m_get_unit_count(); i++ ) {
    wxString tmp = wxString(m_get_unit_full_name(i), wxConvUTF8) + _T("(");
    tmp += wxString(m_get_unit_name(i), wxConvUTF8) + _T(")");
    ret.Add( tmp );
  }

  m_mod_units = ret;
  try {

    wxFile f( path, wxFile::write );
    ASSERT_RUNTIME( f.IsOpened(), _T("Couldn't create file") );

    for ( unsigned int i = 0; i < ret.GetCount(); i++ ) {
      std::string tmp = STD_STRING( ret.Item(i) );
      tmp += "\n";
      f.Write( tmp.c_str(), tmp.length() );
    }

    f.Close();

  } catch(...) {}

  return ret;
}


wxString SpringUnitSync::_GetCachedMinimapFileName( const std::string& mapname, int width, int height )
{
  wxString path = sett().GetCachePath(); //wxStandardPaths::Get().GetUserDataDir() + wxFileName::GetPathSeparator() + _T("cache") + wxFileName::GetPathSeparator();
  wxString fname = WX_STRING( mapname );
  fname.Replace( _T("."), _T("_") );
  fname.Replace( _T(" "), _T("_") );
  if ( width != -1 ) fname += wxString::Format( _T("%dx%d"), width, height );
  fname += _T(".png");
  return path + fname;
}


wxImage SpringUnitSync::_GetCachedMinimap( const std::string& mapname, int max_w, int max_h, bool store_size )
{
  wxString fname = store_size? _GetCachedMinimapFileName( mapname, max_w, max_h ) : _GetCachedMinimapFileName( mapname );
  ASSERT_RUNTIME( wxFileExists( fname ), _T("File cached image does not exist") );

  wxImage img( fname, wxBITMAP_TYPE_PNG );
  ASSERT_RUNTIME( img.Ok(), _T("Failed to load chache image") );

  if ( !store_size ) {

    UnitSyncMap map = _GetMap( mapname );
    if ( map.hash != m_map.hash ) map = m_map = _GetMap( mapname, true );
    else map = m_map;

    int height, width;

    width = max_w;
    height = (int)((double)((double)max_w * (double)map.info.height) / (double)map.info.width);
    if ( height > max_h ) {
      width = (int)((double)((double)width * (double)max_h) / (double)height);
      height = max_h;
    }

    img.Rescale( width, height );

  }

  return img;
}


wxImage SpringUnitSync::GetMinimap( const std::string& mapname, int max_w, int max_h, bool store_size )
{
  wxLogDebugFunc( _T("") );
  LOCK_UNITSYNC;

  int height = 1024;
  int width = 512;

  try {
    return _GetCachedMinimap( mapname, max_w, max_h, store_size );
  } catch(...) {
    wxLogMessage( _T("Cache lookup failed.") );
  }

  wxImage ret( width, height );
  UnitSyncColour* colours = (UnitSyncColour*)m_get_minimap( mapname.c_str(), 0 );
  ASSERT_RUNTIME( colours , _T("GetMinimap failed") );
  for ( int y = 0; y < height; y++ ) {
    for ( int x = 0; x < width; x++ ) {
      int pos = y*(width)+x;
      typedef unsigned char uchar;
      ret.SetRGB( x, y, uchar((colours[pos].r/31.0)*255.0), uchar((colours[pos].g/63.0)*255.0), uchar((colours[pos].b/31.0)*255.0) );
    }
  }


  UnitSyncMap map = _GetMap( mapname, true );

  width = max_w;
  height = (int)((double)((double)max_w * (double)map.info.height) / (double)map.info.width);
  if ( height > max_h ) {
    width = (int)((double)((double)width * (double)max_h) / (double)height);
    height = max_h;
  }

  ret.Rescale( 512, 512 );

  wxString fname = _GetCachedMinimapFileName( mapname );
  if ( !wxFileExists( fname ) ) ret.SaveFile( fname, wxBITMAP_TYPE_PNG );

  ret.Rescale( width, height );

  if ( store_size ) {
    ret.SaveFile( _GetCachedMinimapFileName( mapname, max_w, max_h ), wxBITMAP_TYPE_PNG );
  }

  return ret;
}


void SpringUnitSync::_ConvertSpringMapInfo( const SpringMapInfo& in, MapInfo& out )
{
  out.author = in.author;
  out.description = in.description;

  out.extractorRadius = in.extractorRadius;
  out.gravity = in.gravity;
  out.tidalStrength = in.tidalStrength;
  out.maxMetal = in.maxMetal;
  out.minWind = in.minWind;
  out.maxWind = in.maxWind;

  out.width = in.width;
  out.height = in.height;
  out.posCount = in.posCount;
  for ( int i = 0; i < in.posCount; i++) out.positions[i] = in.positions[i];
}


void SpringUnitSync::_ConvertSpringMapInfo( const CachedMapInfo& in, MapInfo& out )
{
  out.author = in.author;
  out.description = in.description;

  out.extractorRadius = in.extractorRadius;
  out.gravity = in.gravity;
  out.tidalStrength = in.tidalStrength;
  out.maxMetal = in.maxMetal;
  out.minWind = in.minWind;
  out.maxWind = in.maxWind;

  out.width = in.width;
  out.height = in.height;
  out.posCount = in.posCount;
  for ( int i = 0; i < in.posCount; i++) out.positions[i] = in.positions[i];
}


void SpringUnitSync::_ConvertSpringMapInfo( const SpringMapInfo& in, CachedMapInfo& out, const std::string& mapname )
{
  strncpy( &out.name[0], mapname.c_str(), 256 );
  strncpy( &out.author[0], in.author, 256 );
  strncpy( &out.description[0], in.description, 256 );

  out.tidalStrength = in.tidalStrength;
  out.gravity = in.gravity;
  out.maxMetal = in.maxMetal;
  out.extractorRadius = in.extractorRadius;
  out.minWind = in.minWind;
  out.maxWind = in.maxWind;

  out.width = in.width;
  out.height = in.height;

  out.posCount = in.posCount;
  for ( int i = 0; i < 16; i++ ) out.positions[i] = in.positions[i];
}


void SpringUnitSync::_LoadMapInfoExCache()
{
  wxLogDebugFunc( _T("") );

  wxString path = sett().GetCachePath() + _T("mapinfoex.cache"); //wxStandardPaths::Get().GetUserDataDir() + wxFileName::GetPathSeparator() + _T("cache") + wxFileName::GetPathSeparator() + _T("mapinfoex.cache");

  if ( !wxFileName::FileExists( path ) ) {
    wxLogMessage( _T("No cache file found.") );
    return;
  }

  wxFile f( path.c_str(), wxFile::read );
  if ( !f.IsOpened() ) {
    wxLogMessage( _T("failed to open file for reading.") );
    return;
  }

  m_mapinfo.clear();

  CachedMapInfo cinfo;
  while ( !f.Eof() ) {
    if ( (unsigned int)f.Read( &cinfo, sizeof(CachedMapInfo) ) < sizeof(CachedMapInfo) ) {
      wxLogError( _T("Cache file invalid") );
      m_mapinfo.clear();
      break;
    }
    m_mapinfo[ std::string( &cinfo.name[0] ) ] = cinfo;
  }
  f.Close();
}


void SpringUnitSync::_SaveMapInfoExCache()
{
  wxLogDebugFunc( _T("") );
  wxString path = sett().GetCachePath() + _T("mapinfoex.cache"); //wxStandardPaths::Get().GetUserDataDir() + wxFileName::GetPathSeparator() + _T("cache") + wxFileName::GetPathSeparator() + _T("mapinfoex.cache");

  wxFile f( path.c_str(), wxFile::write );
  if ( !f.IsOpened() ) {
    wxLogMessage( _T("failed to open file for writing.") );
    return;
  }

  MapCacheType::iterator i = m_mapinfo.begin();
  while ( i != m_mapinfo.end() ) {
    f.Write( &i->second, sizeof(CachedMapInfo) );
    i++;
  }
  f.Close();
}



