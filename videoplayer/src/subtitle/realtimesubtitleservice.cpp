/* ------------------------------------------------------
//     文件名      :    realtimesubtitleservice.cpp
//     创建者      :    雷杨博
//     创建时间    :    2026-09-04
//     功能描述    :    基于 sherpa-onnx Online 的实时字幕识别服务
// -----------------------------------------------------*/
#include "realtimesubtitleservice.h"

#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QtGlobal>

#include "sherpa-onnx/c-api/cxx-api.h"

#include <utility>

namespace
{
	constexpr int SAMPLE_RATE = 16000;
	constexpr int BYTES_PER_SAMPLE = 2;
	constexpr int CHUNK_SAMPLES = 1280;
	constexpr int MAX_QUEUE_CHUNKS = 64;
}

RealtimeSubtitleService::RealtimeSubtitleService(QObject* pParent)
	: QObject(pParent)
{
}

RealtimeSubtitleService::~RealtimeSubtitleService()
{
	stop();
	_stopWorker();
	m_spStream.reset();
	m_spRecognizer.reset();
	m_bReady = false;
}

bool RealtimeSubtitleService::initModel(const QString& modelDir)
{
	stop();
	_stopWorker();
	m_spStream.reset();
	m_spRecognizer.reset();
	m_bReady = false;
	m_lastPartial.clear();

	if (modelDir.isEmpty())
	{
		emit this->sigAsrFailed(tr("Sherpa model directory is empty"));
		return false;
	}

	const QDir dir(modelDir);
	const QString encoder = dir.filePath(QStringLiteral("encoder-epoch-99-avg-1.int8.onnx"));
	const QString decoder = dir.filePath(QStringLiteral("decoder-epoch-99-avg-1.onnx"));
	const QString joiner = dir.filePath(QStringLiteral("joiner-epoch-99-avg-1.int8.onnx"));
	const QString tokens = dir.filePath(QStringLiteral("tokens.txt"));
	if (!QFileInfo::exists(encoder) || !QFileInfo::exists(decoder)
		|| !QFileInfo::exists(joiner) || !QFileInfo::exists(tokens))
	{
		emit this->sigAsrFailed(tr("Sherpa model files missing under: %1").arg(modelDir));
		return false;
	}

	using sherpa_onnx::cxx::OnlineRecognizer;
	using sherpa_onnx::cxx::OnlineRecognizerConfig;

	OnlineRecognizerConfig config;
	config.model_config.transducer.encoder = encoder.toStdString();
	config.model_config.transducer.decoder = decoder.toStdString();
	config.model_config.transducer.joiner = joiner.toStdString();
	config.model_config.tokens = tokens.toStdString();
	config.model_config.num_threads = 2;
	config.model_config.provider = "cpu";
	config.decoding_method = "greedy_search";
	config.enable_endpoint = true;
	config.rule1_min_trailing_silence = 0.8f;
	config.rule2_min_trailing_silence = 0.6f;
	config.rule3_min_utterance_length = 15.0f;

	OnlineRecognizer recognizer = OnlineRecognizer::Create(config);
	if (!recognizer.Get())
	{
		emit this->sigAsrFailed(tr("Failed to create sherpa OnlineRecognizer"));
		return false;
	}

	m_spRecognizer = std::make_unique<OnlineRecognizer>(std::move(recognizer));
	m_bReady = true;
	_startWorker();
	return true;
}

bool RealtimeSubtitleService::isReady() const
{
	return m_bReady;
}

void RealtimeSubtitleService::start()
{
	if (!m_bReady || m_spRecognizer == nullptr)
		return;

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_accumPcm.clear();
		m_pcmQueue.clear();
		m_lastPartial.clear();
		m_bResetStream = false;
		m_spStream = std::make_unique<sherpa_onnx::cxx::OnlineStream>(
			m_spRecognizer->CreateStream());
		if (m_spStream == nullptr || !m_spStream->Get())
		{
			m_spStream.reset();
			return;
		}
	}

	m_bRunning = true;
	m_cv.notify_all();
}

void RealtimeSubtitleService::stop()
{
	m_bRunning = false;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_accumPcm.clear();
		m_pcmQueue.clear();
		m_spStream.reset();
		m_lastPartial.clear();
	}
	m_cv.notify_all();
}

void RealtimeSubtitleService::reset()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_accumPcm.clear();
	m_pcmQueue.clear();
	m_lastPartial.clear();
	m_bResetStream = true;
	m_cv.notify_all();
}

void RealtimeSubtitleService::onAudioPcmReady(qint64, const QByteArray& pcm)
{
	if (!m_bRunning || !m_bReady || pcm.isEmpty() || pcm.size() < BYTES_PER_SAMPLE)
		return;

	const int sampleCount = pcm.size() / BYTES_PER_SAMPLE;
	const qint16* pSamples = reinterpret_cast<const qint16*>(pcm.constData());
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_accumPcm.reserve(m_accumPcm.size() + static_cast<size_t>(sampleCount));
		for (int i = 0; i < sampleCount; ++i)
			m_accumPcm.push_back(static_cast<float>(pSamples[i]) / 32768.0f);
		_flushAccumulated(false);
	}
	m_cv.notify_all();
}

