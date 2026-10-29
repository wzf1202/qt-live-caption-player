#include "qmediaplayerbackend.h"

#include <QAudioBuffer>
#include <QAudioBufferOutput>
#include <QAudioFormat>
#include <QAudioOutput>
#include <QVideoSink>

QMediaPlayerBackend::QMediaPlayerBackend(QObject* pParent)
	: IMediaPlayer(pParent)
	, m_pPlayer(new QMediaPlayer(this))
	, m_pAudioOutput(new QAudioOutput(this))
{
	m_pPlayer->setAudioOutput(m_pAudioOutput);

#if defined(SHERPA_SUBTITLE)
	QAudioFormat pcmFormat;
	pcmFormat.setSampleRate(16000);
	pcmFormat.setChannelCount(1);
	pcmFormat.setSampleFormat(QAudioFormat::Int16);
	m_pAudioBufferOutput = new QAudioBufferOutput(pcmFormat, this);
	m_pPlayer->setAudioBufferOutput(m_pAudioBufferOutput);
	connect(m_pAudioBufferOutput, &QAudioBufferOutput::audioBufferReceived,
		this, &QMediaPlayerBackend::onAudioBufferReceived);
#endif

	connect(m_pPlayer, &QMediaPlayer::playbackStateChanged,
		this, &QMediaPlayerBackend::onPlaybackStateChanged);
	connect(m_pPlayer, &QMediaPlayer::mediaStatusChanged,
		this, &QMediaPlayerBackend::onMediaStatusChanged);
	connect(m_pPlayer, &QMediaPlayer::positionChanged,
		this, &IMediaPlayer::sigProgressSliderTimeChanged);
	connect(m_pPlayer, &QMediaPlayer::durationChanged,
		this, &IMediaPlayer::sigUpdateVideoDuration);
	connect(m_pPlayer, &QMediaPlayer::errorOccurred,
		this, &QMediaPlayerBackend::onErrorOccurred);
}

void QMediaPlayerBackend::setVideoOutput(QVideoSink* pVideoSink)
{
	m_pPlayer->setVideoSink(pVideoSink);
}

void QMediaPlayerBackend::setSource(const QUrl& url)
{
	m_pPlayer->stop();
	m_pPlayer->setSource(url);
}

void QMediaPlayerBackend::play()
{
	m_pPlayer->play();
}

void QMediaPlayerBackend::pause()
{
	m_pPlayer->pause();
}

void QMediaPlayerBackend::stop()
{
	m_pPlayer->stop();
	m_pPlayer->setPosition(0);
	emit this->sigProgressSliderTimeChanged(0);
}

void QMediaPlayerBackend::setProgressSliderTime(qint64 positionMs)
{
	positionMs = qMax(qint64(0), positionMs);
	const qint64 durationMs = m_pPlayer->duration();
	if (durationMs > 0)
		positionMs = qMin(positionMs, durationMs);

	m_pPlayer->setPosition(positionMs);
	emit this->sigProgressSliderTimeChanged(positionMs);
}

void QMediaPlayerBackend::setRate(qreal rate)
{
	m_pPlayer->setPlaybackRate(qBound(0.5, rate, 2.0));
}

void QMediaPlayerBackend::setVolume(qreal volume)
{
	m_pAudioOutput->setVolume(qBound(0.0, volume, 1.0));
}

qreal QMediaPlayerBackend::getVolume() const
{
	return m_pAudioOutput->volume();
}

IMediaPlayer::PlayState QMediaPlayerBackend::getPlayState() const
{
	return m_playState;
}

void QMediaPlayerBackend::onPlaybackStateChanged(QMediaPlayer::PlaybackState state)
{
	PlayState mapped = PlayState::StoppedState;
	switch (state)
	{
	case QMediaPlayer::PlayingState:
		mapped = PlayState::PlayingState;
		break;
	case QMediaPlayer::PausedState:
		mapped = PlayState::PausedState;
		break;
	case QMediaPlayer::StoppedState:
	default:
		break;
	}

	if (m_playState == mapped)
		return;

	m_playState = mapped;
	emit this->sigPlayStateChanged(mapped);
}

void QMediaPlayerBackend::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
	MediaState mapped = MediaState::NoMedia;
	switch (status)
	{
	case QMediaPlayer::NoMedia:
		mapped = MediaState::NoMedia;
		break;
	case QMediaPlayer::LoadingMedia:
		mapped = MediaState::LoadingMedia;
		break;
	case QMediaPlayer::LoadedMedia:
	case QMediaPlayer::BufferingMedia:
	case QMediaPlayer::BufferedMedia:
	case QMediaPlayer::StalledMedia:
		mapped = MediaState::LoadedMedia;
		break;
	case QMediaPlayer::EndOfMedia:
		mapped = MediaState::EndOfMedia;
		break;
	case QMediaPlayer::InvalidMedia:
		mapped = MediaState::InvalidMedia;
		break;
	default:
		break;
	}

	emit this->sigMediaStatusChanged(mapped);
}

void QMediaPlayerBackend::onErrorOccurred(QMediaPlayer::Error error, const QString& errorString)
{
	if (error == QMediaPlayer::NoError)
		return;

	Error mapped = Error::ResourceError;
	switch (error)
	{
	case QMediaPlayer::FormatError:
		mapped = Error::FormatError;
		break;
	case QMediaPlayer::ResourceError:
	case QMediaPlayer::NetworkError:
	case QMediaPlayer::AccessDeniedError:
	default:
		break;
	}

	emit this->sigErrorOccurred(mapped, errorString);
	emit this->sigMediaStatusChanged(MediaState::InvalidMedia);
}

#if defined(SHERPA_SUBTITLE)
void QMediaPlayerBackend::onAudioBufferReceived(const QAudioBuffer& buffer)
{
	if (!buffer.isValid() || buffer.frameCount() <= 0)
	{
		emit this->sigAudioPcmReady(m_pPlayer->position(), QByteArray());
		return;
	}

	const QAudioFormat format = buffer.format();
	if (format.sampleFormat() != QAudioFormat::Int16)
		return;

	const int byteCount = buffer.byteCount();
	if (byteCount <= 0)
		return;

	const qint16* pSamples = buffer.constData<qint16>();
	if (pSamples == nullptr)
		return;

	const QByteArray pcm(reinterpret_cast<const char*>(pSamples), byteCount);
	emit this->sigAudioPcmReady(m_pPlayer->position(), pcm);
}
#endif
