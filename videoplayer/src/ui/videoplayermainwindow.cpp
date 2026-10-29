#include "videoplayermainwindow.h"
#include "videoplayercontroller.h"
#include "fileutil.h"

#if defined(QMEDIAPLAYER) && defined(SHERPA_SUBTITLE)
#include "realtimesubtitleservice.h"
#endif

#include <QAction>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFileDialog>
#include <QFrame>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QGraphicsVideoItem>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMimeData>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QStringList>
#include <QUrl>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QSizePolicy>
#include <QSlider>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QToolTip>
#include <QLineEdit>
#include <QInputDialog>

namespace
{
	QString _getFirstSupportedVideoPath(const QMimeData* pMimeData)
	{
		if (pMimeData == nullptr || !pMimeData->hasUrls())
			return QString();

		for (const QUrl& url : pMimeData->urls())
		{
			if (!url.isLocalFile())
				continue;
			const QString filePath = url.toLocalFile();
			if (fileutils::isSupportedVideoFile(filePath))
				return filePath;
		}
		return QString();
	}

	QString _formatTime(qint64 milliseconds)
	{
		if (milliseconds < 0)
			milliseconds = 0;

		const int totalSeconds = static_cast<int>(milliseconds / 1000);
		const int hours = totalSeconds / 3600;
		const int minutes = (totalSeconds % 3600) / 60;
		const int seconds = totalSeconds % 60;

		return QString("%1:%2:%3")
			.arg(hours, 2, 10, QChar('0'))
			.arg(minutes, 2, 10, QChar('0'))
			.arg(seconds, 2, 10, QChar('0'));
	}

	void _updateTimeLabel(QLabel* pLabel, qint64 positionMs, qint64 durationMs)
	{
		if (pLabel == nullptr)
			return;

		const QString currentText = _formatTime(positionMs);
		const QString totalText = _formatTime(durationMs);
		pLabel->setText(QString("%1 / %2").arg(currentText, totalText));
	}
}

VideoPlayerMainWindow::VideoPlayerMainWindow(QWidget* pParent, VideoPlayerController* pController)
	: QMainWindow(pParent)
	, m_pController(pController)
{
	_initUI();
}

void VideoPlayerMainWindow::dragEnterEvent(QDragEnterEvent* pEvent)
{
	if (_getFirstSupportedVideoPath(pEvent->mimeData()).isEmpty())
	{
		pEvent->ignore();
	}
	else
	{
		setStyleSheet("QMainWindow { border: 2px dashed red; }");
		pEvent->acceptProposedAction();
	}
}

void VideoPlayerMainWindow::dropEvent(QDropEvent* pEvent)
{
	const QString filePath = _getFirstSupportedVideoPath(pEvent->mimeData());
	if (filePath.isEmpty())
	{
		pEvent->ignore();
		return;
	}

	m_pController->openFile(filePath);
	pEvent->acceptProposedAction();
	setStyleSheet(QString());
}

void VideoPlayerMainWindow::dragLeaveEvent(QDragLeaveEvent* pEvent)
{
	QMainWindow::dragLeaveEvent(pEvent);
	setStyleSheet(QString());
}

bool VideoPlayerMainWindow::eventFilter(QObject* pWatched, QEvent* pEvent)
{
	if (m_pVideoView != nullptr && pWatched == m_pVideoView->viewport())
	{
		if (pEvent->type() == QEvent::MouseButtonDblClick)
		{
			QMouseEvent* pMouseEvent = static_cast<QMouseEvent*>(pEvent);
			if (pMouseEvent->button() == Qt::LeftButton)
			{
				onToggleFullScreen();
				return true;
			}
		}
		else if (pEvent->type() == QEvent::Resize)
		{
			_layoutVideoScene();
		}
	}
	return QMainWindow::eventFilter(pWatched, pEvent);
}

void VideoPlayerMainWindow::resizeEvent(QResizeEvent* pEvent)
{
	QMainWindow::resizeEvent(pEvent);
	_layoutVideoScene();
}

void VideoPlayerMainWindow::onOpenFile()
{
	QStringList patterns;
	for (const QString& suffix : fileutils::supportedSuffixes)
		patterns << QString("*.%1").arg(suffix);

	const QString filePath = QFileDialog::getOpenFileName(this, tr("Open Video"),
		QString(), tr("Video Files (%1);;All Files (*.*)").arg(patterns.join(" ")));
	m_pController->openFile(filePath);
}

