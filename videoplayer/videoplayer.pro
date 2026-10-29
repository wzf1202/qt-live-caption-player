QT += widgets

QT += multimedia multimediawidgets

CONFIG += c++17

# 播放后端：只改这一处（可选值见下方各 equals 分支）
# 新增后端时：在此增加可选值，并在下方追加一个 equals(...) { ... } 块
# PLAYER_BACKEND = ffmpegplayer
PLAYER_BACKEND = qmediaplayer

TARGET = videoplayer

TEMPLATE = app

INCLUDEPATH += \
	$$PWD/src \
	$$PWD/src/ui \
	$$PWD/src/controller \
	$$PWD/src/util \
	$$PWD/src/player

SOURCES += \
	src/main.cpp \
	src/controller/videoplayercontroller.cpp \
	src/ui/videoplayermainwindow.cpp \
	src/player/playerfactory.cpp

HEADERS += \
	src/controller/videoplayercontroller.h \
	src/ui/videoplayermainwindow.h \
	src/util/fileutil.h \
	src/player/imediaplayer.h \
	src/player/playerfactory.h

equals(PLAYER_BACKEND, ffmpegplayer) {
	DEFINES += FFMPEGPLAYER
	INCLUDEPATH += \
		$$PWD/src/player/ffmpeg \
		$$PWD/src/player/ffmpeg/worker \
		$$PWD/src/player/ffmpeg/queue \
		$$PWD/src/player/ffmpeg/session

	SOURCES += \
		src/player/ffmpeg/ffmpegplayer.cpp \
		src/player/ffmpeg/session/playsession.cpp \
		src/player/ffmpeg/session/videosyncer.cpp \
		src/player/ffmpeg/session/audiosession.cpp \
		src/player/ffmpeg/worker/demuxer.cpp \
		src/player/ffmpeg/worker/videodecoder.cpp \
		src/player/ffmpeg/worker/audiodecoder.cpp \
		src/player/ffmpeg/queue/avpacketqueue.cpp \
		src/player/ffmpeg/queue/videoframequeue.cpp \
		src/player/ffmpeg/queue/audioringbuffer.cpp

	HEADERS += \
		src/player/ffmpeg/ffmpegplayer.h \
		src/player/ffmpeg/session/playsession.h \
		src/player/ffmpeg/session/videosyncer.h \
		src/player/ffmpeg/session/audiosession.h \
		src/player/ffmpeg/worker/demuxer.h \
		src/player/ffmpeg/worker/videodecoder.h \
		src/player/ffmpeg/worker/audiodecoder.h \
		src/player/ffmpeg/queue/avpacketqueue.h \
		src/player/ffmpeg/queue/videoframequeue.h \
		src/player/ffmpeg/queue/audioringbuffer.h \
		src/player/ffmpeg/avcontext.h

	FFMPEG_ROOT = $$PWD/thirdparty/ffmpeg
	INCLUDEPATH += $$FFMPEG_ROOT/include
	LIBS += -L$$FFMPEG_ROOT/lib \
		-lavformat \
		-lavcodec \
		-lavfilter \
		-lavutil \
		-lswscale \
		-lswresample

	win32 {
		CONFIG(debug, debug|release) {
			FFMPEG_DEST = $$OUT_PWD/debug
		} else {
			FFMPEG_DEST = $$OUT_PWD/release
		}
		QMAKE_POST_LINK += $$quote(cmd /c if not exist \"$$shell_path($$FFMPEG_DEST)\" mkdir \"$$shell_path($$FFMPEG_DEST)\") $$escape_expand(\\n\\t)
		QMAKE_POST_LINK += $$quote(cmd /c copy /y \"$$shell_path($$FFMPEG_ROOT/bin)\\*.dll\" \"$$shell_path($$FFMPEG_DEST)\\\") $$escape_expand(\\n\\t)
	}
}

equals(PLAYER_BACKEND, qmediaplayer) {
	DEFINES += QMEDIAPLAYER
	INCLUDEPATH += $$PWD/src/player/qmedia
	SOURCES += src/player/qmedia/qmediaplayerbackend.cpp
	HEADERS += src/player/qmedia/qmediaplayerbackend.h
}

TRANSLATIONS += \
	mui/zh_CN/videoplayer.ts
CONFIG += lrelease embed_translations

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
