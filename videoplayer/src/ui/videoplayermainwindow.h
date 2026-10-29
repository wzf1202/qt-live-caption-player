#pragma once

#include <QMainWindow>
#include <QString>

#include "imediaplayer.h"

class QDragEnterEvent;
class QDropEvent;
class QDragLeaveEvent;
class QEvent;
class QGraphicsScene;
class QGraphicsTextItem;
class QGraphicsVideoItem;
class QGraphicsView;
class QLabel;
class QPushButton;
class QResizeEvent;
class QWidget;
class VideoPlayerController;
class QSlider;

class VideoPlayerMainWindow : public QMainWindow
{
	Q_OBJECT

public:
	VideoPlayerMainWindow(QWidget* pParent, VideoPlayerController* pController);
	~VideoPlayerMainWindow() override = default;

protected:
	void dragEnterEvent(QDragEnterEvent* pEvent) override;
	void dropEvent(QDropEvent* pEvent) override;
	void dragLeaveEvent(QDragLeaveEvent* pEvent) override;
	bool eventFilter(QObject* pWatched, QEvent* pEvent) override;
	void resizeEvent(QResizeEvent* pEvent) override;

private slots:
	void onOpenFile();
	void onShowFileOpenedMessage(const QString& fileName, const QString& absolutePath);
	void onPlayStateChanged(IMediaPlayer::PlayState state);
	void onToggleFullScreen();
	void onOpenVolumeSlider();
	void onVolumeValueChanged(int volume);
	void onProgressSliderTimeChanged(qint64 positionMs);
	void onUpdateVideoDuration(qint64 durationMs);
	void onMediaStateChanged(IMediaPlayer::MediaState status);
	void onErrorOccurred(IMediaPlayer::Error error, const QString& errorString);
	void onProgressSliderLoosed();
	void onProgressSliderMoved(int positionMs);
	void onOpenRateMenu();
	void onRateChanged();
	void onOpenUrlInputDialog();
#if defined(QMEDIAPLAYER) && defined(SHERPA_SUBTITLE)
	void onSubtitleTextReady(const QString& text);
	void onSubtitlePartialReady(const QString& text);
#endif

private:
	void _initUI();
	void _popupWidget(QWidget* pWidget);
	void _layoutVideoScene();
	void _setSubtitleText(const QString& text);

private:
	VideoPlayerController* m_pController = nullptr;
	QGraphicsView* m_pVideoView = nullptr;
	QGraphicsScene* m_pVideoScene = nullptr;
	QGraphicsVideoItem* m_pVideoItem = nullptr;
	QGraphicsTextItem* m_pSubtitleItem = nullptr;
	QWidget* m_pControlPanel = nullptr;
	QPushButton* m_pBtnPlayPause = nullptr;
	QPushButton* m_pBtnStopPlay = nullptr;
	QLabel* m_pFilePathLabel = nullptr;
	QWidget* m_pVolumeWidget = nullptr;
	QLabel* m_pVolumeLabel = nullptr;
	QWidget* m_pRateWidget = nullptr;
	QSlider* m_pProgressSlider = nullptr;
	QLabel* m_pTimeLabel = nullptr;
};
