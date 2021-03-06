#include "debug.h"
#include "path_info.h"
#include "output.h"
#include "file_wrapper.h"
#include <time.h>
#include <cstdlib>
#include <cstdarg>
#include <iosfwd>
#include <fstream>
#include <streambuf>

#ifndef _MSC_VER
#include <sys/time.h>
#endif

#if !(defined _WIN32 || defined WINDOWS || defined __CYGWIN__)
#include <execinfo.h>
#include <stdlib.h>
#endif

// Static defines                                                   {{{1
// ---------------------------------------------------------------------

#if (defined(DEBUG) || defined(_DEBUG)) && !defined(NDEBUG)
static int debugLevel = DL_ALL;
static int debugClass = DC_ALL;
#else
static int debugLevel = D_ERROR;
static int debugClass = D_MAIN;
#endif

void realDebugmsg( const char *filename, const char *line, const char *mes, ... )
{
    va_list ap;
    va_start( ap, mes );
    const std::string text = vstring_format( mes, ap );
    va_end( ap );
    DebugLog( D_ERROR, D_MAIN ) << filename << ":" << line << " " << text;
    fold_and_print( stdscr, 0, 0, getmaxx( stdscr ), c_red, "DEBUG: %s\n  Press spacebar...",
                    text.c_str() );
    while( getch() != ' ' ) {
        // wait for spacebar
    }
    werase( stdscr );
    refresh();
}

// Normal functions                                                 {{{1
// ---------------------------------------------------------------------

void limitDebugLevel( int level_bitmask )
{
    DebugLog( DL_ALL, DC_ALL ) << "Set debug level to: " << level_bitmask;
    debugLevel = level_bitmask;
}

void limitDebugClass( int class_bitmask )
{
    DebugLog( DL_ALL, DC_ALL ) << "Set debug class to: " << class_bitmask;
    debugClass = class_bitmask;
}

// Debug only                                                       {{{1
// ---------------------------------------------------------------------

#define TRACE_SIZE 20

void *tracePtrs[TRACE_SIZE];

// Debug Includes                                                   {{{2
// ---------------------------------------------------------------------

// Null OStream                                                     {{{2
// ---------------------------------------------------------------------

struct NullBuf : public std::streambuf {
    NullBuf() {}
    int overflow( int c ) {
        return c;
    }
};

// DebugFile OStream Wrapper                                        {{{2
// ---------------------------------------------------------------------

struct DebugFile {
    DebugFile();
    ~DebugFile();
    void init( std::string filename );
    void deinit();

    std::ofstream &currentTime();
    std::ofstream file;
    std::string filename;
};

static NullBuf nullBuf;
static std::ostream nullStream( &nullBuf );

// DebugFile OStream Wrapper                                        {{{2
// ---------------------------------------------------------------------

static DebugFile debugFile;

DebugFile::DebugFile()
{
}

DebugFile::~DebugFile()
{
    if( file.is_open() ) {
        deinit();
    }
}

void DebugFile::deinit()
{
    file << "\n";
    currentTime() << " : Log shutdown.\n";
    file << "-----------------------------------------\n\n";
    file.close();
}

void DebugFile::init( std::string filename )
{
    this->filename = filename;
    file.open( filename.c_str(), std::ios::out | std::ios::app );
    file << "\n\n-----------------------------------------\n";
    currentTime() << " : Starting log.";
}

void setupDebug()
{
    int level = 0;

#ifdef DEBUG_INFO
    level |= D_INFO;
#endif

#ifdef DEBUG_WARNING
    level |= D_WARNING;
#endif

#ifdef DEBUG_ERROR
    level |= D_ERROR;
#endif

#ifdef DEBUG_PEDANTIC_INFO
    level |= D_PEDANTIC_INFO;
#endif

    if( level != 0 ) {
        limitDebugLevel( level );
    }

    int cl = 0;

#ifdef DEBUG_ENABLE_MAIN
    cl |= D_MAIN;
#endif

#ifdef DEBUG_ENABLE_MAP
    cl |= D_MAP;
#endif

#ifdef DEBUG_ENABLE_MAP_GEN
    cl |= D_MAP_GEN;
#endif

#ifdef DEBUG_ENABLE_GAME
    cl |= D_GAME;
#endif

    if( cl != 0 ) {
        limitDebugClass( cl );
    }

    debugFile.init( FILENAMES["debug"] );
}

void deinitDebug()
{
    debugFile.deinit();
}

// OStream Operators                                                {{{2
// ---------------------------------------------------------------------

std::ostream &operator<<( std::ostream &out, DebugLevel lev )
{
    if( lev != DL_ALL ) {
        if( lev & D_INFO ) {
            out << "INFO ";
        }
        if( lev & D_WARNING ) {
            out << "WARNING ";
        }
        if( lev & D_ERROR ) {
            out << "ERROR ";
        }
        if( lev & D_PEDANTIC_INFO ) {
            out << "PEDANTIC ";
        }
    }
    return out;
}

std::ostream &operator<<( std::ostream &out, DebugClass cl )
{
    if( cl != DC_ALL ) {
        if( cl & D_MAIN ) {
            out << "MAIN ";
        }
        if( cl & D_MAP ) {
            out << "MAP ";
        }
        if( cl & D_MAP_GEN ) {
            out << "MAP_GEN ";
        }
        if( cl & D_NPC ) {
            out << "NPC ";
        }
        if( cl & D_GAME ) {
            out << "GAME ";
        }
        if( cl & D_SDL ) {
            out << "SDL ";
        }
    }
    return out;
}

std::ofstream &DebugFile::currentTime()
{
    struct tm *current;
    timeval tv;
    time_t tt;
    gettimeofday( &tv, NULL );
    tt = tv.tv_sec;
    current = localtime( &tt );

    file << current->tm_hour << ":" << current->tm_min << ":" <<
         current->tm_sec << "." << int( tv.tv_usec / 1000 + 0.5 );
    return file;
}

std::ostream &DebugLog( DebugLevel lev, DebugClass cl )
{
    // Error are always logged, they are important,
    // Messages from D_MAIN come from debugmsg and are equally important.
    if( ( ( lev & debugLevel ) && ( cl & debugClass ) ) || lev & D_ERROR || cl & D_MAIN ) {
        debugFile.file << std::endl;
        debugFile.currentTime() << " ";
        if( lev != debugLevel ) {
            debugFile.file << lev;
        }
        if( cl != debugClass ) {
            debugFile.file << cl;
        }
        debugFile.file << ": ";

        // Backtrace on error.
#if !(defined _WIN32 || defined WINDOWS || defined __CYGWIN__)
        if( lev == D_ERROR ) {
            int count = backtrace( tracePtrs, TRACE_SIZE );
            char **funcNames = backtrace_symbols( tracePtrs, count );
            for( int i = 0; i < count; ++i ) {
                debugFile.file << "\n\t(" << funcNames[i] << "), ";
            }
            debugFile.file << "\n\t";
            free( funcNames );
        }
#endif

        return debugFile.file;
    }
    return nullStream;
}

// vim:tw=72:sw=1:fdm=marker:fdl=0:
