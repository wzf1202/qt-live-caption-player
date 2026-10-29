#include <QApplication>
#include <QLocale>
#include <QTranslator>

#include "videoplayermainwindow.h"
#include "videoplayercontroller.h"
#include "playerfactory.h"

int main(int argc, char* argv[])
{
#if defined(QMEDIAPLAYER) && defined(SHERPA_SUBTITLE)
	qputenv("QT_MEDIA_BACKEND", "ffmpeg");
#endif

	QApplication application(argc, argv);
	QTranslator translator(&application);
	const QStringList uiLanguages = QLocale::system().uiLanguages();
	for (const QString &locale : uiLanguages)
	{
		const QString baseName = "videoplayer_" + QLocale(locale).name();
		if (translator.load(":/i18n/" + baseName))
		{
			application.installTranslator(&translator);
			break;
		}
	}

	IMediaPlayer* pPlayer = PlayerFactory::create(&application);
	VideoPlayerController* pController = new VideoPlayerController(&application, pPlayer);
	VideoPlayerMainWindow mainWindow(nullptr, pController);
	mainWindow.show();
	return QApplication::exec();
}