void RealtimeSubtitleService::onPartialText(const QString& text)
{
	emit this->sigSubtitlePartialReady(text);
}

void RealtimeSubtitleService::onFinalText(const QString& text)
{
	emit this->sigSubtitleTextReady(text);
}

void RealtimeSubtitleService::_startWorker()
{
	if (m_worker.joinable())
		return;

	m_bStopWorker = false;
	m_worker = std::thread([this]()
	{
		_workerLoop();
	});
}

void RealtimeSubtitleService::_stopWorker()
{
	m_bStopWorker = true;
	m_cv.notify_all();
	if (m_worker.joinable())
		m_worker.join();
}

void RealtimeSubtitleService::_clearQueue()
{
	m_pcmQueue.clear();
	m_accumPcm.clear();
}

void RealtimeSubtitleService::_flushAccumulated(bool bForce)
{
	while (static_cast<int>(m_accumPcm.size()) >= CHUNK_SAMPLES
		|| (bForce && !m_accumPcm.empty()))
	{
		const size_t take = bForce && static_cast<int>(m_accumPcm.size()) < CHUNK_SAMPLES
			? m_accumPcm.size()
			: static_cast<size_t>(CHUNK_SAMPLES);
		std::vector<float> chunk(m_accumPcm.begin(), m_accumPcm.begin() + static_cast<std::ptrdiff_t>(take));
		m_accumPcm.erase(m_accumPcm.begin(), m_accumPcm.begin() + static_cast<std::ptrdiff_t>(take));
		_enqueuePcm(std::move(chunk));
		if (bForce)
			break;
	}
}

void RealtimeSubtitleService::_enqueuePcm(std::vector<float>&& samples)
{
	if (samples.empty())
		return;

	m_pcmQueue.push_back(std::move(samples));
	while (static_cast<int>(m_pcmQueue.size()) > MAX_QUEUE_CHUNKS)
	{
		m_pcmQueue.pop_front();
		qWarning("RealtimeSubtitleService: ASR queue overflow, dropping oldest chunk");
	}
}

QString RealtimeSubtitleService::_cleanText(const QString& text) const
{
	QString cleaned = text;
	cleaned.replace(QChar(0x2581), QLatin1Char(' '));
	return cleaned.trimmed();
}

void RealtimeSubtitleService::_workerLoop()
{
	while (!m_bStopWorker)
	{
		std::vector<float> chunk;
		bool needReset = false;
		{
			std::unique_lock<std::mutex> lock(m_mutex);
			m_cv.wait(lock, [this]()
			{
				return m_bStopWorker || m_bResetStream || !m_pcmQueue.empty();
			});
			if (m_bStopWorker)
				return;

			if (m_bResetStream)
			{
				needReset = true;
				m_bResetStream = false;
				_clearQueue();
			}
			else if (!m_pcmQueue.empty())
			{
				chunk = std::move(m_pcmQueue.front());
				m_pcmQueue.pop_front();
			}
		}

		if (needReset)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_spRecognizer != nullptr && m_spStream != nullptr && m_spStream->Get())
				m_spRecognizer->Reset(m_spStream.get());
			m_lastPartial.clear();
			continue;
		}

		if (chunk.empty() || !m_bRunning)
			continue;

		QString clean;
		bool isEndpoint = false;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_spRecognizer == nullptr || m_spStream == nullptr || !m_spStream->Get())
				continue;

			sherpa_onnx::cxx::OnlineRecognizer* pRecognizer = m_spRecognizer.get();
			sherpa_onnx::cxx::OnlineStream* pStream = m_spStream.get();
			pStream->AcceptWaveform(SAMPLE_RATE, chunk.data(), static_cast<int32_t>(chunk.size()));
			while (pRecognizer->IsReady(pStream))
				pRecognizer->Decode(pStream);

			const sherpa_onnx::cxx::OnlineRecognizerResult result = pRecognizer->GetResult(pStream);
			clean = _cleanText(QString::fromUtf8(result.text.c_str()));
			isEndpoint = pRecognizer->IsEndpoint(pStream);
			if (isEndpoint)
				pRecognizer->Reset(pStream);
		}

		if (isEndpoint)
		{
			if (!clean.isEmpty())
			{
				QMetaObject::invokeMethod(this, "onFinalText", Qt::QueuedConnection,
					Q_ARG(QString, clean));
			}
			m_lastPartial.clear();
		}
		else if (!clean.isEmpty() && clean != m_lastPartial)
		{
			m_lastPartial = clean;
			QMetaObject::invokeMethod(this, "onPartialText", Qt::QueuedConnection,
				Q_ARG(QString, clean));
		}
	}
}
