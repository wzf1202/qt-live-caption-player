#pragma once

#include "imediaplayer.h"

#include <QMediaPlayer>

#if defined(SHERPA_SUBTITLE)
#include <QAudioBuffer>
#endif

class QAudioBufferOutput;
class QAudioOutput;

class QMediaPlayerBackend : public IMediaPlayer
{
	Q_OBJECT

public:
	explicit QMediaPlayerBackend(QObject* pParent);
	~QMediaPlayerBackend() override = default;

	void setVideoOutput(QVideoSink* pVideoSink) override;
	void setSource(const QUrl& url) override;
	void play() override;
	void pause() override;
	void stop() override;
	void setProgressSliderTime(qint64 positionMs) override;
	void setRate(qreal rate) override;
	void setVolume(qreal volume) override;
	qreal getVolume() const override;
	PlayState getPlayState() const override;

private slots:
	void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);
	void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
	void onErrorOccurred(QMediaPlayer::Error error, const QString& errorString);
#if defined(SHERPA_SUBTITLE)
	void onAudioBufferReceived(const QAudioBuffer& buffer);
#endif

private:
	QMediaPlayer* m_pPlayer = nullptr;
	QAudioOutput* m_pAudioOutput = nullptr;
#if defined(SHERPA_SUBTITLE)
	QAudioBufferOutput* m_pAudioBufferOutput = nullptr;
#endif
	PlayState m_playState = PlayState::StoppedState;
};
