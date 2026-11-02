#!/usr/bin/env python3
"""Cross-platform parity gate for GlowDeck GameDay.

Checks shared, user-visible behavior and common NHL team colors across
ESP32, Fire TV and local browser demo. Hardware-specific features and the
separately hosted Lovable demo still need acceptance testing.
"""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(name):
    return (ROOT / name).read_text(encoding="utf-8")


def require(ok, description):
    if not ok:
        raise AssertionError(description)
    print("PASS " + description)


def colors_cpp(s):
    return dict((team, rgb.upper()) for team, rgb in
                re.findall(r'\{"([A-Z]{3})",0x([0-9a-fA-F]{6})\}', s))


def colors_java(s):
    return dict((team, rgb.upper()) for team, rgb in
                re.findall(r'case "([A-Z]{3})":\s*return 0x([0-9a-fA-F]{6})', s))


def main():
    cpp = read("src/GameCore.cpp")
    java = read("firetv/app/src/main/java/com/gkowdeck/gameday/firetv/NhlApi.java")
    esp = read("src/GoalEngine.cpp")
    fire = read("firetv/app/src/main/java/com/gkowdeck/gameday/firetv/GameDayService.java")
    selector = read("firetv/app/src/main/java/com/gkowdeck/gameday/firetv/CelebrationSelection.java")
    ui = read("src/Ui.cpp")
    screen = read("firetv/app/src/main/java/com/gkowdeck/gameday/firetv/MainActivity.java")
    nhl = read("src/NhlClient.cpp")
    web = read("simulator/web/index.html")
    a, b = colors_cpp(cpp), colors_java(java)
    require(len(a) == 32 and a == b, "ESP32 and Fire TV 32 NHL team colors match")
    require(all(v in ui for v in ['"BOTH"', 'CelebrationMode::Away',
                                   'CelebrationMode::Home', 'CelebrationMode::Both']),
            "ESP32 touchscreen shows Away/Home/Both")
    require(all(v in screen for v in ['"BOTH TEAMS"', '"AWAY: "', '"HOME: "']),
            "Fire TV interface shows Away/Home/Both")
    require('case BOTH' in selector and 'CelebrationMode::Both' in esp,
            "Both-mode engine accepts either valid scoring team")
    require('pollGoals(' in nhl and '/play-by-play' in nhl and
            'eventOwnerTeamId' in nhl and 'eventId' in nhl and
            'NhlApi.plays(' in fire,
            "Both firmware paths detect NHL goal event IDs")
    require('8000U' in esp and '},8,TimeUnit.SECONDS' in fire and
            '},8000)' in web,
            "Celebrations last eight seconds across controllers and demo")
    require('20000U' in esp and '20_000' in
            read("firetv/app/src/main/java/com/gkowdeck/gameday/firetv/MonitorRules.java"),
            "Both controllers use 20-second cooldown")
    require('PBP_INTERVAL_MS = 25000' in read("src/main.cpp") and
            '25, 25, TimeUnit.SECONDS' in fire,
            "Both controllers poll play-by-play at approximately 25 seconds")
    require('delaySeconds_ = seconds > 90 ? 90 : seconds' in esp and
            'Math.min(90, requested)' in
            read("firetv/app/src/main/java/com/gkowdeck/gameday/firetv/MonitorRules.java"),
            "Both controllers cap user TV delay to 90 seconds")
    require('putUChar((String("m")+uiState.selectedGameId)' in read("src/main.cpp")
            and 'celebrationMode."+g.id' in screen,
            "Both platforms remember selection per NHL game ID")
    require('persistAutoBackup' in read("src/WledClient.cpp") and
            'restoreSavedState()' in fire,
            "Both controllers journal and restore WLED state")
    require("chooseMode('both')" in web and "s.mode==='both'" in web and
            "v0.4" in web,
            "Local browser demo includes v0.3 Both mode")
    audio = read("src/AudioPlayer.cpp")
    audio_ui = read("src/Ui.cpp")
    audio_model = read("src/AudioModel.cpp")
    android_audio = read("firetv/app/src/main/java/com/gkowdeck/gameday/firetv/GameAudio.java")
    android_model = read("firetv/app/src/main/java/com/gkowdeck/gameday/firetv/AudioProfile.java")
    android_ui = read("firetv/app/src/main/java/com/gkowdeck/gameday/firetv/MainActivity.java")
    require(all(x in audio for x in ['SoundKind::Horn', 'SoundKind::DoubleHorn',
                                   'SoundKind::Siren', 'SoundKind::CustomWav']),
            "ESP32 supports three original sounds and custom WAV")
    require(all(x in android_audio for x in ['AudioProfile.HORN', 'AudioProfile.DOUBLE',
                                          'AudioProfile.SIREN', 'AudioProfile.CUSTOM']),
            "Fire TV supports three original sounds and custom WAV")
    require('inspectCanonicalWav' in audio_model and 'wavBytes' in android_model and
            '16000' in audio_model and '16000' in android_model,
            "ESP32 and Fire TV validate 16-kHz PCM WAV input")
    require('Page::Audio' in audio_ui and 'AudioPreviewAway' in audio_ui and
            'pageAudio()' in android_ui and 'PREVIEW' in android_ui,
            "Both platforms expose audio preferences and standalone preview")
    require('soundEnabled=false' in read("src/main.cpp") and
            'getBoolean("enabled",false)' in android_audio,
            "Goal audio defaults OFF on both physical platforms")
    require('reactions.lastGoalHome()' in read("src/main.cpp") and
            'sound.play(team,false)' in fire,
            "Audio follows the same scoring-side lighting event on both platforms")
    require('function playSound(team,preview=false)' in web and
            "s.sound[team]" in web and "customBuffers" in web,
            "Browser demo includes per-team built-in/custom sound preview")
    lan_server = read("firetv/app/src/main/java/com/gkowdeck/gameday/firetv/LanSoundServer.java")
    lan_proto = read("firetv/app/src/main/java/com/gkowdeck/gameday/firetv/LanSoundProtocol.java")
    lan_html = read("firetv/app/src/main/assets/sound_manager.html")
    require('LanSoundProtocol.pinMatches' in lan_server and
            'isPrivateLanIpv4' in lan_server and
            'origin!=null' in lan_server and
            'MAX_UPLOAD=320044' in lan_proto,
            "Fire TV LAN upload requires pairing, private IPv4 and size limits")
    require('GameDayService.isRunning()' in lan_server and
            'audio.play(team,true)' in lan_server and
            'LanSoundProtocol.canonicalUpload' in lan_server,
            "Fire TV LAN upload/preview cannot interfere with active goal monitoring")
    require('readyWav(file)' in lan_html and
            'Play on Fire TV' in lan_html and
            'NHL team' in lan_html,
            "Hosted-on-Fire-TV iPhone page imports and assigns team WAVs")
    require('web.on("/sound"' in read("src/main.cpp") and
            'CustomWav' in read("src/AudioPlayer.cpp"),
            "ESP32 keeps existing local sound settings and microSD custom playback")
    print("ALL CHECKS PASSED: cross-platform parity contract v0.4.1")


if __name__ == "__main__":
    main()
