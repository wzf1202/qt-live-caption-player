#pragma once

#include <QString>
#include <QStringList>
#include <QFileInfo>

namespace fileutils
{
	inline const QStringList supportedSuffixes = {"mp4", "mkv", "avi", "mov", "wmv", "flv"};

	inline bool isSupportedVideoFile(const QString& filePath)
	{
		return supportedSuffixes.contains(QFileInfo(filePath).suffix().toLower());
	}
}
