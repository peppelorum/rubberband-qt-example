/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/* Copyright 2020 Particular Programs Ltd. All Rights Reserved. */

#include "Processor.h"

#include <bqaudiostream/AudioReadStreamFactory.h>
#include <bqaudiostream/AudioWriteStreamFactory.h>
#include <bqaudiostream/AudioReadStream.h>
#include <bqaudiostream/AudioWriteStream.h>
#include <bqvec/Allocators.h>

#include <rubberband/RubberBandStretcher.h>

#include <QSettings>
#include <QFileInfo>
#include <QUrl>
#include <QDir>
#include <QCoreApplication>

using namespace std;
using namespace breakfastquay;
using namespace RubberBand;

Processor::Processor() :
    m_stretcher(0),
    m_processBlock(0),
    m_previewBlock(-1),
    m_playing(false),
    m_blockSize(2048),
    m_blocks(0, 0, m_blockSize),
    m_stretchIn(0),
    m_stretchOutPtrs(0),
    m_timeRatio(1.0),
    m_pitchScale(1.0),
    m_formantPreserving(false),
    m_centreFocus(false),
    m_lastCentreFocus(false),
    m_crispness(5),
    m_hqshift(false),
    m_windowSort(1),
    m_fileReadThread(0),
    m_rs(0)
{
}

Processor::~Processor()
{
    if (m_fileReadThread) {
        m_fileReadThread->cancel();
        m_fileReadThread->wait();
        delete m_fileReadThread;
        m_fileReadThread = 0;
    }

    delete m_rs;

    clearBlocks(); // uses mutex, so must come before the locker

    QMutexLocker locker(&m_mutex);

    if (m_stretcher) {
        for (int c = 0; c < (int)m_stretcher->getChannelCount(); ++c) {
            delete[] m_stretchIn[c];
        }
        delete[] m_stretchIn;
        delete[] m_stretchOutPtrs;
        delete m_stretcher;
    }
}

int
Processor::getApplicationSampleRate() const
{
    return getSampleRate();
}

int
Processor::getApplicationChannelCount() const
{
    return m_blocks.channels;
}

string
Processor::getClientName() const
{
    return QCoreApplication::applicationName().toUtf8().data();
}

void
Processor::setSystemPlaybackBlockSize(int)
{
}

void
Processor::setSystemPlaybackSampleRate(int)
{
}

void
Processor::setSystemPlaybackLatency(int)
{
}

void
Processor::setSystemPlaybackChannelCount(int)
{
}

void
Processor::setOutputLevels(float, float)
{
}

QString
Processor::getFilename() const
{
    QMutexLocker locker(&m_mutex);
    return m_filename;
}

int
Processor::getSampleRate() const
{
    QMutexLocker locker(&m_mutex);
    return m_blocks.sampleRate;
}

int
Processor::getChannelCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_blocks.channels;
}

double 
Processor::getTimeRatio() const
{
    QMutexLocker locker(&m_mutex);
    return m_timeRatio;
}

void 
Processor::setTimeRatio(double ratio)
{
    QMutexLocker locker(&m_mutex);
    m_timeRatio = ratio;
}

double 
Processor::getPitchScale() const
{
    QMutexLocker locker(&m_mutex);
    return m_pitchScale;
}

void 
Processor::setPitchScale(double scale)
{
    QMutexLocker locker(&m_mutex);
    m_pitchScale = scale;
}

bool 
Processor::getFormantPreserving() const
{
    QMutexLocker locker(&m_mutex);
    return m_formantPreserving;
}

void 
Processor::setFormantPreserving(bool fp)
{
    QMutexLocker locker(&m_mutex);
    m_formantPreserving = fp;
}

bool 
Processor::getCentreFocus() const
{
    QMutexLocker locker(&m_mutex);
    return m_centreFocus;
}

void 
Processor::setCentreFocus(bool fp)
{
    QMutexLocker locker(&m_mutex);
    m_centreFocus = fp;
}

int 
Processor::getCrispness() const
{
    QMutexLocker locker(&m_mutex);
    return m_crispness;
}

void 
Processor::setCrispness(int c)
{
    QMutexLocker locker(&m_mutex);
    m_crispness = c;
}

bool 
Processor::getHighQualityShiftMode() const
{
    QMutexLocker locker(&m_mutex);
    return m_hqshift;
}