void VideoPlayerMainWindow::onShowFileOpenedMessage(const QString& fileName, const QString& absolutePath)
{
	m_pFilePathLabel->setToolTip(absolutePath);
	m_pFilePathLabel->setText(tr("Current file: %1").arg(fileName));
	m_pProgressSlider->show();
}

void VideoPlayerMainWindow::onPlayStateChanged(IMediaPlayer::PlayState state)
{
	switch (state)
	{
	case IMediaPlayer::PlayState::PlayingState:
		m_pBtnStopPlay->setEnabled(true);
		m_pBtnPlayPause->setText(tr("Pause"));
		break;
	case IMediaPlayer::PlayState::PausedState:
		m_pBtnPlayPause->setText(tr("Play"));
		break;
	case IMediaPlayer::PlayState::StoppedState:
		m_pBtnStopPlay->setEnabled(false);
		m_pBtnPlayPause->setText(tr("Play"));
		_setSubtitleText(QString());
		break;
	}
}

void VideoPlayerMainWindow::onToggleFullScreen()
{
	if (isFullScreen())
	{
		m_pControlPanel->show();
		setStyleSheet(QString());
		showNormal();
	}
	else
	{
		m_pControlPanel->hide();
		setStyleSheet("QMainWindow { background-color: black; }");
		showFullScreen();
	}
	_layoutVideoScene();
}

void VideoPlayerMainWindow::onOpenVolumeSlider()
{
	_popupWidget(m_pVolumeWidget);
}

void VideoPlayerMainWindow::onVolumeValueChanged(int volume)
{
	m_pController->setVolume(volume);
	m_pVolumeLabel->setText(QString::number(volume));
}

void VideoPlayerMainWindow::onProgressSliderTimeChanged(qint64 positionMs)
{
	if (m_pProgressSlider->isSliderDown())
		return;

	m_pProgressSlider->setValue(static_cast<int>(positionMs));
	_updateTimeLabel(m_pTimeLabel, positionMs, m_pProgressSlider->maximum());
}

void VideoPlayerMainWindow::onUpdateVideoDuration(qint64 durationMs)
{
	m_pProgressSlider->setMaximum(static_cast<int>(durationMs));
	_updateTimeLabel(m_pTimeLabel, m_pProgressSlider->value(), durationMs);
}

void VideoPlayerMainWindow::onMediaStateChanged(IMediaPlayer::MediaState status)
{
	switch (status)
	{
	case IMediaPlayer::MediaState::LoadedMedia:
		m_pBtnPlayPause->setEnabled(true);
		m_pTimeLabel->setStyleSheet(QString());
		m_pProgressSlider->setEnabled(true);
		break;
	case IMediaPlayer::MediaState::NoMedia:
	case IMediaPlayer::MediaState::InvalidMedia:
		m_pBtnPlayPause->setEnabled(false);
		m_pBtnStopPlay->setEnabled(false);
		m_pProgressSlider->setEnabled(false);
		_setSubtitleText(QString());
		break;
	case IMediaPlayer::MediaState::EndOfMedia:
		m_pProgressSlider->setEnabled(true);
		m_pProgressSlider->setValue(0);
		_updateTimeLabel(m_pTimeLabel, 0, m_pProgressSlider->maximum());
		break;
	default:
		break;
	}
}

void VideoPlayerMainWindow::onErrorOccurred(IMediaPlayer::Error, const QString& errorString)
{
	m_pBtnPlayPause->setEnabled(false);
	m_pBtnStopPlay->setEnabled(false);
	_setSubtitleText(QString());
	QMessageBox::warning(this, tr("Playback Error"), errorString);
}

void VideoPlayerMainWindow::onProgressSliderLoosed()
{
	m_pController->setProgressSliderTime(m_pProgressSlider->value());
}

void VideoPlayerMainWindow::onProgressSliderMoved(int positionMs)
{
	_updateTimeLabel(m_pTimeLabel, positionMs, m_pProgressSlider->maximum());

	const QString tip = _formatTime(positionMs);
	QStyleOptionSlider option = QStyleOptionSlider();
	option.initFrom(m_pProgressSlider);
	option.minimum = m_pProgressSlider->minimum();
	option.maximum = m_pProgressSlider->maximum();
	option.sliderPosition = positionMs;
	const QRect handle = m_pProgressSlider->style()->subControlRect(
		QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, m_pProgressSlider);
	const QPoint globalPos = m_pProgressSlider->mapToGlobal(
		QPoint(handle.center().x(), handle.top()));
	QToolTip::showText(globalPos, tip, m_pProgressSlider);
}

