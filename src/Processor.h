/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/* Copyright 2020 Particular Programs Ltd. All Rights Reserved. */

#ifndef EXAMPLE_PROCESSOR_H
#define EXAMPLE_PROCESSOR_H

#include <bqaudioio/ApplicationPlaybackSource.h>
#include <bqaudioio/AudioFactory.h>
#include <bqaudiostream/AudioReadStream.h>

#include <iostream>

#include <set>
#include <vector>

#include <QThread>
#include <QMutex>

namespace RubberBand {
    class RubberBandStretcher;
}

class Processor : public QObject,
                  public breakfastquay::ApplicationPlaybackSource
{
    Q_OBJECT

public:
    Processor();
    virtual ~Processor();

    // ApplicationPlaybackSource methods
    int getApplicationSampleRate() const override;
    int getApplicationChannelCount() const override;
    std::string getClientName() const override;
    
    void setSystemPlaybackBlockSize(int) override;
    void setSystemPlaybackSampleRate(int) override;
    void setSystemPlaybackLatency(int) override;
    void setSystemPlaybackChannelCount(int) override;
    void setOutputLevels(float, float) override;

    int getSourceSamples(float *const *samples, int nchannels, int nframes) override;
    
    void cancelFileLoad();

    QString getFilename() const;
    QString getTrackName() const;
    
    int getSampleRate() const;
    int getChannelCount() const;
    
    double getTimeRatio() const;
    void setTimeRatio(double ratio);

    double getPitchScale() const;
    void setPitchScale(double scale);

    bool getFormantPreserving() const;
    void setFormantPreserving(bool fp);

    bool getCentreFocus() const;
    void setCentreFocus(bool cf);

    int getCrispness() const;
    void setCrispness(int c);

    bool getHighQualityShiftMode() const;
    void setHighQualityShiftMode(bool);

    bool isPlaying() const;

    int getTotalAudioBlocks() const;
    int getTotalAudioFrames() const;
    int getPlayingAudioBlock() const;
    float getPlayProportion() const;
    bool isAtRangeStartOrEnd() const;
    int getBlockSize() const;
    
    enum LoadStatus {
        LoadNone,
        LoadPending,
        LoadCancelled,
        LoadOutOfMemory,
        LoadComplete
    };
    LoadStatus getLoadingStatus() const;

    struct PlayStatus {
        int playingBlock;
        int totalBlocks;
        int totalFrames;
        int blockSize;
        int sampleRate;
        int channelCount;
        double ratio;
        LoadStatus loadStatus;
    };
    PlayStatus getPlayStatus() const;
    
    void open(QString filename); // may throw

signals:
    void playEnded();

public slots:
    void play();
    void pause();
    void rewind();

protected:
    QString m_filename;
    QString m_trackName;

    breakfastquay::SystemPlaybackTarget *m_target;
    RubberBand::RubberBandStretcher *m_stretcher;

    int m_processBlock;
    int m_previewBlock;
    bool m_playing;

    mutable QMutex m_mutex;

    const int m_blockSize;

    struct BlockRec {
        BlockRec(int rate, int c, int bs) :
            sampleRate(rate),
            channels(c),
            blockSize(bs),
            lastBlockFill(bs) { }
        ~BlockRec();
        void clear();

        int sampleRate;
        int channels;
        int blockSize;
        int lastBlockFill;
        std::vector<float *> blocks;
        mutable QMutex mutex;
    };

    BlockRec m_blocks;
    void clearBlocks();

    float **m_stretchIn;
    float **m_stretchOutPtrs;

    double m_timeRatio;
    double m_pitchScale;

    bool m_formantPreserving;
    bool m_centreFocus;
    bool m_lastCentreFocus;
    int m_crispness;
    bool m_hqshift;
    int m_windowSort;

    void configure(RubberBand::RubberBandStretcher *,
                   int crispness,
                   bool formantPreserving,
                   bool hqshift);

    static int getTotalAudioBlocks(const BlockRec &);
    static int getTotalAudioFrames(const BlockRec &);

    int getAndUpdateBlockNo(bool &lastBlock, bool &playEnded);
    
    class FileReadThread : public QThread
    {
    public:
        FileReadThread(Processor *processor, BlockRec *blocks,
                       breakfastquay::AudioReadStream *rs);
        virtual ~FileReadThread();
        
        void cancel();
        
        enum Status {
            Working,
            Done,
            Cancelled,
            OutOfMemory
        };
        Status getStatus() const { return m_status; }

    protected:
        Processor *m_processor;
        BlockRec *m_blocks;
        breakfastquay::AudioReadStream *m_rs; // belongs to containing Processor object
        void run() override;
        Status m_status;
    };

    FileReadThread *m_fileReadThread;
    breakfastquay::AudioReadStream *m_rs;
};

#endif