void 
Processor::setHighQualityShiftMode(bool h)
{
    QMutexLocker locker(&m_mutex);
    m_hqshift = h;
}

bool 
Processor::isPlaying() const
{
    QMutexLocker locker(&m_mutex);
    return m_playing;
}

void
Processor::play()
{
    QMutexLocker locker(&m_mutex);
    m_playing = true;
}

void
Processor::pause()
{
    QMutexLocker locker(&m_mutex);
    m_playing = false;
    if (m_stretcher) {
        m_stretcher->reset();
    }
}

void
Processor::rewind()
{
    QMutexLocker locker(&m_mutex);
    m_processBlock = 0;
    if (m_stretcher) {
        m_stretcher->reset();
    }
}

Processor::BlockRec::~BlockRec()
{
    clear();
}

void
Processor::BlockRec::clear()
{
    QMutexLocker locker(&mutex);
    for (int i = 0; i < (int)blocks.size(); ++i) {
        deallocate<float>(blocks[i]);
    }
    blocks.clear();
}

void
Processor::clearBlocks()
{
    QMutexLocker locker(&m_mutex);
    m_blocks.clear();
}

Processor::LoadStatus
Processor::getLoadingStatus() const
{
    QMutexLocker locker(&m_mutex);
    if (m_fileReadThread) {
        switch (m_fileReadThread->getStatus()) {
        case FileReadThread::Working: return LoadPending;
        case FileReadThread::Done: return LoadComplete;
        case FileReadThread::Cancelled: return LoadCancelled;
        case FileReadThread::OutOfMemory: return LoadOutOfMemory;
        }
    }
    // Note that this depends on m_fileReadThread always being set
    // from the moment the first file load is requested (it is not
    // deleted when file load completes, only when the next load
    // happens)
    return LoadNone;
}

Processor::PlayStatus
Processor::getPlayStatus() const
{
    PlayStatus s;
    {
        QMutexLocker locker(&m_mutex);
        s.playingBlock = m_processBlock;
        s.totalBlocks = getTotalAudioBlocks();
        s.totalFrames = getTotalAudioFrames();
        s.blockSize = m_blockSize;
        s.sampleRate = m_blocks.sampleRate;
        s.channelCount = m_blocks.channels;
        s.ratio = m_timeRatio;
    }
    s.loadStatus = getLoadingStatus();
    return s;
}

void
Processor::cancelFileLoad()
{
    QMutexLocker locker(&m_mutex);
    if (m_fileReadThread) m_fileReadThread->cancel();
}

Processor::FileReadThread::FileReadThread(Processor *p,
                                          BlockRec *rec,
                                          AudioReadStream *rs) :
    m_processor(p),
    m_blocks(rec),
    m_rs(rs),
    m_status(Working)
{
}

Processor::FileReadThread::~FileReadThread()
{
}

void
Processor::FileReadThread::cancel()
{
    m_status = Cancelled;
}

void
Processor::FileReadThread::run()
{
    int bs = m_processor->m_blockSize;
    int ch = m_rs->getChannelCount();

    while (m_status == Working) {

        //!!! this should be a call out to a function in m_processor

        float *newBlock = 0;

        try {
            newBlock = allocate<float>(bs * ch);
        } catch (const bad_alloc &) {
            m_processor->clearBlocks();
            cerr << "Failed to allocate " << bs << " frames "
                      << " of " << ch << " channels" << endl;
            m_status = OutOfMemory;
            return;
        }

        int got = m_rs->getInterleavedFrames(bs, newBlock);

        if (got < bs) {
            v_zero(newBlock + got * ch, (bs - got) * ch);
        }

        m_blocks->mutex.lock();

        if (got > 0) {
            m_blocks->blocks.push_back(newBlock);
        }

        if (got < bs) {
            if (got == 0 && !m_blocks->blocks.empty()) {
                m_blocks->lastBlockFill = bs;
            } else {
                m_blocks->lastBlockFill = got;
            }
            m_status = Done;
        }

        m_blocks->mutex.unlock();
    }

    cerr << "Finished reading data" << endl;
}    

