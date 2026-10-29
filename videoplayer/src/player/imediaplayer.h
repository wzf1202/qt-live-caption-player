#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QUrl>

class QVideoSink;

class IMediaPlayer : public QObject
{
	Q_OBJECT

public:
	enum class PlayState
	{
		StoppedState,
		PlayingState,
		PausedState
	};

	enum class MediaState
	{
		NoMedia,
		LoadingMedia,
		LoadedMedia,
		EndOfMedia,
		InvalidMedia
	};

	enum class Error
	{
		NoError,
		ResourceError,
		FormatError
	};

	Q_ENUM(PlayState)
	Q_ENUM(MediaState)
	Q_ENUM(Error)

	explicit IMediaPlayer(QObject* pParent = nullptr) : QObject(pParent) {}
	~IMediaPlayer() override = default;

	virtual void setVideoOutput(QVideoSink* pVideoSink) = 0;
	virtual void setSource(const QUrl& url) = 0;
	virtual void play() = 0;
	virtual void pause() = 0;
	virtual void stop() = 0;
	virtual void setProgressSliderTime(qint64 positionMs) = 0;
	virtual void setRate(qreal rate) = 0;
	virtual void setVolume(qreal volume) = 0;
	virtual qreal getVolume() const = 0;
	virtual PlayState getPlayState() const = 0;

signals:
	void sigPlayStateChanged(IMediaPlayer::PlayState state);
	void sigProgressSliderTimeChanged(qint64 positionMs);
	void sigUpdateVideoDuration(qint64 durationMs);
	void sigMediaStatusChanged(IMediaPlayer::MediaState status);
	void sigErrorOccurred(IMediaPlayer::Error error, const QString& errorString);
	void sigAudioPcmReady(qint64 ptsMs, const QByteArray& pcm);
};
