
TEMPLATE = app

QT += gui widgets

CONFIG += release c++11 object_parallel_to_source

TARGET = "Example"

DEFINES += USE_SPEEX USE_KISSFFT

INCLUDEPATH += \
        ext/rubberband \
        ext/rubberband/src \
        ext/bqresample \
        ext/bqvec \
        ext/bqvec/bqvec \
        ext/bqaudioio \
        ext/bqaudioio/bqaudioio \
        ext/bqaudiostream \
        ext/bqaudiostream/bqaudiostream \
        ext/bqthingfactory
        
RB_SOURCES += \
      	ext/rubberband/src/RubberBandStretcher.cpp \
	ext/rubberband/src/StretcherProcess.cpp \
	ext/rubberband/src/StretchCalculator.cpp \
	ext/rubberband/src/base/Profiler.cpp \
	ext/rubberband/src/dsp/AudioCurveCalculator.cpp \
	ext/rubberband/src/audiocurves/CompoundAudioCurve.cpp \
	ext/rubberband/src/audiocurves/SpectralDifferenceAudioCurve.cpp \
	ext/rubberband/src/audiocurves/HighFrequencyAudioCurve.cpp \
	ext/rubberband/src/audiocurves/SilentAudioCurve.cpp \
	ext/rubberband/src/audiocurves/ConstantAudioCurve.cpp \
	ext/rubberband/src/audiocurves/PercussiveAudioCurve.cpp \
	ext/rubberband/src/dsp/Resampler.cpp \
	ext/rubberband/src/dsp/FFT.cpp \
	ext/rubberband/src/system/Allocators.cpp \
	ext/rubberband/src/system/sysutils.cpp \
	ext/rubberband/src/system/Thread.cpp \
	ext/rubberband/src/StretcherChannelData.cpp \
	ext/rubberband/src/StretcherImpl.cpp \
	ext/rubberband/src/speex/resample.c \
	ext/rubberband/src/kissfft/kiss_fft.c \
	ext/rubberband/src/kissfft/kiss_fftr.c

BQ_SOURCES += \
	ext/bqvec/src/Allocators.cpp \
	ext/bqvec/src/Barrier.cpp \
	ext/bqvec/src/VectorOpsComplex.cpp \
	ext/bqresample/src/Resampler.cpp \
	ext/bqresample/speex/resample.c \
	ext/bqaudioio/src/AudioFactory.cpp \
	ext/bqaudioio/src/JACKAudioIO.cpp \
	ext/bqaudioio/src/Log.cpp \
	ext/bqaudioio/src/PortAudioIO.cpp \
	ext/bqaudioio/src/PulseAudioIO.cpp \
	ext/bqaudioio/src/ResamplerWrapper.cpp \
	ext/bqaudioio/src/SystemPlaybackTarget.cpp \
	ext/bqaudioio/src/SystemRecordSource.cpp \
        ext/bqaudiostream/src/AudioReadStream.cpp \
        ext/bqaudiostream/src/AudioReadStreamFactory.cpp \
        ext/bqaudiostream/src/AudioStreamExceptions.cpp
        
win32-msvc* {
    DEFINES += HAVE_PORAUDIO HAVE_MEDIAFOUNDATION _USE_MATH_DEFINES
    LIBS += -lportaudio -lmfplat -lmfreadwrite -lmfuuid -lpropsys -ladvapi32 -lwinmm -lws2_32
}

macx* {
    DEFINES += HAVE_COREAUDIO HAVE_PORTAUDIO HAVE_VDSP USE_PTHREADS
    LIBS += -lportaudio -framework CoreAudio -framework CoreMidi -framework AudioUnit -framework AudioToolbox -framework CoreFoundation -framework CoreServices -framework Accelerate
}

linux* {
    DEFINES += HAVE_SNDFILE HAVE_LIBPULSE USE_PTHREADS
    LIBS += -lsndfile -lpulse
}

for (file, BQ_SOURCES)       { SOURCES += $$file }
for (file, RB_SOURCES)       { SOURCES += $$file }

HEADERS += \
        src/Interface.h \
        src/Processor.h

SOURCES += \
        src/Interface.cpp \
        src/Processor.cpp \
        src/main.cpp
        