void
Processor::open(QString filename)
{
    if (m_fileReadThread) {
        m_fileReadThread->cancel();
        m_fileReadThread->wait();
        delete m_fileReadThread;
        m_fileReadThread = 0;
    }

    delete m_rs;
    m_rs = 0;

    if (!QFileInfo(filename).exists()) {
        // try it as a url
        QUrl url(filename);
        if (url.isValid() && QFileInfo(url.toLocalFile()).exists()) {
            filename = url.toLocalFile();
        }
        if (!QFileInfo(filename).exists()) {
            url = QUrl::fromEncoded(filename.toUtf8());
            if (url.isValid() && QFileInfo(url.toLocalFile()).exists()) {
                filename = url.toLocalFile();
            }
        }
    }

    m_filename = filename;

    try {
        m_rs = AudioReadStreamFactory::createReadStream
            (m_filename.toUtf8().data()); // may throw
    } catch (...) {
        delete m_rs;
        m_rs = 0;
        throw;
    }
    if (!m_rs || !m_rs->getChannelCount()) {
        delete m_rs;
        m_rs = 0;
        throw runtime_error("Unknown audio format");
    }

    // we could reuse these, but let's be lazy to start with
    clearBlocks();

    QMutexLocker locker(&m_mutex);
    m_blocks.channels = m_rs->getChannelCount();
    m_blocks.sampleRate = m_rs->getSampleRate();

    if (m_stretcher) {
        for (int c = 0; c < (int)m_stretcher->getChannelCount(); ++c) {
            delete[] m_stretchIn[c];
        }
        delete[] m_stretchIn;
        delete[] m_stretchOutPtrs;
        delete m_stretcher;
        m_stretcher = 0;
    }

    m_stretcher = new RubberBandStretcher
        (m_blocks.sampleRate,
         m_blocks.channels,
         RubberBandStretcher::OptionProcessRealTime |
         RubberBandStretcher::OptionDetectorCompound,
         m_timeRatio,
         m_pitchScale);

    m_stretcher->setMaxProcessSize(m_blockSize);

    m_stretchIn = new float *[m_stretcher->getChannelCount()];

    for (int c = 0; c < (int)m_stretcher->getChannelCount(); ++c) {
        m_stretchIn[c] = new float[m_blockSize];
    }
    m_stretchOutPtrs = new float *[m_stretcher->getChannelCount()];

    m_fileReadThread = new FileReadThread(this, &m_blocks, m_rs);
    m_fileReadThread->start();

    m_stretcher->reset();

    m_filename = filename;

    m_processBlock = 0;
    m_previewBlock = -1;
}

int
Processor::getTotalAudioBlocks() const
{
    return getTotalAudioBlocks(m_blocks);
}

int
Processor::getTotalAudioBlocks(const BlockRec &rec) 
{
    QMutexLocker locker(&rec.mutex);
    int b = rec.blocks.size();
    return b;
}    

int
Processor::getTotalAudioFrames() const
{
    return getTotalAudioFrames(m_blocks);
}

int
Processor::getTotalAudioFrames(const BlockRec &rec) 
{
    QMutexLocker locker(&rec.mutex);
    int b = rec.blocks.size();
    int lb = rec.lastBlockFill;
    if (b == 0) return 0;
    else return((b-1) * rec.blockSize) + lb;
}

int
Processor::getPlayingAudioBlock() const
{
    QMutexLocker locker(&m_mutex);
    return m_processBlock;
}

float
Processor::getPlayProportion() const
{
    int p = getPlayingAudioBlock();
    int n = getTotalAudioBlocks();
    float proportion = 0.f;
    if (n > 0) {
        proportion = float(p)/float(n);
    }
    if (proportion > 1.f) {
        proportion = 1.f;
    }
    return proportion;
}

int
Processor::getBlockSize() const
{
    QMutexLocker locker(&m_mutex);
    return m_blockSize;
}

