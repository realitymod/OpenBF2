// Starts/stops round demo capture and runs the configured completion hook in a worker thread.
// The server update picks up hook completion before announcing the recording to clients.

#include "AutoRecorder.h"

#include "GameServer.h"
#include "ServerSettings.h"                   // sv.autoRecord, sv.autoDemoHook
#include "../IGame.h"                         // game->getCurrentMapName()
#include "../IGameLogic.h"                    // gameLogic->getBeginRoundState()
#include "../Main/BF2Engine.h"                // bf2Engine->bf2Console
#include "../Main/Demo.h"                     // Demo::recordDemo / Demo::stop
#include "../../Core/Debug.h"                 // the three attested diagnostics
#include "../../Core/EventManager.h"          // register/unregister at vptr +0x2C / +0x30
#include "../../Core/StringUtil.h"            // extractFileName
#include "../../Core/System.h"                // g_system->sleep - the hook thread's 500 ms step
#include "../../Core/SystemInfo.h"            // getDateTimeString - the filename stamp
#include "../../Core/SettingsRepostitory.h"   // GSModDirectory

#include <cstdio>                             // fprintf / fflush - the one [FRONTIER] stop
#include <cstdlib>                            // abort

#if defined(_WIN32)
#include <windows.h>                          // ShellExecuteEx - the Win32 hook runner
#include <shellapi.h>
#endif

#include <sstream>
#include <string>