void VideoPlayerMainWindow::onOpenRateMenu()
{
	_popupWidget(m_pRateWidget);
}

void VideoPlayerMainWindow::onRateChanged()
{
	QPushButton* pBtnRate = qobject_cast<QPushButton*>(sender());
	if (pBtnRate == nullptr)
		return;

	m_pController->setRate(pBtnRate->property("rate").toDouble());
	m_pRateWidget->hide();
}

void VideoPlayerMainWindow::onOpenUrlInputDialog()
{
	bool isClick = false;
	const QString url = QInputDialog::getText(this, tr("Open Url"),
		tr("Video Url:"), QLineEdit::Normal, QString(), &isClick).trimmed();

	if (!isClick || url.isEmpty())
		return;

	if (!m_pController->openUrl(url))
		QMessageBox::warning(this, tr("Open Url"),
			tr("Invalid URL. Please enter an http:// or https:// address "
			"with a supported video format (%1).")
			.arg(fileutils::supportedSuffixes.join(QStringLiteral(", "))));
}

void VideoPlayerMainWindow::_initUI()
{
	if (m_pController == nullptr)
		return;

	const QRect screenGeometry = screen()->availableGeometry();
	resize(qRound(screenGeometry.width() * 0.5), qRound(screenGeometry.height() * 0.5));
	setWindowTitle(tr("VideoPlayerMainWindow"));

	QWidget* pCentralWidget = new QWidget(this);
	setCentralWidget(pCentralWidget);
	QVBoxLayout* pMainLayout = new QVBoxLayout(pCentralWidget);
	pMainLayout->setContentsMargins(0, 0, 0, 0);

	m_pVideoScene = new QGraphicsScene(this);
	m_pVideoView = new QGraphicsView(m_pVideoScene, pCentralWidget);
	m_pVideoView->setFrameShape(QFrame::NoFrame);
	m_pVideoView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_pVideoView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_pVideoView->setAlignment(Qt::AlignCenter);
	m_pVideoView->setBackgroundBrush(Qt::black);
	m_pVideoView->setStyleSheet(QStringLiteral("background-color: black;"));
	m_pVideoView->viewport()->installEventFilter(this);

	m_pVideoItem = new QGraphicsVideoItem();
	m_pVideoScene->addItem(m_pVideoItem);
	m_pVideoItem->setZValue(0);
	m_pController->setVideoOutput(m_pVideoItem->videoSink());
	connect(m_pVideoItem, &QGraphicsVideoItem::nativeSizeChanged,
		this, [this](const QSizeF&) { _layoutVideoScene(); });

	m_pSubtitleItem = new QGraphicsTextItem();
	m_pSubtitleItem->setZValue(1);
	m_pSubtitleItem->setVisible(false);
	m_pVideoScene->addItem(m_pSubtitleItem);

	pMainLayout->addWidget(m_pVideoView);
	_layoutVideoScene();

	m_pControlPanel = new QWidget(pCentralWidget);
	m_pControlPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
	QVBoxLayout* pControlLayout = new QVBoxLayout(m_pControlPanel);
	m_pProgressSlider = new QSlider(Qt::Horizontal, m_pControlPanel);
	m_pProgressSlider->hide();
	pControlLayout->addWidget(m_pProgressSlider);
	QHBoxLayout* pBtnLayout = new QHBoxLayout();
	QPushButton* pBtnOpenFile = new QPushButton(tr("Open File"), m_pControlPanel);

	m_pTimeLabel = new QLabel("00:00:00 / 00:00:00", m_pControlPanel);
	m_pTimeLabel->setStyleSheet("color: transparent;");
	m_pBtnPlayPause = new QPushButton(tr("Play"), m_pControlPanel);
	m_pBtnPlayPause->setEnabled(false);
	QPushButton* pBtnRate = new QPushButton(tr("Playback Rate"), m_pControlPanel);
	QPushButton* pBtnVolume = new QPushButton(tr("Volume"), m_pControlPanel);
	m_pBtnStopPlay = new QPushButton(tr("Stop"), m_pControlPanel);
	m_pBtnStopPlay->setEnabled(false);
	QPushButton* pBtnOpenUrl = new QPushButton(tr("Open Url"), m_pControlPanel);

	pBtnLayout->addWidget(pBtnOpenFile);
	pBtnLayout->addWidget(pBtnOpenUrl);
	pBtnLayout->addWidget(m_pTimeLabel);
	pBtnLayout->addStretch();
	pBtnLayout->addWidget(m_pBtnPlayPause);
	pBtnLayout->addStretch();
	pBtnLayout->addWidget(pBtnRate);
	pBtnLayout->addWidget(pBtnVolume);
	pBtnLayout->addWidget(m_pBtnStopPlay);

	pControlLayout->addLayout(pBtnLayout);
	m_pFilePathLabel = new QLabel(m_pControlPanel);
	pControlLayout->addWidget(m_pFilePathLabel);
	pMainLayout->addWidget(m_pControlPanel);

	m_pVolumeWidget = new QWidget(pCentralWidget);
	m_pVolumeWidget->setWindowFlags(Qt::Popup);
	m_pVolumeWidget->hide();
	QHBoxLayout* pVolumeLayout = new QHBoxLayout(m_pVolumeWidget);
	QSlider* pVolumeSlider = new QSlider(Qt::Horizontal, m_pVolumeWidget);
	pVolumeSlider->setRange(0, 100);
	pVolumeSlider->setValue(m_pController->getVolume());
	m_pVolumeLabel = new QLabel(QString::number(pVolumeSlider->value()), m_pVolumeWidget);
	pVolumeLayout->addWidget(pVolumeSlider);
	pVolumeLayout->addWidget(m_pVolumeLabel);

	m_pRateWidget = new QWidget(pCentralWidget);
	m_pRateWidget->setWindowFlags(Qt::Popup);
	m_pRateWidget->hide();
	QVBoxLayout* pRateLayout = new QVBoxLayout(m_pRateWidget);
	for (qreal rate : {2.0, 1.5, 1.25, 1.0, 0.75, 0.5})
	{
		const QString btnText = QString::number(rate, 'g', 3) + QStringLiteral("x");
		QPushButton* pButton = new QPushButton(btnText, m_pRateWidget);
		pButton->setCheckable(true);
		pButton->setAutoExclusive(true);
		pButton->setProperty("rate", rate);
		if (qFuzzyCompare(rate, 1.0))
			pButton->setChecked(true);
		pRateLayout->addWidget(pButton);
		connect(pButton, &QPushButton::clicked,
			this, &VideoPlayerMainWindow::onRateChanged);
	}

	connect(pBtnOpenFile, &QPushButton::clicked,
		this, &VideoPlayerMainWindow::onOpenFile);
	connect(m_pBtnPlayPause, &QPushButton::clicked,
		m_pController, &VideoPlayerController::playPause);
	connect(m_pBtnStopPlay, &QPushButton::clicked,
		m_pController, &VideoPlayerController::stopPlay);
	connect(pBtnVolume, &QPushButton::clicked,
		this, &VideoPlayerMainWindow::onOpenVolumeSlider);
	connect(pBtnRate, &QPushButton::clicked,
		this, &VideoPlayerMainWindow::onOpenRateMenu);
	connect(pVolumeSlider, &QSlider::valueChanged,
		this, &VideoPlayerMainWindow::onVolumeValueChanged);
	connect(m_pProgressSlider, &QSlider::sliderReleased,
		this, &VideoPlayerMainWindow::onProgressSliderLoosed);
	connect(m_pProgressSlider, &QSlider::sliderMoved,
		this, &VideoPlayerMainWindow::onProgressSliderMoved);
	connect(m_pController, &VideoPlayerController::sigShowFileOpenedMessage,
		this, &VideoPlayerMainWindow::onShowFileOpenedMessage);
	connect(m_pController, &VideoPlayerController::sigPlayStateChanged,
		this, &VideoPlayerMainWindow::onPlayStateChanged);
	connect(m_pController, &VideoPlayerController::sigProgressSliderTimeChanged,
		this, &VideoPlayerMainWindow::onProgressSliderTimeChanged);
	connect(m_pController, &VideoPlayerController::sigUpdateVideoDuration,
		this, &VideoPlayerMainWindow::onUpdateVideoDuration);
	connect(m_pController, &VideoPlayerController::sigMediaStatusChanged,
		this, &VideoPlayerMainWindow::onMediaStateChanged);
	connect(m_pController, &VideoPlayerController::sigErrorOccurred,
		this, &VideoPlayerMainWindow::onErrorOccurred);
	connect(pBtnOpenUrl, &QPushButton::clicked,
		this, &VideoPlayerMainWindow::onOpenUrlInputDialog);

#if defined(QMEDIAPLAYER) && defined(SHERPA_SUBTITLE)
	if (m_pController->subtitleService() != nullptr)
	{
		connect(m_pController->subtitleService(), &RealtimeSubtitleService::sigSubtitleTextReady,
			this, &VideoPlayerMainWindow::onSubtitleTextReady);
		connect(m_pController->subtitleService(), &RealtimeSubtitleService::sigSubtitlePartialReady,
			this, &VideoPlayerMainWindow::onSubtitlePartialReady);
	}
#endif

	QAction* pPlayPauseAction = new QAction(this);
	pPlayPauseAction->setShortcut(QKeySequence(Qt::Key_Space));
	connect(pPlayPauseAction, &QAction::triggered,
		m_pController, &VideoPlayerController::playPause);
	addAction(pPlayPauseAction);
}