void
Processor::configure(RubberBandStretcher *stretcher,
                     int crispness,
                     bool formantPreserving,
                     bool hqshift)
{
    stretcher->setFormantOption
        (formantPreserving ?
         RubberBandStretcher::OptionFormantPreserved :
         RubberBandStretcher::OptionFormantShifted);

    stretcher->setPitchOption
        (hqshift ?
         RubberBandStretcher::OptionPitchHighQuality :
         RubberBandStretcher::OptionPitchHighSpeed);

    switch (crispness) {
    case 0:
        stretcher->setTransientsOption(RubberBandStretcher::OptionTransientsSmooth);
        stretcher->setPhaseOption(RubberBandStretcher::OptionPhaseIndependent);
        stretcher->setDetectorOption(RubberBandStretcher::OptionDetectorCompound);
        break;
    case 1:
        stretcher->setTransientsOption(RubberBandStretcher::OptionTransientsCrisp);
        stretcher->setPhaseOption(RubberBandStretcher::OptionPhaseIndependent);
        stretcher->setDetectorOption(RubberBandStretcher::OptionDetectorSoft);
        break;
    case 2:
        stretcher->setTransientsOption(RubberBandStretcher::OptionTransientsSmooth);
        stretcher->setPhaseOption(RubberBandStretcher::OptionPhaseIndependent);
        stretcher->setDetectorOption(RubberBandStretcher::OptionDetectorCompound);
        break;
    case 3:
        stretcher->setTransientsOption(RubberBandStretcher::OptionTransientsSmooth);
        stretcher->setPhaseOption(RubberBandStretcher::OptionPhaseLaminar);
        stretcher->setDetectorOption(RubberBandStretcher::OptionDetectorCompound);
        break;
    case 4:
        stretcher->setTransientsOption(RubberBandStretcher::OptionTransientsMixed);
        stretcher->setPhaseOption(RubberBandStretcher::OptionPhaseLaminar);
        stretcher->setDetectorOption(RubberBandStretcher::OptionDetectorCompound);
        break;
    case 5:
        stretcher->setTransientsOption(RubberBandStretcher::OptionTransientsCrisp);
        stretcher->setPhaseOption(RubberBandStretcher::OptionPhaseLaminar);
        stretcher->setDetectorOption(RubberBandStretcher::OptionDetectorCompound);
        break;
    case 6:
        stretcher->setTransientsOption(RubberBandStretcher::OptionTransientsCrisp);
        stretcher->setPhaseOption(RubberBandStretcher::OptionPhaseIndependent);
        stretcher->setDetectorOption(RubberBandStretcher::OptionDetectorCompound);
        break;
    }        
}