namespace dice {
namespace hfe {

// ds:0x8AACBD8 - Core/Globals.cpp. roundStarted hands its BF2Console* to Demo::recordDemo, which is
// where "already recording" and the recorder's own console output go. Declared the same way
// GameServer.cpp and GameLogic.cpp declare it.
extern BF2Engine* bf2Engine;

// Runs the demo-processing hook synchronously. Returns null on success or a static
// error message. [PLATFORM] Windows passes a triple-quoted filename to ShellExecuteEx;
// Linux passes an unquoted command to system. Preserve the Windows wait/retry behavior.
const char* process_autodemo_synchronized_(const char* hookProgram, const char* file)
{
#if defined(_WIN32)
    std::ostringstream oss;
    oss << "\"\"\"" << file << "\"\"\"";
    const std::string parameters = oss.str();

    SHELLEXECUTEINFOA info;
    std::memset(&info, 0, sizeof(info));
    info.cbSize       = sizeof(SHELLEXECUTEINFOA);            // 60
    info.fMask        = 1344;                                 // 0x540 - literal in both ports
    info.lpVerb       = "open";
    info.lpFile       = hookProgram;
    info.lpParameters = parameters.c_str();

    if (!ShellExecuteExA(&info))
        return "failed to launch hook program";

    DWORD exitCode;
    if (info.hProcess)
    {
        for (;;)
        {
            while (WaitForSingleObject(info.hProcess, INFINITE))
                ;
            if (GetExitCodeProcess(info.hProcess, &exitCode) && exitCode != STILL_ACTIVE)
                break;
        }
    }
    return exitCode != 0 ? "dep: autodemo hook program failed w/ non-zero exit code" : 0;
#else
    // Executes the hook without output redirection. Any nonzero system status is failure.
    std::ostringstream oss;
    oss << hookProgram << " " << file;
    const std::string command = oss.str();
    const int status = std::system(command.c_str());
    if (status == 0)
        return 0;
    if (status == -1)
        return "dep: autodemo hook system(3) failed (fork failed?)";
    return "dep: autodemo hook program failed w/ non-zero exit code";
#endif
}

// AutoRecorderHookThread

//  (C1), 91 B. Thread's base constructor first, then this class's own vtable.
AutoRecorderHookThread::AutoRecorderHookThread()
    : m_state(0)
    , m_owner(0)
    , m_mutex(0)
    , m_quit(false)
{
}

AutoRecorderHookThread::~AutoRecorderHookThread()
{
}

// two stores and a tail-jump into std::string::assign.
void AutoRecorderHookThread::init(AutoRecorder* owner, Mutex* mutex, const std::string& hookCommand)
{
    m_owner = owner;
    m_mutex = mutex;
    m_hookCommand = hookCommand;
}

// the whole body is one string assign at +0x0C.
void AutoRecorderHookThread::processFile(const std::string& file)
{
    m_pendingFile = file;
}

// run - ELF 0x80A0DA0, 333 B. The thread body.
// Holds the owner'S mutex across its state changes, and drops it while sleeping and while the hook
// program runs. m_state is published so roundFinished can tell idle from busy without blocking.
void AutoRecorderHookThread::run()
{
    m_mutex->lock();

    while (!m_quit)
    {
        m_state = 1;                                          // idle, ready for a file

        if (m_pendingFile.empty())
        {
            do
            {
                m_mutex->unlock();
                g_system->sleep(500);
                m_mutex->lock();
            }
            while (!m_quit && m_pendingFile.empty());
        }

        m_state = 2;                                          // busy
        const char* error = 0;
        m_mutex->unlock();

        // Runs a pending hook during shutdown as well as normal operation. Releases the mutex while running it.
        if (!m_quit || !m_pendingFile.empty())
            error = process_autodemo_synchronized_(m_hookCommand.c_str(), m_pendingFile.c_str());

        m_mutex->lock();
        m_pendingFile.erase();                                // _M_mutate(0, size, 0)
        m_owner->hookCompleted(error);                        // reports even on the shutdown pass
        if (m_quit)
            break;
    }

    m_mutex->unlock();
}

// AutoRecorder

//  (C1), 298 B. The hook thread only starts when sv.autoDemoHook is set, which is what
// leaves m_state at 0 and makes roundFinished's "no hook" arm silent on a default server.
AutoRecorder::AutoRecorder(GameServer* gameServer, Demo* demo)
    : m_gameServer(gameServer)
    , m_demo(demo)
    , m_recording(false)
    , m_hookDone(false)
{
    const std::string hook(serverSettings->m_autoDemoHook);   // ds:0x8AAB7A0 +0xB8
    if (!hook.empty())
    {
        m_hookThread.init(this, &m_mutex, hook);
        m_hookThread.start(0, static_cast<Thread::Priority>(2));   // name NULL, priority 2
    }

    eventManager->registerEventHandler(ECGameLogic, this, 0);      // vptr +0x2C
}

//  (D1), 319 B. Unregister first, raise the thread's quit flag under the mutex, then
// join. The unlock uses the same mutex the lock took; the members are destroyed in reverse order
// by the compiler, exactly as the shipped body does by hand.
AutoRecorder::~AutoRecorder()
{
    eventManager->unregisterEventHandler(ECGameLogic, this);       // vptr +0x30

    m_mutex.lock();
    m_hookThread.m_quit = true;
    m_mutex.unlock();

    m_hookThread.stop();
}

// . The only event it acts on.
void AutoRecorder::handleEvent(EventCategory category, unsigned int id, EventNode*, void* data)
{
    if (category == ECGameLogic && id == 1 && *static_cast<int*>(data) == 2)
        roundFinished();
}

// . Called from the hook thread, under the mutex.
void AutoRecorder::hookCompleted(const char* error)
{
    m_hookDone = true;
    if (!error)
        m_hookError.erase();
    else
        m_hookError = error;
}

// Builds a demo filename under the mod's demos directory, replacing date punctuation with underscores.
std::string AutoRecorder::makeFilename()
{
    std::string stamp = systemInfo()->getDateTimeString();

    for (;;)
    {
        const std::string::size_type bad = stamp.find_first_not_of("0123456789_");
        if (bad == std::string::npos)
            break;
        stamp[bad] = '_';
    }

    std::string modDir = settings->stringGet(std::string("GSModDirectory"), std::string("mods/bf2"));
    modDir.append("/");

    std::string name = modDir + std::string("demos/auto_") + stamp;
    name.append(".bf2demo");
    return name;
}

// roundStarted - ELF 0x80A04B0, 272 B.
// Three conditions, all under the mutex: the round really is in the playing state, nothing is
// recording, and sv.autoRecord is on. The empty-filename test after makeFilename is shipped.
void AutoRecorder::roundStarted()
{
    m_mutex.lock();

    if (gameLogic->getBeginRoundState() == 3
        && !m_recording
        && serverSettings->m_autoRecord)
    {
        m_demoFile = makeFilename();
        if (!m_demoFile.empty())
        {
            m_demo->recordDemo(m_gameServer, m_demoFile, bf2Engine->bf2Console);
            m_recording = true;
            if (game)
                m_mapName = game->getCurrentMapName();
        }
    }

    m_mutex.unlock();
}

// Stops recording and hands the file to an idle hook. A busy hook drops this demo without queuing it.
void AutoRecorder::roundFinished()
{
    m_mutex.lock();

    if (m_recording)
    {
        m_demo->stop(m_gameServer);

        const int hookState = m_hookThread.m_state;
        if (hookState != 0)
        {
            if (hookState == 2)
            {
                Debug(DEBUGTYPE_8, "Game/GameServer/AutoRecorder.cpp", 136, __FUNCTION__)
                    << "dep: cannot call auto-recording hook for " << m_demoFile
                    << ", last file (" << m_lastFile << ") is still processing";
            }
            else
            {
                Debug(DEBUGTYPE_1, "Game/GameServer/AutoRecorder.cpp", 139, __FUNCTION__)
                    << "Uploading demo file " << m_demoFile << " ...";
                m_lastFile = m_demoFile;
                m_demoFile.erase();                           // _M_mutate(0, size, 0)
                m_hookThread.processFile(m_lastFile);
            }
        }
        m_recording = false;
    }

    m_mutex.unlock();
}

// update - ELF 0x80A02B0, 497 B. Ticked by GameServer::update whenever a recorder exists.
// Note the shape: the announcement happens after the mutex is released, off two locals captured
// while it was held. Only a hook that completed without an error produces an announcement.
void AutoRecorder::update()
{
    bool completed = false;
    bool failed    = false;
    std::string fileName;

    m_mutex.lock();

    if (m_hookDone)
    {
        completed = true;
        fileName  = extractFileName(m_lastFile);
        m_hookDone = false;

        if (!m_hookError.empty())
        {
            Debug(DEBUGTYPE_1, "Game/GameServer/AutoRecorder.cpp", 87, __FUNCTION__)
                << "Demo upload failed. Check 'rotate_demo_err.txt' for more info.";
            failed = true;
        }
    }

    m_mutex.unlock();

    if (completed && !failed)
        m_gameServer->onAutoRecordingAvailable(fileName, m_mapName);
}

} // namespace hfe
} // namespace dice