void VideoPlayerMainWindow::_popupWidget(QWidget* pWidget)
{
	QPushButton* pBtn = qobject_cast<QPushButton*>(sender());
	if (pBtn == nullptr || pWidget == nullptr)
		return;
	pWidget->adjustSize();
	const int x = (pBtn->width() - pWidget->width()) / 2;
	const QPoint globalPos = pBtn->mapToGlobal(QPoint(x, -pWidget->height()));
	pWidget->move(globalPos);
	pWidget->show();
	pWidget->raise();
	pWidget->activateWindow();
}

void VideoPlayerMainWindow::_layoutVideoScene()
{
	if (m_pVideoView == nullptr || m_pVideoScene == nullptr
		|| m_pVideoItem == nullptr || m_pSubtitleItem == nullptr)
	{
		return;
	}

	const QSize viewSize = m_pVideoView->viewport()->size();
	if (viewSize.width() <= 0 || viewSize.height() <= 0)
		return;

	m_pVideoScene->setSceneRect(0, 0, viewSize.width(), viewSize.height());

	const QSizeF nativeSize = m_pVideoItem->nativeSize();
	if (nativeSize.width() > 0 && nativeSize.height() > 0)
	{
		const Qt::AspectRatioMode aspectMode = isFullScreen()
			? Qt::KeepAspectRatio
			: Qt::KeepAspectRatioByExpanding;
		const QSizeF targetSize = nativeSize.scaled(viewSize, aspectMode);
		m_pVideoItem->setSize(targetSize);
		m_pVideoItem->setPos(
			(viewSize.width() - targetSize.width()) / 2.0,
			(viewSize.height() - targetSize.height()) / 2.0);
	}
	else
	{
		m_pVideoItem->setSize(viewSize);
		m_pVideoItem->setPos(0, 0);
	}

	if (m_pSubtitleItem->isVisible())
	{
		const QRectF subtitleRect = m_pSubtitleItem->boundingRect();
		m_pSubtitleItem->setPos(
			(viewSize.width() - subtitleRect.width()) / 2.0,
			viewSize.height() - subtitleRect.height() - 24.0);
	}
}

void VideoPlayerMainWindow::_setSubtitleText(const QString& text)
{
	if (m_pSubtitleItem == nullptr)
		return;

	const QString trimmed = text.trimmed();
	if (trimmed.isEmpty())
	{
		m_pSubtitleItem->setPlainText(QString());
		m_pSubtitleItem->setVisible(false);
		return;
	}

	m_pSubtitleItem->setHtml(
		QStringLiteral("<div style=\"background-color:rgba(0,0,0,160);color:white;"
			"font-size:18px;padding:8px 16px;\">%1</div>")
			.arg(trimmed.toHtmlEscaped()));
	m_pSubtitleItem->setVisible(true);
	_layoutVideoScene();
}

#if defined(QMEDIAPLAYER) && defined(SHERPA_SUBTITLE)
void VideoPlayerMainWindow::onSubtitleTextReady(const QString& text)
{
	_setSubtitleText(text);
}

void VideoPlayerMainWindow::onSubtitlePartialReady(const QString& text)
{
	_setSubtitleText(text);
}
#endif