int
Processor::getSourceSamples(float *const *samples, int nchannels, int nframes)
{
    bool locked = false;
    bool unavailable = false;

    if (!m_mutex.tryLock()) {
	unavailable = true;
    } else {
	locked = true;
        if (!m_blocks.channels || m_blocks.blocks.empty()) {
	    unavailable = true;
        } else if (!m_stretcher) {
            unavailable = true;
        } else if (m_playing && (m_stretcher->available() < 0)) {
	    unavailable = true;
        } else if (!m_playing && (m_previewBlock < 0)) {
            unavailable = true;
	}
    }
    
    if (unavailable) {

        // not playing, nothing to play, or mutex unavailable: return zeros
        for (int c = 0; c < nchannels; ++c) {
            const int n = nframes;
            for (int i = 0; i < n; ++i) {
                samples[c][i] = 0.f;
            }
        }

        if (locked) m_mutex.unlock();
        return nframes;
    }

    const double timeRatio = m_timeRatio;
    const double pitchScale = m_pitchScale;
    const bool hqshift = m_hqshift;
    const bool formantPreserving = m_formantPreserving;
    const int crispness = m_crispness;

    int windowSort = 1;

    if (crispness == 6) {
        windowSort = 0;
    } else if (crispness == 0 || crispness == 1) {
        windowSort = 2;
    }
    
    if (m_windowSort != windowSort ||
        m_centreFocus != m_lastCentreFocus ||
        !m_stretcher ||
        nchannels || m_stretcher->getChannelCount()) {
        delete m_stretcher;
        RubberBandStretcher::Options options =
            RubberBandStretcher::OptionProcessRealTime |
            RubberBandStretcher::OptionTransientsCrisp |
            RubberBandStretcher::OptionPhaseIndependent;

        if (windowSort == 0) options |= RubberBandStretcher::OptionWindowShort;
        if (windowSort == 2) options |= RubberBandStretcher::OptionWindowLong;
        if (m_centreFocus) options |= RubberBandStretcher::OptionChannelsTogether;

        m_stretcher = new RubberBandStretcher
            (m_blocks.sampleRate, nchannels, options, timeRatio, pitchScale);
        m_windowSort = windowSort;
    }

    // These calls are very cheap if they won't change anything, so no
    // harm in calling them every time
    m_stretcher->setTimeRatio(timeRatio);
    m_stretcher->setPitchScale(pitchScale);

    configure(m_stretcher, crispness, formantPreserving, hqshift);

    m_lastCentreFocus = m_centreFocus;

    // We de-interleave the audio data and write the input for the
    // stretcher into m_stretchIn.  m_blockSize is an arbitrary size
    // which is both the number of interleaved frames in each block of
    // m_blocks, and the amount of space allocated for m_stretchIn:
    // the maximum we can de-interleave and feed in any one chunk.

    int done = 0;
    int fileChannels = m_blocks.channels;

    while (done < (int)nframes) {

//        cout << "getSourceSamples: nframes = "<< nframes << ", done = " << done << ", m_processBlock = " << m_processBlock << ", blocks = " << m_blocks.blocks.size() << endl;

        int available = m_stretcher->available();
        if (available < 0) break;

        int reqd = m_stretcher->getSamplesRequired();

        if (available < ((int)nframes - done) || reqd > 0) {
        
            int toProcess = m_blockSize;

            bool lastBlock = false;
            bool ended = false;
            int block = getAndUpdateBlockNo(lastBlock, ended);

            if (ended) {
                m_playing = false;
                emit playEnded();
                break;
            }
            
            float *source = 0;

            m_blocks.mutex.lock();
            if (m_blocks.blocks.empty()) {
                m_blocks.mutex.unlock();
                m_playing = false;
                emit playEnded();
                break;
            }
            if (block >= (int)m_blocks.blocks.size()) {
                block = m_blocks.blocks.size() - 1;
            }
            if (block < 0) {
                block = 0;
            }
            source = m_blocks.blocks[block];
            int lastBlockFill = m_blocks.lastBlockFill;
            m_blocks.mutex.unlock();

            if (lastBlock) {
                toProcess = lastBlockFill;
                m_playing = false;
                emit playEnded();
            }

            // nchannels is the number of channels required for output
            // to the audio device. This value came directly from the
            // device handler, and the stretcher has been configured
            // with a matching channel count. fileChannels is the
            // number in the audio file, which may differ. We always
            // run the stretcher at the audio device channel count.

            for (int c = 0; c < nchannels && c < fileChannels; ++c) {
                for (int i = 0; i < toProcess; ++i) {
                    m_stretchIn[c][i] = source[(i * fileChannels) + c];
                }
            }
            
            for (int c = fileChannels; c < nchannels; ++c) {
                if (c > 0) {
                    // excess channels on audio output: duplicate the
                    // first file channel for them (an arbitrary decision)
                    for (int i = 0; i < toProcess; ++i) {
                        m_stretchIn[c][i] = m_stretchIn[0][i];
                    }
                } else {
                    // zero channels in the file!
                    for (int i = 0; i < toProcess; ++i) {
                        m_stretchIn[c][i] = 0.f;
                    }
                }
            }
            
            m_stretcher->process(m_stretchIn, toProcess, false);
        }

        int count = m_stretcher->available();
        if (count == 0) continue;

        if (count > ((int)nframes - done)) count = (int)nframes - done;

        // m_stretchOutPtrs is a set of temporary pointers indicating
        // where to write the output to, as a set of offsets into the
        // desired samples arrays
        for (int c = 0; c < nchannels; ++c) {
            m_stretchOutPtrs[c] = samples[c] + done;
        }

        m_stretcher->retrieve(m_stretchOutPtrs, count);
        done += count;
    }

    // any excess should be filled up with zero samples
    for (int c = 0; c < nchannels; ++c) {
        for (int i = done; i < (int)nframes; ++i) {
            samples[c][i] = 0.f;
        }
    }

    m_mutex.unlock();

    return nframes;
}

int
Processor::getAndUpdateBlockNo(bool &lastBlock, bool &ended)
{
    int block = m_processBlock;
        
    lastBlock = false;
    ended = false;
    
    m_blocks.mutex.lock();

    int n = (int)m_blocks.blocks.size();
    
    ++m_processBlock;

    lastBlock = (block+1 >= n);

    if (lastBlock) {
        m_processBlock = 0;
    }
    m_blocks.mutex.unlock();

    return block;
}

