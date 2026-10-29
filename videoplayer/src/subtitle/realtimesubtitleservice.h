/* ------------------------------------------------------
//     文件名      :    realtimesubtitleservice.h
//     创建者      :    雷杨博
//     创建时间    :    2026-09-04
//     功能描述    :    基于 sherpa-onnx Online 的实时字幕识别服务
// -----------------------------------------------------*/
#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace sherpa_onnx
{
namespace cxx
{
class OnlineRecognizer;
class OnlineStream;
}
}

class RealtimeSubtitleService : public QObject
{
	Q_OBJECT

public:
	explicit RealtimeSubtitleService(QObject* pParent);
	~RealtimeSubtitleService() override;

	bool initModel(const QString& modelDir);
	bool isReady() const;
	void start();
	void stop();
	void reset();

public slots:
	void onAudioPcmReady(qint64 ptsMs, const QByteArray& pcm);

signals:
	void sigSubtitlePartialReady(const QString& text);
	void sigSubtitleTextReady(const QString& text);
	void sigAsrFailed(const QString& errorString);

private slots:
	void onPartialText(const QString& text);
	void onFinalText(const QString& text);

private:
	void _startWorker();
	void _stopWorker();
	void _clearQueue();
	void _workerLoop();
	void _flushAccumulated(bool bForce);
	void _enqueuePcm(std::vector<float>&& samples);
	QString _cleanText(const QString& text) const;

private:
	std::unique_ptr<sherpa_onnx::cxx::OnlineRecognizer> m_spRecognizer;
	std::unique_ptr<sherpa_onnx::cxx::OnlineStream> m_spStream;
	std::deque<std::vector<float>> m_pcmQueue = {};
	std::vector<float> m_accumPcm = {};
	std::mutex m_mutex = {};
	std::condition_variable m_cv = {};
	std::thread m_worker;
	std::atomic<bool> m_bReady = false;
	std::atomic<bool> m_bRunning = false;
	std::atomic<bool> m_bStopWorker = false;
	std::atomic<bool> m_bResetStream = false;
	QString m_lastPartial = {};
};
