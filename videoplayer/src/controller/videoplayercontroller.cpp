#include "videoplayercontroller.h"
#include "fileutil.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QVideoSink>
#include <QtGlobal>

#if defined(QMEDIAPLAYER) && defined(SHERPA_SUBTITLE)
#include "realtimesubtitleservice.h"
#include <QByteArray>
#endif

namespace
{
#if defined(QMEDIAPLAYER) && defined(SHERPA_SUBTITLE)
	QString _resolveSherpaModelDir()
	{
		constexpr const char* MODEL_DIR_NAME =
			"sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20";

		const QByteArray envPath = qgetenv("VIDEOPLAYER_SHERPA_MODEL_DIR");
		if (!envPath.isEmpty())
		{
			const QString path = QString::fromLocal8Bit(envPath);
			if (QFileInfo::exists(QDir(path).filePath(QStringLiteral("tokens.txt"))))
				return path;
		}

		const QString appModel = QDir(QCoreApplication::applicationDirPath())
			.filePath(QStringLiteral("models/%1").arg(QLatin1String(MODEL_DIR_NAME)));
		if (QFileInfo::exists(QDir(appModel).filePath(QStringLiteral("tokens.txt"))))
			return appModel;

		return QString();
	}
#endif
}

VideoPlayerController::VideoPlayerController(QObject* pParent, IMediaPlayer* pPlayer)
	: QObject(pParent)
	, m_pPlayer(pPlayer)
{
	Q_ASSERT(m_pPlayer != nullptr);
	if (m_pPlayer->parent() == nullptr)
		m_pPlayer->setParent(this);

	connect(m_pPlayer, &IMediaPlayer::sigPlayStateChanged,
		this, &VideoPlayerController::sigPlayStateChanged);
	connect(m_pPlayer, &IMediaPlayer::sigProgressSliderTimeChanged,
		this, &VideoPlayerController::sigProgressSliderTimeChanged);
	connect(m_pPlayer, &IMediaPlayer::sigUpdateVideoDuration,
		this, &VideoPlayerController::sigUpdateVideoDuration);
	connect(m_pPlayer, &IMediaPlayer::sigMediaStatusChanged,
		this, &VideoPlayerController::sigMediaStatusChanged);
	connect(m_pPlayer, &IMediaPlayer::sigErrorOccurred,
		this, &VideoPlayerController::sigErrorOccurred);

#if defined(QMEDIAPLAYER) && defined(SHERPA_SUBTITLE)
	_initSubtitleService();
	connect(m_pPlayer, &IMediaPlayer::sigPlayStateChanged,
		this, &VideoPlayerController::onPlayStateChangedForSubtitle);
	connect(m_pPlayer, &IMediaPlayer::sigMediaStatusChanged,
		this, &VideoPlayerController::onMediaStatusChangedForSubtitle);
	connect(m_pPlayer, &IMediaPlayer::sigAudioPcmReady,
		m_pSubtitleService, &RealtimeSubtitleService::onAudioPcmReady);
#endif
}

void VideoPlayerController::setVideoOutput(QVideoSink* pVideoSink)
{
	m_pPlayer->setVideoOutput(pVideoSink);
}

bool VideoPlayerController::openFile(const QString& filePath)
{
	if (filePath.isEmpty() || !fileutils::isSupportedVideoFile(filePath))
		return false;

#if defined(QMEDIAPLAYER) && defined(SHERPA_SUBTITLE)
	if (m_pSubtitleService != nullptr)
	{
		m_pSubtitleService->stop();
		m_pSubtitleService->reset();
	}
#endif

	m_pPlayer->setSource(QUrl::fromLocalFile(filePath));
	const QFileInfo fileInfo(filePath);
	emit this->sigShowFileOpenedMessage(fileInfo.fileName(), fileInfo.absoluteFilePath());
	return true;
}

void VideoPlayerController::playPause()
{
	if (m_pPlayer->getPlayState() == IMediaPlayer::PlayState::PlayingState)
		m_pPlayer->pause();
	else
		m_pPlayer->play();
}

void VideoPlayerController::stopPlay()
{
	m_pPlayer->stop();
#if defined(QMEDIAPLAYER) && defined(SHERPA_SUBTITLE)
	if (m_pSubtitleService != nullptr)
	{
		m_pSubtitleService->stop();
		m_pSubtitleService->reset();
	}
#endif
}

void VideoPlayerController::setVolume(int volume)
{
	volume = qBound(0, volume, 100);
	m_pPlayer->setVolume(volume / 100.0f);
}

int VideoPlayerController::getVolume() const
{
	return static_cast<int>(m_pPlayer->getVolume() * 100.0f);
}

void VideoPlayerController::setProgressSliderTime(qint64 positionMs)
{
#if defined(QMEDIAPLAYER) && defined(SHERPA_SUBTITLE)
	if (m_pSubtitleService != nullptr)
		m_pSubtitleService->reset();
#endif
	m_pPlayer->setProgressSliderTime(positionMs);
}

void VideoPlayerController::setRate(qreal rate)
{
	m_pPlayer->setRate(rate);
}

bool VideoPlayerController::openUrl(const QString& url)
{
	const QUrl qUrl(url, QUrl::StrictMode);
	if (!qUrl.isValid() || qUrl.isRelative() || qUrl.host().isEmpty()
		|| !fileutils::isSupportedVideoFile(qUrl.path())
		|| (qUrl.scheme() != QLatin1String("http") && qUrl.scheme() != QLatin1String("https")))
		return false;

#if defined(QMEDIAPLAYER) && defined(SHERPA_SUBTITLE)
	if (m_pSubtitleService != nullptr)
	{
		m_pSubtitleService->stop();
		m_pSubtitleService->reset();
	}
#endif

	const QString name = qUrl.fileName().isEmpty() ? qUrl.host() : qUrl.fileName();
	emit this->sigShowFileOpenedMessage(name, qUrl.toString());
	m_pPlayer->setSource(qUrl);
	m_pPlayer->play();
	return true;
}

#if defined(QMEDIAPLAYER) && defined(SHERPA_SUBTITLE)
RealtimeSubtitleService* VideoPlayerController::subtitleService() const
{
	return m_pSubtitleService;
}

void VideoPlayerController::_initSubtitleService()
{
	m_pSubtitleService = new RealtimeSubtitleService(this);
	const QString modelDir = _resolveSherpaModelDir();
	if (modelDir.isEmpty())
	{
		qWarning("Sherpa model not found. Place sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20 "
			"under models/ next to the executable, or set VIDEOPLAYER_SHERPA_MODEL_DIR.");
		return;
	}
	m_pSubtitleService->initModel(modelDir);
}

void VideoPlayerController::onPlayStateChangedForSubtitle(IMediaPlayer::PlayState state)
{
	if (m_pSubtitleService == nullptr || !m_pSubtitleService->isReady())
		return;

	if (state == IMediaPlayer::PlayState::PlayingState)
		m_pSubtitleService->start();
	else
		m_pSubtitleService->stop();
}

void VideoPlayerController::onMediaStatusChangedForSubtitle(IMediaPlayer::MediaState status)
{
	if (m_pSubtitleService == nullptr)
		return;

	if (status == IMediaPlayer::MediaState::NoMedia
		|| status == IMediaPlayer::MediaState::InvalidMedia)
	{
		m_pSubtitleService->stop();
		m_pSubtitleService->reset();
	}
}
#endif
