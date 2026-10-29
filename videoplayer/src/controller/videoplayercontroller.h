#pragma once

#include <QObject>
#include <QString>

#include "imediaplayer.h"

class QVideoSink;
#if defined(QMEDIAPLAYER) && defined(SHERPA_SUBTITLE)
class RealtimeSubtitleService;
#endif

class VideoPlayerController : public QObject
{
	Q_OBJECT

public:
	explicit VideoPlayerController(QObject* pParent, IMediaPlayer* pPlayer);
	~VideoPlayerController() override = default;

	void setVideoOutput(QVideoSink* pVideoSink);
	bool openFile(const QString& filePath);
	void playPause();
	void stopPlay();
	void setVolume(int volume);
	int getVolume() const;
	void setProgressSliderTime(qint64 positionMs);
	void setRate(qreal rate);
	bool openUrl(const QString& url);

#if defined(QMEDIAPLAYER) && defined(SHERPA_SUBTITLE)
	RealtimeSubtitleService* subtitleService() const;
#endif

signals:
	void sigShowFileOpenedMessage(const QString& fileName, const QString& absolutePath);
	void sigPlayStateChanged(IMediaPlayer::PlayState state);
	void sigProgressSliderTimeChanged(qint64 positionMs);
	void sigUpdateVideoDuration(qint64 durationMs);
	void sigMediaStatusChanged(IMediaPlayer::MediaState status);
	void sigErrorOccurred(IMediaPlayer::Error error, const QString& errorString);

#if defined(QMEDIAPLAYER) && defined(SHERPA_SUBTITLE)
private slots:
	void onPlayStateChangedForSubtitle(IMediaPlayer::PlayState state);
	void onMediaStatusChangedForSubtitle(IMediaPlayer::MediaState status);
#endif

private:
	IMediaPlayer* m_pPlayer = nullptr;
#if defined(QMEDIAPLAYER) && defined(SHERPA_SUBTITLE)
	RealtimeSubtitleService* m_pSubtitleService = nullptr;
	void _initSubtitleService();
#endif
};
